/* scanfs.c */

/* Filesystem scanner */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * Copyright (C) 2021 Janne Blomqvist <blomqvist.janne@gmail.com>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#include "common.h"
#include "scanfs.h"

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <errno.h>

#include "dirtree.h"
#include "filelist.h"
#include "geometry.h" /* geometry_free( ) */
#include "gui.h" /* gui_update( ) */
#include "viewport.h" /* viewport_pass_node_table( ) */
#include "window.h"


#ifndef HAVE_SCANDIR
int scandir( const char *dir, struct dirent ***namelist, int (*selector)( const struct dirent * ), int (*cmp)( const void *, const void * ) );
int alphasort( const void *a, const void *b );
#endif


/* On-the-fly progress display is updated at intervals this far apart
 * (integer value in milliseconds) */
#define SCAN_MONITOR_PERIOD 500


/* Name strings are stored here */
static GStringChunk *name_strchunk = NULL;

/* Node ID counter */
static unsigned int node_id;

/* Numbers for the on-the-fly progress readout */
static int node_counts[NUM_NODE_TYPES];
static int64 size_counts[NUM_NODE_TYPES];
static int stat_count = 0;


/* Official stat function. Returns 0 on success, -1 on error */
static int
stat_node( GNode *node )
{
	struct stat st;

	if (lstat( node_absname( node ), &st ))
		return -1;

	/* Determine node type */
	if (S_ISDIR(st.st_mode))
		NODE_DESC(node)->type = NODE_DIRECTORY;
	else if (S_ISREG(st.st_mode))
		NODE_DESC(node)->type = NODE_REGFILE;
	else if (S_ISLNK(st.st_mode))
		NODE_DESC(node)->type = NODE_SYMLINK;
	else if (S_ISFIFO(st.st_mode))
		NODE_DESC(node)->type = NODE_FIFO;
	else if (S_ISSOCK(st.st_mode))
		NODE_DESC(node)->type = NODE_SOCKET;
	else if (S_ISCHR(st.st_mode))
		NODE_DESC(node)->type = NODE_CHARDEV;
	else if (S_ISBLK(st.st_mode))
		NODE_DESC(node)->type = NODE_BLOCKDEV;
	else
		NODE_DESC(node)->type = NODE_UNKNOWN;

	/* A corrupted DOS filesystem once gave me st_size = -4GB */
	g_assert( st.st_size >= 0 );

	NODE_DESC(node)->size = st.st_size;
	NODE_DESC(node)->size_alloc = 512 * st.st_blocks;
	NODE_DESC(node)->user_id = st.st_uid;
	NODE_DESC(node)->group_id = st.st_gid;
	/*NODE_DESC(node)->perms = st.st_mode;*/
	NODE_DESC(node)->atime = st.st_atime;
	NODE_DESC(node)->mtime = st.st_mtime;
	NODE_DESC(node)->ctime = st.st_ctime;

	return 0;
}


/* Selector function for use with scandir( ). This lets through all
 * directory entries except for "." and ".." */
static int
de_select( const struct dirent *de )
{
	if (de->d_name[0] != '.')
		return 1; /* Allow "whatever" */
	if (de->d_name[1] == '\0')
		return 0; /* Disallow "." */
	if (de->d_name[1] != '.')
		return 1; /* Allow ".whatever" */
	if (de->d_name[2] == '\0')
		return 0; /* Disallow ".." */

	/* Allow "..whatever", "...whatever", etc. */
	return 1;
}


static int
process_dir( const char *dir, GNode *dnode )
{
	union AnyNodeDesc any_node_desc, *andesc;
	struct dirent **dir_entries;
	GNode *node;
	int num_entries, i;
	char strbuf[1024];

	/* Scan in directory entries */
	num_entries = scandir( dir, &dir_entries, de_select, alphasort );
	if (num_entries < 0)
		return -1;

	/* Update display */
	snprintf( strbuf, sizeof(strbuf), _("Scanning: %s"), dir );
	window_statusbar( SB_RIGHT, strbuf );

	/* Process directory entries */
	for (i = 0; i < num_entries; i++) {
		/* Create new node */
		node = g_node_prepend_data( dnode, &any_node_desc );
		NODE_DESC(node)->id = node_id;
		NODE_DESC(node)->name = g_string_chunk_insert( name_strchunk, dir_entries[i]->d_name );
		if (stat_node( node )) {
			/* Stat failed */
			g_node_unlink( node );
			g_node_destroy( node );
			continue;
		}
		++stat_count;
		++node_id;

		if (NODE_IS_DIR(node)) {
			/* Create corresponding directory tree entry */
			dirtree_entry_new( node );

			/* Recurse down */
			process_dir( node_absname( node ), node );

			/* Move new descriptor into working memory */
			andesc = (union AnyNodeDesc *) g_slice_new(DirNodeDesc);
			memcpy( andesc, DIR_NODE_DESC(node), sizeof(DirNodeDesc) );
			node->data = andesc;
		}
		else {
			/* Move new descriptor into working memory */
			andesc = (union AnyNodeDesc *) g_slice_new(NodeDesc);
			memcpy( andesc, NODE_DESC(node), sizeof(NodeDesc) );
			node->data = andesc;
		}

		/* Add to appropriate node/size counts
		 * (for dynamic progress display) */
		++node_counts[NODE_DESC(node)->type];
		size_counts[NODE_DESC(node)->type] += NODE_DESC(node)->size;

		free( dir_entries[i] ); /* !xfree */

		/* Keep the user interface responsive */
		gui_update( );
	}

	free( dir_entries ); /* !xfree */

	return 0;
}


/* Dynamic scan progress readout */
static boolean
scan_monitor(gpointer user_data)
{
	char strbuf[64];

	/* Running totals in file list area */
	filelist_scan_monitor( node_counts, size_counts );

	/* Stats-per-second readout in left statusbar */
	sprintf( strbuf, _("%d stats/sec"), 1000 * stat_count / SCAN_MONITOR_PERIOD );
	window_statusbar( SB_LEFT, strbuf );
	gui_update( );
	stat_count = 0;

	return TRUE;
}


/* Compare function for sorting nodes
 * (directories first, then larger to smaller, then alphabetically A-Z)
 * Note: Directories must *always* go before leafs-- this speeds up
 * recursion, as it allows iteration to stop at the first leaf */
static int
compare_node( NodeDesc *a, NodeDesc *b )
{
	int64 a_size, b_size;
	int s = 0;

	a_size = a->size;
	if (a->type == NODE_DIRECTORY) {
		a_size += ((DirNodeDesc *)a)->subtree.size;
		s -= 2;
	}

	b_size = b->size;
	if (b->type == NODE_DIRECTORY) {
		b_size += ((DirNodeDesc *)b)->subtree.size;
		s += 2;
	}

	if (a_size > b_size)
		--s;
	if (a_size < b_size)
		++s;
	if (!s)
		return strcmp( a->name, b->name );

	return s;
}


/* This does major post-scan housekeeping on the filesystem tree. It
 * sorts everything, assigns subtree size/count information to directory
 * nodes, sets up the node table, etc. */
static void
setup_fstree_recursive( GNode *node, GNode **node_table )
{
	GNode *child_node;
	int i;

	/* Assign entry in the node table */
	node_table[NODE_DESC(node)->id] = node;

	if (NODE_IS_DIR(node) || NODE_IS_METANODE(node)) {
		/* Initialize subtree quantities */
		DIR_NODE_DESC(node)->subtree.size = 0;
		for (i = 0; i < NUM_NODE_TYPES; i++)
			DIR_NODE_DESC(node)->subtree.counts[i] = 0;

		/* Recurse down */
		child_node = node->children;
		while (child_node != NULL) {
			setup_fstree_recursive( child_node, node_table );
			child_node = child_node->next;
		}
	}

	if (!NODE_IS_METANODE(node)) {
		/* Increment subtree quantities of parent */
		DIR_NODE_DESC(node->parent)->subtree.size += NODE_DESC(node)->size;
		++DIR_NODE_DESC(node->parent)->subtree.counts[NODE_DESC(node)->type];
	}

	if (NODE_IS_DIR(node)) {
		/* Sort directory contents */
		node->children = (GNode *)g_list_sort( (GList *)node->children, (GCompareFunc)compare_node );
		/* Propagate subtree size/counts upward */
		DIR_NODE_DESC(node->parent)->subtree.size += DIR_NODE_DESC(node)->subtree.size;
		for (i = 0; i < NUM_NODE_TYPES; i++)
			DIR_NODE_DESC(node->parent)->subtree.counts[i] += DIR_NODE_DESC(node)->subtree.counts[i];
	}
}


// Free data in a dir or file node. Can be used as a GNodeTraverseFunc
static gboolean
node_data_free(GNode *node, gpointer data)
{
	if (NODE_IS_DIR(node) || NODE_IS_METANODE(node)) {
		DirNodeDesc *p = DIR_NODE_DESC(node);
		g_slice_free(DirNodeDesc, p);
	} else {
		g_slice_free(NodeDesc, NODE_DESC(node));
	}
	return FALSE;
}

/* Top-level call to recursively scan a filesystem */
void
scanfs( const char *dir )
{
	const char *root_dir;
        GNode **node_table;
	guint handler_id;
	char *name;

	if (globals.fstree != NULL) {
		/* Free existing geometry and filesystem tree */
		geometry_free_recursive( globals.fstree );
		g_node_traverse(globals.fstree, G_IN_ORDER, G_TRAVERSE_ALL,
				-1, node_data_free, NULL);
		g_node_destroy( globals.fstree );
	}

	/* Setup string chunks to hold name strings */
	if (name_strchunk != NULL)
		g_string_chunk_free( name_strchunk );
	name_strchunk = g_string_chunk_new( 8192 );

	/* Clear out directory tree */
	dirtree_clear( );

	/* Reset node numbering */
	node_id = 0;

	/* Get absolute path of desired root (top-level) directory */
	if (chdir(dir) != 0) {
		g_error("Failed to change dir to %s, error msg: %s\n", dir, g_strerror(errno));
		return;
	}
	root_dir = xgetcwd( );

	/* Set up fstree metanode */
	globals.fstree = g_node_new(g_slice_new(DirNodeDesc));
	NODE_DESC(globals.fstree)->type = NODE_METANODE;
	NODE_DESC(globals.fstree)->id = node_id++;
	name = g_path_get_dirname( root_dir );
	NODE_DESC(globals.fstree)->name = g_string_chunk_insert( name_strchunk, name );
	g_free( name );
	DIR_NODE_DESC(globals.fstree)->tnode = NULL; /* needed in dirtree_entry_new( ) */

	/* Set up root directory node */
	g_node_append_data(globals.fstree, g_slice_new(DirNodeDesc));
	/* Note: We can now use root_dnode to refer to the node just
	 * created (it is an alias for globals.fstree->children) */
	NODE_DESC(root_dnode)->id = node_id++;
	name = g_path_get_basename( root_dir );
	NODE_DESC(root_dnode)->name = g_string_chunk_insert( name_strchunk, name );
	g_free(name);
	// TODO: Invalidate VBO's, need to upload new ones.
	stat_node( root_dnode );
	dirtree_entry_new( root_dnode );

	/* GUI stuff */
	filelist_scan_monitor_init( );
	handler_id = g_timeout_add( SCAN_MONITOR_PERIOD, (GSourceFunc)scan_monitor, NULL);

	/* Let the disk thrashing begin */
	process_dir( root_dir, root_dnode );

	/* GUI stuff again */
	g_source_remove( handler_id );
	window_statusbar( SB_RIGHT, "" );
	dirtree_no_more_entries( );
	gui_update( );

	/* Allocate node table and perform final tree setup */
	node_table = NEW_ARRAY(GNode *, node_id);
	setup_fstree_recursive( globals.fstree, node_table );

	/* Pass off new node table to the viewport handler */
	viewport_pass_node_table(node_table, node_id);
}


/* end scanfs.c */
