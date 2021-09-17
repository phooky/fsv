/* filelist.c */

/* File list control */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * Copyright (C) 2021 Janne Blomqvist <blomqvist.janne@gmail.com>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#include "common.h"
#include "filelist.h"

#include <gtk/gtk.h>

#include "about.h"
#include "camera.h"
#include "dialog.h"
#include "dirtree.h"
#include "geometry.h"
#include "gui.h"
#include "window.h"


/* Time for the filelist to scroll to a given entry (in seconds) */
#define FILELIST_SCROLL_TIME 0.5


/* The file list widget */
static GtkWidget *file_list_w;

/* Directory currently listed */
static GNode *filelist_current_dnode;

/* Mini node type icons */
static Icon node_type_mini_icons[NUM_NODE_TYPES];


/* Loads the mini node type icons (from XPM data) */
static void
filelist_icons_init( void )
{
	GtkStyle *style;
	GdkColor *trans_color;
	GdkWindow *window;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	int i;

	style = gtk_widget_get_style( file_list_w );
	trans_color = &style->bg[GTK_STATE_NORMAL];
	gtk_widget_realize( file_list_w );
	window = file_list_w->window;

	/* Make mini node type icons */
	for (i = 1; i < NUM_NODE_TYPES; i++) {
		pixmap = gdk_pixmap_create_from_xpm_d( window, &mask, trans_color, node_type_mini_xpms[i] );
		node_type_mini_icons[i].pixmap = pixmap;
		node_type_mini_icons[i].mask = mask;
		GdkPixbuf *pixbuf = gdk_pixbuf_new_from_xpm_data(node_type_mini_xpms[i]);
		node_type_mini_icons[i].pixbuf = pixbuf;
	}
}


/* Correspondence from window_init( ) */
void
filelist_pass_widget( GtkWidget *list_w )
{
	file_list_w = list_w;
	filelist_icons_init( );
}


/* This makes entries in the file list selectable or unselectable,
 * depending on whether the directory they are in is expanded or not */
void
filelist_reset_access( void )
{
	boolean enabled;

        enabled = dirtree_entry_expanded( filelist_current_dnode );
	gtk_widget_set_sensitive( file_list_w, enabled );

	/* Extra fluff for interface niceness */
	if (enabled)
		gui_cursor( file_list_w, -1 );
	else {
		GtkTreeSelection *select
			= gtk_tree_view_get_selection(GTK_TREE_VIEW(file_list_w));
		gtk_tree_selection_unselect_all(select);
		gui_cursor( file_list_w, GDK_X_CURSOR );
	}
}


/* Compare function for sorting nodes alphabetically */
static int
compare_node( GNode *a, GNode *b )
{
	return strcmp( NODE_DESC(a)->name, NODE_DESC(b)->name );
}


/* Displays contents of a directory in the file list */
void
filelist_populate( GNode *dnode )
{
	GNode *node;
	GList *node_list = NULL, *node_llink;
	Icon *icon;
	int count = 0;
	char strbuf[64];

	g_assert( NODE_IS_DIR(dnode) );

        /* Get an alphabetized list of directory's immediate children */
	node = dnode->children;
	while (node != NULL) {
		G_LIST_PREPEND(node_list, node);
		node = node->next;
	}
	G_LIST_SORT(node_list, compare_node);

	/* Update file list */
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_list_w));
	g_object_ref(model);
	/* Detach model from view, similar to clist_freeze. */
	gtk_tree_view_set_model(GTK_TREE_VIEW(file_list_w), NULL);
	GtkListStore *store = GTK_LIST_STORE(model);
	gtk_list_store_clear(store);
	node_llink = node_list;
	while (node_llink != NULL) {
		node = (GNode *)node_llink->data;
		icon = &node_type_mini_icons[NODE_DESC(node)->type];
		GtkTreeIter it;
		gtk_list_store_append(store, &it);

		gtk_list_store_set(store, &it,
				   FILELIST_ICON_COLUMN, icon->pixbuf,
				   FILELIST_NAME_COLUMN, NODE_DESC(node)->name,
				   FILELIST_NODE_COLUMN, node,
				   -1);

		++count;
		node_llink = node_llink->next;
	}
	gtk_tree_view_set_model(GTK_TREE_VIEW(file_list_w), model); /* Re-attach model to view */
	g_object_unref(model);

	g_list_free( node_list );

	/* Set node count message in the left statusbar */
	switch (count) {
		case 0:
		strcpy( strbuf, "" );
		break;

		case 1:
		strcpy( strbuf, _("1 node") );
		break;

		default:
		sprintf( strbuf, _("%d nodes"), count );
		break;
	}
	window_statusbar( SB_LEFT, strbuf );

	filelist_current_dnode = dnode;
	filelist_reset_access( );
}


/* This updates the file list to show (and select) a particular node
 * entry. The directory tree is also updated appropriately */
void
filelist_show_entry( GNode *node )
{
	GNode *dnode;
	int row = 0;

	/* Corresponding directory */
	if (NODE_IS_DIR(node))
		dnode = node;
	else
		dnode = node->parent;

	if (dnode != filelist_current_dnode) {
		/* Scroll directory tree to proper entry */
		dirtree_entry_show( dnode );
	}

	// Get the first iter in the list, check it is valid and walk
	// through the list, reading each row.
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_list_w));
	GtkTreeIter iter;
	gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
	while (valid)
	{
		gchar *fname;

		// Make sure you terminate calls to `gtk_tree_model_get()` with a “-1” value
		gtk_tree_model_get(model, &iter,
				   FILELIST_NAME_COLUMN, &fname,
				   -1);

		// Do something with the data
		if (strcmp(fname, NODE_DESC(node)->name) == 0)
		{
			g_free(fname);
			break;
		}
		g_free(fname);

		valid = gtk_tree_model_iter_next(model, &iter);
		row++;
	}
	GtkTreeSelection *select = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_list_w));
	if (valid) {
		gtk_tree_selection_select_iter(select, &iter);
		/* Scroll file list to proper entry */
		GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(file_list_w), path,
			NULL, FALSE, 0, 0);
		gtk_tree_path_free(path);
	}
	else
		gtk_tree_selection_unselect_all(select);
}


/* Callback for a click in the file list area */
static void
filelist_select_cb(GtkTreeSelection *selection, gpointer data)
{
	GNode *dnode = NULL;
	GtkTreeIter iter;
	GtkTreeModel *model;

	/* If About presentation is up, end it */
	about( ABOUT_END );

	if (globals.fsv_mode == FSV_SPLASH)
		return;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, FILELIST_NODE_COLUMN, &dnode, -1);
		if (!dnode)
			return;
		camera_look_at(dnode);
		//g_signal_stop_emission_by_name(G_OBJECT(selection), "changed" );
		geometry_highlight_node(dnode, FALSE);
		window_statusbar(SB_RIGHT, node_absname(dnode));
	}

	// gtk_clist_get_selection_info(GTK_CLIST(list_w), ev_button->x, ev_button->y, &row, NULL);
	// if (row < 0)
	//     return FALSE;

	// node = (GNode *)gtk_clist_get_row_data(GTK_CLIST(list_w), row);
	// if (node == NULL)
	//     return FALSE;

	// /* A single-click from button 1 highlights the node and shows the
	//  * name (and also selects the row, but GTK+ does that for us) */
	// if ((ev_button->button == 1) && (ev_button->type == GDK_BUTTON_PRESS))
	// {
	//     geometry_highlight_node(node, FALSE);
	//     window_statusbar(SB_RIGHT, node_absname(node));
	//     return FALSE;
	// }

	// /* A double-click from button 1 gets the camera moving */
	// if ((ev_button->button == 1) && (ev_button->type == GDK_2BUTTON_PRESS))
	// {
	//     camera_look_at(node);
	//     return FALSE;
	// }

	// /* A click from button 3 selects the row, highlights the node,
	//  * shows the name, and pops up a context-sensitive menu */
	// if (ev_button->button == 3)
	// {
	//     gtk_clist_select_row(GTK_CLIST(list_w), row, 0);
	//     geometry_highlight_node(node, FALSE);
	//     window_statusbar(SB_RIGHT, node_absname(node));
	//     context_menu(node, ev_button);
	//     return FALSE;
	// }

	// return FALSE;
}

/* Creates/initializes the file list widget */
void
filelist_init( void )
{
	GtkWidget *parent_w;

	/* Replace current clist widget with a single-column one */
	parent_w = file_list_w->parent->parent;
	gtk_widget_destroy( file_list_w->parent );
	file_list_w = gui_filelist_new(parent_w);

	GtkTreeSelection *select = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_list_w));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);

	g_signal_connect( G_OBJECT(select), "changed", G_CALLBACK(filelist_select_cb), NULL );

	filelist_populate( root_dnode );

	/* Do this so that directory tree gets scrolled to the to at
	 * end of initial camera pan (right after filesystem scan) */
	filelist_current_dnode = NULL;
}


/* This replaces the file list widget with another one made specifically
 * to monitor the progress of an impending scan */
void
filelist_scan_monitor_init( void )
{
	GtkWidget *parent_w;
	Icon *icon;
	int i;

	/* Replace current clist widget with a 3-column one */
	parent_w = file_list_w->parent->parent;
	gtk_widget_destroy( file_list_w->parent );
	file_list_w = gui_filelist_scan_new( parent_w );

	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_list_w));
	GtkListStore *store = GTK_LIST_STORE(model);

	/* Place icons and static text */
	for (i = 1; i <= NUM_NODE_TYPES; i++) {
		GtkTreeIter it;
		gtk_list_store_append(store, &it);
		if (i < NUM_NODE_TYPES) {
			icon = &node_type_mini_icons[i];
			gtk_list_store_set(store, &it,
				FILELIST_SCAN_ICON_COLUMN, icon->pixbuf,
				FILELIST_SCAN_FOUND_COLUMN, 0,
				FILELIST_SCAN_BYTES_COLUMN, 0,
				-1);
		}
		else
			gtk_list_store_set(store, &it,
				// TODO: fixme need "total" icon?!!
				// FILELIST_SCAN_ICON_COLUMN, icon->pixbuf,
				FILELIST_SCAN_FOUND_COLUMN, 0,
				FILELIST_SCAN_BYTES_COLUMN, 0,
				-1);
    }
}


/* Updates the scan-monitoring file list with the given values */
void
filelist_scan_monitor( int *node_counts, int64 *size_counts )
{
	int64 size_total = 0;
	int node_total = 0;
	int row = 1;

	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_list_w));
	GtkListStore *store = GTK_LIST_STORE(model);
	GtkTreeIter iter;

	gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
	while (valid)
	{
		int f, b;
		if (row < NUM_NODE_TYPES)
		{
			f = node_counts[row];
			b = size_counts[row];
			node_total += f;
			size_total += b;
		}
		else
		{
			f = node_total;
			b = size_total;
		}
		gtk_list_store_set(store, &iter,
				   FILELIST_SCAN_FOUND_COLUMN, f,
				   FILELIST_SCAN_BYTES_COLUMN, b,
				   -1);

		valid = gtk_tree_model_iter_next(model, &iter);
		row++;
	}
}


/* Creates the clist widget used in the "Contents" page of the Properties
 * dialog for a directory */
GtkWidget *
dir_contents_list( GNode *dnode )
{
        char *col_titles[2];
	char *clist_row[2];
	GtkWidget *list_w;
	Icon *icon;
	int i;

	g_assert( NODE_IS_DIR(dnode) );

	col_titles[0] = _("Node type");
	col_titles[1] = _("Quantity");

	/* Don't use gui_clist_add( ) as this one shouldn't be placed
	 * inside a scrolled window */
        list_w = gtk_clist_new_with_titles( 2, col_titles );
	gtk_clist_set_selection_mode( GTK_CLIST(list_w), GTK_SELECTION_SINGLE );
	for (i = 0; i < 2; i++)
		gtk_clist_set_column_auto_resize( GTK_CLIST(list_w), i, TRUE );

	clist_row[0] = NULL;
	for (i = 1; i < NUM_NODE_TYPES; i++) {
		clist_row[1] = (char *)i64toa( DIR_NODE_DESC(dnode)->subtree.counts[i] );
		gtk_clist_append( GTK_CLIST(list_w), clist_row );
		icon = &node_type_mini_icons[i];
		gtk_clist_set_pixtext( GTK_CLIST(list_w), i - 1, 0, _(node_type_plural_names[i]), 2, icon->pixmap, icon->mask );
	}

	return list_w;
}


/* end filelist.c */
