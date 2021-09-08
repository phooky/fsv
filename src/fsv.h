/* fsv.h */

/* Program entry */

/*
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#ifdef FSV_FSV_H
	#error
#endif
#define FSV_FSV_H


void fsv_set_mode( FsvMode mode );
void fsv_load( const char *dir );
void fsv_write_config( void );


/* end fsv.h */
