/* ogl.h */

/* Primary OpenGL interface */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#ifdef FSV_OGL_H
	#error
#endif
#define FSV_OGL_H


void ogl_resize( void );
void ogl_refresh( void );
double ogl_aspect_ratio( void );
void ogl_draw( void );
#ifdef GL_NO_ERROR
int ogl_select( int x, int y, const GLuint **selectbuf_ptr );
#endif
#ifdef __GTK_H__
GtkWidget *ogl_widget_new( void );
#endif


/* end ogl.h */
