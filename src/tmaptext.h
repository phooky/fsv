/* tmaptext.h */

/* Texture-mapped text */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#ifdef FSV_TMAPTEXT_H
	#error
#endif
#define FSV_TMAPTEXT_H


void text_init( void );
void text_pre( void );
void text_post( void );
void text_draw_straight( const char *text, const XYZvec *text_pos, const XYvec *text_max_dims );
void text_draw_straight_rotated( const char *text, const RTZvec *text_pos, const XYvec *text_max_dims );
void text_draw_curved( const char *text, const RTZvec *text_pos, const RTvec *text_max_dims );


/* end tmaptext.h */
