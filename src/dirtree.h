/* dirtree.h */

/* Directory tree control */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * Copyright (C) 2021 Janne Blomqvist <blomqvist.janne@gmail.com>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#ifdef FSV_DIRTREE_H
	#error
#endif
#define FSV_DIRTREE_H


#ifdef __GTK_H__
void dirtree_pass_widget( GtkWidget *tree_w );
#endif
void dirtree_clear( void );
void dirtree_entry_new( GNode *dnode );
void dirtree_no_more_entries( void );
void dirtree_entry_show( GNode *dnode );
boolean dirtree_entry_expanded( GNode *dnode );
void dirtree_entry_collapse_recursive( GNode *dnode );
void dirtree_entry_expand( GNode *dnode );
void dirtree_entry_expand_recursive( GNode *dnode );


/* end dirtree.h */
