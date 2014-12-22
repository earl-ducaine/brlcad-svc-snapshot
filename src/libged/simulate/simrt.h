/*                       S I M R T . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/*
 * The header for the raytrace based manifold generator
 * for the simulate command.
 *
 *
 */

#ifndef LIBGED_SIMULATE_SIMRT_H
#define LIBGED_SIMULATE_SIMRT_H

#include "common.h"

/* System Headers */
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

/* Public Headers */
#include "vmath.h"
#include "db.h"

/* Private Headers */
#include "../ged_private.h"

#define NOT_FOUND 0
#define MAX_MANIFOLDS 4
#define MAX_CONTACTS_PER_MANIFOLD 100


/**
 * Deletes a prim/comb if it exists.
 *
 * TODO: lower to librt
 */
int
sim_kill(struct ged *gedp, char *name);



/**
 * Adds a prim/comb to an existing comb or creates it if not existing.
 *
 * TODO: lower to librt
 */
int
add_to_comb(struct ged *gedp, char *target, char *add);


/**
 * This function creates and inserts a RPP
 * Used to show AABB overlap volume
 *
 * TODO: this function should be lowered to librt
 */
int
make_rpp(struct ged *gedp, vect_t max, vect_t min, char* name);




struct sim_contact {
    vect_t ptA;
    vect_t ptB;
    vect_t normalWorldOnB;
    fastf_t depth;
};




struct sim_manifold {
    struct rigid_body *rbA, *rbB;
    int num_contacts;
    struct sim_contact contacts[MAX_CONTACTS_PER_MANIFOLD];
};



/* Contains information about a single rigid body constructed from a
 * BRL-CAD region.  This structure is the node of a linked list
 * containing the geometry to be added to the sim.
 *
 * TODO: Only the bb is currently present, but physical properties
 * like elasticity, custom forces will be added later.
 */
struct rigid_body {
    int index;
    char *rb_namep;                 /**< @brief pointer to name string */
    point_t bb_min;                 /**< @brief body min bb bounds, only calculated 1st time */
    point_t bb_max;                 /**< @brief body max bb bounds, only calculated 1st time */
    point_t bb_center;              /**< @brief bb center */
    point_t bb_dims;                /**< @brief bb dimensions */
    point_t btbb_min;               /**< @brief Bullet body min bb bounds, updated after each iter. */
    point_t btbb_max;               /**< @brief Bullet body max bb bounds, updated after each iter. */
    point_t btbb_center;            /**< @brief Bullet bb center */
    point_t btbb_dims;              /**< @brief Bullet bb dimensions */
    mat_t m;                        /**< @brief transformation matrix from Bullet */
    mat_t m_prev;                   /**< @brief previous transformation matrix from Bullet */
    int state;                      /**< @brief rigid body state from Bullet */
    struct directory
	    *dp;           /**< @brief directory pointer to the related region */
    struct rt_db_internal
	    intern;	/**< @brief internal format of the related region */
    struct rigid_body *next;        /**< @brief link to next body */

    /* Can be set by libged or Bullet(checked and inserted into sim) */
    vect_t linear_velocity;         /**< @brief linear velocity components */
    vect_t angular_velocity;        /**< @brief angular velocity components */

    /* Manifold generated by Bullet, where this body is B, only body B has manifold info */
    int num_bt_manifolds;			 	   			/**< @brief number of manifolds for this body */
    struct sim_manifold
	    bt_manifold[MAX_MANIFOLDS]; /**< @brief the manifolds for this body */

    /* Manifold generated by RT, where this body is B, ONLY 1 needed as its injected into the physics
     * pipeline immediately after creation thus allowing the next one to replace it
     */
    struct sim_manifold
	    rt_manifold; /**< @brief only 1 manifold struct needed as the manifold is used
									 immediately after being generated in the nearphase callback*/

    /* Debugging */
    int iter;
};

/**
 * This routine is called at the leaf nodes of the comb tree and checks
 * if the leaf contains a prim of the same name as that passed in 'object'
 * Returns SOLID_FOUND if the current(the passed) leaf contains a solid with
 * the same name as that of 'object', otherwise SOLID_NOT_FOUND
 *
 */
HIDDEN int
find_solid(struct db_i *dbip,
	   struct rt_comb_internal *comb,
	   union tree *comb_leaf,
	   void *object);




/**
 * This routine traverses a combination (union tree) in LNR order and
 * calls the provided function for each OP_DB_LEAF node.  Note that
 * this routine does not go outside this one combination!!!!
 *
 * similar to db_tree_funcleaf() with just an extra return statement
 */
int
check_tree_funcleaf(
    struct db_i *dbip,
    struct rt_comb_internal *comb,
    union tree *comb_tree,
    int (*leaf_func)(),
    void *user_ptr1);




/* Contains the simulation parameters, such as number of rigid bodies,
 * the head node of the linked list containing the bodies and
 * time/steps for which the simulation will be run.
 */
struct simulation_params {
    int duration;                  /**< @brief contains either the number of steps or the time */
    int num_bodies;                /**< @brief number of rigid bodies participating in the sim */
    struct bu_vls
	    *result_str;     /**< @brief handle to the libged object to access geometry info */
    char *sim_comb_name;           /**< @brief name of the group which contains all sim regions*/
    char *ground_plane_name;       /**< @brief name of the ground plane region */
    struct rigid_body *head_node;  /**< @brief link to first rigid body node */

    struct rt_i
	    *rtip;			   /**< @brief the raytrace instance used by rt to find contact points */
    struct ged
	    *gedp;			   /**< @brief pass the gfx context to allow lines to be drawn by rt */

    /* Debugging */
    int iter;
};





/**
 * Shoots rays within the AABB overlap regions only, to allow more rays to be shot
 * in a grid of finer granularity and to increase performance. The bodies to be targeted
 * are got from the list of manifolds returned by Bullet which carries out AABB
 * intersection checks. These manifolds are stored in the corresponding rigid_body
 * structures of each body participating in the simulation. The manifolds are then used
 * to generate manifolds based on raytracing and stored in a separate list for the body B
 * of that particular manifold. The list is freed in the next iteration in this function
 * as well, to prevent memory leaks, before a new set manifolds are created.
 */

int
generate_manifolds(struct simulation_params *sim_params,
		   struct rigid_body *rbA,
		   struct rigid_body *rbB);

#endif /* LIBGED_SIMULATE_SIMRT_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
