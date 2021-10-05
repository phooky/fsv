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
#include <epoxy/gl.h>
#include <GL/glu.h> /* gluPickMatrix( ) */

#include "animation.h" /* redraw( ) */
#include "camera.h"
#include "geometry.h"
#include "tmaptext.h" /* text_init( ) */


/* Main viewport OpenGL area widget */
static GtkWidget *viewport_gl_area_w = NULL;

static GLuint vao; // OpenGL Vertex Array Object Names


/* Initializes OpenGL state */
static void
ogl_init( void )
{
	float light_ambient[] = {0.1, 0.1, 0.1, 1};
	float light_diffuse[] = {1, 1, 1, 1};
	float light_specular[] = {1, 1, 1, 1};
	float light_position[] = { 1.0, 0.0, 0.0, 0.0 };
	float material_specular[] = {.1, .1, .1, 1};

	// Modern OpenGL initialization. Even if we don't use the VAO it must be
	// initialized or VBO stuff might fail.
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// Should be eventually switched to glEnableVertexAttribArray once
	// shaders are taken into use
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

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

	/* Set up lighting */
	glEnable( GL_LIGHTING );
	glEnable( GL_LIGHT0 );
	glLightfv( GL_LIGHT0, GL_AMBIENT, light_ambient );
	glLightfv( GL_LIGHT0, GL_DIFFUSE, light_diffuse );
	glLightfv( GL_LIGHT0, GL_SPECULAR, light_specular );
	glLightfv( GL_LIGHT0, GL_POSITION, light_position );

	/* Set up materials */
	glColorMaterial( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE );
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, material_specular);
	glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 1);
	glEnable( GL_COLOR_MATERIAL );

	/* Miscellaneous */
	glAlphaFunc( GL_GEQUAL, 0.0625 );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glEnable( GL_CULL_FACE );
	glShadeModel(GL_SMOOTH);
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glEnable( GL_POLYGON_OFFSET_FILL );
	glPolygonOffset( 1.0, 1.0 );
	glClearColor( 0.0, 0.0, 0.0, 0.0 );

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
}


/* Sets up the modelview matrix */
static void
setup_modelview_matrix( void )
{
	glMatrixMode( GL_MODELVIEW );
	/* Remember, base matrix lives just below top of stack */
	glPopMatrix( );
	glPushMatrix( );

	switch (globals.fsv_mode) {
		case FSV_SPLASH:
		break;

		case FSV_DISCV:
		glTranslated( - camera->distance, 0.0, 0.0 );
		glRotated( 90.0, 0.0, 1.0, 0.0 );
		glRotated( 90.0, 0.0, 0.0, 1.0 );
		glTranslated( - DISCV_CAMERA(camera)->target.x, - DISCV_CAMERA(camera)->target.y, 0.0 );
		break;

		case FSV_MAPV:
		glTranslated( - camera->distance, 0.0, 0.0 );
		glRotated( camera->phi, 0.0, 1.0, 0.0 );
		glRotated( - camera->theta, 0.0, 0.0, 1.0 );
		glTranslated( - MAPV_CAMERA(camera)->target.x, - MAPV_CAMERA(camera)->target.y, - MAPV_CAMERA(camera)->target.z );
		break;

		case FSV_TREEV:
		glTranslated( - camera->distance, 0.0, 0.0 );
		glRotated( camera->phi, 0.0, 1.0, 0.0 );
		glRotated( - camera->theta, 0.0, 0.0, 1.0 );
		glTranslated( TREEV_CAMERA(camera)->target.r, 0.0, - TREEV_CAMERA(camera)->target.z );
		glRotated( 180.0 - TREEV_CAMERA(camera)->target.theta, 0.0, 0.0, 1.0 );
		break;

		SWITCH_FAIL
	}
}


/* (Re)draws the viewport
 * NOTE: Don't call this directly! Use redraw( ) */
void
ogl_draw( void )
{
	static FsvMode prev_mode = FSV_NONE;
	int err;

	geometry_highlight_node( NULL, TRUE );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	setup_projection_matrix( TRUE );
	setup_modelview_matrix( );
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


/* This returns an array of names (unsigned ints) of the primitives which
 * occur under the given viewport coordinates (x,y) (where (0,0) indicates
 * the upper left corner). Return value is the number of names (hit records)
 * stored in the select buffer */
int
ogl_select( int x, int y, const GLuint **selectbuf_ptr )
{
	static GLuint selectbuf[1024];
	GLint viewport[4];
	int ogl_y, hit_count;

	glSelectBuffer( 1024, selectbuf );
	glRenderMode( GL_SELECT );

	/* Set up picking matrix */
	glGetIntegerv( GL_VIEWPORT, viewport );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity( );
	ogl_y = viewport[3] - y;
	gluPickMatrix( (double)x, (double)ogl_y, 1.0, 1.0, viewport );

	/* Draw geometry */
	setup_projection_matrix( FALSE );
	setup_modelview_matrix( );
	geometry_draw( FALSE );

	/* Get the hits */
	hit_count = glRenderMode( GL_RENDER );
	*selectbuf_ptr = selectbuf;

	/* Leave matrices in a usable state */
	setup_projection_matrix( TRUE );
	setup_modelview_matrix( );

	return hit_count;
}


/* Helper callback for ogl_area_new( ) */
static void
realize_cb( GtkWidget *gl_area_w )
{
	gtk_gl_area_make_current( GTK_GL_AREA(gl_area_w) );
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
