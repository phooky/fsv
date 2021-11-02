/* ogl.c */

/* Primary OpenGL interface */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * Copyright (C) 2021 Janne Blomqvist <blomqvist.janne@gmail.com>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#include "common.h"
#include "ogl.h"

#include <gtk/gtk.h>
#include <gtkgl/gtkglarea.h>
#include <GL/glu.h> /* gluPickMatrix( ) */

#include "animation.h" /* redraw( ) */
#include "camera.h"
#include "geometry.h"
#include "tmaptext.h" /* text_init( ) */


/* Main viewport OpenGL area widget */
static GtkWidget *viewport_gl_area_w = NULL;

FsvGlState gl;
AboutGlState aboutGL;

GLuint
ogl_create_shader(GLenum shader_type, const char *source)
{
	GLuint shader = glCreateShader(shader_type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLint log_len;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);

		char *buffer = g_malloc(log_len + 1);
		glGetShaderInfoLog(shader, log_len, NULL, buffer);

		g_error("Compilation failure in %s shader: %s",
			shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
			buffer);

		g_free(buffer);

		glDeleteShader(shader);
		shader = 0;
	}
	return shader;
}

// Initialize OpenGL shaders
static GLuint
init_shaders(const char* vertex_resource, const char* fragment_resource)
{
	GBytes *source;
	GLuint program = 0;

	/* load the vertex shader */
	source = g_resources_lookup_data(vertex_resource, 0, NULL);
	GLuint vertex = ogl_create_shader(GL_VERTEX_SHADER, g_bytes_get_data(source, NULL));
	g_bytes_unref(source);
	if (vertex == 0)
		goto out;

	/* load the fragment shader */
	source = g_resources_lookup_data(fragment_resource, 0, NULL);
	GLuint fragment = ogl_create_shader(GL_FRAGMENT_SHADER, g_bytes_get_data(source, NULL));
	g_bytes_unref(source);
	if (fragment == 0)
		goto out;

	/* link the vertex and fragment shaders together */
	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);

	GLint status = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLint log_len = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);

		char *buffer = g_malloc(log_len + 1);
		glGetProgramInfoLog(program, log_len, NULL, buffer);

		g_error("Linking failure in program: %s", buffer);

		g_free(buffer);

		glDeleteProgram(program);
		program = 0;

		goto out;
	}

	/* the individual shaders can be detached and destroyed */
	glDetachShader(program, vertex);
	glDetachShader(program, fragment);

out:
	if (vertex != 0)
		glDeleteShader(vertex);
	if (fragment != 0)
		glDeleteShader(fragment);

	return program;
}

/* Initializes OpenGL state */
static void
ogl_init( void )
{
	float light_ambient[] = {0.2, 0.2, 0.2, 1};
	float light_diffuse[] = {0.6, 0.6, 0.6, 1};
	float light_specular[] = {.3, .3, .3, 1};
	float light_position[] = { 0.2, 0.0, 1.0, 0.0 };
	//float material_specular[] = {1, 1, 1, 1};

	// Modern OpenGL initialization. Even if we don't use the VAO it must be
	// initialized or VBO stuff might fail.
	glGenVertexArrays(1, &gl.vao);
	glBindVertexArray(gl.vao);

	// Shader programs for the normal view
	gl.program = init_shaders("/jabl/fsv/fsv-vertex.glsl",
				  "/jabl/fsv/fsv-fragment.glsl");
	if (!gl.program)
		g_error("Compiling shaders failed");
	/* get the location of the "mvp" uniform */
	gl.mvp_location = glGetUniformLocation(gl.program, "mvp");
	gl.modelview_location = glGetUniformLocation(gl.program, "modelview");
	gl.normal_matrix_location = glGetUniformLocation(gl.program, "normal_matrix");
	gl.ambient_location = glGetUniformLocation(gl.program, "ambient");
	gl.diffuse_location = glGetUniformLocation(gl.program, "diffuse");
	gl.specular_location = glGetUniformLocation(gl.program, "specular");
	gl.light_pos_location = glGetUniformLocation(gl.program, "light_pos");

	gl.color_location = glGetUniformLocation(gl.program, "color");
	gl.lightning_enabled_location = glGetUniformLocation(gl.program, "lightning_enabled");

	/* get the location of the "position" and "color" attributes */
	gl.position_location = glGetAttribLocation(gl.program, "position");
	gl.normal_location = glGetAttribLocation(gl.program, "normal");


	// Shader programs for the splash and about screens
	aboutGL.program = init_shaders("/jabl/fsv/fsv-about-vertex.glsl",
				       "/jabl/fsv/fsv-about-fragment.glsl");
	if (!aboutGL.program)
		g_error("Compiling shaders for about/splash screens failed");
	aboutGL.mvp_location = glGetUniformLocation(aboutGL.program, "mvp");
	aboutGL.modelview_location = glGetUniformLocation(aboutGL.program, "modelview");
	aboutGL.fog_color_location = glGetUniformLocation(aboutGL.program, "fog_color");
	aboutGL.fog_start_location = glGetUniformLocation(aboutGL.program, "fog_start");
	aboutGL.fog_end_location = glGetUniformLocation(aboutGL.program, "fog_end");
	aboutGL.ambient_location = glGetUniformLocation(aboutGL.program, "ambient");
	aboutGL.diffuse_location = glGetUniformLocation(aboutGL.program, "diffuse");
	aboutGL.specular_location = glGetUniformLocation(aboutGL.program, "specular");
	aboutGL.light_pos_location = glGetUniformLocation(aboutGL.program, "light_pos");
	aboutGL.normal_matrix_location = glGetUniformLocation(aboutGL.program, "normal_matrix");

	/* get the location of the "position", "normal" and "color" attributes */
	aboutGL.position_location = glGetAttribLocation(aboutGL.program, "position");
	aboutGL.normal_location = glGetAttribLocation(aboutGL.program, "normal");
	aboutGL.color_location = glGetAttribLocation(aboutGL.program, "color");

	/* Set viewport size */
	ogl_resize( );

	/* Create the initial modelview matrix
	 * (right-handed coordinate system, +z = straight up,
	 * camera at origin looking in -x direction) */
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity( );
	glRotated( -90.0, 1.0, 0.0, 0.0 );
	glRotated( -90.0, 0.0, 0.0, 1.0 );
	glPushMatrix( ); /* Matrix will stay just below top of MVM stack */
	// Same but for the modern OpenGL code
	glm_mat4_identity(gl.modelview);
	glm_rotate_x(gl.modelview, -M_PI_2, gl.modelview);
	glm_rotate_z(gl.modelview, -M_PI_2, gl.modelview);
	glm_mat4_copy(gl.modelview, gl.base_modelview);
	glm_mat4_identity(gl.projection);

	/* Set up lighting */
	glUseProgram(gl.program);
	ogl_enable_lightning();
	glUniform1f(gl.ambient_location, light_ambient[0]);
	glUniform1f(gl.diffuse_location, light_diffuse[0]);
	glUniform1f(gl.specular_location, light_specular[0]);
	glUniform4fv(gl.light_pos_location, 1, light_position);
	glUseProgram(0);
	glUseProgram(aboutGL.program);
	glUniform1f(aboutGL.ambient_location, light_ambient[0]);
	glUniform1f(aboutGL.diffuse_location, light_diffuse[0]);
	glUniform1f(aboutGL.specular_location, light_specular[0]);
	glUniform4fv(aboutGL.light_pos_location, 1, light_position);
	glUseProgram(0);

	/* Miscellaneous */
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glEnable( GL_CULL_FACE );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glEnable( GL_POLYGON_OFFSET_FILL );
	glPolygonOffset( 1.0, 1.0 );
	glClearColor( 0.0, 0.0, 0.0, 0.0 );
	glEnable(GL_LINE_SMOOTH);

	/* Initialize texture-mapped text engine */
	text_init( );
}


/* Changes viewport size, after a window resize */
void
ogl_resize( void )
{
	GtkAllocation allocation;

	gtk_widget_get_allocation(viewport_gl_area_w, &allocation);
	glViewport(0, 0, allocation.width, allocation.height);
}


/* Refreshes viewport after a window unhide, etc. */
void
ogl_refresh( void )
{
	redraw( );
}


/* Returns the viewport's current aspect ratio */
double
ogl_aspect_ratio( void )
{
	GLint viewport[4];

	glGetIntegerv( GL_VIEWPORT, viewport );

	/* aspect_ratio = width / height */
	return (double)viewport[2] / (double)viewport[3];
}


/* Sets up the projection matrix. full_reset should be TRUE unless the
 * current matrix is to be multiplied in */
static void
setup_projection_matrix( boolean full_reset )
{
	double dx, dy;

	dx = camera->near_clip * tan( 0.5 * RAD(camera->fov) );
	dy = dx / ogl_aspect_ratio( );
	glMatrixMode( GL_PROJECTION );
	if (full_reset)
		glLoadIdentity( );
	glFrustum( - dx, dx, - dy, dy, camera->near_clip, camera->far_clip );

	// Modern OpenGL using cglm
	mat4 frustum;
	glm_frustum(-dx, dx, -dy, dy, camera->near_clip, camera->far_clip, frustum);
	if (full_reset)
		glm_mat4_identity(gl.projection);
	glm_mat4_mul(gl.projection, frustum, gl.projection);
}


/* Sets up the modelview matrix */
static void
setup_modelview_matrix( void )
{
	glMatrixMode( GL_MODELVIEW );
	/* Remember, base matrix lives just below top of stack */
	glPopMatrix( );
	glPushMatrix( );
	glm_mat4_copy(gl.base_modelview, gl.modelview);

	switch (globals.fsv_mode) {
		case FSV_SPLASH:
		break;

		case FSV_DISCV:
		glTranslated( - camera->distance, 0.0, 0.0 );
		glRotated( 90.0, 0.0, 1.0, 0.0 );
		glRotated( 90.0, 0.0, 0.0, 1.0 );
		glTranslated( - DISCV_CAMERA(camera)->target.x, - DISCV_CAMERA(camera)->target.y, 0.0 );
		glm_translate(gl.modelview, (vec3){-camera->distance, 0.f, 0.f});
		glm_rotate_y(gl.modelview, M_PI_2, gl.modelview);
		glm_rotate_z(gl.modelview, M_PI_2, gl.modelview);
		glm_translate(gl.modelview, (vec3){-DISCV_CAMERA(camera)->target.x,
						   -DISCV_CAMERA(camera)->target.y,
						   0.f});
		break;

		case FSV_MAPV:
		glTranslated( - camera->distance, 0.0, 0.0 );
		glRotated( camera->phi, 0.0, 1.0, 0.0 );
		glRotated( - camera->theta, 0.0, 0.0, 1.0 );
		glTranslated( - MAPV_CAMERA(camera)->target.x, - MAPV_CAMERA(camera)->target.y, - MAPV_CAMERA(camera)->target.z );
		glm_translate(gl.modelview, (vec3){-camera->distance, 0.f, 0.f});
		glm_rotate_y(gl.modelview, camera->phi * M_PI / 180, gl.modelview);
		glm_rotate_z(gl.modelview, -camera->theta * M_PI / 180, gl.modelview);
		glm_translate(gl.modelview, (vec3){-MAPV_CAMERA(camera)->target.x,
						   -MAPV_CAMERA(camera)->target.y,
						   -MAPV_CAMERA(camera)->target.z});
		break;

		case FSV_TREEV:
		glTranslated( - camera->distance, 0.0, 0.0 );
		glRotated( camera->phi, 0.0, 1.0, 0.0 );
		glRotated( - camera->theta, 0.0, 0.0, 1.0 );
		glTranslated( TREEV_CAMERA(camera)->target.r, 0.0, - TREEV_CAMERA(camera)->target.z );
		glRotated( 180.0 - TREEV_CAMERA(camera)->target.theta, 0.0, 0.0, 1.0 );
		glm_translate(gl.modelview, (vec3){-camera->distance, 0.f, 0.f});
		glm_rotate_y(gl.modelview, camera->phi * M_PI / 180, gl.modelview);
		glm_rotate_z(gl.modelview, -camera->theta * M_PI / 180, gl.modelview);
		glm_translate(gl.modelview, (vec3){TREEV_CAMERA(camera)->target.r,
						   0.0f,
						   -TREEV_CAMERA(camera)->target.z});
		glm_rotate_z(gl.modelview,
			     (180.0 - TREEV_CAMERA(camera)->target.theta) * M_PI / 180,
			     gl.modelview);
		break;

		SWITCH_FAIL
	}
}


// Upload modified projection and modelview matrices to the GPU
void
ogl_upload_matrices(gboolean text)
{
	// As we're not doing any shading yet, just create the single MVP
	// matrix instead of uploading separate projection and modelview
	// matrices.
	mat4 mvp;
	glm_mat4_mul(gl.projection, gl.modelview, mvp);

	mat3 normmat;
	glm_mat4_pick3(gl.modelview, normmat);
	glm_mat3_inv(normmat, normmat);
	glm_mat3_transpose(normmat);

	/* load our program */
	glUseProgram(gl.program);

	glUniformMatrix4fv(gl.modelview_location, 1, GL_FALSE, (float*) gl.modelview);
	glUniformMatrix3fv(gl.normal_matrix_location, 1, GL_FALSE, (float*) normmat);
	/* update the "mvp" matrix we use in the shader */
	glUniformMatrix4fv(gl.mvp_location, 1, GL_FALSE, (float*)mvp);

	glUseProgram(0);

	if (text)
		text_upload_mvp((float*) mvp);
}


// Note: gl Program must be in use before calling this.
void
ogl_enable_lightning()
{
	glUniform1i(gl.lightning_enabled_location, 1);
}

// Note: gl Program must be in use before calling this.
void
ogl_disable_lightning()
{
	glUniform1i(gl.lightning_enabled_location, 0);
}


/* (Re)draws the viewport
 * NOTE: Don't call this directly! Use redraw( ) */
void
ogl_draw( void )
{
	static FsvMode prev_mode = FSV_NONE;
	int err;

	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	setup_projection_matrix( TRUE );
	setup_modelview_matrix( );
	ogl_upload_matrices(FALSE);
	geometry_draw( TRUE );

	/* Error check */
	err = glGetError( );
	if (err != 0)
		g_warning( "GL error: 0x%X", err );

	/* First frame after a mode switch is not drawn
	 * (with the exception of splash screen mode) */
	if (globals.fsv_mode != prev_mode) {
		prev_mode = globals.fsv_mode;
                if (globals.fsv_mode != FSV_SPLASH)
			return;
	}

	gtk_gl_area_swapbuffers( GTK_GL_AREA(viewport_gl_area_w) );
}


// Node selection with modern GL. Use the slow but simple trick described in
// http://www.opengl-tutorial.org/miscellaneous/clicking-on-objects/picking-with-an-opengl-hack/
GLuint
ogl_select_modern(GLint x, GLint y)
{
	gl.render_mode = RENDERMODE_SELECT;
	setup_projection_matrix(TRUE);
	setup_modelview_matrix();
	ogl_upload_matrices(FALSE);
	// Enable depth test
	//glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	//glDepthFunc(GL_LESS);

	// Cull triangles which normal is not towards the camera
	//glEnable(GL_CULL_FACE);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	geometry_draw(FALSE);

	// Wait until all the pending drawing commands are really done.
	// Ultra-mega-over slow !
	// There are usually a long time between glDrawElements() and
	// all the fragments completely rasterized.
	glFlush();
	glFinish();

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// Read the pixel at the specified coordinates. Have to massage y
	// coordinate since GTk and OpenGL use different coord systems.
	// Ultra-mega-over slow too, even for 1 pixel,
	// because the framebuffer is on the GPU.
	GLubyte color[4];
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	GLint yy = viewport[3] - y;
	glReadPixels(x, yy, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &color);
	//g_print("ogl_select_modern: Color red %u green %u blue %u alpha %u\n", color[0], color[1], color[2], color[3]);
	GLuint node_id = color[0] + (color[1] << 8) + (color[2] << 16);

	// Debugging, view pick rendering.
	//gtk_gl_area_swapbuffers( GTK_GL_AREA(viewport_gl_area_w) );

	/* Leave matrices in a usable state */
	setup_projection_matrix(TRUE);
	setup_modelview_matrix();
	ogl_upload_matrices(FALSE);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	gl.render_mode = RENDERMODE_RENDER;
	return node_id;
}


/* Helper callback for ogl_area_new( ) */
static void
realize_cb( GtkWidget *gl_area_w )
{
	gtk_gl_area_make_current( GTK_GL_AREA(gl_area_w) );
	/* Check for OpenGL 3.1 support */
	if (epoxy_gl_version() < 31)
		g_warning(_(PACKAGE " assumes OpenGL 3.1 / GLSL 1.40 support "
				    "which was not found. Continuing anyway."));
	ogl_init( );
}


/* Creates the viewport GL widget */
GtkWidget *
ogl_widget_new( void )
{
	int gl_area_attributes[] = {
		GDK_GL_RGBA,
		GDK_GL_RED_SIZE, 1,
		GDK_GL_GREEN_SIZE, 1,
		GDK_GL_BLUE_SIZE, 1,
		GDK_GL_DEPTH_SIZE, 1,
		GDK_GL_DOUBLEBUFFER,
		GDK_GL_NONE
	};

	/* Create the widget */
	viewport_gl_area_w = gtk_gl_area_new( gl_area_attributes );

	/* Initialize widget's GL state when realized */
	g_signal_connect(G_OBJECT(viewport_gl_area_w), "realize", G_CALLBACK(realize_cb), NULL);

	return viewport_gl_area_w;
}


/* end ogl.c */
