/*
 *			N M G _ C L A S S . C
 *
 *  Subroutines to classify one object with respect to another.
 *  Possible classifications are AinB, AoutB, AinBshared, AinBanti.
 *
 *  The first set of routines (nmg_class_pt_xxx) are used to classify
 *  an arbitrary point specified by it's Cartesian coordinates,
 *  against various kinds of NMG elements.
 *  nmg_class_pt_f() and nmg_class_pt_s() are available to
 *  applications programmers for direct use, and have no side effects.
 *
 *  The second set of routines (class_xxx_vs_s) are used only to support
 *  the routine nmg_class_shells() mid-way through the NMG Boolean
 *  algorithm.  These routines operate with special knowledge about
 *  the state of the data structures after the intersector has been called,
 *  and depends on all geometric equivalences to have been converted into
 *  shared topology.
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"
#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "rtlist.h"
#include "raytrace.h"
#include "./debug.h"

extern int nmg_class_nothing_broken;

/* XXX These should go the way of the dodo bird. */
#define INSIDE	32
#define ON_SURF	64
#define OUTSIDE	128

/*	Structure for keeping track of how close a point/vertex is to
 *	its potential neighbors.
 */
struct neighbor {
	union {
		CONST struct vertexuse *vu;
		CONST struct edgeuse *eu;
	} p;
	/* XXX For efficiency, this should have been dist_sq */
	fastf_t	dist;	/* distance from point to neighbor */
	int	class;	/* Classification of this neighbor */
};

static void	joint_hitmiss2 RT_ARGS( (struct neighbor *closest,
			CONST struct edgeuse *eu, CONST point_t pt,
			int code) );
static void	nmg_class_pt_e RT_ARGS( (struct neighbor *closest,
			CONST point_t pt, CONST struct edgeuse *eu,
			CONST struct rt_tol *tol) );
static void	nmg_class_pt_l RT_ARGS( (struct neighbor *closest, 
			CONST point_t pt, CONST struct loopuse *lu,
			CONST struct rt_tol *tol) );
static int	class_vu_vs_s RT_ARGS( (struct vertexuse *vu, struct shell *sB,
			long *classlist[4], CONST struct rt_tol	*tol) );
static int	class_eu_vs_s RT_ARGS( (struct edgeuse *eu, struct shell *s,
			long *classlist[4], CONST struct rt_tol	*tol) );
static int	class_lu_vs_s RT_ARGS( (struct loopuse *lu, struct shell *s,
			long *classlist[4], CONST struct rt_tol	*tol) );
static void	class_fu_vs_s RT_ARGS( (struct faceuse *fu, struct shell *s,
			long *classlist[4], CONST struct rt_tol	*tol) );

/*
 *			N M G _ C L A S S _ S T A T U S
 *
 *  Convert classification status to string.
 */
CONST char *
nmg_class_status(status)
int	status;
{
	switch(status)  {
	case INSIDE:
		return "INSIDE";
	case OUTSIDE:
		return "OUTSIDE";
	case ON_SURF:
		return "ON_SURF";
	}
	return "BOGUS_CLASS_STATUS";
}

/*
 *			N M G _ P R _ C L A S S _ S T A T U S
 */
void
nmg_pr_class_status( prefix, status )
char	*prefix;
int	status;
{
	rt_log("%s has classification status %s\n",
		prefix, nmg_class_status(status) );
}

/*
 *			J O I N T _ H I T M I S S 2
 *
 *	The ray hit an edge.  We have to decide whether it hit the
 *	edge, or missed it.
 */
static void joint_hitmiss2(closest, eu, pt, code)
struct neighbor		*closest;
CONST struct edgeuse	*eu;
CONST point_t		pt;
int			code;
{
	CONST struct edgeuse *eu_rinf;

	eu_rinf = nmg_faceradial(eu);

	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		rt_log("joint_hitmiss2\n");
	}
	if( eu_rinf == eu )  {
		rt_bomb("joint_hitmiss2: radial eu is me?\n");
	}
	/* If eu_rinf == eu->eumate_p, thats OK, this is a dangling face,
	 * or a face that has not been fully hooked up yet.
	 * It's OK as long the the orientations both match.
	 */
	if( eu->up.lu_p->orientation == eu_rinf->up.lu_p->orientation ) {
		if (eu->up.lu_p->orientation == OT_SAME) {
			closest->class = NMG_CLASS_AonBshared;
		} else if (eu->up.lu_p->orientation == OT_OPPOSITE) {
			closest->class = NMG_CLASS_AoutB;
		} else {
			nmg_pr_lu_briefly(eu->up.lu_p, (char *)0);
			rt_bomb("joint_hitmiss2: bad loop orientation\n");
		}
		closest->dist = 0.0;
		switch(code)  {
		case 0:
			/* Hit the edge */
			closest->p.eu = eu;
			break;
		case 1:
			/* Hit first vertex */
			closest->p.vu = eu->vu_p;
			break;
		case 2:
			/* Hit second vertex */
			closest->p.vu = RT_LIST_PNEXT_CIRC(edgeuse,eu)->vu_p;
			break;
		}
		if (rt_g.NMG_debug & DEBUG_CLASSIFY) rt_log("\t\t%s\n", nmg_class_name(closest->class) );
	    	return;
	}

	/* XXX What goes here? */
	rt_bomb("nmg_class.c/joint_hitmiss2() unable to resolve ray/edge hit\n");
	rt_log("joint_hitmiss2: NO CODE HERE, assuming miss\n");

	if (rt_g.NMG_debug & (DEBUG_CLASSIFY|DEBUG_NMGRT) )  {
		nmg_euprint("Hard question time", eu);
		rt_log(" eu_rinf=x%x, eu->eumate_p=x%x, eu=x%x\n", eu_rinf, eu->eumate_p, eu);
		rt_log(" eu lu orient=%s, eu_rinf lu orient=%s\n",
			nmg_orientation(eu->up.lu_p->orientation),
			nmg_orientation(eu_rinf->up.lu_p->orientation) );
	}
}

/*
 *			N M G _ C L A S S _ P T _ E
 *
 *  Given the Cartesian coordinates of a point, determine if the point
 *  is closer to this edgeuse than the previous neighbor(s) as given
 *  in the "closest" structure.
 *  If it is, record how close the point is, and whether it is IN, ON, or OUT.
 *  The neighor's "p" element will indicate the edgeuse or vertexuse closest.
 *
 *  This routine should print everything indented two tab stops.
 *
 *  Implicit Return -
 *	Updated "closest" structure if appropriate.
 */
static void
nmg_class_pt_e(closest, pt, eu, tol)
struct neighbor		*closest;
CONST point_t		pt;
CONST struct edgeuse	*eu;
CONST struct rt_tol	*tol;
{
	vect_t	ptvec;	/* vector from lseg to pt */
	vect_t	left;	/* vector left of edge -- into inside of loop */
	CONST fastf_t	*eupt;
	CONST fastf_t	*matept;
	point_t pca;	/* point of closest approach from pt to edge lseg */
	fastf_t dist;	/* distance from pca to pt */
	fastf_t dot, mag;
	int	code;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_EDGEUSE(eu->eumate_p);
	NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
	NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);
	RT_CK_TOL(tol);
	VSETALL(left, 0);

	eupt = eu->vu_p->v_p->vg_p->coord;
	matept = eu->eumate_p->vu_p->v_p->vg_p->coord;

	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		VPRINT("nmg_class_pt_e\tPt", pt);
		nmg_euprint("          \tvs. eu", eu);
	}
	/*
	 * Note that "pca" can be one of the edge endpoints,
	 * even if "pt" is far, far away.  This can be confusing.
	 */
	{
		struct rt_tol	xtol;
		/*XXXX HACK:  To keep from hitting vertices & edges,
		 *XXXX  use ultra-strict hard-coded tolerance here */
		xtol = *tol;	/* struct copy */
		xtol.dist = 0.0005;
		xtol.dist_sq = xtol.dist_sq * xtol.dist_sq;
		code = rt_dist_pt3_lseg3( &dist, pca, eupt, matept, pt, &xtol);
	}
	if( code <= 0 )  dist = 0;
	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		rt_log("          \tcode=%d, dist: %g\n", code, dist);
		VPRINT("          \tpca:", pca);
	}

	if (dist >= closest->dist + tol->dist ) {
 		if(rt_g.NMG_debug & DEBUG_CLASSIFY) {
 			rt_log("\t\tskipping, earlier eu is closer (%g)\n", closest->dist);
 		}
		return;
 	}
 	if( dist >= closest->dist - tol->dist )  {
 		/*
 		 *  Distances are very nearly the same.
 		 *  If "closest" result so far was a NMG_CLASS_AinB or
		 *  or NMG_CLASS_AonB, then keep it,
 		 *  otherwise, replace that result with whatever we find
 		 *  here.  Logic:  Two touching loops, one concave ("A")
		 *  which wraps around part of the other ("B"), with the
 		 *  point inside A near the contact with B.  If loop B is
		 *  processed first, the closest result will be NMG_CLASS_AoutB,
 		 *  and when loop A is visited the distances will be exactly
 		 *  equal, not giving A a chance to claim it's hit.
 		 */
 		if( closest->class == NMG_CLASS_AinB ||
		    closest->class == NMG_CLASS_AonBshared )  {
	 		if(rt_g.NMG_debug & DEBUG_CLASSIFY)
				rt_log("\t\tSkipping, earlier eu at same dist, is IN or ON\n");
 			return;
 		}
 		if(rt_g.NMG_debug & DEBUG_CLASSIFY)
			rt_log("\t\tEarlier eu at same dist, is OUT, continue processing.\n");
 	}

	/* Plane hit point is closer to this edgeuse than previous one(s) */
	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		rt_log("\t\tCLOSER dist=%g (closest=%g), tol=%g\n",
			dist, closest->dist, tol->dist);
	}

	if (*eu->up.magic_p != NMG_LOOPUSE_MAGIC ||
	    *eu->up.lu_p->up.magic_p != NMG_FACEUSE_MAGIC) {
		rt_log("Trying to classify a pt (%g, %g, %g)\n\tvs a wire edge? (%g, %g, %g -> %g, %g, %g)\n",
	    		V3ARGS(pt),
	    		V3ARGS(eupt),
	    		V3ARGS(matept)
		);
	    	return;
	}

	if( code <= 2 )  {
		/* code==0:  The point is ON the edge! */
		/* code==1 or 2:  The point is ON a vertex! */
    		if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
    			rt_log("\t\tThe point is ON the edge, calling joint_hitmiss2()\n");
    		}
   		joint_hitmiss2(closest, eu, pt, code);
		return;
    	} else {
    		if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
			rt_log("\t\tThe point is not on the edge\n");
    		}
    	}

	/* The point did not lie exactly ON the edge, calculate in/out */

    	/* Get vector which lies on the plane, and points
    	 * left, towards the interior of the loop, regardless of
	 * whether it's an interior (OT_OPPOSITE) or exterior (OT_SAME) loop.
    	 */
	if( nmg_find_eu_leftvec( left, eu ) < 0 )  rt_bomb("nmg_class_pt_e() bad LEFT vector\n");

	VSUB2(ptvec, pt, pca);		/* pt - pca */
    	if (rt_g.NMG_debug & DEBUG_CLASSIFY)  {
		VPRINT("\t\tptvec unnorm", ptvec);
    		VPRINT("\t\tleft", left);
    	}
	VUNITIZE( ptvec );

	dot = VDOT(left, ptvec);
	if( NEAR_ZERO( dot, RT_DOT_TOL )  )  {
	    	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
	    		rt_log("\t\tpt lies on line of edge, outside verts. Skipping this edge\n");
		goto out;
	}

	if (dot >= 0.0) {
		closest->dist = dist;
		closest->p.eu = eu;
		closest->class = NMG_CLASS_AinB;
	    	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
	    		rt_log("\t\tpt is left of edge, INSIDE loop, dot=%g\n", dot);
	} else {
		closest->dist = dist;
		closest->p.eu = eu;
		closest->class = NMG_CLASS_AoutB;
	    	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
	    		rt_log("\t\tpt is right of edge, OUTSIDE loop\n");
	}

out:
	/* XXX Temporary addition for chasing bug in Bradley r5 */
	/* XXX Should at least add DEBUG_PLOTEM check, later */
	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		struct faceuse	*fu;
		char	buf[128];
		static int	num;
		FILE	*fp;
		long	*bits;
		point_t	mid_pt;
		point_t	left_pt;
		fu = eu->up.lu_p->up.fu_p;
		bits = (long *)rt_calloc( nmg_find_model(&fu->l.magic)->maxindex, sizeof(long), "bits[]");
		sprintf(buf,"faceclass%d.pl", num++);
		if( (fp = fopen(buf, "w")) == NULL) rt_bomb(buf);
		nmg_pl_fu( fp, fu, bits, 0, 0, 255 );	/* blue */
		pl_color( fp, 0, 255, 0 );	/* green */
		pdv_3line( fp, pca, pt );
		pl_color( fp, 255, 0, 0 );	/* red */
		VADD2SCALE( mid_pt, eupt, matept, 0.5 );
		VJOIN1( left_pt, mid_pt, 500, left);
		pdv_3line( fp, mid_pt, left_pt );
		fclose(fp);
		rt_free( (char *)bits, "bits[]");
		rt_log("wrote %s\n", buf);
	}
}


/*
 *			N M G _ C L A S S _ P T _ L
 *
 *  Given the coordinates of a point which lies on the plane of the face
 *  containing a loopuse, determine if the point is IN, ON, or OUT of the
 *  area enclosed by the loop.
 *
 *  Implicit Return -
 *	Updated "closest" structure if appropriate.
 */
static void
nmg_class_pt_l(closest, pt, lu, tol)
struct neighbor		*closest;
CONST point_t		pt;
CONST struct loopuse	*lu;
CONST struct rt_tol	*tol;
{
	vect_t		delta;
	pointp_t	lu_pt;
	fastf_t		dist;
	struct edgeuse	*eu;
	struct loop_g	*lg;

	NMG_CK_LOOPUSE(lu);
	NMG_CK_LOOP(lu->l_p);
	lg = lu->l_p->lg_p;
	NMG_CK_LOOP_G(lg);

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)  {
		VPRINT("nmg_class_pt_l\tPt:", pt);
	}

	if (*lu->up.magic_p != NMG_FACEUSE_MAGIC)
		return;

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)  {
		plane_t		peqn;
		nmg_pr_lu_briefly(lu, 0);
		NMG_GET_FU_PLANE( peqn, lu->up.fu_p );
		HPRINT("\tplane eqn", peqn);
	}

	if( !V3PT_IN_RPP_TOL( pt, lg->min_pt, lg->max_pt, tol ) )  {
		if (rt_g.NMG_debug & DEBUG_CLASSIFY)
			rt_log("\tPoint is outside loop RPP\n");
		return;
	}
	if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
		for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			nmg_class_pt_e(closest, pt, eu, tol);
			/* If point lies ON edge, we are done */
			if( closest->class == NMG_CLASS_AonBshared )  break;
		}
	} else if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
		register struct vertexuse *vu;
		vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
		lu_pt = vu->v_p->vg_p->coord;
		VSUB2(delta, pt, lu_pt);
		if ( (dist = MAGNITUDE(delta)) < closest->dist) {
			if (lu->orientation == OT_OPPOSITE) {
				closest->class = NMG_CLASS_AoutB;
			} else if (lu->orientation == OT_SAME) {
				closest->class = NMG_CLASS_AonBshared;
			} else {
				nmg_pr_orient(lu->orientation, "\t");
				rt_bomb("nmg_class_pt_l: bad orientation for face loopuse\n");
			}
			if (rt_g.NMG_debug & DEBUG_CLASSIFY)
				rt_log("\t\t closer to loop pt (%g, %g, %g)\n",
					V3ARGS(lu_pt) );

			closest->dist = dist;
			closest->p.vu = vu;
		}
	} else {
		rt_bomb("nmg_class_pt_l() bad child of loopuse\n");
	}
	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		rt_log("nmg_class_pt_l\treturning, closest=%g %s\n",
			closest->dist, nmg_class_name(closest->class) );
}

/*
 *			N M G _ C L A S S _ P T _ F _ E X C E P T
 *
 *  This is intended as a general user interface routine.
 *  Given the Cartesian coordinates for a point which is known to
 *  lie on a face, return the classification for that point
 *  with respect to all the loops on that face.
 *
 *  The algorithm used is to find the edge which the point is closest
 *  to, and classifiy with respect to that.
 *
 *  "ignore_lu" is optional.  When non-null, it points to a loopuse (and it's
 *  mate) which will not be considered in the assessment of this point.
 *  This is used by nmg_lu_reorient() to work on one lu in the face.
 *
 *  The point is "A", and the face is "B".
 *
 *  Returns -
 *	NMG_CLASS_AinB		pt is INSIDE the area of the face.
 *	NMG_CLASS_AonBshared	pt is ON a loop boundary.
 *	NMG_CLASS_AoutB		pt is OUTSIDE the area of the face.
 */
int
nmg_class_pt_f_except(pt, fu, ignore_lu, tol)
CONST point_t		pt;
CONST struct faceuse	*fu;
CONST struct loopuse	*ignore_lu;
CONST struct rt_tol	*tol;
{
	struct loopuse *lu;
	struct neighbor closest;
	fastf_t		dist;
	plane_t		n;

	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		VPRINT("nmg_class_pt_f\tPt:", pt);
	}
	NMG_CK_FACEUSE(fu);
	NMG_CK_FACE(fu->f_p);
	NMG_CK_FACE_G_PLANE(fu->f_p->g.plane_p);
	if(ignore_lu) NMG_CK_LOOPUSE(ignore_lu);
	RT_CK_TOL(tol);

	/* Validate distance from point to plane */
	NMG_GET_FU_PLANE( n, fu );
	if( (dist=fabs(DIST_PT_PLANE( pt, n ))) > tol->dist )  {
		rt_log("nmg_class_pt_f() ERROR, point (%g,%g,%g) not on face, dist=%g\n",
			V3ARGS(pt), dist );
	}

	/* find the closest approach in this face to the projected point */
	closest.dist = MAX_FASTF;
	closest.p.eu = (struct edgeuse *)NULL;
	closest.class = NMG_CLASS_AoutB;	/* default return */

	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if( ignore_lu && (ignore_lu == lu || ignore_lu == lu->lumate_p) )
			continue;

		nmg_class_pt_l( &closest, pt, lu, tol );
		/* If point lies ON loop edge, we are done */
		if( closest.class == NMG_CLASS_AonBshared )  break;
	}

	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		rt_log("nmg_class_pt_f\tdist=%g, return=%s\n",
			closest.dist,
			nmg_class_name(closest.class) );
	}
	return closest.class;
}

/*
 *			N M G _ C L A S S _ P T _ F
 *
 *  Compatability wrapper.
 */
int
nmg_class_pt_f(pt, fu, tol)
CONST point_t		pt;
CONST struct faceuse	*fu;
CONST struct rt_tol	*tol;
{
	return nmg_class_pt_f_except(pt, fu, (struct loopuse *)0, tol);
}

/*
 *			N M G _ C L A S S _ L U _ F U
 *
 *  This is intended as an internal routine to support nmg_lu_reorient().
 *
 *  Given a loopuse in a face, pick one of it's vertexuses, and classify
 *  that point with respect to all the rest of the loopuses in the face.
 *  The containment status of that point is the status of the loopuse.
 *
 *  The algorithm used is to find the edge which the point is closest
 *  to, and classifiy with respect to that.
 *
 *  If the first vertex chosen is "ON" another loop boundary,
 *  choose the next vertex and try again.  Only return an "ON"
 *  status if _all_ the vertices are ON.
 *
 *  The point is "A", and the face is "B".
 *
 *  Returns -
 *	NMG_CLASS_AinB		lu is INSIDE the area of the face.
 *	NMG_CLASS_AonBshared	ALL of lu is ON other loop boundaries.
 *	NMG_CLASS_AoutB		lu is OUTSIDE the area of the face.
 */
int
nmg_class_lu_fu(lu, tol)
CONST struct loopuse	*lu;
CONST struct rt_tol	*tol;
{
	CONST struct faceuse	*fu;
	struct vertexuse	*vu;
	CONST struct vertex_g	*vg;
	struct loopuse *lu2;
	struct neighbor closest;
	struct edgeuse	*eu;
	struct edgeuse	*eu_first;
	fastf_t		dist;
	plane_t		n;

	NMG_CK_LOOPUSE(lu);
	RT_CK_TOL(tol);

	fu = lu->up.fu_p;
	NMG_CK_FACEUSE(fu);
	NMG_CK_FACE(fu->f_p);
	NMG_CK_FACE_G_PLANE(fu->f_p->g.plane_p);

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		rt_log("nmg_class_lu_fu(lu=x%x) START\n", lu);

	/* Pick first vertex in loopuse, for point */
	if( RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )  {
		vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
		eu = (struct edgeuse *)NULL;
	} else {
		eu = RT_LIST_FIRST(edgeuse, &lu->down_hd);
		NMG_CK_EDGEUSE(eu);
		vu = eu->vu_p;
	}
	eu_first = eu;
again:
	NMG_CK_VERTEXUSE(vu);
	NMG_CK_VERTEX(vu->v_p);
	vg = vu->v_p->vg_p;
	NMG_CK_VERTEX_G(vg);

	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		VPRINT("nmg_class_lu_fu\tPt:", vg->coord);
	}

	/* Validate distance from point to plane */
	NMG_GET_FU_PLANE( n, fu );
	if( (dist=fabs(DIST_PT_PLANE( vg->coord, n ))) > tol->dist )  {
		rt_log("nmg_class_lu_fu() ERROR, point (%g,%g,%g) not on face, dist=%g\n",
			V3ARGS(vg->coord), dist );
	}

	/* find the closest approach in this face to the projected point */
	closest.dist = MAX_FASTF;
	closest.p.eu = (struct edgeuse *)NULL;
	closest.class = NMG_CLASS_AoutB;	/* default return */

	for (RT_LIST_FOR(lu2, loopuse, &fu->lu_hd)) {
		/* Do not use the supplied loopuse in the comparison! */
		if( lu2 == lu )  continue;
		if( lu2 == lu->lumate_p )  continue;

		/* Any other OT_UNSPEC or OT_BOOLPLACE lu's don't help either */
		if( lu2->orientation != OT_SAME && lu2->orientation != OT_OPPOSITE )  {
			if (rt_g.NMG_debug & DEBUG_CLASSIFY)  {
				rt_log("nmg_class_lu_fu(lu=x%x) WARNING:  skipping %s lu=x%x in fu=x%x!\n",
					lu, nmg_orientation(lu2->orientation), lu2, fu);
			}
			continue;
		}

		/* XXX Any point to doing a topology search first? */
		nmg_class_pt_l( &closest, vg->coord, lu2, tol );

		/* If this vertex lies ON loop edge, must check all others. */
		if( closest.class == NMG_CLASS_AonBshared )  {
			if( !eu_first )  break;  /* was self-loop */
			eu = RT_LIST_PNEXT_CIRC(edgeuse, &eu->l);
			if( eu == eu_first )  break;	/* all match */
			vu = eu->vu_p;
			goto again;
		}
	}

	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		rt_log("nmg_class_lu_fu(lu=x%x) END, dist=%g, ret=%s\n",
			lu,
			closest.dist,
			nmg_class_name(closest.class) );
	}
	return closest.class;
}

/* Ray direction vectors for Jordan curve algorithm */
static CONST point_t nmg_good_dirs[10] = {
	3, 2, 1,
	-3,-2,-1,
	1, 0, 0,
	0, 1, 0,
	0, 0, 1,
	-1,0, 0,
	0,-1, 0,
	0, 0,-1,
	1, 1, 1,
	-1,-1,-1
};

/*
 *			N M G _ C L A S S _ P T _ S
 *
 *  This is intended as a general user interface routine.
 *  Given the Cartesian coordinates for a point in space,
 *  return the classification for that point with respect to a shell.
 *
 *  The algorithm used is to fire a ray from the point, and count
 *  the number of times it crosses a face.
 *
 *  The point is "A", and the face is "B".
 *
 *  Returns -
 *	NMG_CLASS_AinB		pt is INSIDE the volume of the shell.
 *	NMG_CLASS_AonBshared	pt is ON the shell boundary.
 *	NMG_CLASS_AoutB		pt is OUTSIDE the volume of the shell.
 */
int
nmg_class_pt_s(pt, s, tol)
CONST point_t		pt;
CONST struct shell	*s;
CONST struct rt_tol	*tol;
{
	int		hitcount = 0;
	int		stat;
	fastf_t 	dist;
	point_t 	plane_pt;
	CONST struct faceuse	*fu;
	struct model	*m;
	long		*faces_seen;
	vect_t		region_diagonal;
	fastf_t		region_diameter;
	int		class;
	vect_t		projection_dir;
	int		try;

	NMG_CK_SHELL(s);
	RT_CK_TOL(tol);

	/* Choose an unlikely direction */
	try = 0;
retry:
	VMOVE( projection_dir, nmg_good_dirs[try] );
	try++;
	VUNITIZE(projection_dir);

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		rt_log("nmg_class_pt_s:\tpt=(%g, %g, %g), s=x%x\n",
			V3ARGS(pt), s );

	if( !V3PT_IN_RPP_TOL(pt, s->sa_p->min_pt, s->sa_p->max_pt, tol) )  {
		if (rt_g.NMG_debug & DEBUG_CLASSIFY)
			rt_log("	OUT, point not in RPP\n");
		return NMG_CLASS_AoutB;
	}

	m = s->r_p->m_p;
	NMG_CK_MODEL(m);
	faces_seen = (long *)rt_calloc( m->maxindex, sizeof(long), "nmg_class_pt_s faces_seen[]" );
	NMG_CK_REGION_A(s->r_p->ra_p);
	VSUB2( region_diagonal, s->r_p->ra_p->max_pt, s->r_p->ra_p->min_pt );
	region_diameter = MAGNITUDE(region_diagonal);

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		rt_log("\tPt=(%g, %g, %g) dir=(%g, %g, %g), reg_diam=%g\n",
			V3ARGS(pt), V3ARGS(projection_dir), region_diameter);

	/*
	 *  First pass:  Try hard to see if point is ON a face.
	 */
	for( RT_LIST_FOR(fu, faceuse, &s->fu_hd) )  {
		plane_t	n;

		/* If this face processed before, skip on */
		if( NMG_INDEX_TEST( faces_seen, fu->f_p ) )  continue;

		/* Only consider the outward pointing faceuses */
		if( fu->orientation != OT_SAME )  continue;

		/* Mark this face as having been processed */
		NMG_INDEX_SET(faces_seen, fu->f_p);

		/* See if this point lies on this face */
		NMG_GET_FU_PLANE( n, fu );
		if( (dist = fabs(DIST_PT_PLANE(pt, n))) < tol->dist)  {
			/* Point lies on this plane, it may be possible to
			 * short circuit everything.
			 */
			class = nmg_class_pt_f( pt, fu, tol );
			if( class == NMG_CLASS_AonBshared )  {
				/* Point is ON face, therefore it must be
				 * ON the shell also.
				 */
				class = NMG_CLASS_AonBshared;
				goto out;
			}
			if( class == NMG_CLASS_AinB )  {
				/* Point is IN face, therefor it must be
				 * ON the shell also.
				 */
				class = NMG_CLASS_AonBshared;
				goto out;
			}
			/* Point is OUTside face, its undecided. */
		}

		/* Dangling faces don't participate in Jordan Curve calc */
		if (nmg_dangling_face(fu))  continue;

/* XXX Adding this code in breaks Test1.r! */
#if 0
		/* Un-mark this face, handle it in the second pass */
		NMG_INDEX_CLEAR(faces_seen, fu->f_p);
	}

	/*
	 *  Second pass:  Jordan Curve algorithm.
	 *  Fire a ray in "projection_dir", and count face crossings.
	 */
	for( RT_LIST_FOR(fu, faceuse, &s->fu_hd) )  {
		plane_t	n;

		/* If this face processed before, skip on */
		if( NMG_INDEX_TEST( faces_seen, fu->f_p ) )  continue;

		/* Mark this face as having been processed */
		NMG_INDEX_SET(faces_seen, fu->f_p);
#endif

		/* Only consider the outward pointing faceuses */
		if( fu->orientation != OT_SAME )  continue;

		/* Find point where ray hits the plane. */
		NMG_GET_FU_PLANE( n, fu );
		stat = rt_isect_line3_plane(&dist, pt, projection_dir,
			n, tol);

		if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
			rt_log("\tray/plane: stat:%d dist:%g\n", stat, dist);
			PLPRINT("\tplane", n);
		}

		if( stat < 0 )  continue;	/* Ray missed */
		if( dist < 0 )  continue;	/* Hit was behind start pt. */

		/* XXX This case needs special handling */
		if( stat == 0 )  rt_bomb("nmg_class_pt_s: ray is ON face!\n");

		/*
		 *  The ray hit the face.  Determine if this is a reasonable
		 *  hit distance by comparing with the region diameter.
		 */
		if( dist > region_diameter )  {
			if (rt_g.NMG_debug & DEBUG_CLASSIFY)
				rt_log("\tnmg_class_pt_s: hit plane outside region, skipping\n");
			continue;
		}

		/*
		 * Construct coordinates of hit point, and classify.
		 * XXX In the case of an ON result,
		 * XXX this really needs to be a ray/edge classification,
		 * XXX not a point/edge classification.
		 * XXX The ray can go in/in, out/out, in/out, out/in,
		 * XXX with different meanings.  Can't tell the difference here.
		 */
	    	VJOIN1(plane_pt, pt, dist, projection_dir);
		if (rt_g.NMG_debug & DEBUG_CLASSIFY)
			rt_log("\tClassify ray/face intercept point\n");
		class = nmg_class_pt_f( plane_pt, fu, tol );
		if( class == NMG_CLASS_AinB )  hitcount++;
		else if( class == NMG_CLASS_AonBshared )  {
			/* XXX Can't handle this case.
			 * XXX Keep picking different directions until
			 * XXX no "hard" cases come up.  (Limit 10 per customer).
			 */
			if( try < 10 )  {
				rt_log("nmg_class_pt_s(%g, %g, %g) try=%d, grazed edge\n", V3ARGS(pt), try);
				goto retry;
			}
			hitcount++;
			rt_bomb("nmg_class_pt_s: ray grazed an edge, could be 1 hit (in/out) or 2 hits (in/in, out/out), can't tell which!\n");
		}
		if (rt_g.NMG_debug & DEBUG_CLASSIFY)
			rt_log("nmg_class_pt_s:\t ray hitcount=%d\n", hitcount);
	}
	rt_free( (char *)faces_seen, "nmg_class_pt_s faces_seen[]" );

	/*  Using Jordan Curve Theorem, if hitcount is even, point is OUT.
	 *  If hiscount is odd, point is IN.
	 */
	if (hitcount & 1) {
		class = NMG_CLASS_AinB;
	} else {
		class = NMG_CLASS_AoutB;
	}
out:
	if (rt_g.NMG_debug & DEBUG_CLASSIFY || try > 1)
		rt_log("nmg_class_pt_s: returning %s, s=x%x\n",
			nmg_class_name(class), s );
	return class;
}


/*
 *			C L A S S _ V U _ V S _ S
 *
 *	Classify a loopuse/vertexuse from shell A WRT shell B.
 */
static int class_vu_vs_s(vu, sB, classlist, tol)
struct vertexuse	*vu;
struct shell		*sB;
long			*classlist[4];
CONST struct rt_tol	*tol;
{
	struct vertexuse *vup;
	pointp_t pt;
	char	*reason;
	int	status;
	int	class;

	NMG_CK_VERTEXUSE(vu);
	NMG_CK_SHELL(sB);
	RT_CK_TOL(tol);

	pt = vu->v_p->vg_p->coord;

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		rt_log("class_vu_vs_s(vu=x%x) pt=(%g,%g,%g)\n", vu, V3ARGS(pt) );

	if( !(rt_g.NMG_debug & DEBUG_CLASSIFY) )  {
		/* As an efficiency & consistency measure, check for vertex in class list */
		reason = "of classlist";
		if( NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], vu->v_p) )  {
			status = INSIDE;
			goto out;
		}
		if( NMG_INDEX_TEST(classlist[NMG_CLASS_AonBshared], vu->v_p) )  {
			status = ON_SURF;
			goto out;
		}
		if( NMG_INDEX_TEST(classlist[NMG_CLASS_AoutB], vu->v_p) )  {
			status = OUTSIDE;
			goto out;
		}
	}

	/* we use topology to determing if the vertex is "ON" the
	 * other shell.
	 */
	for(RT_LIST_FOR(vup, vertexuse, &vu->v_p->vu_hd)) {

		if (*vup->up.magic_p == NMG_LOOPUSE_MAGIC )  {
			if( nmg_find_s_of_lu(vup->up.lu_p) != sB) continue;
		    	NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], 
		    		vu->v_p );
		    	reason = "a loopuse of vertex is on shell";
		    	status = ON_SURF;
			goto out;
		} else if (*vup->up.magic_p == NMG_EDGEUSE_MAGIC )  {
			if( nmg_find_s_of_eu(vup->up.eu_p) != sB) continue;
		    	NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared],
		    		vu->v_p );
		    	reason = "an edgeuse of vertex is on shell";
		    	status = ON_SURF;
			goto out;
		} else if( *vup->up.magic_p == NMG_SHELL_MAGIC )  {
			if( vup->up.s_p != sB ) continue;
		    	NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared],
		    		vu->v_p );
		    	reason = "vertex is shell's lone vertex";
		    	status = ON_SURF;
			goto out;
		} else {
			rt_bomb("class_vu_vs_s() bad vertex UP pointer\n");
		}
	}

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		rt_log("\tCan't classify vertex via topology\n");

	/* The topology doesn't tell us about the vertex being "on" the shell
	 * so now it's time to look at the geometry to determine the vertex
	 * relationship to the shell.
	 *
	 * The vertex should *not* lie ON any of the faces or
	 * edges, since the intersection algorithm would have combined the
	 * topology if that had been the case.
	 */
	/* XXX eventually, make this conditional on DEBUG_CLASSIFY */
	{
		/* Verify this assertion */
		struct vertex	*sv;
		if( (sv = nmg_find_pt_in_shell( sB, pt, tol ) ) )  {
			rt_log("vu=x%x, v=x%x, sv=x%x, pt=(%g,%g,%g)\n",
				vu, vu->v_p, sv, V3ARGS(pt) );
			rt_bomb("nmg_class_pt_s() vertex topology not shared properly\n");
		}
	}

	reason = "of nmg_class_pt_s()";
	class = nmg_class_pt_s(pt, sB, tol);
	
	if( class == NMG_CLASS_AoutB )  {
		NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], vu->v_p);
		status = OUTSIDE;
	}  else if( class == NMG_CLASS_AinB )  {
		NMG_INDEX_SET(classlist[NMG_CLASS_AinB], vu->v_p);
		status = INSIDE;
	}  else if( class == NMG_CLASS_AonBshared )  {
		rt_bomb("class_vu_vs_s:  classifier found point ON, vertex topology should have been shared\n");
	}  else  {
		rt_log("class=%s\n", nmg_class_name(class) );
		VPRINT("pt", pt);
		rt_bomb("class_vu_vs_s: nmg_class_pt_s() failure\n");
	}

out:
	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		rt_log("class_vu_vs_s(vu=x%x) return %s because %s\n",
			vu, nmg_class_status(status), reason );
	}
	return(status);
}

/*
 *			C L A S S _ E U _ V S _ S
 */
static int class_eu_vs_s(eu, s, classlist, tol)
struct edgeuse	*eu;
struct shell	*s;
long		*classlist[4];
CONST struct rt_tol	*tol;
{
	int euv_cl, matev_cl;
	int	status;
	struct edgeuse *eup;
	point_t pt;
	pointp_t eupt, matept;
	char	*reason = "Unknown";
	int	class;

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		nmg_euprint("class_eu_vs_s\t", eu);

	NMG_CK_EDGEUSE(eu);	
	NMG_CK_SHELL(s);	
	RT_CK_TOL(tol);
	
	if( !(rt_g.NMG_debug & DEBUG_CLASSIFY) )  {
		/* check to see if edge is already in one of the lists */
		reason = "of classlist";
		if( NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], eu->e_p) )  {
			status = INSIDE;
			goto out;
		}
		if( NMG_INDEX_TEST(classlist[NMG_CLASS_AonBshared], eu->e_p) )  {
			status = ON_SURF;
			goto out;
		}
		if( NMG_INDEX_TEST(classlist[NMG_CLASS_AoutB], eu->e_p) )  {
			status = OUTSIDE;
			goto out;
		}
	}
	euv_cl = class_vu_vs_s(eu->vu_p, s, classlist, tol);
	matev_cl = class_vu_vs_s(eu->eumate_p->vu_p, s, classlist, tol);
	
	/* sanity check */
	if ((euv_cl == INSIDE && matev_cl == OUTSIDE) ||
	    (euv_cl == OUTSIDE && matev_cl == INSIDE)) {
	    	static int	num=0;
	    	char	buf[128];
	    	FILE	*fp;

	    	sprintf(buf, "class%d.pl", num++ );
	    	if( (fp = fopen(buf, "w")) == NULL ) rt_bomb(buf);
	    	nmg_pl_s( fp, s );
		/* A yellow line for the angry edge */
		pl_color(fp, 255, 255, 0);
		pdv_3line(fp, eu->vu_p->v_p->vg_p->coord,
			eu->eumate_p->vu_p->v_p->vg_p->coord );
		fclose(fp);

	    	nmg_pr_class_status("eu vu", euv_cl);
	    	nmg_pr_class_status("eumate vu", matev_cl);
	    	if( rt_g.debug || rt_g.NMG_debug )  {
		    	/* Do them over, so we can watch */
	    		rt_log("Edge not cut, doing it over\n");
	    		NMG_INDEX_CLEAR( classlist[NMG_CLASS_AinB], eu->vu_p);
	    		NMG_INDEX_CLEAR( classlist[NMG_CLASS_AoutB], eu->vu_p);
	    		NMG_INDEX_CLEAR( classlist[NMG_CLASS_AinB], eu->eumate_p->vu_p);
	    		NMG_INDEX_CLEAR( classlist[NMG_CLASS_AoutB], eu->eumate_p->vu_p);
/**	    		rt_g.debug |= DEBUG_MATH; **/
			rt_g.NMG_debug |= DEBUG_CLASSIFY;
			(void)class_vu_vs_s(eu->vu_p, s, classlist, tol);
			(void)class_vu_vs_s(eu->eumate_p->vu_p, s, classlist, tol);
		    	nmg_euprint("didn't this edge get cut?", eu);
		    	nmg_pr_eu(eu, "  ");
	    	}

		rt_log("wrote %s\n", buf);
		rt_bomb("class_eu_vs_s:  edge didn't get cut\n");
	}

	if (euv_cl == ON_SURF && matev_cl == ON_SURF) {
		/* check for radial uses of this edge by the shell */
		eup = eu->radial_p->eumate_p;
		do {
			NMG_CK_EDGEUSE(eup);
			if (nmg_find_s_of_eu(eup) == s) {
				NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared],
					eu->e_p );
				reason = "a radial edgeuse is on shell";
				status = ON_SURF;
				goto out;
			}
			eup = eup->radial_p->eumate_p;
		} while (eup != eu->radial_p->eumate_p);

		/* although the two endpoints are "on" the shell,
		 * the edge would appear to be either "inside" or "outside",
		 * since there are no uses of this edge in the shell.
		 *
		 * So we classify the midpoint of the line WRT the shell.
		 */
		eupt = eu->vu_p->v_p->vg_p->coord;
		matept = eu->eumate_p->vu_p->v_p->vg_p->coord;
		VADD2SCALE(pt, eupt, matept, 0.5);

		if (rt_g.NMG_debug & DEBUG_CLASSIFY)
			VPRINT("class_eu_vs_s: midpoint of edge", pt);

		class = nmg_class_pt_s(pt, s, tol);
		reason = "midpoint classification (both verts ON)";
		if( class == NMG_CLASS_AoutB )  {
			NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], eu->e_p);
			status = OUTSIDE;
		}  else if( class == NMG_CLASS_AinB )  {
			NMG_INDEX_SET(classlist[NMG_CLASS_AinB], eu->e_p);
			status = INSIDE;
		} else if( class == NMG_CLASS_AonBshared )  {
#if 0
			/* XXX bug hunt helper */
			rt_g.NMG_debug |= DEBUG_FINDEU;
			rt_g.NMG_debug |= DEBUG_MESH;
			eup = nmg_findeu( eu->vu_p->v_p, eu->eumate_p->vu_p->v_p, s, eu, 0 );
			if(!eup) rt_log("Unable to find it\n");
			nmg_mesh_face_shell( eu->up.lu_p->up.fu_p, s );
#endif
			rt_bomb("class_eu_vs_s:  classifier found edge midpoint ON, edge topology should have been shared\n");
		}  else  {
			rt_log("class=%s\n", nmg_class_name(class) );
			nmg_euprint("Why wasn't this edge in or out?", eu);
			rt_bomb("class_eu_vs_s: nmg_class_pt_s() midpoint failure\n");
		}
		goto out;
	}

	if (euv_cl == OUTSIDE || matev_cl == OUTSIDE) {
		NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], eu->e_p);
		reason = "at least one vert OUT";
		status = OUTSIDE;
		goto out;
	}
	if( euv_cl == INSIDE && matev_cl == INSIDE )  {
		NMG_INDEX_SET(classlist[NMG_CLASS_AinB], eu->e_p);
		reason = "both verts IN";
		status = INSIDE;
		goto out;
	}
	if( (euv_cl == INSIDE && matev_cl == ON_SURF) ||
	    (euv_cl == ON_SURF && matev_cl == INSIDE) )  {
		NMG_INDEX_SET(classlist[NMG_CLASS_AinB], eu->e_p);
		reason = "one vert IN, one ON";
		status = INSIDE;
		goto out;
	}
	rt_log("eu's vert is %s, mate's vert is %s\n",
		nmg_class_status(euv_cl), nmg_class_status(matev_cl) );
	rt_bomb("class_eu_vs_s() inconsistent edgeuse\n");
out:
	if (rt_g.NMG_debug & DEBUG_GRAPHCL)
		show_broken_stuff((long *)eu, classlist, nmg_class_nothing_broken, 0, (char *)NULL);
	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		rt_log("class_eu_vs_s(eu=x%x) return %s because %s\n",
			eu, nmg_class_status(status), reason );
	}
	return(status);
}

/* XXX move to raytrace.h, in section for nmg_info.c */
RT_EXTERN(int		nmg_2lu_identical, (CONST struct edgeuse *eu1,
			CONST struct edgeuse *eu2));

/* XXX move to nmg_info.c */
/*
 *			N M G _ 2 L U _ I D E N T I C A L
 *
 *  Given two edgeuses in different faces that share a common edge,
 *  determine if they are from identical loops or not.
 *
 *  Note that two identical loops in an anti-shared pair of faces
 *  (faces with opposite orientations) will also have opposite orientations.
 *
 *  Returns -
 *	0	Loops not identical
 *	1	Loops identical, faces are ON-shared
 *	2	Loops identical, faces are ON-anti-shared
 *	3	Loops identical, at least one is a wire loop.
 */
int
nmg_2lu_identical( eu1, eu2 )
CONST struct edgeuse	*eu1;
CONST struct edgeuse	*eu2;
{
	CONST struct loopuse	*lu1;
	CONST struct loopuse	*lu2;
	CONST struct edgeuse	*eu1_first;
	CONST struct faceuse	*fu1;
	CONST struct faceuse	*fu2;
	int			ret;

	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);

	if( eu1->e_p != eu2->e_p )  rt_bomb("nmg_2lu_identical() differing edges?\n");

	/* get the starting vertex of each edgeuse to be the same. */
	if (eu2->vu_p->v_p != eu1->vu_p->v_p) {
		eu2 = eu2->eumate_p;
		if (eu2->vu_p->v_p != eu1->vu_p->v_p)
			rt_bomb("nmg_2lu_identical() radial edgeuse doesn't share verticies\n");
	}

	lu1 = eu1->up.lu_p;
	lu2 = eu2->up.lu_p;

	NMG_CK_LOOPUSE(lu1);
	NMG_CK_LOOPUSE(lu2);

    	/* march around the two loops to see if they 
    	 * are the same all the way around.
    	 */
	eu1_first = eu1;
	do {
		if( eu1->vu_p->v_p != eu2->vu_p->v_p )  {
			ret = 0;
			goto out;
		}
		eu1 = RT_LIST_PNEXT_CIRC(edgeuse, &eu1->l);
		eu2 = RT_LIST_PNEXT_CIRC(edgeuse, &eu2->l);
	} while ( eu1 != eu1_first );

	if( *lu1->up.magic_p != NMG_FACEUSE_MAGIC ||
	    *lu2->up.magic_p != NMG_FACEUSE_MAGIC )  {
		ret = 3;	/* one is a wire loop */
	    	goto out;
	    }

	fu1 = lu1->up.fu_p;
	fu2 = lu2->up.fu_p;

	if( fu1->f_p->g.plane_p != fu2->f_p->g.plane_p )  {
		rt_log("nmg_2lu_identical() loops lu1=x%x lu2=x%x are shared, face geometry is not? fg1=x%x, fg2=x%x\n",
			lu1, lu2, fu1->f_p->g.plane_p, fu2->f_p->g.plane_p);
		rt_log("---- fu1, f=x%x, flip=%d\n", fu1->f_p, fu1->f_p->flip);
		nmg_pr_fg(fu1->f_p->g.plane_p, 0);
		nmg_pr_fu_briefly(fu1, 0);

		rt_log("---- fu2, f=x%x, flip=%d\n", fu2->f_p, fu2->f_p->flip);
		nmg_pr_fg(fu2->f_p->g.plane_p, 0);
		nmg_pr_fu_briefly(fu2, 0);

		/* Drop back to using a geometric calculation */
		if( VDOT( fu1->f_p->g.plane_p->N, fu2->f_p->g.plane_p->N ) < 0 )
			ret = 2;	/* ON anti-shared */
		else
			ret = 1;	/* ON shared */
		goto out;
	}

	if( rt_g.NMG_debug & DEBUG_BASIC )  {
		rt_log("---- fu1, f=x%x, flip=%d\n", fu1->f_p, fu1->f_p->flip);
		nmg_pr_fu_briefly(fu1, 0);
		rt_log("---- fu2, f=x%x, flip=%d\n", fu2->f_p, fu2->f_p->flip);
		nmg_pr_fu_briefly(fu2, 0);
	}

	/*
	 *  The two loops are identical, compare the two faces.
	 *  Only raw face orientations count here.
	 *  Loopuse and faceuse orientations do not matter.
	 */
	if( fu1->f_p->flip != fu2->f_p->flip )
		ret = 2;		/* ON anti-shared */
	else
		ret = 1;		/* ON shared */
out:
	if( rt_g.NMG_debug & DEBUG_BASIC )  {
		rt_log("nmg_2lu_identical(eu1=x%x, eu2=x%x) ret=%d\n",
			eu1, eu2, ret);
	}
	return ret;
}

/*
 *			C L A S S _ L U _ V S _ S
 */
static int class_lu_vs_s(lu, s, classlist, tol)
struct loopuse		*lu;
struct shell		*s;
long			*classlist[4];
CONST struct rt_tol	*tol;
{
	int class;
	unsigned int	in, outside, on;
	struct edgeuse *eu, *p, *q;
	struct loopuse *q_lu;
	struct vertexuse *vu;
	long		magic1;
	char		*reason = "Unknown";
	int		seen_error = 0;
	int		status = 0;

	NMG_CK_LOOPUSE(lu);
	NMG_CK_SHELL(s);
	RT_CK_TOL(tol);

	/* check to see if loop is already in one of the lists */
	if( NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], lu->l_p) )  {
		reason = "of classlist";
		status = INSIDE;
		goto out;
	}

	if( NMG_INDEX_TEST(classlist[NMG_CLASS_AonBshared], lu->l_p) )  {
		reason = "of classlist";
		status = ON_SURF;
		goto out;
	}

	if( NMG_INDEX_TEST(classlist[NMG_CLASS_AoutB], lu->l_p) )  {
		reason = "of classlist";
		status = OUTSIDE;
		goto out;
	}

	magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC) {
		/* Loop of a single vertex */
		reason = "of vertex classification";
		vu = RT_LIST_PNEXT( vertexuse, &lu->down_hd );
		NMG_CK_VERTEXUSE(vu);
		class = class_vu_vs_s(vu, s, classlist, tol);
		switch (class) {
		case INSIDE:
			NMG_INDEX_SET(classlist[NMG_CLASS_AinB], lu->l_p);
			break;
		case OUTSIDE:
			NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], lu->l_p);
			 break;
		case ON_SURF:
			NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], lu->l_p);
			break;
		default:
			rt_bomb("class_lu_vs_s: bad vertexloop classification\n");
		}
		status = class;
		goto out;
	} else if (magic1 != NMG_EDGEUSE_MAGIC) {
		rt_bomb("class_lu_vs_s: bad child of loopuse\n");
	}

	/* loop is collection of edgeuses */
retry:
	in = outside = on = 0;
	for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		/* Classify each edgeuse */
		class = class_eu_vs_s(eu, s, classlist, tol);
		switch (class) {
		case INSIDE	: ++in; 
				break;
		case OUTSIDE	: ++outside;
				break;
		case ON_SURF	: ++on;
				break;
		default		: rt_bomb("class_lu_vs_s: bad class for edgeuse\n");
		}
	}

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		rt_log("class_lu_vs_s: Loopuse edges in:%d on:%d out:%d\n", in, on, outside);

	if (in > 0 && outside > 0) {
		FILE *fp;
		rt_log("Loopuse edges in:%d on:%d out:%d, turning on DEBUG_CLASSIFY\n", in, on, outside);
		if( rt_g.NMG_debug & DEBUG_CLASSIFY )  {
			char		buf[128];
			static int	num;
			long		*b;
			struct model	*m;

			m = nmg_find_model(lu->up.magic_p);
			b = (long *)rt_calloc(m->maxindex, sizeof(long), "nmg_pl_lu flag[]");

			for(RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
				if (NMG_INDEX_TEST(classlist[NMG_CLASS_AinB], eu->e_p))
					nmg_euprint("In:  edgeuse", eu);
				else if (NMG_INDEX_TEST(classlist[NMG_CLASS_AoutB], eu->e_p))
					nmg_euprint("Out: edgeuse", eu);
				else if (NMG_INDEX_TEST(classlist[NMG_CLASS_AonBshared], eu->e_p))
					nmg_euprint("On:  edgeuse", eu);
				else
					nmg_euprint("BAD: edgeuse", eu);
			}

			sprintf(buf, "badloop%d.pl", num++);
			if ((fp=fopen(buf, "w")) != NULL) {
				nmg_pl_lu(fp, lu, b, 255, 255, 255);
				nmg_pl_s(fp, s);
				fclose(fp);
				rt_log("wrote %s\n", buf);
			}
			nmg_pr_lu(lu, "");
			nmg_stash_model_to_file( "class.g", nmg_find_model((long *)lu), "class_ls_vs_s: loop transits plane of shell/face?");
			rt_free( (char *)b, "nmg_pl_lu flag[]" );
		}
		rt_g.NMG_debug |= DEBUG_CLASSIFY;
		if(seen_error)
			rt_bomb("class_lu_vs_s: loop transits plane of shell/face?\n");
		seen_error = 1;
		goto retry;
	}
	if (outside > 0) {
		NMG_INDEX_SET(classlist[NMG_CLASS_AoutB], lu->l_p);
		reason = "edgeuses were OUT and ON";
		status = OUTSIDE;
		goto out;
	} else if (in > 0) {
		NMG_INDEX_SET(classlist[NMG_CLASS_AinB], lu->l_p);
		reason = "edgeuses were IN and ON";
		status = INSIDE;
		goto out;
	} else if (on == 0)
		rt_bomb("class_lu_vs_s: alright, who's the wiseguy that stole my edgeuses?\n");


	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		rt_log("\tAll edgeuses of loop are ON\n");

	/* since all of the edgeuses of this loop are "on" the other shell,
	 * we need to see if this loop is "on" the other shell
	 *
	 * if we're a wire edge loop, simply having all edges "on" is
	 *	sufficient.
	 *
	 * foreach radial edgeuse
	 * 	if edgeuse vertex is same and edgeuse parent shell is the one
	 *	 	desired, then....
	 *
	 *		p = edgeuse, q = radial edgeuse
	 *		while p's vertex equals q's vertex and we
	 *			haven't come full circle
	 *			move p and q forward
	 *		if we made it full circle, loop is on
	 */

	if (*lu->up.magic_p == NMG_SHELL_MAGIC) {
		NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], lu->l_p);
		reason = "loop is a wire loop in the shell";
		status = ON_SURF;
		goto out;
	}

	NMG_CK_FACEUSE(lu->up.fu_p);

	eu = RT_LIST_FIRST(edgeuse, &lu->down_hd);
	for(
	    eu = eu->radial_p->eumate_p;
	    eu != RT_LIST_FIRST(edgeuse, &lu->down_hd);
	    eu = eu->radial_p->eumate_p
	)  {
		int	code;

		/* if the radial edge is a part of a loop which is part of
		 * a face, then it's one that we might be "on"
		 */
		if( *eu->up.magic_p != NMG_LOOPUSE_MAGIC ) continue;
	    	q_lu = eu->up.lu_p;
		if( *q_lu->up.magic_p != NMG_FACEUSE_MAGIC ) continue;

		if( q_lu == lu )  continue;

		/* Only consider faces from shell 's' */
		if( q_lu->up.fu_p->s_p != s )  continue;

		code = nmg_2lu_identical( eu,
			RT_LIST_FIRST(edgeuse, &lu->down_hd) );
	    	switch(code)  {
	    	default:
	    	case 0:
	    		/* Not identical */
	    		break;
	    	case 1:
	    		/* ON-shared */
		    	NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared],
		    		lu->l_p );
			if (rt_g.NMG_debug & DEBUG_CLASSIFY)
				rt_log("Loop is on-shared\n");
			reason = "edges identical with radial face, normals colinear";
	    		status = ON_SURF;
	    		goto out;
	    	case 2:
	    		/* ON-antishared */
			NMG_INDEX_SET(classlist[NMG_CLASS_AonBanti],
				lu->l_p );
			if (rt_g.NMG_debug & DEBUG_CLASSIFY)
				rt_log("Loop is on-antishared\n");
			reason = "edges identical with radial face, normals opposite";
			status = ON_SURF;
			goto out;
	    	case 3:
	    		rt_bomb("class_lu_vs_s() unexpected wire ON\n");
		}
	}



	/* OK, the edgeuses are all "on", but the loop isn't.  Time to
	 * decide if the loop is "out" or "in".  To do this, we look for
	 * an edgeuse radial to one of the edgeuses in the loop which is
	 * a part of a face in the other shell.  If/when we find such a
	 * radial edge, we check the direction (in/out) of the faceuse normal.
	 * if the faceuse normal is pointing out of the shell, we are outside.
	 * if the faceuse normal is pointing into the shell, we are inside.
	 */

	for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {

		p = eu->radial_p;
		do {
			if (*p->up.magic_p == NMG_LOOPUSE_MAGIC &&
			    *p->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
			    p->up.lu_p->up.fu_p->s_p == s) {

			    	if (p->up.lu_p->up.fu_p->orientation == OT_OPPOSITE) {
			    		NMG_INDEX_SET(classlist[NMG_CLASS_AinB],
			    			lu->l_p );
					if (rt_g.NMG_debug & DEBUG_CLASSIFY)
						rt_log("Loop is INSIDE\n");
			    		reason = "radial faceuse is OT_OPPOSITE";
			    		status = INSIDE;
			    		goto out;
			    	} else if (p->up.lu_p->up.fu_p->orientation == OT_SAME) {
			    		NMG_INDEX_SET(classlist[NMG_CLASS_AoutB],
			    			lu->l_p );
					if (rt_g.NMG_debug & DEBUG_CLASSIFY)
						rt_log("Loop is OUTSIDE\n");
			    		reason = "radial faceuse is OT_SAME";
			    		status = OUTSIDE;
					goto out;
			    	} else {
			    		rt_bomb("class_lu_vs_s() bad fu orientation\n");
			    	}
			}
			p = p->eumate_p->radial_p;
		} while (p != eu->eumate_p);

	}

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		rt_log("Loop is OUTSIDE 'cause it isn't anything else\n");

	/* Since we didn't find any radial faces to classify ourselves against
	 * and we already know that the edges are all "on" that must mean that
	 * the loopuse is "on" a wireframe portion of the shell.
	 */
	NMG_INDEX_SET( classlist[NMG_CLASS_AoutB], lu->l_p );
	reason = "loopuse is ON a wire loop in the shell";
	status = OUTSIDE;
out:
	if (rt_g.NMG_debug & DEBUG_CLASSIFY) {
		rt_log("class_lu_vs_s(lu=x%x) return %s because %s\n",
			lu, nmg_class_status(status), reason );
	}
	return status;
}

/*
 *			C L A S S _ F U _ V S _ S
 */
static void class_fu_vs_s(fu, s, classlist, tol)
struct faceuse		*fu;
struct shell		*s;
long			*classlist[4];
CONST struct rt_tol	*tol;
{
	struct loopuse *lu;
	plane_t		n;
	
	NMG_CK_FACEUSE(fu);
	NMG_CK_SHELL(s);

	NMG_GET_FU_PLANE( n, fu );

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
        	PLPRINT("\nclass_fu_vs_s plane equation:", n);

	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd))
		(void)class_lu_vs_s(lu, s, classlist, tol);

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		rt_log("class_fu_vs_s() END\n");
}

/*
 *			N M G _ C L A S S _ S H E L L S
 *
 *	Classify one shell WRT the other shell
 *
 *  Implicit return -
 *	Each element's classification will be represented by a
 *	SET entry in the appropriate classlist[] array.
 */
void
nmg_class_shells(sA, sB, classlist, tol)
struct shell	*sA;
struct shell	*sB;
long		*classlist[4];
CONST struct rt_tol	*tol;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;

	NMG_CK_SHELL(sA);
	NMG_CK_SHELL(sB);
	RT_CK_TOL(tol);

	if (rt_g.NMG_debug & DEBUG_CLASSIFY &&
	    RT_LIST_NON_EMPTY(&sA->fu_hd))
		rt_log("nmg_class_shells - doing faces\n");

	fu = RT_LIST_FIRST(faceuse, &sA->fu_hd);
	while (RT_LIST_NOT_HEAD(fu, &sA->fu_hd)) {

		class_fu_vs_s(fu, sB, classlist, tol);

		if (RT_LIST_PNEXT(faceuse, fu) == fu->fumate_p)
			fu = RT_LIST_PNEXT_PNEXT(faceuse, fu);
		else
			fu = RT_LIST_PNEXT(faceuse, fu);
	}
	
	if (rt_g.NMG_debug & DEBUG_CLASSIFY &&
	    RT_LIST_NON_EMPTY(&sA->lu_hd))
		rt_log("nmg_class_shells - doing loops\n");

	lu = RT_LIST_FIRST(loopuse, &sA->lu_hd);
	while (RT_LIST_NOT_HEAD(lu, &sA->lu_hd)) {

		(void)class_lu_vs_s(lu, sB, classlist, tol);

		if (RT_LIST_PNEXT(loopuse, lu) == lu->lumate_p)
			lu = RT_LIST_PNEXT_PNEXT(loopuse, lu);
		else
			lu = RT_LIST_PNEXT(loopuse, lu);
	}

	if (rt_g.NMG_debug & DEBUG_CLASSIFY &&
	    RT_LIST_NON_EMPTY(&sA->eu_hd))
		rt_log("nmg_class_shells - doing edges\n");

	eu = RT_LIST_FIRST(edgeuse, &sA->eu_hd);
	while (RT_LIST_NOT_HEAD(eu, &sA->eu_hd)) {

		(void)class_eu_vs_s(eu, sB, classlist, tol);

		if (RT_LIST_PNEXT(edgeuse, eu) == eu->eumate_p)
			eu = RT_LIST_PNEXT_PNEXT(edgeuse, eu);
		else
			eu = RT_LIST_PNEXT(edgeuse, eu);
	}

	if (sA->vu_p) {
		if (rt_g.NMG_debug)
			rt_log("nmg_class_shells - doing vertex\n");
		(void)class_vu_vs_s(sA->vu_p, sB, classlist, tol);
	}
}

/*	N M G _ C L A S S I F Y _ P T _ L O O P
 *
 *	A generally available interface to nmg_class_pt_l
 *
 *	returns the classification from nmg_class_pt_l
 *	or a (-1) on error
 */
int
nmg_classify_pt_loop( pt , lu , tol )
CONST point_t pt;
CONST struct loopuse *lu;
CONST struct rt_tol *tol;
{
	struct neighbor	closest;
	struct faceuse *fu;
	vect_t n;
	fastf_t dist;

	NMG_CK_LOOPUSE( lu );
	RT_CK_TOL( tol );

	if( *lu->up.magic_p != NMG_FACEUSE_MAGIC )
	{
		rt_log( "nmg_classify_pt_loop: lu not part of a faceuse!!\n" );
		return( -1 );
	}

	fu = lu->up.fu_p;

	/* Validate distance from point to plane */
	NMG_GET_FU_PLANE( n, fu );
	if( (dist=fabs(DIST_PT_PLANE( pt, n ))) > tol->dist )  {
		rt_log("nmg_classify_pt_l() ERROR, point (%g,%g,%g) not on face, dist=%g\n",
			V3ARGS(pt), dist );
		return( -1 );
	}


	/* find the closest approach in this face to the projected point */
	closest.dist = MAX_FASTF;
	closest.p.eu = (struct edgeuse *)NULL;
	closest.class = NMG_CLASS_AoutB;	/* default return */

	nmg_class_pt_l( &closest , pt , lu , tol );

	return( closest.class );
}

/*	N M G _ C L A S S I F Y _ L U _ L U
 *
 *	Generally available classifier for
 *	determining if one loop is within another
 *
 *	returns classification based on nmg_class_pt_l results
 */
int
nmg_classify_lu_lu( lu1 , lu2 , tol )
CONST struct loopuse *lu1,*lu2;
CONST struct rt_tol *tol;
{
	struct faceuse *fu1,*fu2;
	struct edgeuse *eu;
	struct neighbor	closest;
	int same_loop;

	NMG_CK_LOOPUSE( lu1 );
	NMG_CK_LOOPUSE( lu2 );
	RT_CK_TOL( tol );

	if( rt_g.NMG_debug & DEBUG_CLASSIFY )
		rt_log( "nmg_classify_lu_lu( lu1=x%x , lu2=x%x )\n", lu1, lu2 );

	if( lu1 == lu2 || lu1 == lu2->lumate_p )
		return( NMG_CLASS_AonBshared );

	if( *lu1->up.magic_p != NMG_FACEUSE_MAGIC )
	{
		rt_log( "nmg_classify_lu_lu: lu1 not part of a faceuse\n" );
		return( -1 );
	}

	if( *lu2->up.magic_p != NMG_FACEUSE_MAGIC )
	{
		rt_log( "nmg_classify_lu_lu: lu2 not part of a faceuse\n" );
		return( -1 );
	}

	fu1 = lu1->up.fu_p;
	NMG_CK_FACEUSE( fu1 );
	fu2 = lu2->up.fu_p;
	NMG_CK_FACEUSE( fu2 );

	if( fu1->f_p != fu2->f_p )
	{
		rt_log( "nmg_classify_lu_lu: loops are not in same face\n" );
		return( -1 );
	}

	/* do simple check for two loops of the same vertices */
	if( RT_LIST_FIRST_MAGIC( &lu1->down_hd ) == NMG_EDGEUSE_MAGIC &&
	    RT_LIST_FIRST_MAGIC( &lu2->down_hd ) == NMG_EDGEUSE_MAGIC )
	{
		struct edgeuse *eu1_start,*eu2_start;
		struct edgeuse *eu1,*eu2;

		same_loop = 1;
		eu1_start = RT_LIST_FIRST( edgeuse , &lu1->down_hd );
		NMG_CK_EDGEUSE( eu1_start );
		eu2_start = RT_LIST_FIRST( edgeuse , &lu2->down_hd );
		NMG_CK_EDGEUSE( eu2_start );
		while( RT_LIST_NOT_HEAD( eu2_start , &lu2->down_hd ) &&
			eu2_start->vu_p->v_p != eu1_start->vu_p->v_p )
			{
				NMG_CK_EDGEUSE( eu2_start );
				eu2_start = RT_LIST_PNEXT( edgeuse , &eu2_start->l );
			}

		if( RT_LIST_NOT_HEAD( eu2_start , &lu2->down_hd ) &&
			eu1_start->vu_p->v_p == eu2_start->vu_p->v_p )
		{
			/* check the rest of the loop */
			eu1 = eu1_start;
			eu2 = eu2_start;
			while( RT_LIST_NOT_HEAD( eu1 , &lu1->down_hd ) )
			{
				if( eu1->vu_p->v_p != eu2->vu_p->v_p )
				{
					same_loop = 0;
					break;
				}
				eu1 = RT_LIST_PNEXT( edgeuse , &eu1->l );
				eu2 = RT_LIST_PNEXT_CIRC( edgeuse , &eu2->l );
			}

			if( same_loop )
				return( NMG_CLASS_AonBshared );

			/* maybe the other way round */
			same_loop = 1;
			eu1 = eu1_start;
			eu2 = eu2_start;
			while( RT_LIST_NOT_HEAD( eu1 , &lu1->down_hd ) )
			{
				if( eu1->vu_p->v_p != eu2->vu_p->v_p )
				{
					same_loop = 0;
					break;
				}
				eu1 = RT_LIST_PNEXT( edgeuse , &eu1->l );
				eu2 = RT_LIST_PPREV_CIRC( edgeuse , &eu2->l );
			}

			if( same_loop )
				return( NMG_CLASS_AonBshared );
		}
	}
	else if( RT_LIST_FIRST_MAGIC( &lu1->down_hd ) == NMG_VERTEXUSE_MAGIC &&
		 RT_LIST_FIRST_MAGIC( &lu2->down_hd ) == NMG_VERTEXUSE_MAGIC )
	{
		struct vertexuse *vu1,*vu2;

		vu1 = RT_LIST_FIRST( vertexuse , &lu1->down_hd );
		vu2 = RT_LIST_FIRST( vertexuse , &lu2->down_hd );

		if( vu1->v_p == vu2->v_p )
			return( NMG_CLASS_AonBshared );
		else
			return( NMG_CLASS_AoutB );
	}

	/* initialize the "closest" structure */
	closest.dist = MAX_FASTF;
	closest.p.eu = (struct edgeuse *)NULL;
	closest.class = NMG_CLASS_AoutB;	/* default return */

	if( RT_LIST_FIRST_MAGIC( &lu1->down_hd ) == NMG_VERTEXUSE_MAGIC )
	{
		struct vertexuse *vu;
		struct vertex_g *vg;

		vu = RT_LIST_FIRST( vertexuse , &lu1->down_hd );
		NMG_CK_VERTEXUSE( vu );
		vg = vu->v_p->vg_p;
		NMG_CK_VERTEX_G( vg );

		nmg_class_pt_l( &closest , vg->coord , lu2 , tol );

		return( closest.class );
		
	}

	for( RT_LIST_FOR( eu , edgeuse , &lu1->down_hd ) )
	{
		struct vertex_g *vg;

		NMG_CK_EDGEUSE( eu );

		vg = eu->vu_p->v_p->vg_p;
		NMG_CK_VERTEX_G( vg );

		/* reset the closest structure for each call */
		closest.dist = MAX_FASTF;
		closest.p.eu = (struct edgeuse *)NULL;
		closest.class = NMG_CLASS_AoutB;	/* default return */

		nmg_class_pt_l( &closest , vg->coord , lu2 , tol );

		if( closest.class != NMG_CLASS_AonBshared )
			return( closest.class );
	}

	return( NMG_CLASS_AonBshared );
}
