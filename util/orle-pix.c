/*
 *			R L E - P I X . C
 *
 *  Author -
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
#include "fb.h"		/* For Pixel typedef */
#include "rle.h"

typedef unsigned char	u_char;
static char	*usage[] =
	{
"",
"rle-pix (%I%)",
"",
"Usage: rle-pix [-dv][-b (rgbBG)][file.rle]",
"",
"If no rle file is specifed, rle-pix will read its standard input.",
"Pix(5B) format is written to the standard output.",
0
	};

static FILE	*fp = stdin;
static Pixel	bgpixel = { 0, 0, 0, 0 };
static int	bgflag = 0;
static int	pars_Argv();
static int	xpos, ypos;
static int	xlen = -1, ylen = -1;
static void	prnt_Cmap();
static void	prnt_Usage();

/*	m a i n ( )							*/
main( argc, argv )
int	argc;
char	*argv[];
	{	register int	scan_ln;
		register int	fb_size = 512;
		static Pixel	scanbuf[1025];
		static Pixel	bg_scan[1025];
		static ColorMap	cmap;
		int		get_flags;
		int		scan_bytes;

	if( ! pars_Argv( argc, argv ) )
		{
		prnt_Usage();
		return	1;
		}
	if( rle_rhdr( fp, &get_flags, bgflag ? NULL : &bgpixel ) == -1 )
		return	1;

	rle_rlen( &xlen, &ylen );
	rle_rpos( &xpos, &ypos );

	/* Automatic selection of high res. device.			*/
	if( xpos + xlen > 512 || ypos + ylen > 512 )
		fb_size = 1024;
	if( xpos + xlen > fb_size )
		xlen = fb_size - xpos;
	if( ypos + ylen > fb_size )
		ylen = fb_size - ypos;
	rle_wlen( xlen, ylen, 0 );

	scan_bytes = fb_size * sizeof(Pixel);

	if( rle_verbose )
		(void) fprintf( stderr,
				"Background is %d %d %d\n",
				bgpixel.red, bgpixel.green, bgpixel.blue
				);

	/* If color map provided, use it, else go with standard map. */
	if( ! (get_flags & NO_COLORMAP) )
		{
		if( rle_verbose )
			(void) fprintf( stderr,
					"Loading saved color map from file\n"
					);
		if( rle_rmap( fp, &cmap ) == -1 )
			return	1;
		if( rle_verbose )
			prnt_Cmap( &cmap );
		}

	/* Fill buffer with background.					*/
	if( (get_flags & NO_BOX_SAVE) )
		{	register int	i;
			register Pixel	*to;
			register Pixel	*from;
		to = bg_scan;
		from = &bgpixel;
		for( i = 0; i < fb_size; i++ )
			*to++ = *from;
		}

	{	register int	by = fb_size;
	for( scan_ln = fb_size-1; scan_ln >= 0; scan_ln-- )
		{	static int	touched = 1;
			register int	pix;
		if( touched && (get_flags & NO_BOX_SAVE) )
			fill_Buffer(	(char *) scanbuf,
					(char *) bg_scan,
					scan_bytes,
					1
					);
		if( (touched = rle_decode_ln( fp, scanbuf )) == -1 )
			return	1;
		for( pix = 0; pix < fb_size; pix++ )
			{
			(void) putchar( scanbuf[pix].red );
			(void) putchar( scanbuf[pix].green );
			(void) putchar( scanbuf[pix].blue );
			}
		} /* end for */
	} /* end block */
	return	0;
	}

/*	f i l l _ B u f f e r ( )
	Fill cluster buffer from scanline (as fast as possible).
 */
fill_Buffer( buff_p, scan_p, scan_bytes, scan_lines )
register char	*buff_p;	/* On VAX, known to be R11 */
register char	*scan_p;	/* VAX R10 */
register int	scan_bytes;	/* VAX R9 */
register int	scan_lines;
	{	register int	i;
	for( i = 0; i < scan_lines; ++i )
		{
#if ! defined( vax ) || defined( lint )
		(void) strncpy( buff_p, scan_p, scan_bytes );
#else
		/* Pardon the efficiency.  movc3 len,src,dest */
		asm("	movc3	r9,(r10),(r11)");
#endif
		buff_p += scan_bytes;
		}
	return;
	}

/*	p a r s _ A r g v ( )						*/
static int
pars_Argv( argc, argv )
register char	**argv;
	{	register int	c;
		extern int	optind;
		extern char	*optarg;
	/* Parse options.						*/
	while( (c = getopt( argc, argv, "b:dv" )) != EOF )
		{
		switch( c )
			{
		case 'b' : /* User-specified background.		*/
			bgflag = optarg[0];
			switch( bgflag )
				{
			case 'r':
				bgpixel.red = 255;
				break;
			case 'g':
				bgpixel.green = 255;
				break;
			case 'b':
				bgpixel.blue = 255;
				break;
			case 'w':
				bgpixel.red =
				bgpixel.green =
				bgpixel.blue = 255;
				break;
			case 'B':		/* Black */
				break;
			case 'G':		/* 18% grey, for alignments */
				bgpixel.red =
				bgpixel.green =
				bgpixel.blue = 255.0 * 0.18;
				break;
			default:
				(void) fprintf( stderr,
						"Background '%c' unknown\n",
						bgflag
						);
				bgflag = 0;
				break;
				} /* End switch */
			break;
		case 'd' :
			rle_debug = 1;
			break;
		case 'v' :
			rle_verbose = 1;
			break;
		case '?' :
			return	0;
			} /* end switch */
		} /* end while */

	if( argv[optind] != NULL )
		if( (fp = fopen( argv[optind], "r" )) == NULL )
			{
			(void) fprintf( stderr,
					"Can't open %s for reading!\n",
					argv[optind]
					);
			return	0;
			}
	if( argc > ++optind )
		{
		(void) fprintf( stderr, "Too many arguments!\n" );
		return	0;
		}
	return	1;
	}

/*	p r n t _ U s a g e ( )
	Print usage message.
 */
static void
prnt_Usage()
	{	register char	**p = usage;
	while( *p )
		(void) fprintf( stderr, "%s\n", *p++ );
	return;
	}

static void
prnt_Cmap( cmap )
ColorMap	*cmap;
	{	register u_char	*cp;
		register int	i;
	(void) fprintf( stderr, "\t\t\t_________ Color map __________\n" );
	(void) fprintf( stderr, "Red segment :\n" );
	for( i = 0, cp = cmap->cm_red; i < 16; ++i, cp += 16 )
		{
		(void) fprintf( stderr,
	"%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n",
	/* 1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16 */
				cp[0],
				cp[1],
				cp[2],
				cp[3],
				cp[4],
				cp[5],
				cp[6],
				cp[7],
				cp[8],
				cp[9],
				cp[10],
				cp[11],
				cp[12],
				cp[13],
				cp[14],
				cp[15]
				);
		}
	(void) fprintf( stderr, "Green segment :\n" );
	for( i = 0, cp = cmap->cm_green; i < 16; ++i, cp += 16 )
		{
		(void) fprintf( stderr, 
	"%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n",
	/* 1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16 */
				cp[0],
				cp[1],
				cp[2],
				cp[3],
				cp[4],
				cp[5],
				cp[6],
				cp[7],
				cp[8],
				cp[9],
				cp[10],
				cp[11],
				cp[12],
				cp[13],
				cp[14],
				cp[15]
				);
		}
	(void) fprintf( stderr, "Blue segment :\n" );
	for( i = 0, cp = cmap->cm_blue; i < 16; ++i, cp += 16 )
		{
		(void) fprintf( stderr,
	"%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n",
	/* 1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16 */
				cp[0],
				cp[1],
				cp[2],
				cp[3],
				cp[4],
				cp[5],
				cp[6],
				cp[7],
				cp[8],
				cp[9],
				cp[10],
				cp[11],
				cp[12],
				cp[13],
				cp[14],
				cp[15]
				);
		}
	return;
	}
