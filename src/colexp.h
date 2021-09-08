/* colexp.h */

/* Collapse/expansion engine */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#ifdef FSV_COLEXP_H
	#error
#endif
#define FSV_COLEXP_H


typedef enum {
	COLEXP_COLLAPSE_RECURSIVE,
	COLEXP_EXPAND,
	COLEXP_EXPAND_ANY,
	COLEXP_EXPAND_RECURSIVE
} ColExpMesg;


void colexp( GNode *dnode, ColExpMesg mesg );


/* end colexp.h */
