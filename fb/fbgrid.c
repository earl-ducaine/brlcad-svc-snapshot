/*
 *			F B G R I D . C
 *
 *  Author -
 *	Phillip Dykstra
 *  	Includes the old fbgrid code by:
 *	Michael John Muuss
 *	Gary S. Moss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "fb.h"

extern int	getopt();
extern char	*optarg;
extern int	optind;
extern char	*malloc();

static RGBpixel	*white_line, *grey_line, *dark_line;
static FBIO	*fbp;
static char	*framebuffer = NULL;

#define OLD	0
#define	BINARY	1
#define	DECIMAL 2

static int	fbwidth = 0;
static int	fbheight = 0;
static int	flavor = DECIMAL;
static int	clear = 0;

void		grid(), oldflavor();

static char usage[] = "\
Usage: fbgrid [-h -c] [-b | -d | -o] [-F framebuffer]\n\
	[-S squaresize] [-W width] [-N height]\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "hcbdoF:s:w:n:S:W:N:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			fbheight = fbwidth = 1024;
			break;
		case 'c':
			clear = 1;
			break;
		case 'b':
			flavor = BINARY;
			break;
		case 'd':
			flavor = DECIMAL;
			break;
		case 'o':
			flavor = OLD;
			break;
		case 'F':
			framebuffer = optarg;
			break;
		case 'S':
		case 's':
			/* square size */
			fbheight = fbwidth = atoi(optarg);
			break;
		case 'W':
		case 'w':
			fbwidth = atoi(optarg);
			break;
		case 'N':
		case 'n':
			fbheight = atoi(optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "fbgrid: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

main( argc, argv )
int argc; char **argv;
{
	int	i;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( flavor == OLD )
		oldflavor();	/* exits */

	if( (fbp = fb_open( NULL, fbwidth, fbheight )) == NULL )
		exit( 2 );

	fbwidth = fb_getwidth( fbp );
	fbheight = fb_getheight( fbp );

	/* Initialize the color lines */
	white_line = (RGBpixel *)malloc( fbwidth * sizeof(RGBpixel) );
	grey_line  = (RGBpixel *)malloc( fbwidth * sizeof(RGBpixel) );
	dark_line  = (RGBpixel *)malloc( fbwidth * sizeof(RGBpixel) );
	for( i = 0; i < fbwidth; i++ ) {
		white_line[i][RED] = white_line[i][GRN] = white_line[i][BLU] = 255;
		grey_line[i][RED] = grey_line[i][GRN] = grey_line[i][BLU] = 128;
		dark_line[i][RED] = dark_line[i][GRN] = dark_line[i][BLU] = 64;
	}

	if( clear )
		fb_clear( fbp, PIXEL_NULL );

	if( flavor == BINARY ) {
		/* Dark lines every 8 */
		grid( fbp, dark_line, 8 );
		/* Grey lines every 64 */
		grid( fbp, grey_line, 64 );
		/* White line every 128 */
		grid( fbp, white_line, 128 );
	} else { /* DECIMAL */
		/* Dark lines every 10 */
		grid( fbp, dark_line, 10 );
		/* Grey lines every 50 */
		grid( fbp, grey_line, 50 );
		/* White line every 100 */
		grid( fbp, white_line, 100 );
	}

	fb_close( fbp );
}

void
grid( fbp, line, spacing )
FBIO *fbp;
RGBpixel line[];
int	spacing;
{
	int	x, y;

	for( y = 0; y < fbheight; y += spacing )
		fb_write( fbp, 0, y, line, fbwidth );
	for( x = 0; x < fbwidth; x += spacing ) {
#if 0
		for( y = 0; y < fbheight; y++ )
			fb_write( fbp, x, y, line[0], 1 );
#else
		fb_writerect( fbp, x, 0, x, fbheight-1, line );
#endif
	}
}

void
oldflavor()
{
	register FBIO	*fbp;
	register int	x, y;
	register int	middle;
	register int	mask;
	register int	fb_sz;
	static RGBpixel	black, white, red;

	if( (fbp = fb_open( NULL, fbwidth, fbheight )) == NULL ) {
		exit( 1 );
	}

	fb_sz = fb_getwidth(fbp);
	white[RED] = white[GRN] = white[BLU] = 255;
	black[RED] = black[GRN] = black[BLU] = 0;
	red[RED] = 255;
	middle = fb_sz/2;
	fb_ioinit(fbp);
	if( fb_sz <= 512 )
		mask = 0x7;
	else
		mask = 0xf;

	for( y = fb_sz-1; y >= 0; y-- )  {
		for( x = 0; x < fb_sz; x++ ) {
			if( x == y || x == fb_sz - y ) {
				FB_WPIXEL( fbp, white );
			} else
			if( x == middle || y == middle ) {
				FB_WPIXEL( fbp, red );
			} else
			if( (x & mask) && (y & mask) ) {
				FB_WPIXEL( fbp, black );
			} else {
				FB_WPIXEL( fbp, white );
			}
		}
	}
	fb_close( fbp );
	exit( 0 );
}
