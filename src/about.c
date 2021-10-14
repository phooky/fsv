/* about.c */

/* Help -> About... */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * Copyright (C) 2021 Janne Blomqvist <blomqvist.janne@gmail.com>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#include "common.h"
#include "about.h"

#include <epoxy/gl.h>

#include "animation.h"
#include "geometry.h"
#include "ogl.h"
#include "tmaptext.h"


/* Interval normalization macro */
#define INTERVAL_PART(x,x0,x1)	(((x) - (x0)) / ((x1) - (x0)))


/* Normalized time variable (in range [0, 1]) */
static double about_part;

/* TRUE while giving About presentation */
static boolean about_active = FALSE;


/* Draws the "fsv" 3D letters */
static void
draw_fsv( void )
{
	double dy, p, q;

	if (about_part < 0.5) {
		/* Set up a black, all-encompassing fog */
		glEnable( GL_FOG );
		glFogi( GL_FOG_MODE, GL_LINEAR );
		glFogf( GL_FOG_START, 200.0 );
		glFogf( GL_FOG_END, 1800.0 );
	}

	/* Set up projection matrix */
	glMatrixMode( GL_PROJECTION );
	glPushMatrix( );
	glLoadIdentity( );
	dy = 80.0 / ogl_aspect_ratio( );
	glFrustum( - 80.0, 80.0, - dy, dy, 80.0, 2000.0 );
	mat4 proj;
	glm_frustum(-80.0, 80.0, -dy, dy, 80.0, 2000.0, proj);

	/* Set up modelview matrix */
	glMatrixMode( GL_MODELVIEW );
	glPushMatrix( );
	glLoadIdentity( );
	mat4 mv;
	glm_mat4_identity(mv);
	if (about_part < 0.5) {
		/* Spinning and approaching fast */
		p = INTERVAL_PART(about_part, 0.0, 0.5);
		q = pow( 1.0 - p, 1.5 );
		glTranslated( 0.0, 0.0, -150.0 - 1800.0 * q );
		glm_translate(mv, (vec3){0.0, 0.0, -150.0 - 1800.0 * q});
		glRotated( 900.0 * q, 0.0, 1.0, 0.0 );
		glm_rotate_y(mv, 900.0 * q * M_PI/180., mv);
	}
	else if (about_part < 0.625) {
		/* Holding still for a moment */
		glTranslated( 0.0, 0.0, -150.0 );
		glm_translate(mv, (vec3){0.0, 0.0, -150.0});
	}
	else if (about_part < 0.75) {
		/* Flipping up and back */
		p = INTERVAL_PART(about_part, 0.625, 0.75);
		q = 1.0 - SQR(1.0 - p);
		glTranslated( 0.0, 40.0 * q, -150.0 - 50.0 * q );
		glm_translate(mv, (vec3){0.0, 40.0 * q, -150.0 - 50.0 * q});
		glRotated( 365.0 * q, 1.0, 0.0, 0.0 );
		glm_rotate_x(mv, 365.0 * q * M_PI/180., mv);
	}
	else {
		/* Holding still again */
		glTranslated( 0.0, 40.0, -200.0 );
		glm_translate(mv, (vec3){0.0, 40.0, -200.0});
		glRotated( 5.0, 1.0, 0.0, 0.0 );
		glm_rotate_x(mv, 5.0 * M_PI/180.0, mv);
	}

	mat4 mvp;
	glm_mat4_mul(proj, mv, mvp);
	glUseProgram(aboutGL.program);
	/* update the "mvp" matrix we use in the shader */
	glUniformMatrix4fv(aboutGL.mvp_location, 1, GL_FALSE, (float*)mvp);
	glUseProgram(0);

	/* Draw "fsv" geometry */
	geometry_gldraw_fsv( );

	/* Restore previous matrices */
	glMatrixMode( GL_PROJECTION );
	glPopMatrix( );
	glMatrixMode( GL_MODELVIEW );
	glPopMatrix( );

	glDisable( GL_FOG );
}


/* Draws the lines of text */
static void
draw_text( void )
{
        XYZvec tpos;
	XYvec tdims;
	double dy, p, q;

	if (about_part < 0.625)
		return;

	/* Set up projection matrix */
	dy = 1.0 / ogl_aspect_ratio( );
	mat4 proj;
	glm_frustum(-1.0, 1.0, -dy, dy, 1.0, 205.0, proj);

	/* Set up modelview matrix */
	// Modelview matrix is the identity, so mvp is just proj.
	text_upload_mvp((float*) proj);

        if (about_part < 0.75)
		p = INTERVAL_PART(about_part, 0.625, 0.75);
	else
		p = 1.0;
	q = (1.0 - SQR(1.0 - p));

	text_pre( );

	tdims.x = 400.0;
	tdims.y = 18.0;
	tpos.x = 0.0;
	tpos.y = -35.0; /* -35 */
	tpos.z = -200.0 * q;
	text_set_color(1.0, 1.0, 1.0);
	text_draw_straight( "fsv - 3D File System Visualizer", &tpos, &tdims );

	tdims.y = 15.0;
	tpos.y = 40.0 * q - 95.0; /* -55 */
	text_draw_straight( "Version " VERSION, &tpos, &tdims );

	tdims.y = 12.0;
	tpos.y = 100.0 * q - 180.0; /* -80 */
	text_set_color(0.5, 0.5, 0.5);
	text_draw_straight( "Copyright (C)1999 by Daniel Richard G.", &tpos, &tdims );

	tpos.y = 140.0 * q - 235.0; /* -95 */
	text_draw_straight( "Copyright (C) 2021 Janne Blomqvist", &tpos, &tdims );

	/* Finally, fade in the home page URL */
	if (about_part > 0.75) {
		tpos.y = -115.0;
		p = INTERVAL_PART(about_part, 0.75, 1.0);
		q = SQR(SQR(p));
		text_set_color(q, q, 0.0);
		text_draw_straight( "https://github.com/jabl/fsv/", &tpos, &tdims );
		//text_draw_straight( "__________________________________", &tpos, &tdims );
	}

	text_post( );
}


/* Progress callback; keeps viewport updated during presentation */
static void
about_progress_cb( Morph *unused )
{
	globals.need_redraw = TRUE;
}


/* Control routine */
boolean
about( AboutMesg mesg )
{
	switch (mesg) {
		case ABOUT_BEGIN:
		/* Begin the presentation */
		morph_break( &about_part );
		about_part = 0.0;
		morph_full( &about_part, MORPH_LINEAR, 1.0, 8.0, about_progress_cb, about_progress_cb, NULL );
		about_active = TRUE;
		break;

		case ABOUT_END:
		if (!about_active)
			return FALSE;
		/* We now return you to your regularly scheduled program */
		morph_break( &about_part );
		redraw( );
		about_active = FALSE;
		return TRUE;

		case ABOUT_DRAW:
		/* Draw all presentation elements */
		draw_fsv( );
		draw_text( );
		break;

		case ABOUT_CHECK:
		/* Return current presentation status */
		return about_active;

		SWITCH_FAIL
	}

	return FALSE;
}


/* end about.c */
