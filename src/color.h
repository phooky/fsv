/* color.h */

/* Node coloration */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * SPDX-FileCopyrightText: 2021 Janne Blomqvist <blomqvist.janne@gmail.com>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#ifdef FSV_COLOR_H
	#error
#endif
#define FSV_COLOR_H


/* The various coloring modes */
typedef enum {
	COLOR_BY_NODETYPE,
	COLOR_BY_TIMESTAMP,
	COLOR_BY_WPATTERN,
        COLOR_NONE
} ColorMode;

/* Every file has three timestamps */
typedef enum {
	TIMESTAMP_ACCESS, /* atime - time of last access */
	TIMESTAMP_MODIFY, /* mtime - time of last modification */
	TIMESTAMP_ATTRIB, /* ctime - time of last attribute change */
	TIMESTAMP_NONE
} TimeStampType;

/* Various kinds of spectrums */
typedef enum {
	SPECTRUM_RAINBOW,
        SPECTRUM_HEAT,
	SPECTRUM_GRADIENT,
	SPECTRUM_NONE
} SpectrumType;


/* Used indirectly in struct ColorConfig (see below) */
struct WPatternGroup {
	RGBAColor color;
	GList *wp_list; /* elements: char * */
};

struct ColorConfig {
	/* Node type colors */
	struct ColorByNodeType {
		RGBAColor colors[NUM_NODE_TYPES];
	} by_nodetype;

	/* Temporal spectrum type and range */
	struct ColorByTime {
		SpectrumType spectrum_type;
		TimeStampType timestamp_type;
		time_t new_time;
		time_t old_time;
		/* Following two are for gradient spectrums */
		RGBAColor old_color;
		RGBAColor new_color;
	} by_timestamp;

	/* Wildcard patterns */
	struct ColorByWPattern {
		GList *wpgroup_list; /* elements: struct WPatternGroup */
		RGBAColor default_color;
	} by_wpattern;
};


void color_config_destroy( struct ColorConfig *ccfg );
ColorMode color_get_mode( void );
void color_get_config( struct ColorConfig *ccfg );
void color_assign_recursive( GNode *dnode );
void color_set_mode( ColorMode mode );
RGBAColor color_spectrum_color( SpectrumType type, double x, void *data );
void color_set_config( struct ColorConfig *new_ccfg, ColorMode mode );
void color_write_config( void );
void color_init( void );


/* end color.h */
