/* viewport.h */

/* Viewport routines */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#ifdef FSV_VIEWPORT_H
	#error
#endif
#define FSV_VIEWPORT_H


void viewport_pass_node_table( GNode **new_node_table );
#ifdef __GTK_H__
int viewport_cb( GtkWidget *gl_area_w, GdkEvent *event );
#endif


/* end viewport.h */
