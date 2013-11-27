/*                        B N _ O B R . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"


int
main()
{
#if 0
    /* FIXME: finish this */
    point2d_t center;
    vect2d_t u, v;
    point2d_t pnts[4+1] = {{0}};
    int n = 4;

    V2SET(pnts[0], 1.5, 1.5);
    V2SET(pnts[1], 3.0, 2.0);
    V2SET(pnts[2], 2.0, 2.5);
    V2SET(pnts[3], 1.0, 2.0);

    bn_2d_obr(&center, &u, &v, const point2d_t *)pnts, n);
/*
    bu_log("result: P1 (%f, %f, %f)\n", V3ARGS(p0));
    bu_log("        P2 (%f, %f, %f)\n", V3ARGS(p1));
    bu_log("        P3 (%f, %f, %f)\n", V3ARGS(p2));
    bu_log("        P4 (%f, %f, %f)\n", V3ARGS(p3));*/
/*
    if (expected_result == actual_result) {
	return 0;
    } else {
	return -1;
    }
*/
#endif
    return 0;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
