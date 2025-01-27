/* geometry.c */

/* 3D geometry generation and rendering */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * SPDX-FileCopyrightText: 2021 Janne Blomqvist <blomqvist.janne@gmail.com>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#include "common.h"
#include "geometry.h"

#include <cglm/call.h>

#include "about.h"
#include "animation.h"
#include "camera.h"
#include "color.h"
#include "dirtree.h" /* dirtree_entry_expanded( ) */
#include "ogl.h"
#include "tmaptext.h"

/* 3D geometry for splash screen */
#include "fsv3d.h"


/* Cursor position remapping from linear to quarter-sine
 * (input and output are both [0, 1]) */
#define CURSOR_POS(x)			sin( (0.5 * PI) * (x) )


// TODO: Implement this caching strategy with VBO's
/* Current "drawing stage" for low- and high-detail geometry:
 * Stage 0: Full recursive draw, some geometry will be rebuilt along the way
 * Stage 1: Full recursive draw, no geometry rebuilt, capture everything in
 *          a display list (fstree_*_dlist, see above)
 * Stage 2: Fast draw using display list from stage 1 (no recursion) */
static int fstree_low_draw_stage;
static int fstree_high_draw_stage;


/* Forward declarations */
static void cursor_pre( void );
static void cursor_hidden_part( void );
static void cursor_visible_part( void );
static void cursor_post( void );
static void queue_uncached_draw( void );


// Vertex struct for modern OpenGL with normals
typedef struct Vertex {
	GLfloat position[3];
	GLfloat normal[3];
} Vertex;

// Vertex struct with only position
typedef struct VertexPos {
	GLfloat position[3];
} VertexPos;


// Print the legacy and modern OpenGL projection and modelview matrices.
// which = 0: both modelview and projection matrices
// which = 1: Only modelview
// which = 2: Only projection
__attribute__((unused)) static void
debug_print_matrices(int which)
{
#ifdef DEBUG
	if (which == 0 || which == 1) {
		g_print("Modelview matrix:\n");
		glmc_mat4_print(gl.modelview, stdout);
	}
	if (which == 0 || which == 2) {
		g_print("\nProjection matrix:\n");
		glmc_mat4_print(gl.projection, stdout);
	}
#endif
}


static unsigned int highlight_node_id;

// Set node color and lightning enabled uniform. GL Program must be in use when
// calling this.
static void
node_set_color(GNode *node)
{
	GLfloat color[4];
	color[3] = 1.0;	 // Alpha
	if (gl.render_mode == RENDERMODE_RENDER) {
		memcpy(color, NODE_DESC(node)->color, 3 * sizeof(GLfloat));
		// Check highlight
		if (NODE_DESC(node)->id == highlight_node_id) {
			for (size_t i = 0; i < 3; i++)
				color[i] *= 1.3f;
		}
		glUniform1i(gl.lightning_enabled_location, 1);
	} else {
		GLuint c = NODE_DESC(node)->id;
		// const char *name = NODE_DESC(node)->name;
		GLuint r = (c & 0x000000FF) >> 0;
		GLuint g = (c & 0x0000FF00) >> 8;
		GLuint b = (c & 0x00FF0000) >> 16;
		color[0] = (GLfloat)r / G_MAXUINT8;
		color[1] = (GLfloat)g / G_MAXUINT8;
		color[2] = (GLfloat)b / G_MAXUINT8;
		// g_print("Painting node %s id %u with Color red %f green %f
		// blue %f\n", 	name, c, (double)color[0], (double)color[1],
		//(double)color[2]);
		glUniform1i(gl.lightning_enabled_location, 0);
	}

	glUniform4fv(gl.color_location, 1, color);
}

static const RGBcolor color_black = {0, 0, 0};

// Upload and draw a bunch of VertexPos vertices.
// Note this is highly inefficient and implements every known modern GL
// anti-pattern (e.g. does not take any advantage of batching, or keeping
// vertex data on the GPU instead of reuploading it every time). But hey,
// it's simple.
static void
drawVertexPos(GLenum mode, VertexPos *vert, size_t vert_cnt, const RGBcolor *color)
{
	static GLuint vbo;
	if (!vbo)
		glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPos) * vert_cnt, vert, GL_STREAM_DRAW);

	glEnableVertexAttribArray(gl.position_location);
	glVertexAttribPointer(gl.position_location, 3, GL_FLOAT, GL_FALSE,
			      sizeof(VertexPos), (void *)offsetof(VertexPos, position));

	glUseProgram(gl.program);
	glUniform4f(gl.color_location, color->r, color->g, color->b, 1);
	glUniform1i(gl.lightning_enabled_location, 0);
	glDrawArrays(mode, 0, vert_cnt);
	glUseProgram(0);

	// Avoid implicit sync by allowing GL to dealloc memory
	glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPos) * vert_cnt, NULL, GL_STREAM_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}


// Similar to above, but draw a Vertex.
// The color is taken either from the color argument, or the node argument.
// One of these must be NULL and the other non-null.
static void
drawVertex(GLenum mode, Vertex *vert, size_t vert_cnt, const RGBcolor *color, GNode *node)
{
	if (color != NULL)
		g_assert(node == NULL);
	else
		g_assert(node != NULL);

	static GLuint vbo;
	if (!vbo) glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vert_cnt, vert,
		     GL_STREAM_DRAW);

	glEnableVertexAttribArray(gl.position_location);
	glVertexAttribPointer(gl.position_location, 3, GL_FLOAT, GL_FALSE,
			      sizeof(Vertex),
			      (void *)offsetof(Vertex, position));
	glEnableVertexAttribArray(gl.normal_location);
	glVertexAttribPointer(gl.normal_location, 3, GL_FLOAT, GL_FALSE,
			      sizeof(Vertex), (void *)offsetof(Vertex, normal));

	glUseProgram(gl.program);
	if (color) {
		glUniform4f(gl.color_location, color->r, color->g, color->b, 1);
		glUniform1i(gl.lightning_enabled_location, 1);
	} else
		node_set_color(node);
	glDrawArrays(mode, 0, vert_cnt);
	glUseProgram(0);

	// Avoid implicit sync by allowing GL to dealloc memory
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vert_cnt, NULL,
		     GL_STREAM_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/**** DISC VISUALIZATION **************************************/


/* Geometry constants */
#define DISCV_CURVE_GRANULARITY		15.0
#define DISCV_LEAF_RANGE_ARC_WIDTH	315.0
#define DISCV_LEAF_STEM_PROPORTION	0.5

/* Messages for discv_draw_recursive( ) */
enum {
	DISCV_DRAW_GEOMETRY,
	DISCV_DRAW_LABELS
};


/* Returns the absolute position of the given node */
XYvec *
geometry_discv_node_pos( GNode *node )
{
	static XYvec pos;
	DiscVGeomParams *gparams;
	GNode *up_node;

	pos.x = 0.0;
	pos.y = 0.0;
	up_node = node;
	while (up_node != NULL) {
		gparams = DISCV_GEOM_PARAMS(up_node);
		pos.x += gparams->pos.x;
		pos.y += gparams->pos.y;
		up_node = up_node->parent;
	}

	return &pos;
}


/* Compare function for sorting nodes (by size) */
static int
discv_node_compare( GNode *a, GNode *b )
{
	int64 a_size, b_size;

	a_size = NODE_DESC(a)->size;
	if (NODE_IS_DIR(a))
		a_size += DIR_NODE_DESC(a)->subtree.size;

	b_size = NODE_DESC(b)->size;
	if (NODE_IS_DIR(b))
		b_size += DIR_NODE_DESC(b)->subtree.size;

	if (a_size < b_size)
		return 1;
	if (a_size > b_size)
		return -1;

	return strcmp( NODE_DESC(a)->name, NODE_DESC(b)->name );
}


/* Helper function for discv_init( ) */
static void
discv_init_recursive( GNode *dnode, double stem_theta )
{
	DiscVGeomParams *gparams;
	GNode *node;
	GList *node_list = NULL, *nl_llink;
	int64 node_size;
	double dir_radius, radius, dist;
	double arc_width, total_arc_width = 0.0;
	double theta0, theta1;
	double k;
	boolean even = TRUE;
	boolean stagger, out = TRUE;

	g_assert( NODE_IS_DIR(dnode) || NODE_IS_METANODE(dnode) );

	if (NODE_IS_DIR(dnode)) {
		morph_break( &DIR_NODE_DESC(dnode)->deployment );
		if (dirtree_entry_expanded( dnode ))
			DIR_NODE_DESC(dnode)->deployment = 1.0;
		else
			DIR_NODE_DESC(dnode)->deployment = 0.0;
		geometry_queue_rebuild( dnode );
	}

	/* If this directory has no children,
	 * there is nothing further to do here */
	if (dnode->children == NULL)
		return;

	dir_radius = DISCV_GEOM_PARAMS(dnode)->radius;

	/* Assign radii (and arc widths, temporarily) to leaf nodes */
	node = dnode->children;
	while (node != NULL) {
		node_size = MAX(64, NODE_DESC(node)->size);
                if (NODE_IS_DIR(node))
			node_size += DIR_NODE_DESC(node)->subtree.size;
		/* Area of disc == node_size */
		radius = sqrt( (double)node_size / PI );
		/* Center-to-center distance (parent to leaf) */
		dist = dir_radius + radius * (1.0 + DISCV_LEAF_STEM_PROPORTION);
		arc_width = 2.0 * DEG(asin( radius / dist ));
		gparams = DISCV_GEOM_PARAMS(node);
		gparams->radius = radius;
		gparams->theta = arc_width; /* temporary value */
		gparams->pos.x = dist; /* temporary value */
		total_arc_width += arc_width;
		node = node->next;
	}

	/* Create a list of leaf nodes, sorted by size */
	node = dnode->children;
	while (node != NULL) {
		G_LIST_PREPEND(node_list, node);
		node = node->next;
	}
	G_LIST_SORT(node_list, discv_node_compare);

	k = DISCV_LEAF_RANGE_ARC_WIDTH / total_arc_width;
	/* If this is going to be a tight fit, stagger the leaf nodes */
	stagger = k <= 1.0;

	/* Assign angle positions to leaf nodes, arranging them in clockwise
	 * order (spread out to occupy the entire available range), and
	 * recurse into subdirectories */
	theta0 = stem_theta - 180.0;
	theta1 = stem_theta + 180.0;
	nl_llink = node_list;
	while (nl_llink != NULL) {
		node = nl_llink->data;
		gparams = DISCV_GEOM_PARAMS(node);
		arc_width = k * gparams->theta;
		dist = gparams->pos.x;
		if (stagger && out) {
			/* Push leaf out */
			dist += 2.0 * gparams->radius;
		}
		if (nl_llink->prev == NULL) {
			/* First (largest) node */
			gparams->theta = theta0;
			theta0 += 0.5 * arc_width;
			theta1 -= 0.5 * arc_width;
			out = !out;
		}
		else if (even) {
			gparams->theta = theta0 + 0.5 * arc_width;
			theta0 += arc_width;
			out = !out;
		}
		else {
			gparams->theta = theta1 - 0.5 * arc_width;
			theta1 -= arc_width;
		}
		gparams->pos.x = dist * cos( RAD(gparams->theta) );
		gparams->pos.y = dist * sin( RAD(gparams->theta) );
		if (NODE_IS_DIR(node))
			discv_init_recursive( node, gparams->theta + 180.0 );
		even = !even;
		nl_llink = nl_llink->next;
	}

	g_list_free( node_list );
}


static void
discv_init( void )
{
	DiscVGeomParams *gparams;

	gparams = DISCV_GEOM_PARAMS(globals.fstree);
	gparams->radius = 0.0;
	gparams->theta = 0.0;

	discv_init_recursive( globals.fstree, 270.0 );

	gparams->pos.x = 0.0;
	gparams->pos.y = - DISCV_GEOM_PARAMS(root_dnode)->radius;

	/* DiscV mode is entirely 2D, normal should always be {0, 0, 1} */
}


/* Draws a DiscV node. dir_deployment is deployment of parent directory */
static void
discv_gldraw_node( GNode *node, double dir_deployment )
{
	static const int seg_count = (int)(360.0 / DISCV_CURVE_GRANULARITY + 0.999);
	DiscVGeomParams *gparams;
	XYvec center, p;
	double theta;
	int s;

	gparams = DISCV_GEOM_PARAMS(node);

	center.x = dir_deployment * gparams->pos.x;
	center.y = dir_deployment * gparams->pos.y;

	/* Draw disc */
	size_t vert_cnt = seg_count + 2;
	Vertex *vert = NEW_ARRAY(Vertex, vert_cnt);
	vert[0] = (Vertex){{center.x, center.y, 0}, {0, 0, 1}};
	for (s = 0; s <= seg_count; s++) {
		theta = (double)s / (double)seg_count * 360.0;
		p.x = center.x + gparams->radius * cos( RAD(theta) );
		p.y = center.y + gparams->radius * sin( RAD(theta) );
		vert[s + 1] = (Vertex){{p.x, p.y, 0}, {0, 0, 1}};
	}
	drawVertex(GL_TRIANGLE_FAN, vert, vert_cnt, NULL, node);
	xfree(vert);
}


static void
discv_gldraw_folder( GNode *node )
{

	/* To be written... */

}


/* Builds the leaf nodes of a directory (but not the directory itself--
 * that geometry belongs to the parent) */
static void
discv_build_dir( GNode *dnode )
{
	GNode *node;
	double dpm;

	dpm = DIR_NODE_DESC(dnode)->deployment;
	/* TODO: Fix this, please */
	dpm = 1.0;

	node = dnode->children;
	while (node != NULL) {
		discv_gldraw_node( node, dpm );
		node = node->next;
	}
}


static void
discv_apply_label( GNode *node )
{

}


/* Helper function for discv_draw( ) */
static void
discv_draw_recursive( GNode *dnode, int action )
{
	DirNodeDesc *dir_ndesc;
	DiscVGeomParams *dir_gparams;
	GNode *node;
	boolean dir_collapsed;
	boolean dir_expanded;

	dir_ndesc = DIR_NODE_DESC(dnode);
	dir_gparams = DISCV_GEOM_PARAMS(dnode);

	mat4 tmpmat;
	glm_mat4_copy(gl.modelview, tmpmat);

	dir_collapsed = DIR_COLLAPSED(dnode);
	dir_expanded = DIR_EXPANDED(dnode);

	glm_translate(gl.modelview, (vec3){dir_gparams->pos.x, dir_gparams->pos.y, 0.0});
	glm_scale(gl.modelview, (vec3){dir_ndesc->deployment,  dir_ndesc->deployment,  1.0f});
	ogl_upload_matrices(TRUE);

	if (action == DISCV_DRAW_GEOMETRY) {
		/* Draw folder or leaf nodes */
		if (!dir_collapsed)
			discv_build_dir(dnode);
		if (!dir_expanded)
			discv_gldraw_folder(dnode);
	}

	if (action == DISCV_DRAW_LABELS) {
		/* Draw name label(s) */

		/* Label leaf nodes */
		node = dnode->children;
		while (node != NULL)
		{
			discv_apply_label(node);
			node = node->next;
		}
	}

	/* Update geometry status */
	dir_ndesc->geom_expanded = !dir_collapsed;

	if (dir_expanded) {
		/* Recurse into subdirectories */
		node = dnode->children;
		while (node != NULL) {
                        if (!NODE_IS_DIR(node))
				break;
			discv_draw_recursive( node, action );
			node = node->next;
		}
	}

	glm_mat4_copy(tmpmat, gl.modelview);
}


/* Draws DiscV geometry */
static void
discv_draw( boolean high_detail )
{
	glLineWidth( 3.0 );

	/* Draw low-detail geometry */

	discv_draw_recursive( globals.fstree, DISCV_DRAW_GEOMETRY );

	if (fstree_low_draw_stage <= 1)
		++fstree_low_draw_stage;


	if (high_detail) {
		/* Draw additional high-detail geometry */

		if (fstree_high_draw_stage <= 1) {
			/* Node name labels */
			text_pre( );
			text_set_color(0.0, 0.0, 0.0);
			discv_draw_recursive( globals.fstree, DISCV_DRAW_LABELS );
			text_post( );
		}
		if (fstree_high_draw_stage <= 1)
			++fstree_high_draw_stage;

		/* Node cursor */
		/* draw_cursor( ); */
	}

	glLineWidth( 1.0 );
}


/**** MAP VISUALIZATION ***************************************/


/* Geometry constants */
#define MAPV_BORDER_PROPORTION	0.01
#define MAPV_ROOT_ASPECT_RATIO	1.2

/* Messages for mapv_draw_recursive( ) */
enum {
	MAPV_DRAW_GEOMETRY,
	MAPV_DRAW_LABELS
};


/* Node side face offset ratios, by node type
 * (these define the obliqueness of a node's side faces) */
static const float mapv_side_slant_ratios[NUM_NODE_TYPES] = {
	NIL,	/* Metanode (not used) */
	0.032,	/* Directory */
	0.064,	/* Regular file */
	0.333,	/* Symlink */
	0.0,	/* FIFO */
	0.0,	/* Socket */
	0.25,	/* Character device */
	0.25,	/* Block device */
	0.0	/* Unknown */
};

/* Heights of directory and leaf nodes */
static double mapv_dir_height = 384.0;
static double mapv_leaf_height = 128.0;

/* Previous steady-state positions of the cursor corners
 * (if the cursor is moving from A to B, these delineate A) */
static XYZvec mapv_cursor_prev_c0;
static XYZvec mapv_cursor_prev_c1;


/* Returns the z-position of the bottom of a node */
double
geometry_mapv_node_z0( GNode *node )
{
	GNode *up_node;
	double z = 0.0;

	up_node = node->parent;
	while (up_node != NULL) {
		z += MAPV_GEOM_PARAMS(up_node)->height;
		up_node = up_node->parent;
	}

	return z;
}


/* Returns the peak height of a directory's contents (measured relative
 * to its top face), dictated by its expansion state as indicated by the
 * directory tree */
double
geometry_mapv_max_expanded_height( GNode *dnode )
{
	GNode *node;
	double height, max_height = 0.0;

	g_assert( NODE_IS_DIR(dnode) );

	if (dirtree_entry_expanded( dnode )) {
		node = dnode->children;
		while (node != NULL) {
			height = MAPV_GEOM_PARAMS(node)->height;
			if (NODE_IS_DIR(node)) {
				height += geometry_mapv_max_expanded_height( node );
				max_height = MAX(max_height, height);
			}
			else {
				max_height = MAX(max_height, height);
				break;
			}
			node = node->next;
		}
	}

	return max_height;
}


/* Helper function for mapv_init( ).
 * This is, in essence, the MapV layout engine */
static void
mapv_init_recursive( GNode *dnode )
{
	struct MapVBlock {
		GNode *node;
		double area;
	} *block, *next_first_block;
	struct MapVRow {
		struct MapVBlock *first_block;
		double area;
	} *row = NULL;
	MapVGeomParams *gparams;
	GNode *node;
	GList *block_list = NULL, *block_llink;
	GList *row_list = NULL, *row_llink;
	XYvec dir_dims, block_dims;
	XYvec start_pos, pos;
	double area, dir_area, total_block_area = 0.0;
	double nominal_border, border;
	double scale_factor;
	double a, b, k;
	int64 size;

	g_assert( NODE_IS_DIR(dnode) );

	morph_break( &DIR_NODE_DESC(dnode)->deployment );
	if (dirtree_entry_expanded( dnode ))
		DIR_NODE_DESC(dnode)->deployment = 1.0;
	else
		DIR_NODE_DESC(dnode)->deployment = 0.0;
	geometry_queue_rebuild( dnode );

	/* If this directory has no children,
	 * there is nothing further to do here */
	if (dnode->children == NULL)
		return;

	/* Obtain dimensions of top face of directory */
	dir_dims.x = MAPV_NODE_WIDTH(dnode);
	dir_dims.y = MAPV_NODE_DEPTH(dnode);
	k = mapv_side_slant_ratios[NODE_DIRECTORY];
	dir_dims.x -= 2.0 * MIN(MAPV_GEOM_PARAMS(dnode)->height, k * dir_dims.x);
	dir_dims.y -= 2.0 * MIN(MAPV_GEOM_PARAMS(dnode)->height, k * dir_dims.y);

	/* Approximate/nominal node border width (nodes will be spaced
	 * apart at about twice this distance) */
	a = MAPV_BORDER_PROPORTION * sqrt( dir_dims.x * dir_dims.y );
	b = MIN(dir_dims.x, dir_dims.y) / 3.0;
	nominal_border = MIN(a, b);

	/* Trim half a border width off the perimeter of the directory,
	 * so that nodes aren't laid down too close to the edges */
	dir_dims.x -= nominal_border;
	dir_dims.y -= nominal_border;
	dir_area = dir_dims.x * dir_dims.y;

	/* First pass
	 * 1. Create blocks. (A block is equivalent to a node, except
	 *    that it includes the node's surrounding border area)
	 * 2. Find total area of the blocks
	 * 3. Create a list of the blocks */
	node = dnode->children;
	while (node != NULL) {
		size = MAX(256, NODE_DESC(node)->size);
		if (NODE_IS_DIR(node))
			size += DIR_NODE_DESC(node)->subtree.size;
		k = sqrt( (double)size ) + nominal_border;
		area = SQR(k);
		total_block_area += area;

		block = NEW(struct MapVBlock);
		block->node = node;
		block->area = area;
		G_LIST_APPEND(block_list, block);

		node = node->next;
	}

	/* The blocks are going to have a total area greater than the
	 * directory can provide, so they'll have to be scaled down */
	scale_factor = dir_area / total_block_area;

	/* Second pass
	 * 1. Scale down the blocks
	 * 2. Generate a first-draft set of rows */
	block_llink = block_list;
	while (block_llink != NULL) {
		block = (struct MapVBlock *)block_llink->data;
		block->area *= scale_factor;

		if (row == NULL) {
			/* Begin new row */
			row = NEW(struct MapVRow);
			row->first_block = block;
			row->area = 0.0;
			G_LIST_APPEND(row_list, row);
		}

		/* Add block to row */
		row->area += block->area;

		/* Dimensions of block (block_dims.y == depth of row) */
		block_dims.y = row->area / dir_dims.x;
		block_dims.x = block->area / block_dims.y;

		/* Check aspect ratio of block */
		if ((block_dims.x / block_dims.y) < 1.0) {
			/* Next block will go into next row */
			row = NULL;
		}

		block_llink = block_llink->next;
	}

	/* Third pass - optimize layout */
	/* Note to self: write layout optimization routine sometime */

	/* Fourth pass - output final arrangement
	 * Start at right/rear corner, laying out rows of (mostly)
	 * successively smaller blocks */
	start_pos.x = MAPV_NODE_CENTER_X(dnode) + 0.5 * dir_dims.x;
	start_pos.y = MAPV_NODE_CENTER_Y(dnode) + 0.5 * dir_dims.y;
	pos.y = start_pos.y;
	block_llink = block_list;
	row_llink = row_list;
	while (row_llink != NULL) {
		row = (struct MapVRow *)row_llink->data;
		block_dims.y = row->area / dir_dims.x;
		pos.x = start_pos.x;

		/* Note first block of next row */
		if (row_llink->next == NULL)
			next_first_block = NULL;
		else
			next_first_block = ((struct MapVRow *)row_llink->next->data)->first_block;

		/* Output one row */
		while (block_llink != NULL) {
			block = (struct MapVBlock *)block_llink->data;
			if (block == next_first_block)
				break; /* finished with row */
			block_dims.x = block->area / block_dims.y;

			size = MAX(256, NODE_DESC(block->node)->size);
			if (NODE_IS_DIR(block->node))
				size += DIR_NODE_DESC(block->node)->subtree.size;
			area = scale_factor * (double)size;

			/* Calculate exact width of block's border region */
			k = block_dims.x + block_dims.y;
			/* Note: area == scaled area of node,
			 * block->area == scaled area of node + border */
			border = 0.25 * (k - sqrt( SQR(k) - 4.0 * (block->area - area) ));

			/* Assign geometry
			 * (Note: pos is right/rear corner of block) */
			gparams = MAPV_GEOM_PARAMS(block->node);
			gparams->c0.x = pos.x - block_dims.x + border;
			gparams->c0.y = pos.y - block_dims.y + border;
			gparams->c1.x = pos.x - border;
			gparams->c1.y = pos.y - border;

			if (NODE_IS_DIR(block->node)) {
				gparams->height = mapv_dir_height;

				/* Recurse into directory */
				mapv_init_recursive( block->node );
			}
			else
				gparams->height = mapv_leaf_height;

			pos.x -= block_dims.x;
			block_llink = block_llink->next;
		}

		pos.y -= block_dims.y;
		row_llink = row_llink->next;
	}

	/* Clean up */

	block_llink = block_list;
	while (block_llink != NULL) {
		xfree( block_llink->data );
		block_llink = block_llink->next;
	}
	g_list_free( block_list );

	row_llink = row_list;
	while (row_llink != NULL) {
		xfree( row_llink->data );
		row_llink = row_llink->next;
	}
	g_list_free( row_list );
}


/* Top-level call to initialize MapV mode */
static void
mapv_init( void )
{
	MapVGeomParams *gparams;
	XYvec root_dims;
	double k;

	/* Determine dimensions of bottommost (root) node */
	root_dims.y = sqrt( (double)DIR_NODE_DESC(globals.fstree)->subtree.size / MAPV_ROOT_ASPECT_RATIO );
	root_dims.x = MAPV_ROOT_ASPECT_RATIO * root_dims.y;

	/* Set up base geometry */
	MAPV_GEOM_PARAMS(globals.fstree)->height = 0.0;
	gparams = MAPV_GEOM_PARAMS(root_dnode);
	gparams->c0.x = -0.5 * root_dims.x;
	gparams->c0.y = -0.5 * root_dims.y;
	gparams->c1.x = 0.5 * root_dims.x;
	gparams->c1.y = 0.5 * root_dims.y;
	gparams->height = mapv_dir_height;

	mapv_init_recursive( root_dnode );

	/* Initial cursor state */
	if (globals.current_node == root_dnode)
		k = 4.0;
	else
		k = 1.25;
	mapv_cursor_prev_c0.x = k * MAPV_GEOM_PARAMS(root_dnode)->c0.x;
	mapv_cursor_prev_c0.y = k * MAPV_GEOM_PARAMS(root_dnode)->c0.y;
	mapv_cursor_prev_c0.z = - 0.25 * k * MAPV_NODE_DEPTH(root_dnode);
	mapv_cursor_prev_c1.x = k * MAPV_GEOM_PARAMS(root_dnode)->c1.x;
	mapv_cursor_prev_c1.y = k * MAPV_GEOM_PARAMS(root_dnode)->c1.y;
	mapv_cursor_prev_c1.z = 0.25 * k * MAPV_NODE_DEPTH(root_dnode);
}


/* Hook function for camera pan completion */
static void
mapv_camera_pan_finished( void )
{
	/* Save cursor position */
	mapv_cursor_prev_c0.x = MAPV_GEOM_PARAMS(globals.current_node)->c0.x;
	mapv_cursor_prev_c0.y = MAPV_GEOM_PARAMS(globals.current_node)->c0.y;
	mapv_cursor_prev_c0.z = geometry_mapv_node_z0( globals.current_node );
	mapv_cursor_prev_c1.x = MAPV_GEOM_PARAMS(globals.current_node)->c1.x;
	mapv_cursor_prev_c1.y = MAPV_GEOM_PARAMS(globals.current_node)->c1.y;
	mapv_cursor_prev_c1.z = mapv_cursor_prev_c0.z + MAPV_GEOM_PARAMS(globals.current_node)->height;
}


/* Draws a MapV node */
static void
mapv_gldraw_node( GNode *node )
{
	MapVGeomParams *gparams;
	XYZvec dims;
	XYvec offset, normal;
	double normal_z_nx, normal_z_ny;
	double a, b, k;

	/* Dimensions of node */
	dims.x = MAPV_NODE_WIDTH(node);
	dims.y = MAPV_NODE_DEPTH(node);
	dims.z = MAPV_GEOM_PARAMS(node)->height;

	/* Calculate normals for slanted sides */
	k = mapv_side_slant_ratios[NODE_DESC(node)->type];
	offset.x = MIN(dims.z, k * dims.x);
	offset.y = MIN(dims.z, k * dims.y);
	a = sqrt( SQR(offset.x) + SQR(dims.z) );
	b = sqrt( SQR(offset.y) + SQR(dims.z) );
	normal.x = dims.z / a;
	normal.y = dims.z / b;
	normal_z_nx = offset.x / a;
	normal_z_ny = offset.y / b;

	gparams = MAPV_GEOM_PARAMS(node);

	Vertex vertex_data[] = {
	    {{gparams->c0.x, gparams->c1.y, 0.0}, /* Rear face */
	     {0.0, normal.y, normal_z_ny}},
	    {{gparams->c0.x + offset.x, gparams->c1.y - offset.y,
	      gparams->height},
	     {0.0, normal.y, normal_z_ny}},
	    {{gparams->c1.x, gparams->c1.y, 0.0}, {0.0, normal.y, normal_z_ny}},
	    {{gparams->c1.x - offset.x, gparams->c1.y - offset.y,
	      gparams->height},
	     {0.0, normal.y, normal_z_ny}},
	    {{gparams->c1.x, gparams->c1.y, 0.0}, /* Right face */
	     {normal.x, 0.0, normal_z_nx}},
	    {{gparams->c1.x - offset.x, gparams->c1.y - offset.y,
	      gparams->height},
	     {normal.x, 0.0, normal_z_nx}},
	    {{gparams->c1.x, gparams->c0.y, 0.0}, {normal.x, 0.0, normal_z_nx}},
	    {{gparams->c1.x - offset.x, gparams->c0.y + offset.y,
	      gparams->height},
	     {normal.x, 0.0, normal_z_nx}},
	    {{gparams->c1.x, gparams->c0.y, 0.0},  // Front face
	     {0.0, -normal.y, normal_z_ny}},
	    {{gparams->c1.x - offset.x, gparams->c0.y + offset.y,
	      gparams->height},
	     {0.0, -normal.y, normal_z_ny}},
	    {{gparams->c0.x, gparams->c0.y, 0.0},
	     {0.0, -normal.y, normal_z_ny}},
	    {{gparams->c0.x + offset.x, gparams->c0.y + offset.y,
	      gparams->height},
	     {0.0, -normal.y, normal_z_ny}},
	    {{gparams->c0.x, gparams->c0.y, 0.0},  // Left face
	     {-normal.x, 0.0, normal_z_nx}},
	    {{gparams->c0.x + offset.x, gparams->c0.y + offset.y,
	      gparams->height},
	     {-normal.x, 0.0, normal_z_nx}},
	    {{gparams->c0.x, gparams->c1.y, 0.0},
	     {-normal.x, 0.0, normal_z_nx}},
	    {{gparams->c0.x + offset.x, gparams->c1.y - offset.y,
	      gparams->height},
	     {-normal.x, 0.0, normal_z_nx}},
	    // Top face
	    {{gparams->c0.x + offset.x, gparams->c0.y + offset.y,
	      gparams->height},	 // 1
	     {0.0f, 0.0f, 1.0f}},
	    {{gparams->c1.x - offset.x, gparams->c0.y + offset.y,
	      gparams->height},	 // 2
	     {0.0f, 0.0f, 1.0f}},
	    {{gparams->c0.x + offset.x, gparams->c1.y - offset.y,
	      gparams->height},	 // 4
	     {0.0f, 0.0f, 1.0f}},
	    {{gparams->c1.x - offset.x, gparams->c1.y - offset.y,
	      gparams->height},	 // 3
	     {0.0f, 0.0f, 1.0f}}};
	static const GLushort elements[] = {
	    0,	1,  2,	2,  1,	3,   // Rear face
	    4,	5,  6,	6,  5,	7,   // Right face
	    8,	9,  10, 10, 9,	11,  // Front face
	    12, 13, 14, 14, 13, 15,  // Left face
	    16, 17, 18, 18, 17, 19   // Top face
	};
	ogl_error();
	//debug_print_matrices(0);
	static GLuint vbo;
	if (!vbo)
		glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), &vertex_data, GL_DYNAMIC_DRAW);

	static GLuint ebo;
	if (!ebo) {
		glGenBuffers(1, &ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), &elements, GL_STATIC_DRAW);
	}

	glEnableVertexAttribArray(gl.position_location);
	glVertexAttribPointer(gl.position_location, 3, GL_FLOAT, GL_FALSE,
			      sizeof(Vertex), (void *)offsetof(Vertex, position));

	glEnableVertexAttribArray(gl.normal_location);
	glVertexAttribPointer(gl.normal_location, 3, GL_FLOAT, GL_FALSE,
			      sizeof(Vertex), (void *)offsetof(Vertex, normal));

	ogl_error();

	glUseProgram(gl.program);

	node_set_color(node);
#if 0
#ifdef DEBUG
	mat4 mvp;
	glm_mat4_mul(gl.projection, gl.modelview, mvp);
	// Check coords
	vec3 out;
	g_print("quad coords with rendermode %d:\n", gl.render_mode);
	glm_mat4_mulv3(mvp, vertex_data[0].position, 1, out);
	glmc_vec3_print(out, stdout);
	glm_mat4_mulv3(mvp, vertex_data[1].position, 1, out);
	glmc_vec3_print(out, stdout);
	glm_mat4_mulv3(mvp, vertex_data[3].position, 1, out);
	glmc_vec3_print(out, stdout);
#endif
#endif
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	GLsizei cnt = sizeof(elements) / sizeof(GLushort);
	glDrawElements(GL_TRIANGLES, cnt, GL_UNSIGNED_SHORT, 0);
	glUseProgram(0);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}


/* Draws a "folder" shape atop a directory */
static void
mapv_gldraw_folder( GNode *dnode )
{
	XYvec dims, offset;
	XYvec c0, c1;
	XYvec folder_c0, folder_c1, folder_tab;
	double k, border;

	g_assert( NODE_IS_DIR(dnode) );

	/* Obtain corners/dimensions of top face */
	dims.x = MAPV_NODE_WIDTH(dnode);
	dims.y = MAPV_NODE_DEPTH(dnode);
	k = mapv_side_slant_ratios[NODE_DIRECTORY];
	offset.x = MIN(MAPV_GEOM_PARAMS(dnode)->height, k * dims.x);
	offset.y = MIN(MAPV_GEOM_PARAMS(dnode)->height, k * dims.y);
	c0.x = MAPV_GEOM_PARAMS(dnode)->c0.x + offset.x;
	c0.y = MAPV_GEOM_PARAMS(dnode)->c0.y + offset.y;
	c1.x = MAPV_GEOM_PARAMS(dnode)->c1.x - offset.x;
	c1.y = MAPV_GEOM_PARAMS(dnode)->c1.y - offset.y;
	dims.x -= 2.0 * offset.x;
	dims.y -= 2.0 * offset.y;

	/* Folder geometry */
	border = 0.0625 * MIN(dims.x, dims.y);
	folder_c0.x = c0.x + border;
	folder_c0.y = c0.y + border;
	folder_c1.x = c1.x - border;
	folder_c1.y = c1.y - border;
	/* Coordinates of the concave vertex */
	folder_tab.x = folder_c1.x - (MAGIC_NUMBER - 1.0) * (folder_c1.x - folder_c0.x);
	folder_tab.y = folder_c1.y - border;

	VertexPos vert[] = {
		{{folder_c0.x, folder_c0.y, 0}},
		{{folder_c0.x, folder_tab.y, 0}},
		{{folder_c0.x + border, folder_c1.y, 0}},
		{{folder_tab.x - border, folder_c1.y, 0}},
		{{folder_tab.x, folder_tab.y, 0}},
		{{folder_c1.x, folder_tab.y, 0}},
		{{folder_c1.x, folder_c0.y, 0}}
	};

	drawVertexPos(GL_LINE_LOOP, vert, 7, &color_black);
}


/* Builds the children of a directory (but not the directory itself;
 * that geometry belongs to the parent) */
static void
mapv_build_dir( GNode *dnode )
{
	GNode *node;

	g_assert( NODE_IS_DIR(dnode) || NODE_IS_METANODE(dnode) );

	node = dnode->children;
	while (node != NULL) {
		/* Draw node */
		mapv_gldraw_node( node );
		ogl_error();
		node = node->next;
	}
}


/* Draws a node name label */
static void
mapv_apply_label( GNode *node )
{
	XYZvec label_pos;
	XYvec dims, label_dims;
	double k;

	/* Obtain dimensions of top face */
	dims.x = MAPV_NODE_WIDTH(node);
	dims.y = MAPV_NODE_DEPTH(node);
	k = mapv_side_slant_ratios[NODE_DESC(node)->type];
	dims.x -= 2.0 * MIN(MAPV_GEOM_PARAMS(node)->height, k * dims.x);
	dims.y -= 2.0 * MIN(MAPV_GEOM_PARAMS(node)->height, k * dims.y);

	/* (Maximum) dimensions of label */
	label_dims.x = 0.8125 * dims.x;
	label_dims.y = (2.0 - MAGIC_NUMBER) * dims.y;

	/* Center position of label */
	label_pos.x = MAPV_NODE_CENTER_X(node);
	label_pos.y = MAPV_NODE_CENTER_Y(node);
	if (NODE_IS_DIR(node))
		label_pos.z = 0.0;
	else
		label_pos.z = MAPV_GEOM_PARAMS(node)->height;

	text_draw_straight( NODE_DESC(node)->name, &label_pos, &label_dims );
}


/* MapV mode "full draw" */
static void
mapv_draw_recursive( GNode *dnode, int action )
{
	DirNodeDesc *dir_ndesc;
	GNode *node;
	boolean dir_collapsed;
	boolean dir_expanded;

	g_assert( NODE_IS_DIR(dnode) || NODE_IS_METANODE(dnode) );

	mat4 tmpmat;
	glm_mat4_copy(gl.modelview, tmpmat);
	glm_translate(gl.modelview, (vec3){0.0f, 0.0f, MAPV_GEOM_PARAMS(dnode)->height});

	dir_ndesc = DIR_NODE_DESC(dnode);
	dir_collapsed = DIR_COLLAPSED(dnode);
	dir_expanded = DIR_EXPANDED(dnode);

	if (!dir_collapsed && !dir_expanded) {
		/* Grow/shrink children heightwise */
		glm_scale(gl.modelview, (vec3){1.0f, 1.0f, dir_ndesc->deployment});
	}

	ogl_error();
	ogl_upload_matrices(TRUE);
	ogl_error();

	if (action == MAPV_DRAW_GEOMETRY) {
		/* Draw directory face or geometry of children
		 */
		if (dir_collapsed)
			mapv_gldraw_folder(dnode);
		else
			mapv_build_dir(dnode);
	}
	ogl_error();

	if (action == MAPV_DRAW_LABELS) {
		/* Draw name label(s) */
		if (dir_collapsed)
		{
			/* Label directory */
			mapv_apply_label(dnode);
		}
		else
		{
			/* Label non-subdirectory children */
			node = dnode->children;
			while (node != NULL)
			{
				if (!NODE_IS_DIR(node))
					mapv_apply_label(node);
				node = node->next;
			}
		}
	}

	/* Update geometry status */
	dir_ndesc->geom_expanded = !dir_collapsed;

	if (!dir_collapsed) {
		/* Recurse into subdirectories */
		node = dnode->children;
		while (node != NULL) {
			if (!NODE_IS_DIR(node))
				break;
			mapv_draw_recursive( node, action );
			node = node->next;
		}
	}

	glm_mat4_copy(tmpmat, gl.modelview);
	ogl_upload_matrices(FALSE);
}


/* Draws the node cursor, size/position specified by corners */
static void
mapv_gldraw_cursor( const XYZvec *c0, const XYZvec *c1 )
{
	static const double bar_part = SQR(SQR(MAGIC_NUMBER - 1.0));
	XYZvec corner_dims;
	XYZvec p, delta;
	int i, c;

	corner_dims.x = bar_part * (c1->x - c0->x);
	corner_dims.y = bar_part * (c1->y - c0->y);
	corner_dims.z = bar_part * (c1->z - c0->z);

	cursor_pre( );
	static GLuint vbo;
	if (!vbo) glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	for (i = 0; i < 2; i++) {
		if (i == 0)
			cursor_hidden_part( );
		else if (i == 1)
			cursor_visible_part( );

		for (c = 0; c < 8; c++) {
			if (c & 1) {
				p.x = c1->x;
				delta.x = - corner_dims.x;
			}
			else {
				p.x = c0->x;
				delta.x = corner_dims.x;
			}

			if (c & 2) {
				p.y = c1->y;
				delta.y = - corner_dims.y;
			}
			else {
				p.y = c0->y;
				delta.y = corner_dims.y;
			}

			if (c & 4) {
				p.z = c1->z;
				delta.z = - corner_dims.z;
			}
			else {
				p.z = c0->z;
				delta.z = corner_dims.z;
			}

			VertexPos vert[] = {
				{{p.x, p.y, p.z}}, // First line
				{{p.x + delta.x, p.y, p.z}},
				{{p.x, p.y, p.z}}, // Second
				{{p.x, p.y + delta.y, p.z}},
				{{p.x, p.y, p.z}}, // Third
				{{p.x, p.y, p.z + delta.z}}
			};

			glBufferData(GL_ARRAY_BUFFER, sizeof(vert),
				     vert, GL_STREAM_DRAW);
			glEnableVertexAttribArray(gl.position_location);
			glVertexAttribPointer(
			    gl.position_location, 3, GL_FLOAT, GL_FALSE,
			    sizeof(VertexPos),
			    (void *)offsetof(VertexPos, position));
			glDrawArrays(GL_LINES, 0, 6);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vert),
				     NULL, GL_STREAM_DRAW);
		}
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	cursor_post( );
}


/* Draws the node cursor in an intermediate position between its previous
 * steady-state position and the current node (pos=0 indicates the former,
 * pos=1 the latter) */
static void
mapv_draw_cursor( double pos )
{
	MapVGeomParams *gparams;
	XYZvec cursor_c0, cursor_c1;
	double z0;

	gparams = MAPV_GEOM_PARAMS(globals.current_node);
        z0 = geometry_mapv_node_z0( globals.current_node );

	/* Interpolate corners */
	cursor_c0.x = INTERPOLATE(pos, mapv_cursor_prev_c0.x, gparams->c0.x);
	cursor_c0.y = INTERPOLATE(pos, mapv_cursor_prev_c0.y, gparams->c0.y);
	cursor_c0.z = INTERPOLATE(pos, mapv_cursor_prev_c0.z, z0);
	cursor_c1.x = INTERPOLATE(pos, mapv_cursor_prev_c1.x, gparams->c1.x);
	cursor_c1.y = INTERPOLATE(pos, mapv_cursor_prev_c1.y, gparams->c1.y);
	cursor_c1.z = INTERPOLATE(pos, mapv_cursor_prev_c1.z, z0 + gparams->height);

	mapv_gldraw_cursor( &cursor_c0, &cursor_c1 );
}


/* Draws MapV geometry */
static void
mapv_draw( boolean high_detail )
{
	/* Draw low-detail geometry */

	mapv_draw_recursive( globals.fstree, MAPV_DRAW_GEOMETRY );

	if (fstree_low_draw_stage <= 1)
		++fstree_low_draw_stage;

	if (high_detail) {
		/* Draw additional high-detail stuff */

		/* Node name labels */
		text_pre( );
		text_set_color(0.0, 0.0, 0.0); /* all labels are black */
		mapv_draw_recursive( globals.fstree, MAPV_DRAW_LABELS );
		text_post( );
		if (fstree_high_draw_stage <= 1)
			++fstree_high_draw_stage;

		/* Node cursor */
		mapv_draw_cursor( CURSOR_POS(camera->pan_part) );
	}
}


/**** TREE VISUALIZATION **************************************/


/* Geometry constants */
#define TREEV_MIN_ARC_WIDTH		90.0
#define TREEV_MAX_ARC_WIDTH		225.0
#define TREEV_BRANCH_WIDTH		256.0
#define TREEV_MIN_CORE_RADIUS		8192.0
#define TREEV_CORE_GROW_FACTOR		1.25
#define TREEV_CURVE_GRANULARITY		5.0
#define TREEV_PLATFORM_HEIGHT		158.2
#define TREEV_PLATFORM_SPACING_WIDTH	512.0
#define TREEV_LEAF_HEIGHT_MULTIPLIER	1.0
#define TREEV_LEAF_PADDING		(0.125 * TREEV_LEAF_NODE_EDGE)
#define TREEV_PLATFORM_PADDING		(0.5 * TREEV_PLATFORM_SPACING_WIDTH)

/* Extra flags for TreeV mode */
enum {
	TREEV_NEED_REARRANGE	= 1 << 0
};

/* Messages for treev_draw_recursive( ) */
enum {
	/* Note: don't change order of these */
	TREEV_DRAW_LABELS,
	TREEV_DRAW_GEOMETRY,
	TREEV_DRAW_GEOMETRY_WITH_BRANCHES
};


/* Color of interconnecting branches */
static RGBcolor branch_color = { 0.5, 0.0, 0.0 };

/* Label colors for platform and leaf nodes */
static RGBcolor treev_platform_label_color = { 1.0, 1.0, 1.0 };
static RGBcolor treev_leaf_label_color = { 0.0, 0.0, 0.0 };

/* Point buffers used in drawing curved geometry */
static XYvec *inner_edge_buf = NULL;
static XYvec *outer_edge_buf = NULL;

/* Radius of innermost loop */
static double treev_core_radius;

/* Previous steady-state positions of the cursor corners */
static RTZvec treev_cursor_prev_c0;
static RTZvec treev_cursor_prev_c1;


/* Checks if a node is currently a leaf (i.e. a collapsed directory or some
 * other node), or not (an expanded directory) according to the directory
 * tree */
boolean
geometry_treev_is_leaf( GNode *node )
{
	if (NODE_IS_DIR(node))
		if (dirtree_entry_expanded( node ))
			return FALSE;

	return TRUE;
}


/* Returns the inner radius of a directory platform */
double
geometry_treev_platform_r0( GNode *dnode )
{
	GNode *up_node;
	double r0 = 0.0;

	if (NODE_IS_METANODE(dnode))
		return treev_core_radius;

	up_node = dnode->parent;
	while (up_node != NULL) {
		r0 += TREEV_PLATFORM_SPACING_DEPTH;
		r0 += TREEV_GEOM_PARAMS(up_node)->platform.depth;
		up_node = up_node->parent;
	}
	r0 += treev_core_radius;

	return r0;
}


/* Returns the absolute angular position of a directory platform (which
 * is referenced along the platform's radial centerline) */
double
geometry_treev_platform_theta( GNode *dnode )
{
	GNode *up_node;
	double theta = 0.0;

	g_assert( !geometry_treev_is_leaf( dnode ) || NODE_IS_METANODE(dnode) );

	up_node = dnode;
	while (up_node != NULL) {
		theta += TREEV_GEOM_PARAMS(up_node)->platform.theta;
		up_node = up_node->parent;
	}

	return theta;
}


/* This returns the height of the tallest leaf on the given directory
 * platform. Height does not include that of the platform itself */
double
geometry_treev_max_leaf_height( GNode *dnode )
{
	GNode *node;
	double max_height = 0.0;

	g_assert( !geometry_treev_is_leaf( dnode ) );

	node = dnode->children;
	while (node != NULL) {
		if (geometry_treev_is_leaf( node ))
			max_height = MAX(max_height, TREEV_GEOM_PARAMS(node)->leaf.height);
		node = node->next;
	}

	return max_height;
}


/* Helper function for treev_get_extents( ) */
static void
treev_get_extents_recursive( GNode *dnode, RTvec *c0, RTvec *c1, double r0, double theta )
{
	GNode *node;
	double subtree_r0;

	g_assert( NODE_IS_DIR(dnode) );

	subtree_r0 = r0 + TREEV_GEOM_PARAMS(dnode)->platform.depth + TREEV_PLATFORM_SPACING_DEPTH;
	node = dnode->children;
	while (node != NULL) {
		if (!geometry_treev_is_leaf( node ))
			treev_get_extents_recursive( node, c0, c1, subtree_r0, theta + TREEV_GEOM_PARAMS(node)->platform.theta );
/* TODO: try putting this check at top of loop */
		if (!NODE_IS_DIR(node))
			break;
		node = node->next;
	}

	c0->r = MIN(c0->r, r0);
	c0->theta = MIN(c0->theta, theta - TREEV_GEOM_PARAMS(dnode)->platform.arc_width);
	c1->r = MAX(c1->r, r0 + TREEV_GEOM_PARAMS(dnode)->platform.depth);
	c1->theta = MAX(c1->theta, theta + TREEV_GEOM_PARAMS(dnode)->platform.arc_width);
}


/* This returns the 2D corners of the entire subtree rooted at the given
 * directory platform, including the subtree root. (Note that the extents
 * returned depend on the current expansion state) */
void
geometry_treev_get_extents( GNode *dnode, RTvec *ext_c0, RTvec *ext_c1 )
{
	RTvec c0, c1;

	g_assert( !geometry_treev_is_leaf( dnode ) );

	c0.r = DBL_MAX;
	c0.theta = DBL_MAX;
	c1.r = DBL_MIN;
	c1.theta = DBL_MIN;

	treev_get_extents_recursive( dnode, &c0, &c1, geometry_treev_platform_r0( dnode ), geometry_treev_platform_theta( dnode ) );

	if (ext_c0 != NULL)
		*ext_c0 = c0; /* struct assign */
	if (ext_c1 != NULL)
		*ext_c1 = c1; /* struct assign */
}


/* Returns the corners (min/max RTZ points) of the given leaf node or
 * directory platform in absolute polar coordinates, with some padding
 * added on all sides for a not-too-tight fit */
static void
treev_get_corners( GNode *node, RTZvec *c0, RTZvec *c1 )
{
	RTZvec pos;
	double leaf_arc_width;
	double padding_arc_width;

	if (geometry_treev_is_leaf( node )) {
		/* Absolute position of center of leaf node bottom */
		pos.r = geometry_treev_platform_r0( node->parent ) + TREEV_GEOM_PARAMS(node)->leaf.distance;
		pos.theta = geometry_treev_platform_theta( node->parent ) + TREEV_GEOM_PARAMS(node)->leaf.theta;
		pos.z = TREEV_GEOM_PARAMS(node->parent)->platform.height;

		/* Calculate corners of leaf node */
		leaf_arc_width = (180.0 * TREEV_LEAF_NODE_EDGE / PI) / pos.r;
		c0->r = pos.r - (0.5 * TREEV_LEAF_NODE_EDGE);
		c0->theta = pos.theta - 0.5 * leaf_arc_width;
		c0->z = pos.z;
		c1->r = pos.r + (0.5 * TREEV_LEAF_NODE_EDGE);
		c1->theta = pos.theta + 0.5 * leaf_arc_width;
		c1->z = pos.z + TREEV_GEOM_PARAMS(node)->leaf.height;

		/* Push corners outward a bit */
		padding_arc_width = (180.0 * TREEV_LEAF_PADDING / PI) / pos.r;
		c0->r -= TREEV_LEAF_PADDING;
		c0->theta -= padding_arc_width;
		c0->z -= (0.5 * TREEV_LEAF_PADDING);
		c1->r += TREEV_LEAF_PADDING;
		c1->theta += padding_arc_width;
		c1->z += (0.5 * TREEV_LEAF_PADDING);
	}
	else {
		/* Position of center of inner edge of platform */
		pos.r = geometry_treev_platform_r0( node );
		pos.theta = geometry_treev_platform_theta( node );

		/* Calculate corners of platform region */
		c0->r = pos.r;
		c0->theta = pos.theta - 0.5 * TREEV_GEOM_PARAMS(node)->platform.arc_width;
		c0->z = 0.0;
		c1->r = pos.r + TREEV_GEOM_PARAMS(node)->platform.depth;
		c1->theta = pos.theta + 0.5 * TREEV_GEOM_PARAMS(node)->platform.arc_width;
		c1->z = TREEV_GEOM_PARAMS(node)->platform.height;

		/* Push corners outward a bit. Because the sides already
		 * encompass the platform spacing regions, there is no need
                 * to add extra padding there */
		c0->r -= TREEV_PLATFORM_PADDING;
		c1->r += TREEV_PLATFORM_PADDING;
	}
}


/* This assigns an arc width and depth to a directory platform.
 * Note: depth value is only an estimate; the final value can only be
 * determined by actually laying down leaf nodes */
static void
treev_reshape_platform( GNode *dnode, double r0 )
{
#define edge05 (0.5 * TREEV_LEAF_NODE_EDGE)
#define edge15 (1.5 * TREEV_LEAF_NODE_EDGE)
	static const double w = TREEV_PLATFORM_SPACING_WIDTH;
	static const double w_2 = SQR(TREEV_PLATFORM_SPACING_WIDTH);
	static const double w_3 = SQR(TREEV_PLATFORM_SPACING_WIDTH) * TREEV_PLATFORM_SPACING_WIDTH;
	static const double w_4 = SQR(TREEV_PLATFORM_SPACING_WIDTH) * SQR(TREEV_PLATFORM_SPACING_WIDTH);
	double area;
	double A, A_2, A_3, r, r_2, r_3, r_4, ka, kb, kc, kd, d, theta;
	double depth, arc_width, min_arc_width;
	double k;
	int n;

	/* Estimated area, based on number of (immediate) children */
	n = g_list_length( (GList *)dnode->children );
	k = edge15 * ceil( sqrt( (double)MAX(1, n) ) ) + edge05;
	area = SQR(k);

	/* Known: Area and inner radius of directory, plus the fact that
	 * the aspect ratio (length_of_outer_edge / depth) is exactly 1.
	 * Unknown: depth and arc width of directory.
	 * Raw and distilled equations:
	 * { A ~= PI*theta/360*((r + d)^2 - r^2) - w*d,
	 * s ~= PI*theta*(r + d)/180 - w,
	 * s/d = 1  -->  s = d,
	 * theta = 180*(d + w)/(PI*(r + d)),
	 * d^3 + (2*r + w)*d^2 + (2*w*r - 2*A - w)*d - 2*A*r = 0,
	 * A = area, w = TREEV_PLATFORM_SPACING_WIDTH, r = r0,
	 * s = (length of outer edge), d = depth, theta = arc_width }
	 * Solution: Thank god for Maple */
	A = area;
	A_2 = SQR(A);
	A_3 = A*A_2;
	r = r0;
	r_2 = SQR(r);
	r_3 = r*r_2;
	r_4 = SQR(r_2);
	ka = 72.0*(A*r - w*(A + r)) - 64.0*r_3 + 48.0*r_2*w - 36.0*w_2 + 24.0*r*w_2 - 8.0*w_3;
#define T1 72.0*A*w_2 - 132.0*A*r*w_2 - 240.0*A*w*r_3 + 120.0*A*w_2*r_2 - 24.0*A_2*w*r - 60.0*w_3*r
#define T2 12.0*(w_2*r_2 + A_2*w_2 - w_4*r + w_4*r_2 + A*w_3 + w_3)
#define T3 48.0*(w_2*r_4 - w_2*r_3 - w_3*r_3) + 96.0*(A_3 + w_3*r_2)
#define T4 192.0*A*r_4 + 156.0*A_2*r_2 + 3.0*w_4 + 144.0*A_2*w + 264.0*A*w*r_2
	kb = 12.0*sqrt( T1 + T2 + T3 + T4 );
#undef T1
#undef T2
#undef T3
#undef T4
	kc = cos( atan2( kb, ka ) / 3.0 );
	kd = cbrt( hypot( ka, kb ) );
	/* Bring it all together */
	d = (- w - 2.0*r)/3.0 + ((8.0*r_2 - 4.0*w*r + 2.0*w_2)/3.0 + 4.0*A + 2.0*w)*kc/kd + kc*kd/6.0;
	theta = 180.0*(d + w)/(PI*(r + d));

	depth = d;
	arc_width = theta;

	/* Adjust depth upward to accomodate an integral number of rows */
	depth += (edge15 - fmod( depth - edge05, edge15 )) + edge05;

	/* Final arc width must be at least large enough to yield an
	 * inner edge length that is two leaf node edges long */
	min_arc_width = (180.0 * (2.0 * TREEV_LEAF_NODE_EDGE + TREEV_PLATFORM_SPACING_WIDTH) / PI) / r0;

	TREEV_GEOM_PARAMS(dnode)->platform.arc_width = MAX(min_arc_width, arc_width);
	TREEV_GEOM_PARAMS(dnode)->platform.depth = depth;

	/* Directory will need rebuilding, obviously */
	geometry_queue_rebuild( dnode );

#undef edge05
#undef edge15
}


/* Helper function for treev_arrange( ). @reshape_tree flag should be TRUE
 * if platform radiuses have changed (thus requiring reshaping) */
static void
treev_arrange_recursive( GNode *dnode, double r0, boolean reshape_tree )
{
	GNode *node;
	double subtree_r0;
	double arc_width, subtree_arc_width = 0.0;
	double theta;

	g_assert( NODE_IS_DIR(dnode) || NODE_IS_METANODE(dnode) );

	if (!reshape_tree && !(NODE_DESC(dnode)->flags & TREEV_NEED_REARRANGE))
		return;

	if (reshape_tree && NODE_IS_DIR(dnode)) {
		if (geometry_treev_is_leaf(dnode)) {
			/* Ensure directory leaf gets repositioned */
			geometry_queue_rebuild( dnode );
			return;
		}
		else {
			/* Reshape directory platform */
			treev_reshape_platform( dnode, r0 );
		}
	}

	/* Recurse into expanded subdirectories, and obtain the overall
	 * arc width of the subtree */
	subtree_r0 = r0 + TREEV_GEOM_PARAMS(dnode)->platform.depth + TREEV_PLATFORM_SPACING_DEPTH;
	node = dnode->children;
	while (node != NULL) {
		if (!NODE_IS_DIR(node))
			break;
		treev_arrange_recursive( node, subtree_r0, reshape_tree );
		arc_width = DIR_NODE_DESC(node)->deployment * MAX(TREEV_GEOM_PARAMS(node)->platform.arc_width, TREEV_GEOM_PARAMS(node)->platform.subtree_arc_width);
		TREEV_GEOM_PARAMS(node)->platform.theta = arc_width; /* temporary value */
		subtree_arc_width += arc_width;
		node = node->next;
	}
	TREEV_GEOM_PARAMS(dnode)->platform.subtree_arc_width = subtree_arc_width;

	/* Spread the subdirectories, sweeping counterclockwise */
	theta = -0.5 * subtree_arc_width;
	node = dnode->children;
	while (node != NULL) {
                if (!NODE_IS_DIR(node))
			break;
		arc_width = TREEV_GEOM_PARAMS(node)->platform.theta;
		TREEV_GEOM_PARAMS(node)->platform.theta = theta + 0.5 * arc_width;
		theta += arc_width;
		node = node->next;
	}

	/* Clear the "need rearrange" flag */
	NODE_DESC(dnode)->flags &= ~TREEV_NEED_REARRANGE;
}


/* Top-level call to arrange the branches of the currently expanded tree,
 * as needed when directories collapse/expand (initial_arrange == FALSE),
 * or when tree is initially created (initial_arrange == TRUE) */
static void
treev_arrange( boolean initial_arrange )
{
	boolean resized = FALSE;

	treev_arrange_recursive( globals.fstree, treev_core_radius, initial_arrange );

	/* Check that the tree's total arc width is within bounds */
	for (;;) {
		if (TREEV_GEOM_PARAMS(globals.fstree)->platform.subtree_arc_width > TREEV_MAX_ARC_WIDTH) {
			/* Grow core radius */
			treev_core_radius *= TREEV_CORE_GROW_FACTOR;
			treev_arrange_recursive( globals.fstree, treev_core_radius, TRUE );
			resized = TRUE;
		}
		else if ((TREEV_GEOM_PARAMS(globals.fstree)->platform.subtree_arc_width < TREEV_MIN_ARC_WIDTH) && (TREEV_GEOM_PARAMS(globals.fstree)->platform.depth > TREEV_MIN_CORE_RADIUS)) {
			/* Shrink core radius */
			treev_core_radius = MAX(TREEV_MIN_CORE_RADIUS, treev_core_radius / TREEV_CORE_GROW_FACTOR);
			treev_arrange_recursive( globals.fstree, treev_core_radius, TRUE );
			resized = TRUE;
		}
		else
			break;
	}

	if (resized && camera_moving( )) {
		/* Camera's destination has moved, so it will need a
		 * flight path correction */
		camera_pan_break( );
		camera_look_at_full( globals.current_node, MORPH_INV_QUADRATIC, -1.0 );
	}
}


/* Helper function for treev_init( ) */
static void
treev_init_recursive( GNode *dnode )
{
	GNode *node;
	int64 size;

	g_assert( NODE_IS_DIR(dnode) || NODE_IS_METANODE(dnode) );

	if (NODE_IS_DIR(dnode)) {
		morph_break( &DIR_NODE_DESC(dnode)->deployment );
		if (dirtree_entry_expanded( dnode ))
			DIR_NODE_DESC(dnode)->deployment = 1.0;
		else
			DIR_NODE_DESC(dnode)->deployment = 0.0;
		geometry_queue_rebuild( dnode );
	}

	NODE_DESC(dnode)->flags = 0;

	/* Assign heights to leaf nodes */
	node = dnode->children;
	while (node != NULL) {
		size = MAX(64, NODE_DESC(node)->size);
		if (NODE_IS_DIR(node)) {
			size += DIR_NODE_DESC(node)->subtree.size;
			TREEV_GEOM_PARAMS(node)->platform.height = TREEV_PLATFORM_HEIGHT;
			TREEV_GEOM_PARAMS(node)->platform.arc_width = TREEV_MIN_ARC_WIDTH;
			TREEV_GEOM_PARAMS(node)->platform.subtree_arc_width = TREEV_MIN_ARC_WIDTH;
			treev_init_recursive( node );
		}
		TREEV_GEOM_PARAMS(node)->leaf.height = sqrt( (double)size ) * TREEV_LEAF_HEIGHT_MULTIPLIER;
		node = node->next;
	}
}


/* Top-level call to initialize TreeV mode */
static void
treev_init( void )
{
	TreeVGeomParams *gparams;
	int num_points;

	/* Allocate point buffers */
	num_points = (int)ceil( 360.0 / TREEV_CURVE_GRANULARITY ) + 1;
	if (inner_edge_buf == NULL)
		inner_edge_buf = NEW_ARRAY(XYvec, num_points);
	if (outer_edge_buf == NULL)
		outer_edge_buf = NEW_ARRAY(XYvec, num_points);

	treev_core_radius = TREEV_MIN_CORE_RADIUS;

	gparams = TREEV_GEOM_PARAMS(globals.fstree);
	gparams->platform.theta = 90.0;
	gparams->platform.depth = 0.0;
	gparams->platform.arc_width = TREEV_MAX_ARC_WIDTH;
	gparams->platform.height = 0.0;

	gparams = TREEV_GEOM_PARAMS(root_dnode);
	gparams->leaf.theta = 0.0;
	gparams->leaf.distance = (0.5 * TREEV_PLATFORM_SPACING_DEPTH);
	gparams->platform.theta = 0.0;

	treev_init_recursive( globals.fstree );
	treev_arrange( TRUE );

	/* Initial cursor state */
	treev_get_corners( root_dnode, &treev_cursor_prev_c0, &treev_cursor_prev_c1 );
	treev_cursor_prev_c0.r *= 0.875;
	treev_cursor_prev_c0.theta -= TREEV_GEOM_PARAMS(root_dnode)->platform.arc_width;
        treev_cursor_prev_c0.z = 0.0;
	treev_cursor_prev_c1.r *= 1.125;
	treev_cursor_prev_c1.theta += TREEV_GEOM_PARAMS(root_dnode)->platform.arc_width;
	treev_cursor_prev_c1.z = TREEV_GEOM_PARAMS(root_dnode)->platform.height;
}


/* Hook function for camera pan completion */
static void
treev_camera_pan_finished( void )
{
	/* Save cursor position */
	treev_get_corners( globals.current_node, &treev_cursor_prev_c0, &treev_cursor_prev_c1 );
}


/* Called by a directory as it collapses or expands (from the morph's step
 * callback; see colexp.c). This sets all the necessary flags to allow the
 * directory to move side to side without problems */
static void
treev_queue_rearrange( GNode *dnode )
{
	GNode *up_node;

	g_assert( NODE_IS_DIR(dnode) );

	up_node = dnode;
	while (up_node != NULL) {
		NODE_DESC(up_node)->flags |= TREEV_NEED_REARRANGE;

		// TODO: Invalidate uploaded VBO's

		up_node = up_node->parent;
	}

	queue_uncached_draw( );
}


/* Draws a directory platform, with inner radius of r0 */
static void
treev_gldraw_platform( GNode *dnode, double r0 )
{
	XYvec p0, p1;
	XYvec delta;
	double r1, seg_arc_width;
	double theta, sin_theta, cos_theta;
	double z1;
	int s, seg_count;

	g_assert( NODE_IS_DIR(dnode) );

	r1 = r0 + TREEV_GEOM_PARAMS(dnode)->platform.depth;
	seg_count = (int)ceil( TREEV_GEOM_PARAMS(dnode)->platform.arc_width / TREEV_CURVE_GRANULARITY );
	seg_arc_width = TREEV_GEOM_PARAMS(dnode)->platform.arc_width / (double)seg_count;

	/* Calculate and cache inner/outer edge vertices */
	theta = -0.5 * TREEV_GEOM_PARAMS(dnode)->platform.arc_width;
	for (s = 0; s <= seg_count; s++) {
		sin_theta = sin( RAD(theta) );
		cos_theta = cos( RAD(theta) );
		/* p0: point on inner edge */
		p0.x = r0 * cos_theta;
		p0.y = r0 * sin_theta;
		/* p1: point on outer edge */
		p1.x = r1 * cos_theta;
		p1.y = r1 * sin_theta;
		if (s == 0) {
			/* Leading edge offset */
			delta.x = - sin_theta * (0.5 * TREEV_PLATFORM_SPACING_WIDTH);
			delta.y = cos_theta * (0.5 * TREEV_PLATFORM_SPACING_WIDTH);
			p0.x += delta.x;
			p0.y += delta.y;
			p1.x += delta.x;
			p1.y += delta.y;
		}
		else if (s == seg_count) {
			/* Trailing edge offset */
			delta.x = sin_theta * (0.5 * TREEV_PLATFORM_SPACING_WIDTH);
			delta.y = - cos_theta * (0.5 * TREEV_PLATFORM_SPACING_WIDTH);
			p0.x += delta.x;
			p0.y += delta.y;
			p1.x += delta.x;
			p1.y += delta.y;
		}

		/* cache */
		inner_edge_buf[s].x = p0.x;
		inner_edge_buf[s].y = p0.y;
		outer_edge_buf[s].x = p1.x;
		outer_edge_buf[s].y = p1.y;

		theta += seg_arc_width;
	}

	/* Height of top face */
        z1 = TREEV_GEOM_PARAMS(dnode)->platform.height;

	size_t vert_cnt = seg_count * (8 + 4) + 8;
	size_t idx_len = 0;
	Vertex *vert = NEW_ARRAY(Vertex, vert_cnt);
	GLushort *idx = NEW_ARRAY(GLushort, vert_cnt * 2);

	/* Draw inner edge */
	for (s = 0; s < seg_count; s++) {
		/* Going up */
		p0.x = inner_edge_buf[s].x;
		p0.y = inner_edge_buf[s].y;
		vert[s * 4] = (Vertex){{p0.x, p0.y, 0}, {-p0.x / r0, -p0.y / r0, 0}};
		vert[s * 4 + 1] = (Vertex){{p0.x, p0.y, z1}, {-p0.x / r0, -p0.y / r0, 0}};

		/* Going down */
		p0.x = inner_edge_buf[s + 1].x;
		p0.y = inner_edge_buf[s + 1].y;
		vert[s * 4 + 2] = (Vertex){{p0.x, p0.y, z1}, {-p0.x / r0, -p0.y / r0, 0}};
		vert[s * 4 + 3] = (Vertex){{p0.x, p0.y, 0}, {-p0.x / r0, -p0.y / r0, 0}};
		idx[idx_len++] = s * 4;
		idx[idx_len++] = s * 4 + 1;
		idx[idx_len++] = s * 4 + 2;
		idx[idx_len++] = s * 4;
		idx[idx_len++] = s * 4 + 2;
		idx[idx_len++] = s * 4 + 3;
	}

	/* Draw outer edge */
	for (s = seg_count; s > 0; s--) {
		/* Going up */
		size_t s2 = (seg_count + seg_count - s) * 4;
		p1.x = outer_edge_buf[s].x;
		p1.y = outer_edge_buf[s].y;
		vert[s2] = (Vertex){{p1.x, p1.y, 0}, {-p1.x / r1, -p1.y / r1, 0}};
		vert[s2 + 1] = (Vertex){{p1.x, p1.y, z1}, {-p1.x / r1, -p1.y / r1, 0}};

		/* Going down */
		p1.x = outer_edge_buf[s - 1].x;
		p1.y = outer_edge_buf[s - 1].y;
		vert[s2 + 2] = (Vertex){{p1.x, p1.y, z1}, {-p1.x / r1, -p1.y / r1, 0}};
		vert[s2 + 3] = (Vertex){{p1.x, p1.y, 0}, {-p1.x / r1, -p1.y / r1, 0}};
		idx[idx_len++] = s2;
		idx[idx_len++] = s2 + 1;
		idx[idx_len++] = s2 + 2;
		idx[idx_len++] = s2;
		idx[idx_len++] = s2 + 2;
		idx[idx_len++] = s2 + 3;
	}

	/* Draw leading edge face */
	p0.x = inner_edge_buf[0].x;
	p0.y = inner_edge_buf[0].y;
	p1.x = outer_edge_buf[0].x;
	p1.y = outer_edge_buf[0].y;
	size_t s2 = seg_count * 2 * 4;
	vert[s2] = (Vertex){{p0.x, p0.y, 0}, {p0.y / r0, -p0.x / r0, 0}};
	vert[s2 + 1] = (Vertex){{p1.x, p1.y, 0}, {p0.y / r0, -p0.x / r0, 0}};
	vert[s2 + 2] = (Vertex){{p1.x, p1.y, z1}, {p0.y / r0, -p0.x / r0, 0}};
	vert[s2 + 3] = (Vertex){{p0.x, p0.y, z1}, {p0.y / r0, -p0.x / r0, 0}};
	idx[idx_len++] = s2;
	idx[idx_len++] = s2 + 1;
	idx[idx_len++] = s2 + 2;
	idx[idx_len++] = s2;
	idx[idx_len++] = s2 + 2;
	idx[idx_len++] = s2 + 3;
	s2 += 4;

	/* Draw trailing edge face */
	p0.x = inner_edge_buf[seg_count].x;
	p0.y = inner_edge_buf[seg_count].y;
	p1.x = outer_edge_buf[seg_count].x;
	p1.y = outer_edge_buf[seg_count].y;
	vert[s2] = (Vertex){{p0.x, p0.y, z1}, {-p0.y / r0, p0.x / r0, 0}};
	vert[s2 + 1] = (Vertex){{p1.x, p1.y, z1}, {-p0.y / r0, p0.x / r0, 0}};
	vert[s2 + 2] = (Vertex){{p1.x, p1.y, 0}, {-p0.y / r0, p0.x / r0, 0}};
	vert[s2 + 3] = (Vertex){{p0.x, p0.y, 0}, {-p0.y / r0, p0.x / r0, 0}};
	idx[idx_len++] = s2;
	idx[idx_len++] = s2 + 1;
	idx[idx_len++] = s2 + 2;
	idx[idx_len++] = s2;
	idx[idx_len++] = s2 + 2;
	idx[idx_len++] = s2 + 3;
	s2 += 4;

	/* Draw top face */
	for (s = 0; s < seg_count; s++) {
		/* Going out */
		size_t s3 = s2 + s * 4;
		p0.x = inner_edge_buf[s].x;
		p0.y = inner_edge_buf[s].y;
		p1.x = outer_edge_buf[s].x;
		p1.y = outer_edge_buf[s].y;
		vert[s3] = (Vertex){{p0.x, p0.y, z1}, {0, 0, 1}};
		vert[s3 + 1] = (Vertex){{p1.x, p1.y, z1}, {0, 0, 1}};


		/* Going in */
		p0.x = inner_edge_buf[s + 1].x;
		p0.y = inner_edge_buf[s + 1].y;
		p1.x = outer_edge_buf[s + 1].x;
		p1.y = outer_edge_buf[s + 1].y;
		vert[s3 + 2] = (Vertex){{p1.x, p1.y, z1}, {0, 0, 1}};
		vert[s3 + 3] = (Vertex){{p0.x, p0.y, z1}, {0, 0, 1}};
		idx[idx_len++] = s3;
		idx[idx_len++] = s3 + 1;
		idx[idx_len++] = s3 + 2;
		idx[idx_len++] = s3;
		idx[idx_len++] = s3 + 2;
		idx[idx_len++] = s3 + 3;
	}

	g_assert(s2 + (seg_count - 1) * 4 + 3 < vert_cnt);
	g_assert(idx_len <= vert_cnt * 2);

	static GLuint vbo;
	if (!vbo)
		glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vert_cnt, vert, GL_DYNAMIC_DRAW);

	static GLuint ebo;
	if (!ebo)
		glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * idx_len, idx, GL_DYNAMIC_DRAW);

	glEnableVertexAttribArray(gl.position_location);
	glVertexAttribPointer(gl.position_location, 3, GL_FLOAT, GL_FALSE,
			      sizeof(Vertex), (void *)offsetof(Vertex, position));

	glEnableVertexAttribArray(gl.normal_location);
	glVertexAttribPointer(gl.normal_location, 3, GL_FLOAT, GL_FALSE,
			      sizeof(Vertex), (void *)offsetof(Vertex, normal));

	glUseProgram(gl.program);

	node_set_color(dnode);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(GL_TRIANGLES, idx_len, GL_UNSIGNED_SHORT, 0);

	glUseProgram(0);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vert_cnt, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	xfree(vert);
	xfree(idx);
}


/* Draws a leaf node. r0 is inner radius of parent; full_node flag
 * specifies whether the full leaf body should be drawn (TRUE) or merely
 * its "footprint" (FALSE). Note: Transformation matrix should be the same
 * one used to draw the underlying parent directory */
static void
treev_gldraw_leaf( GNode *node, double r0, boolean full_node )
{
	static const int x_verts[] = { 0, 2, 1, 3 };
	XYvec corners[4], p;
	double z0, z1;
	double edge, height;
	double sin_theta, cos_theta;
	int i;

	if (full_node) {
		edge = TREEV_LEAF_NODE_EDGE;
		height = TREEV_GEOM_PARAMS(node)->leaf.height;
		if (NODE_IS_DIR(node))
			height *= (1.0 - DIR_NODE_DESC(node)->deployment);
	}
	else {
		edge = (0.875 * TREEV_LEAF_NODE_EDGE);
		height = (TREEV_LEAF_NODE_EDGE / 64.0);
	}

	/* Set up corners, centered around (r0+distance,0,0) */

	/* Left/front */
	corners[0].x = r0 + TREEV_GEOM_PARAMS(node)->leaf.distance - 0.5 * edge;
	corners[0].y = -0.5 * edge;

	/* Right/front */
	corners[1].x = corners[0].x + edge;
	corners[1].y = corners[0].y;

	/* Right/rear */
	corners[2].x = corners[1].x;
	corners[2].y = corners[0].y + edge;

	/* Left/rear */
	corners[3].x = corners[0].x;
	corners[3].y = corners[2].y;

	/* Bottom and top */
	z0 = TREEV_GEOM_PARAMS(node->parent)->platform.height;
	z1 = z0 + height;

	sin_theta = sin( RAD(TREEV_GEOM_PARAMS(node)->leaf.theta) );
	cos_theta = cos( RAD(TREEV_GEOM_PARAMS(node)->leaf.theta) );

	/* Rotate corners into position (no glRotated( )-- leaf nodes are
	 * not important enough to mess with the transformation matrix) */
	for (i = 0; i < 4; i++) {
		p.x = corners[i].x;
		p.y = corners[i].y;
		corners[i].x = p.x * cos_theta - p.y * sin_theta;
		corners[i].y = p.x * sin_theta + p.y * cos_theta;
	}

	/* Draw top face */
	// Note order of vertices for triangle stripping.
	Vertex vert[] = {
		{{corners[0].x, corners[0].y, z1}, {0, 0, 1}},
		{{corners[1].x, corners[1].y, z1}, {0, 0, 1}},
		{{corners[3].x, corners[3].y, z1}, {0, 0, 1}},
		{{corners[2].x, corners[2].y, z1}, {0, 0, 1}},
	};
	drawVertex(GL_TRIANGLE_STRIP, vert, 4, NULL, node);

	if (!full_node) {
		/* Draw an "X" and we're done */
		VertexPos vertx[4];
		for (i = 0; i < 4; i++)
			vertx[i] = (VertexPos){{corners[x_verts[i]].x, corners[x_verts[i]].y, z1}};
		drawVertexPos(GL_LINES, vertx, 4, &color_black);
		return;
	}

	/* Draw side faces */
	Vertex vside[] = {
	    // Front face
	    {{corners[0].x, corners[0].y, z1}, {sin_theta, -cos_theta, 0}},
	    {{corners[0].x, corners[0].y, z0}, {sin_theta, -cos_theta, 0}},
	    {{corners[1].x, corners[1].y, z1}, {sin_theta, -cos_theta, 0}},
	    {{corners[1].x, corners[1].y, z0}, {sin_theta, -cos_theta, 0}},
	    // Right
	    {{corners[1].x, corners[1].y, z1}, {cos_theta, sin_theta, 0}},
	    {{corners[1].x, corners[1].y, z0}, {cos_theta, sin_theta, 0}},
	    {{corners[2].x, corners[2].y, z1}, {cos_theta, sin_theta, 0}},
	    {{corners[2].x, corners[2].y, z0}, {cos_theta, sin_theta, 0}},
	    // Back
	    {{corners[2].x, corners[2].y, z1}, {-sin_theta, cos_theta, 0}},
	    {{corners[2].x, corners[2].y, z0}, {-sin_theta, cos_theta, 0}},
	    {{corners[3].x, corners[3].y, z1}, {-sin_theta, cos_theta, 0}},
	    {{corners[3].x, corners[3].y, z0}, {-sin_theta, cos_theta, 0}},
	    // Left
	    {{corners[3].x, corners[3].y, z1}, {-cos_theta, -sin_theta, 0}},
	    {{corners[3].x, corners[3].y, z0}, {-cos_theta, -sin_theta, 0}},
	    {{corners[0].x, corners[0].y, z1}, {-cos_theta, -sin_theta, 0}},
	    {{corners[0].x, corners[0].y, z0}, {-cos_theta, -sin_theta, 0}},
	};
	static const GLushort elems[] = {
	    0,	1,  2,	2,  1,	3,   // Front
	    4,	5,  6,	6,  5,	7,   // Right
	    8,	9,  10, 10, 9,	11,  // Back
	    12, 13, 14, 14, 13, 15   // Left
	};
	static GLuint vbo;
	if (!vbo)
		glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vside), &vside, GL_DYNAMIC_DRAW);

	static GLuint ebo;
	if (!ebo) {
		glGenBuffers(1, &ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elems), &elems, GL_STATIC_DRAW);
	}

	glEnableVertexAttribArray(gl.position_location);
	glVertexAttribPointer(gl.position_location, 3, GL_FLOAT, GL_FALSE,
			      sizeof(Vertex), (void *)offsetof(Vertex, position));

	glEnableVertexAttribArray(gl.normal_location);
	glVertexAttribPointer(gl.normal_location, 3, GL_FLOAT, GL_FALSE,
			      sizeof(Vertex), (void *)offsetof(Vertex, normal));

	glUseProgram(gl.program);

	node_set_color(node);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	GLsizei cnt = sizeof(elems) / sizeof(GLushort);
	glDrawElements(GL_TRIANGLES, cnt, GL_UNSIGNED_SHORT, 0);

	glUseProgram(0);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vside), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}


/* Draws a "folder" shape on top of the given directory leaf node */
static void
treev_gldraw_folder( GNode *dnode, double r0 )
{
#define X1 (-0.4375 * TREEV_LEAF_NODE_EDGE)
#define X2 (0.375 * TREEV_LEAF_NODE_EDGE)
#define X3 (0.4375 * TREEV_LEAF_NODE_EDGE)
#define Y1 (-0.4375 * TREEV_LEAF_NODE_EDGE)
#define Y2 (Y1 + (2.0 - MAGIC_NUMBER) * TREEV_LEAF_NODE_EDGE)
#define Y3 (Y2 + 0.0625 * TREEV_LEAF_NODE_EDGE)
#define Y4 (Y5 - 0.0625 * TREEV_LEAF_NODE_EDGE)
#define Y5 (0.4375 * TREEV_LEAF_NODE_EDGE)
	static const XYvec folder_points[] = {
		{ X1, Y1 },
		{ X2, Y1 },
		{ X2, Y2 },
		{ X3, Y3 },
		{ X3, Y4 },
		{ X2, Y5 },
		{ X1, Y5 }
	};
#undef X1
#undef X2
#undef X3
#undef Y1
#undef Y2
#undef Y3
#undef Y4
#undef Y5
	XYZvec p_rot;
	XYvec p;
	double folder_r;
	double sin_theta, cos_theta;
	int i;

	g_assert( NODE_IS_DIR(dnode) );

	folder_r = r0 + TREEV_GEOM_PARAMS(dnode)->leaf.distance;
	sin_theta = sin( RAD(TREEV_GEOM_PARAMS(dnode)->leaf.theta) );
	cos_theta = cos( RAD(TREEV_GEOM_PARAMS(dnode)->leaf.theta) );
	p_rot.z = (1.0 - DIR_NODE_DESC(dnode)->deployment) * TREEV_GEOM_PARAMS(dnode)->leaf.height + TREEV_GEOM_PARAMS(dnode->parent)->platform.height;

	/* Translate, rotate, and draw folder geometry */
	VertexPos vert[8];
	for (i = 0; i <= 7; i++) {
		p.x = folder_r + folder_points[i % 7].x;
		p.y = folder_points[i % 7].y;
		p_rot.x = p.x * cos_theta - p.y * sin_theta;
		p_rot.y = p.x * sin_theta + p.y * cos_theta;

		vert[i] = (VertexPos){{p_rot.x, p_rot.y, p_rot.z}};
	}

	drawVertexPos(GL_LINE_STRIP, vert, 8, &color_black);
}


/* Draws the loop around the TreeV center, with the given radius */
static void
treev_gldraw_loop( double loop_r )
{
	static const int seg_count = (int)(360.0 / TREEV_CURVE_GRANULARITY + 0.5);
	XYvec p0, p1;
	double loop_r0, loop_r1;
	double theta, sin_theta, cos_theta;
	int s;

	/* Inner/outer loop radii */
	loop_r0 = loop_r - (0.5 * TREEV_BRANCH_WIDTH);
	loop_r1 = loop_r + (0.5 * TREEV_BRANCH_WIDTH);

	/* Draw loop */
	static size_t vert_cnt = (seg_count + 1) * 2;
	Vertex *vert = NEW_ARRAY(Vertex, vert_cnt);
	for (s = 0; s <= seg_count; s++) {
		theta = 360.0 * (double)s / (double)seg_count;
		sin_theta = sin( RAD(theta) );
		cos_theta = cos( RAD(theta) );
		/* p0: point on inner edge */
		p0.x = loop_r0 * cos_theta;
		p0.y = loop_r0 * sin_theta;
		/* p1: point on outer edge */
		p1.x = loop_r1 * cos_theta;
		p1.y = loop_r1 * sin_theta;

		vert[2 * s] = (Vertex){{p0.x, p0.y, 0}, {0, 0, 1}};
		vert[2 * s + 1] = (Vertex){{p1.x, p1.y, 0}, {0, 0, 1}};
	}
	drawVertex(GL_TRIANGLE_STRIP, vert, vert_cnt, &branch_color, NULL);
	xfree(vert);
}


/* Draws part of the branch connecting to the inner edge of a platform.
 * r0 is the platform's inner radius */
static void
treev_gldraw_inbranch( double r0 )
{
	XYvec c0, c1;

	/* Left/front */
	c0.x = r0 - (0.5 * TREEV_PLATFORM_SPACING_DEPTH);
	c0.y = (-0.5 * TREEV_BRANCH_WIDTH);

	/* Right/rear */
	c1.x = r0;
	c1.y = (0.5 * TREEV_BRANCH_WIDTH);

	Vertex vert[] = {
		{{c0.x, c0.y, 0}, {0, 0, 1}},
		{{c1.x, c0.y, 0}, {0, 0, 1}},
		{{c0.x, c1.y, 0}, {0, 0, 1}},
		{{c1.x, c1.y, 0}, {0, 0, 1}},
	};
	drawVertex(GL_TRIANGLE_STRIP, vert, 4, &branch_color, NULL);
}


/* Draws part of the branch present on the outer edge of platforms with
 * expanded subdirectories. r1 is the outer radius of the parent directory,
 * and theta0/theta1 are the start/end angles of the arc portion */
static void
treev_gldraw_outbranch( double r1, double theta0, double theta1 )
{
	XYvec p0, p1;
	double arc_r, arc_r0, arc_r1;
	double arc_width, seg_arc_width;
	double supp_arc_width;
	double theta, sin_theta, cos_theta;
	int s, seg_count;

	g_assert( theta1 >= theta0 );

	/* Radii of branch arc (middle, inner, outer) */
	arc_r = r1 + (0.5 * TREEV_PLATFORM_SPACING_DEPTH);
	arc_r0 = arc_r - (0.5 * TREEV_BRANCH_WIDTH);
	arc_r1 = arc_r + (0.5 * TREEV_BRANCH_WIDTH);

	/* Left/front of stem */
	p0.x = r1;
	p0.y = (-0.5 * TREEV_BRANCH_WIDTH);

	/* Right/rear of stem */
	p1.x = arc_r;
	p1.y = (0.5 * TREEV_BRANCH_WIDTH);

	arc_width = theta1 - theta0;

	/* Supplemental arc width, to yield fully square branch corners
	 * (where directories connect to the ends of the arc) */
	supp_arc_width = (180.0 * TREEV_BRANCH_WIDTH / PI) / arc_r0;

	seg_count = (int)ceil( (arc_width + supp_arc_width) / TREEV_CURVE_GRANULARITY );
	seg_arc_width = (arc_width + supp_arc_width) / (double)seg_count;

	const size_t vert_cnt = 4 + (seg_count + 1) * 2;
	Vertex *vert = NEW_ARRAY(Vertex, vert_cnt);
	/* Branch stem */
	vert[0] = (Vertex){{p0.x, p0.y, 0}, {0, 0, 1}};
	vert[1] = (Vertex){{p1.x, p0.y, 0}, {0, 0, 1}};
	vert[3] = (Vertex){{p1.x, p1.y, 0}, {0, 0, 1}};
	vert[2] = (Vertex){{p0.x, p1.y, 0}, {0, 0, 1}};

	/* Draw branch arc */
	theta = theta0 - 0.5 * supp_arc_width;
	for (s = 0; s <= seg_count; s++) {
		sin_theta = sin( RAD(theta) );
		cos_theta = cos( RAD(theta) );
		/* p0: point on inner edge */
		p0.x = arc_r0 * cos_theta;
		p0.y = arc_r0 * sin_theta;
		/* p1: point on outer edge */
		p1.x = arc_r1 * cos_theta;
		p1.y = arc_r1 * sin_theta;

		vert[4 + s * 2] = (Vertex){{p0.x, p0.y, 0}, {0, 0, 1}};
		vert[4 + s * 2 + 1] = (Vertex){{p1.x, p1.y, 0}, {0, 0, 1}};

		theta += seg_arc_width;
	}
	drawVertex(GL_TRIANGLE_STRIP, vert, vert_cnt, &branch_color, NULL);
	xfree(vert);
}


/* Arranges/draws leaf nodes on a directory */
static void
treev_build_dir( GNode *dnode, double r0 )
{
#define edge05 (0.5 * TREEV_LEAF_NODE_EDGE)
#define edge15 (1.5 * TREEV_LEAF_NODE_EDGE)
	GNode *node;
	RTvec pos;
	double arc_len, inter_arc_width;
	int n, row_node_count, remaining_node_count;

	g_assert( NODE_IS_DIR(dnode) );

	/* Build rows of leaf nodes, going from the inner edge outward
	 * (this will require laying down nodes in reverse order) */
	remaining_node_count = g_list_length( (GList *)dnode->children );
	pos.r = r0 + TREEV_LEAF_NODE_EDGE;
	node = (GNode *)g_list_last( (GList *)dnode->children );
	while (node != NULL) {
		/* Calculate (available) arc length of row */
		arc_len = (PI / 180.0) * pos.r * TREEV_GEOM_PARAMS(dnode)->platform.arc_width - TREEV_PLATFORM_SPACING_WIDTH;
		/* Number of nodes this row can accomodate */
		row_node_count = (int)floor( (arc_len - edge05) / edge15 );
		/* Arc width between adjacent leaf nodes */
		inter_arc_width = (180.0 * edge15 / PI) / pos.r;

		/* Lay out nodes in this row, sweeping clockwise */
		pos.theta = 0.5 * inter_arc_width * (double)(MIN(row_node_count, remaining_node_count) - 1);
		for (n = 0; (n < row_node_count) && (node != NULL); n++) {
			TREEV_GEOM_PARAMS(node)->leaf.theta = pos.theta;
			TREEV_GEOM_PARAMS(node)->leaf.distance = pos.r - r0;
			treev_gldraw_leaf( node, r0, !NODE_IS_DIR(node) );
			pos.theta -= inter_arc_width;
			node = node->prev;
		}

		remaining_node_count -= row_node_count;
		pos.r += edge15;
	}

	/* Official directory depth */
	pos.r -= edge05;
	TREEV_GEOM_PARAMS(dnode)->platform.depth = pos.r - r0;

	/* Draw underlying directory */
	treev_gldraw_platform( dnode, r0 );

#undef edge05
#undef edge15
}


/* Draws a node name label. is_leaf indicates whether the given node should
 * be labeled as a leaf, or as a directory platform (if applicable) */
static void
treev_apply_label( GNode *node, double r0, boolean is_leaf )
{
	RTZvec label_pos;
	XYvec leaf_label_dims;
	RTvec platform_label_dims;
	double height;

	if (is_leaf) {
		/* Apply label to top face of leaf node */
		height = TREEV_GEOM_PARAMS(node)->leaf.height;
		if (NODE_IS_DIR(node)) {
			height *= (1.0 - DIR_NODE_DESC(node)->deployment);
			leaf_label_dims.x = (0.8125 * TREEV_LEAF_NODE_EDGE);
		}
		else
			leaf_label_dims.x = (0.875 * TREEV_LEAF_NODE_EDGE);
		leaf_label_dims.y = ((2.0 - MAGIC_NUMBER) * TREEV_LEAF_NODE_EDGE);
		label_pos.r = r0 + TREEV_GEOM_PARAMS(node)->leaf.distance;
		label_pos.theta = TREEV_GEOM_PARAMS(node)->leaf.theta;
		label_pos.z = height + TREEV_GEOM_PARAMS(node->parent)->platform.height;
		text_draw_straight_rotated( NODE_DESC(node)->name, &label_pos, &leaf_label_dims );
	}
	else {
		/* Label directory platform, inside its inner edge */
		label_pos.r = r0 - (0.0625 * TREEV_PLATFORM_SPACING_DEPTH);
		label_pos.theta = 0.0;
		label_pos.z = 0.0;
		platform_label_dims.r = ((2.0 - MAGIC_NUMBER) * TREEV_PLATFORM_SPACING_DEPTH);
		platform_label_dims.theta = TREEV_GEOM_PARAMS(node)->platform.arc_width - (180.0 * TREEV_PLATFORM_SPACING_WIDTH / PI) / label_pos.r;
		text_draw_curved( NODE_DESC(node)->name, &label_pos, &platform_label_dims );
	}
}


/* TreeV mode "full draw" */
static boolean
treev_draw_recursive( GNode *dnode, double prev_r0, double r0, int action )
{
	DirNodeDesc *dir_ndesc;
	TreeVGeomParams *dir_gparams;
	GNode *node;
	GNode *first_node = NULL, *last_node = NULL;
	RTvec leaf;
	double subtree_r0;
	double theta0, theta1;
	boolean dir_collapsed;
	boolean dir_expanded;

	g_assert( NODE_IS_DIR(dnode) || NODE_IS_METANODE(dnode) );
	dir_ndesc = DIR_NODE_DESC(dnode);
	dir_gparams = TREEV_GEOM_PARAMS(dnode);

	mat4 tmpmat;
	glm_mat4_copy(gl.modelview, tmpmat);
	//debug_print_matrices(1);

	dir_collapsed = DIR_COLLAPSED(dnode);
        dir_expanded = DIR_EXPANDED(dnode);

	if (!dir_collapsed) {
		if (!dir_expanded) {
			/* Directory is partially deployed, so
			 * draw/label the shrinking/growing leaf */
			if (action >= TREEV_DRAW_GEOMETRY) {
				treev_gldraw_leaf( dnode, prev_r0, TRUE );
				treev_gldraw_folder( dnode, prev_r0 );
			}
			else if (action == TREEV_DRAW_LABELS) {
				text_set_color(treev_leaf_label_color.r,
					       treev_leaf_label_color.g,
					       treev_leaf_label_color.b);
				treev_apply_label( dnode, prev_r0, TRUE );
			}

			/* Platform should shrink to / grow from
			 * corresponding leaf position */
			leaf.r = prev_r0 + dir_gparams->leaf.distance;
			leaf.theta = dir_gparams->leaf.theta;
			glm_rotate_z(gl.modelview, leaf.theta * M_PI/180.0, gl.modelview);
			glm_translate(gl.modelview, (vec3){leaf.r, 0.0, 0.0});
			glm_scale(gl.modelview, (vec3){dir_ndesc->deployment, dir_ndesc->deployment, dir_ndesc->deployment});
			glm_translate(gl.modelview, (vec3){-leaf.r, 0.0, 0.0});
			glm_rotate_z(gl.modelview, -leaf.theta * M_PI/180.0, gl.modelview);
		}

		glm_rotate_z(gl.modelview, dir_gparams->platform.theta * M_PI / 180.0, gl.modelview);
		ogl_upload_matrices(TRUE);
	}

	if (action >= TREEV_DRAW_GEOMETRY) {
		/* Draw directory, in either leaf or platform form
		 */
		if (dir_collapsed)
		{
			/* Leaf form */
			treev_gldraw_leaf(dnode, prev_r0, TRUE);
			treev_gldraw_folder(dnode, prev_r0);
		}
		else if (NODE_IS_DIR(dnode))
		{
			/* Platform form (with leaf children) */
			treev_build_dir(dnode, r0);
		}
	}

	if (!dir_collapsed) {
		/* Recurse into subdirectories */
		subtree_r0 = r0 + dir_gparams->platform.depth + TREEV_PLATFORM_SPACING_DEPTH;
		node = dnode->children;
		while (node != NULL) {
			if (!NODE_IS_DIR(node))
				break;
			if (treev_draw_recursive( node, r0, subtree_r0, action )) {
				/* This subdirectory is expanded.
				 * Save first/last node information for
				 * drawing interconnecting branches */
				if (first_node == NULL)
					first_node = node;
				last_node = node;
			}
			node = node->next;
		}
	}

	if (dir_expanded && (action == TREEV_DRAW_GEOMETRY_WITH_BRANCHES)) {
		/* Draw interconnecting branches */
		if (NODE_IS_METANODE(dnode))
		{
			treev_gldraw_loop(r0);
			treev_gldraw_outbranch(r0, 0.0, 0.0);
		}
		else
		{
			treev_gldraw_inbranch(r0);
			if (first_node != NULL)
			{
				theta0 = MIN(0.0, TREEV_GEOM_PARAMS(first_node)->platform.theta);
				theta1 = MAX(0.0, TREEV_GEOM_PARAMS(last_node)->platform.theta);
				treev_gldraw_outbranch(r0 + dir_gparams->platform.depth, theta0, theta1);
			}
		}
	}

	if (action == TREEV_DRAW_LABELS) {
		/* Draw name label(s) */
		if (dir_collapsed)
		{
			/* Label directory leaf */
			text_set_color(treev_leaf_label_color.r,
				       treev_leaf_label_color.g,
				       treev_leaf_label_color.b);
			treev_apply_label(dnode, prev_r0, TRUE);
		}
		else if (NODE_IS_DIR(dnode))
		{
			/* Label directory platform */
			text_set_color(treev_platform_label_color.r,
				       treev_platform_label_color.g,
				       treev_platform_label_color.b);
			treev_apply_label(dnode, r0, FALSE);
			/* Label leaf nodes that aren't directories */
			text_set_color(treev_leaf_label_color.r,
				       treev_leaf_label_color.g,
				       treev_leaf_label_color.b);
			node = dnode->children;
			while (node != NULL)
			{
				if (!NODE_IS_DIR(node))
					treev_apply_label(node, r0, TRUE);
				node = node->next;
			}
		}
	}

	/* Update geometry status */
	dir_ndesc->geom_expanded = !dir_collapsed;

	if (!dir_collapsed) {
		glm_mat4_copy(tmpmat, gl.modelview);
		ogl_upload_matrices(FALSE);
	}

	return dir_expanded;
}


/* Draws the node cursor, size/position specified by corners */
static void
treev_gldraw_cursor( RTZvec *c0, RTZvec *c1 )
{
	static const double bar_part = SQR(SQR(MAGIC_NUMBER - 1.0));
	RTZvec corner_dims;
	RTZvec p, delta;
	XYvec cp0, cp1;
	double theta;
	double sin_theta, cos_theta;
	int seg_count;
	int i, c, s;

	g_assert( c1->r > c0->r );
	g_assert( c1->theta > c0->theta );
	g_assert( c1->z > c0->z );

	corner_dims.r = bar_part * (c1->r - c0->r);
	corner_dims.theta = bar_part * (c1->theta - c0->theta);
	corner_dims.z = bar_part * (c1->z - c0->z);

	seg_count = (int)ceil( corner_dims.theta / TREEV_CURVE_GRANULARITY );

	cursor_pre( );
	static GLuint vbo;
	if (!vbo) glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	for (i = 0; i <= 1; i++) {
		if (i == 0)
			cursor_hidden_part( );
		if (i == 1)
			cursor_visible_part( );

		for (c = 0; c < 8; c++) {
			if (c & 1) {
				p.r = c1->r;
				delta.r = - corner_dims.r;
			}
			else {
				p.r = c0->r;
				delta.r = corner_dims.r;
			}

			if (c & 2) {
				p.theta = c1->theta;
				delta.theta = - corner_dims.theta;
			}
			else {
				p.theta = c0->theta;
				delta.theta = corner_dims.theta;
			}

			if (c & 4) {
				p.z = c1->z;
				delta.z = - corner_dims.z;
			}
			else {
				p.z = c0->z;
				delta.z = corner_dims.z;
			}

			sin_theta = sin( RAD(p.theta) );
			cos_theta = cos( RAD(p.theta) );
			cp0.x = p.r * cos_theta;
			cp0.y = p.r * sin_theta;
			cp1.x = (p.r + delta.r) * cos_theta;
			cp1.y = (p.r + delta.r) * sin_theta;

			const size_t vert_cnt = 4 + seg_count + 1;
			VertexPos *vert = NEW_ARRAY(VertexPos, vert_cnt);
			vert[0] = (VertexPos){{cp0.x, cp0.y, p.z + delta.z}}; // Vertical axis start
			vert[1] = (VertexPos){{cp0.x, cp0.y, p.z}}; // Vertical axis end
			vert[2] = (VertexPos){{cp1.x, cp1.y, p.z}}; // Radial axis end
			vert[3] = (VertexPos){{cp0.x, cp0.y, p.z}}; // Back to radial/vertical intersection
			/* Tangent axis (curved part) */
			for (s = 0; s <= seg_count; s++) {
				theta = p.theta + delta.theta * (double)s / (double)seg_count;
				cp0.x = p.r * cos( RAD(theta) );
				cp0.y = p.r * sin( RAD(theta) );
				vert[4 + s] = (VertexPos){{cp0.x, cp0.y, p.z}};
			}

			glBufferData(GL_ARRAY_BUFFER,
				     sizeof(VertexPos) * vert_cnt, vert,
				     GL_STREAM_DRAW);
			glEnableVertexAttribArray(gl.position_location);
			glVertexAttribPointer(
			    gl.position_location, 3, GL_FLOAT, GL_FALSE,
			    sizeof(VertexPos),
			    (void *)offsetof(VertexPos, position));
			glDrawArrays(GL_LINE_STRIP, 0, vert_cnt);
			glBufferData(GL_ARRAY_BUFFER,
				     sizeof(VertexPos) * vert_cnt, NULL,
				     GL_STREAM_DRAW);
		}
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	cursor_post( );
}


/* Draws the node cursor in an intermediate position between its previous
 * steady-state position and the current node (pos=0 indicates the former,
 * pos=1 the latter) */
static void
treev_draw_cursor( double pos )
{
	RTZvec c0, c1;
	RTZvec cursor_c0, cursor_c1;

	treev_get_corners( globals.current_node, &c0, &c1 );

	/* Interpolate corners */
	cursor_c0.r = INTERPOLATE(pos, treev_cursor_prev_c0.r, c0.r);
	cursor_c0.theta = INTERPOLATE(pos, treev_cursor_prev_c0.theta, c0.theta);
	cursor_c0.z = INTERPOLATE(pos, treev_cursor_prev_c0.z, c0.z);
	cursor_c1.r = INTERPOLATE(pos, treev_cursor_prev_c1.r, c1.r);
	cursor_c1.theta = INTERPOLATE(pos, treev_cursor_prev_c1.theta, c1.theta);
	cursor_c1.z = INTERPOLATE(pos, treev_cursor_prev_c1.z, c1.z);

	treev_gldraw_cursor( &cursor_c0, &cursor_c1 );
}


/* Draws TreeV geometry */
static void
treev_draw( boolean high_detail )
{
	if ((fstree_low_draw_stage == 0) || (fstree_high_draw_stage == 0))
		treev_arrange( FALSE );

	/* Draw low-detail geometry */

	treev_draw_recursive( globals.fstree, NIL, treev_core_radius, TREEV_DRAW_GEOMETRY_WITH_BRANCHES );

	if (fstree_low_draw_stage <= 1)
		++fstree_low_draw_stage;

	if (high_detail) {
		/* Draw additional high-detail stuff */

		/* Node name labels */
		text_pre();
		treev_draw_recursive(globals.fstree, NIL, treev_core_radius, TREEV_DRAW_LABELS);
		text_post();

		if (fstree_high_draw_stage <= 1)
			++fstree_high_draw_stage;

		/* Node cursor */
		treev_draw_cursor( CURSOR_POS(camera->pan_part) );
	}
}


/**** COMMON ROUTINES *****************************************/


/* Call before drawing the cursor */
static void
cursor_pre( void )
{
	glUseProgram(gl.program);
	ogl_disable_lightning();
}


/* Call to draw the "hidden" part of the cursor */
static void
cursor_hidden_part( void )
{
	/* Hidden part is drawn with a thin line */
	glDepthFunc( GL_GREATER );
	glLineWidth(2.0);
	glUniform4f(gl.color_location, 0.3, 0.3, 0.3, 1);
}


/* Call to draw the visible part of the cursor */
static void
cursor_visible_part( void )
{
	/* Visible part is drawn with a thick solid line */
	glDepthFunc( GL_LEQUAL );
	glLineWidth( 5.0 );
	glUniform4f(gl.color_location, 1, 1, 1, 1);
}


/* Call after drawing the cursor */
static void
cursor_post( void )
{
	glLineWidth( 1.0 );
	ogl_enable_lightning();
	glUseProgram(0);
}


/* Zeroes the drawing stages for both low- and high-detail geometry, so
 * that a full recursive draw is performed in the next frame without
 * the use of display lists (i.e. caches). This is necessary whenever
 * any geometry needs to be (re)built */
static void
queue_uncached_draw( void )
{
	fstree_low_draw_stage = 0;
	fstree_high_draw_stage = 0;
}


/* Flags a directory's geometry for rebuilding */
void
geometry_queue_rebuild( GNode *dnode )
{
	queue_uncached_draw( );
}


/* Sets up filesystem tree geometry for the specified mode */
void
geometry_init( FsvMode mode )
{
	DIR_NODE_DESC(globals.fstree)->deployment = 1.0;
	geometry_queue_rebuild( globals.fstree );

	switch (mode) {
		case FSV_DISCV:
		discv_init( );
		break;

		case FSV_MAPV:
		mapv_init( );
		break;

		case FSV_TREEV:
		treev_init( );
		break;

		SWITCH_FAIL
	}

	color_assign_recursive( globals.fstree );
}


/* Draws "fsv" in 3D */
void
geometry_gldraw_fsv( void )
{
	XYvec p, n;
	const float *vertices = NULL;
	const int *triangles = NULL, *edges = NULL;
	int v, e, i;
	// Magic constants 490 and 1188 determined by first making these arrays
	// larger and checking vlen and ilen values in the debugger.
#define VERT_MAX_LEN 490
#define IDX_MAX_LEN 1188
	static AboutVertex vert[VERT_MAX_LEN];
	static GLushort idx[IDX_MAX_LEN];
	static GLuint vbo, ebo;
	static size_t vlen;
	static size_t ilen;

	if (!vbo) {
		for (size_t c = 0; c < 3; c++) {
			GLfloat *color = (GLfloat*)&fsv_colors[c];
			vertices = fsv_vertices[c];
			triangles = fsv_triangles[c];
			edges = fsv_edges[c];
			const EdgeSmoothness *es = fsv_edge_smoothness[c];
			// Side faces
			for (e = 0; edges[e] >= 0; e++) {
				// For a smooth edge, calculate the normal from
				// the previous and next vertices. For a sharp
				// edge, duplicate the vertices, normal for the
				// first is calculated from the previous and
				// current vertex, and for the second vertex
				// the normal is calculated from the current
				// and next vertex.
				// Edge here meaning the vertex indices of the
				// vertices forming the sides of the character,
				// not the "edge" from graph theory.
				i = edges[e];
				EdgeSmoothness s = es[e];
				p.x = vertices[2 * i];
				p.y = vertices[2 * i + 1];
				int inext = edges[e + 1];
				int iprev = edges[e - 1];
				XYvec n2;
				if (e == 0) {
					// First edge, must use only "forward"
					// delta
					s = SMOOTH;
					i = inext;
					n.x = vertices[2 * i + 1] - p.y;
					n.y = p.x - vertices[2 * i];
				} else if (inext < 0) {
					// Last edge, must use only "backward"
					// delta
					s = SMOOTH;
					n.x = p.y - vertices[2 * iprev + 1];
					n.y = vertices[2 * iprev] - p.x;
				} else if (s == SMOOTH) {
					i = inext;
					n.x = vertices[2 * i + 1] -
					      vertices[2 * iprev + 1];
					n.y = vertices[2 * iprev] -
					      vertices[2 * i];
				} else if (s == SHARP) {
					// First normal with backward delta.
					n.x = p.y - vertices[2 * iprev + 1];
					n.y = vertices[2 * iprev] - p.x;
					// Second normal with forward delta.
					i = inext;
					n2.x = vertices[2 * i + 1] - p.y;
					n2.y = p.x - vertices[2 * i];
				} else
					g_error("Unable to calculate normal!");

				if (e > 0) {
					idx[ilen++] = vlen - 2;
					idx[ilen++] = vlen - 1;
					idx[ilen++] = vlen;
					idx[ilen++] = vlen;
					idx[ilen++] = vlen - 1;
					idx[ilen++] = vlen + 1;
				}
				vert[vlen++] = (AboutVertex){
				    {p.x, p.y, 30.0},
				    {n.x, n.y, 0.0f},
				    {color[0], color[1], color[2]}};
				vert[vlen++] = (AboutVertex){
				    {p.x, p.y, -30.0},
				    {n.x, n.y, 0.0f},
				    {color[0], color[1], color[2]}};
				if (s == SHARP) {
					// Second set of vertices with forward
					// delta normals.
					vert[vlen++] = (AboutVertex){
					    {p.x, p.y, 30.0},
					    {n2.x, n2.y, 0.0f},
					    {color[0], color[1], color[2]}};
					vert[vlen++] = (AboutVertex){
					    {p.x, p.y, -30.0},
					    {n2.x, n2.y, 0.0f},
					    {color[0], color[1], color[2]}};
				}
			}
			/* Front faces */
			int imax = 0;
			for (v = 0; triangles[v] >= 0; v++) {
				i = triangles[v];
				imax = MAX(i, imax);
				p.x = vertices[2 * i];
				p.y = vertices[2 * i + 1];
				vert[vlen + i] = (AboutVertex){
					{p.x, p.y, 30.0},
					{0.0f, 0.0f, 1.0f},
					{color[0], color[1], color[2]}
				};
				idx[ilen++] = vlen + i;
			}
			vlen += imax + 1;

			/* Back faces */
			imax = 0;
			for (--v; v >= 0; v--) {
				i = triangles[v];
				imax = MAX(i, imax);
				p.x = vertices[2 * i];
				p.y = vertices[2 * i + 1];
				vert[vlen + i] = (AboutVertex){
					{p.x, p.y, -30.0},
					{0.0f, 0.0f, -1.0f},
					{color[0], color[1], color[2]}
				};
				idx[ilen++] = vlen + i;
			}
			vlen += imax + 1;
		}
		g_assert(VERT_MAX_LEN >= vlen);
		g_assert(IDX_MAX_LEN >= ilen);
#undef VERT_MAX_LEN
#undef IDX_MAX_LEN
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(AboutVertex) * vlen, vert, GL_STATIC_DRAW);

		glGenBuffers(1, &ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * ilen, idx, GL_STATIC_DRAW);

		// Upload fog parameters. Emulate legacy GL with a simple
		// linear fog model.
		glUseProgram(aboutGL.program);
		glUniform3f(aboutGL.fog_color_location, 0.0f, 0.0f, 0.0f);
		glUniform1f(aboutGL.fog_start_location, 200.0f);
		glUniform1f(aboutGL.fog_end_location, 1800.0f);

		ogl_error();
	}
#if 0
	// Useful code snippet for checking vertex coords in NDC
	for (size_t i = 0; i < vlen; i += (vlen - 1)) {
		g_print("%zu th vertex in FSV coords:\n", i);
		vec4 c = (vec4){vert[i].position[0], vert[i].position[1],
				vert[i].position[2], 1.0f};
		vec4 cp;
		glm_mat4_mulv(aboutGL.mvp, c, cp);
		glmc_vec4_print(c, stdout);
		glmc_vec4_print(cp, stdout);
		float w = cp[3];
		vec3 pd = (vec3){cp[0]/w, cp[1]/w, cp[2]/w};
		glmc_vec3_print(pd, stdout);
	}
	glmc_mat4_print(aboutGL.mvp, stdout);
#endif
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glEnableVertexAttribArray(aboutGL.position_location);
	glVertexAttribPointer(aboutGL.position_location, 3, GL_FLOAT, GL_FALSE,
			      sizeof(AboutVertex),
			      (void *)offsetof(AboutVertex, position));
	glEnableVertexAttribArray(aboutGL.normal_location);
	glVertexAttribPointer(aboutGL.normal_location, 3, GL_FLOAT, GL_FALSE,
			      sizeof(AboutVertex),
			      (void *)offsetof(AboutVertex, normal));
	glEnableVertexAttribArray(aboutGL.color_location);
	glVertexAttribPointer(aboutGL.color_location, 3, GL_FLOAT, GL_FALSE,
			      sizeof(AboutVertex),
			      (void *)offsetof(AboutVertex, color));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glUseProgram(aboutGL.program);
	glDrawElements(GL_TRIANGLES, ilen, GL_UNSIGNED_SHORT, 0);
	glUseProgram(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	ogl_error();
}


/* Draws the splash screen */
static void
splash_draw( void )
{
	XYZvec text_pos;
	XYvec text_dims;
	double bottom_y;
	double k;

	/* Draw fsv title */

	/* Set up projection matrix */
	k = 82.84 / ogl_aspect_ratio( );
	mat4 proj;
	glm_frustum(-70.82, 95.40, - k, k, 200.0, 400.0, proj);

	/* Set up modelview matrix */
	mat4 mv;
	glm_mat4_identity(mv);
	glm_translate(mv, (vec3){0.0, 0.0, -300.0});
	glm_rotate_x(mv, 10.5 * M_PI/180.0, mv);
	glm_translate(mv, (vec3){20.0, 20.0, -30.0});

	mat4 mvp;
	glm_mat4_mul(proj, mv, mvp);
	mat3 normmat;
	glm_mat4_pick3(mv, normmat);
	glm_mat3_inv(normmat, normmat);
	glm_mat3_transpose(normmat);
	glUseProgram(aboutGL.program);
	/* update the "mvp" matrix we use in the shader */
	glUniformMatrix4fv(aboutGL.mvp_location, 1, GL_FALSE, (float*)mvp);
	glUniformMatrix4fv(aboutGL.modelview_location, 1, GL_FALSE, (float*) mv);
	glUniformMatrix3fv(aboutGL.normal_matrix_location, 1, GL_FALSE, (float*) normmat);
	glUseProgram(0);

	geometry_gldraw_fsv( );

	/* Draw accompanying text */

	/* Set up projection matrix */
	k = 0.5 / ogl_aspect_ratio( );
	// Reuse proj matrix from the "FSV" drawing
	glm_ortho(0.0, 1.0, - k, k, -1.0, 1.0, proj);
	bottom_y = - k;

	/* Set up modelview matrix */
	// Modelview is the identity, so mvp is just the projection matrix.
	text_upload_mvp((float*) proj);

	text_pre( );

	/* Title */
	text_set_color(1.0, 1.0, 1.0);
	text_pos.x = 0.2059;
	text_pos.y = -0.1700;
	text_pos.z = 0.0;
	text_dims.x = 0.9;
	text_dims.y = 0.0625;
	text_draw_straight( "File", &text_pos, &text_dims );
	text_pos.x = 0.4449;
	text_draw_straight( "System", &text_pos, &text_dims );
	text_pos.x = 0.7456;
	text_draw_straight( "Visualizer", &text_pos, &text_dims );

	/* Version */
	text_set_color(0.75, 0.75, 0.75);
	text_pos.x = 0.5000;
	text_pos.y = (2.0 - MAGIC_NUMBER) * (0.2247 + bottom_y) - 0.2013;
	text_dims.y = 0.0386;
	text_draw_straight( "Version " VERSION, &text_pos, &text_dims );

	/* Copyright/author info */
	text_set_color(0.5, 0.5, 0.5);
	text_pos.y = bottom_y + 0.0417;
	text_dims.y = 0.0234;
	text_draw_straight( "Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>", &text_pos, &text_dims );
	text_pos.y = bottom_y + 0.0117;
	text_draw_straight("Copyright (C) 2021 Janne Blomqvist", &text_pos, &text_dims);

	text_post( );
}


/* Top-level call to draw viewport content */
void
geometry_draw( boolean high_detail )
{
	if (about( ABOUT_CHECK )) {
		/* Currently giving About presentation */
		if (high_detail)
			about( ABOUT_DRAW );
		return;
	}

	switch (globals.fsv_mode) {
		case FSV_SPLASH:
		splash_draw( );
		break;

		case FSV_DISCV:
		discv_draw( high_detail );
		break;

		case FSV_MAPV:
		mapv_draw( high_detail );
		break;

		case FSV_TREEV:
		treev_draw( high_detail );
		break;

		SWITCH_FAIL
	}
}


/* This gets called upon completion of a camera pan */
void
geometry_camera_pan_finished( void )
{
	switch (globals.fsv_mode) {
		case FSV_DISCV:
		/* discv_camera_pan_finished( ); */
		break;

		case FSV_MAPV:
		mapv_camera_pan_finished( );
		break;

		case FSV_TREEV:
		treev_camera_pan_finished( );
		break;

		SWITCH_FAIL
	}
}


/* This is called when a directory is about to collapse or expand */
void
geometry_colexp_initiated( GNode *dnode )
{
	g_assert( NODE_IS_DIR(dnode) );

	/* A newly expanding directory in TreeV mode will probably
	 * need (re)shaping (it may be appearing for the first time,
	 * or its inner radius may have changed) */
	if (DIR_COLLAPSED(dnode) && (globals.fsv_mode == FSV_TREEV))
		treev_reshape_platform( dnode, geometry_treev_platform_r0( dnode ) );
}


/* This is called as a directory collapses or expands (and also when it
 * finishes either operation) */
void
geometry_colexp_in_progress( GNode *dnode )
{
	g_assert( NODE_IS_DIR(dnode) );

	/* Check geometry status against deployment. If they don't concur
	 * properly, then directory geometry has to be rebuilt */
        if (DIR_NODE_DESC(dnode)->geom_expanded != (DIR_NODE_DESC(dnode)->deployment > EPSILON))
		geometry_queue_rebuild( dnode );
        else
		queue_uncached_draw( );

	if (globals.fsv_mode == FSV_TREEV) {
		/* Take care of shifting angles */
		treev_queue_rearrange( dnode );
	}
}


/* This tells if the specified node should be highlighted.  */
boolean
geometry_should_highlight(GNode *node)
{
	if (!NODE_IS_DIR(node))
		return TRUE;

	switch (globals.fsv_mode) {
		case FSV_DISCV:
		return TRUE;

		case FSV_MAPV:
		return DIR_COLLAPSED(node);

		case FSV_TREEV:
		return geometry_treev_is_leaf( node );

		SWITCH_FAIL
	}

	return FALSE;
}


/* Draws a single node, in its absolute position */
__attribute__((unused)) static void
draw_node( GNode *node )
{
	mat4 tmpmat;
	glm_mat4_copy(gl.modelview, tmpmat);

	switch (globals.fsv_mode) {
		case FSV_DISCV:
		/* TODO: code to draw single discv node goes HERE */
		break;

		case FSV_MAPV:
		glm_translate(gl.modelview, (vec3){0.0f, 0.0f, geometry_mapv_node_z0(node)});
		ogl_upload_matrices(TRUE);
		mapv_gldraw_node( node );
		break;

		case FSV_TREEV:
		if (geometry_treev_is_leaf( node )) {
			glm_rotate_z(gl.modelview, geometry_treev_platform_theta(node->parent) * M_PI/180, gl.modelview);
			ogl_upload_matrices(TRUE);
			treev_gldraw_leaf( node, geometry_treev_platform_r0( node->parent ), TRUE );
		}
		else {
			glm_rotate_z(gl.modelview, geometry_treev_platform_theta(node) * M_PI/180, gl.modelview);
			ogl_upload_matrices(TRUE);
			treev_gldraw_platform( node, geometry_treev_platform_r0( node ) );
		}
		break;

		SWITCH_FAIL
	}

	glm_mat4_copy(tmpmat, gl.modelview);
	ogl_upload_matrices(FALSE);
}


/* Highlights a node. "strong" flag indicates whether a noticeably heavier
   highlight should be drawn (CURRENTLY NOT USED). Passing NULL as the node
   argument clears the current highlight. */
void
geometry_highlight_node( GNode *node, boolean strong )
{
	if (!node)
		highlight_node_id = 0;
	else {
		highlight_node_id = NODE_DESC(node)->id;
		//g_print("Highlighting node %u %s\n", highlight_node_id, NODE_DESC(node)->name);
		//draw_node(node);
		redraw();
	}
}


/* Frees all allocated GL resources for the subtree rooted at the
 * specified directory node */
void
geometry_free_recursive( GNode *dnode )
{
	//DirNodeDesc *dir_ndesc;
	GNode *node;

	g_assert( NODE_IS_DIR(dnode) || NODE_IS_METANODE(dnode) );

	//dir_ndesc = DIR_NODE_DESC(dnode);

	// TODO delete VBO's?

	/* Recurse into subdirectories */
	node = dnode->children;
	while (node != NULL) {
		if (NODE_IS_DIR(node))
			geometry_free_recursive( node );
		else
			break;
		node = node->next;
	}
}


/* end geometry.c */
