/* filelist.h */

/* File list control */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#ifdef FSV_FILELIST_H
	#error
#endif
#define FSV_FILELIST_H


#ifdef __GTK_H__
void filelist_pass_widget( GtkWidget *clist_w );
GtkWidget *dir_contents_list( GNode *dnode );
#endif
void filelist_reset_access( void );
void filelist_populate( GNode *dnode );
void filelist_show_entry( GNode *node );
void filelist_init( void );
void filelist_scan_monitor_init( void );
void filelist_scan_monitor( int *node_counts, int64 *size_counts );


/* end filelist.h */
