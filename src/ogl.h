/* ogl.h */

/* Primary OpenGL interface */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * SPDX-FileCopyrightText: 2021 Janne Blomqvist <blomqvist.janne@gmail.com>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#pragma once

#include <epoxy/gl.h>
#include <cglm/cglm.h>


typedef struct FsvGlState {
	GLuint vao; // OpenGL Vertex Array Object Names

	GLuint program; // Handle for the shaders

	// These _location variables are handles to input 'slots' in the
	// vertex shader.
	GLint mvp_location;
	GLint position_location;
	GLint normal_location;
	GLint color_location;

	// Projection and modelview matrices (using cglm library)
	mat4 projection;
	mat4 modelview;
	// The base modelview matrix. Not to be used directly, but copy it
	// over the modelview matrix when resetting matrix state.
	mat4 base_modelview;
} FsvGlState;

extern FsvGlState gl;

void ogl_resize( void );
void ogl_refresh( void );
double ogl_aspect_ratio( void );
void ogl_upload_matrices();
void ogl_draw( void );
#ifdef GL_NO_ERROR
int ogl_select( int x, int y, const GLuint **selectbuf_ptr );
#endif
#ifdef __GTK_H__
GtkWidget *ogl_widget_new( void );
#endif


/* end ogl.h */
