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

typedef enum {
	RENDERMODE_RENDER = 0,  // The usual rendering
	RENDERMODE_SELECT	// Render with unique colors when selecting (no display)
} RenderMode;

// Global state for modern GL
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

	RenderMode render_mode;
} FsvGlState;

extern FsvGlState gl;

// State for the About and splash screens in modern GL
typedef struct AboutGlState {

	GLuint program; // Handle for the shaders

	// These _location variables are handles to input 'slots' in the
	// vertex shader.
	GLint mvp_location;
	GLint position_location;
	GLint normal_location;
	GLint color_location;

} AboutGlState;

extern AboutGlState aboutGL;

typedef struct {
	GLfloat position[3];
	GLfloat normal[3];
	GLfloat color[3];
} AboutVertex;


GLuint ogl_create_shader(GLenum shader_type, const char *source);
void ogl_resize( void );
void ogl_refresh( void );
double ogl_aspect_ratio( void );
void ogl_upload_matrices(gboolean text);
void ogl_draw( void );
GLuint ogl_select_modern(GLint x, GLint y);
int ogl_select( int x, int y, const GLuint **selectbuf_ptr );
#ifdef __GTK_H__
GtkWidget *ogl_widget_new( void );
#endif


/* end ogl.h */
