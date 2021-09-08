/* about.h */

/* Help -> About... */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#ifdef FSV_ABOUT_H
	#error
#endif
#define FSV_ABOUT_H


typedef enum {
	ABOUT_BEGIN,
	ABOUT_END,
	ABOUT_DRAW,
	ABOUT_CHECK
} AboutMesg;


boolean about( AboutMesg mesg );


/* end about.h */
