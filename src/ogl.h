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

#ifdef DEBUG
#define ogl_error() _ogl_error(__FILE__, __LINE__)
#else
#define ogl_error()
#endif

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
	GLint modelview_location; // We need the modelview matrix as well
	GLint position_location;
	GLint normal_location;
	GLint color_location;
	GLint lightning_enabled_location;

	// Phong lightning parameters
	GLint ambient_location;
	GLint diffuse_location;
	GLint specular_location;
	GLint light_pos_location;
	GLint normal_matrix_location;

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
	GLint modelview_location; // We need the modelview matrix as well
	GLint position_location;
	GLint normal_location;
	GLint color_location;

	// Phong lightning parameters
	GLint ambient_location;
	GLint diffuse_location;
	GLint specular_location;
	GLint light_pos_location;
	GLint normal_matrix_location;

	// Fog parameters
	GLint fog_color_location;
	GLint fog_start_location;
	GLint fog_end_location;

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
void ogl_enable_lightning();
void ogl_disable_lightning();
void ogl_draw( void );
void _ogl_error(const char *filename, int line_num);
GLuint ogl_select_modern(GLint x, GLint y);
#ifdef __GTK_H__
GtkWidget *ogl_widget_new( void );
#endif


/* end ogl.h */
