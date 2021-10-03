/* gui.h */

/* Higher-level GTK+ interface */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * Copyright (C) 2021 Janne Blomqvist <blomqvist.janne@gmail.com>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#ifdef FSV_GUI_H
	#error
#endif
#define FSV_GUI_H


#ifdef __GTK_H__

#include <gdk-pixbuf/gdk-pixbuf.h>

/* For clearer gui_*_packing( ) syntax */
#define EXPAND		TRUE
#define NO_EXPAND	FALSE
#define FILL		TRUE
#define NO_FILL		FALSE
#define AT_START	TRUE
#define AT_END		FALSE


/* Icon container */
typedef struct _Icon Icon;
struct _Icon {
	GdkPixbuf *pixbuf;
};

// For the TreeView (directory tree view)
enum
{
  DIRTREE_NAME_COLUMN = 0,
  DIRTREE_NODE_COLUMN,		// Hidden column containing GNode pointer
  DIRTREE_NUM_COLS
};

// For the TreeView (directory list view)
enum
{
	FILELIST_ICON_COLUMN = 0,
	FILELIST_NAME_COLUMN,
	FILELIST_NODE_COLUMN, // Hidden column with GNode pointer
	FILELIST_NUM_COLS
};

// For the TreeView (filelist scan progress view)
enum
{
	FILELIST_SCAN_ICON_COLUMN = 0,
	FILELIST_SCAN_FOUND_COLUMN,
	FILELIST_SCAN_BYTES_COLUMN,
	FILELIST_SCAN_NUM_COLS
};

// For the TreeView (Color picker using wildcard patterns)
enum
{
	DIALOG_WPATTERN_COLOR_COLUMN = 0,
	DIALOG_WPATTERN_WPATTERN_COLUMN,
	DIALOG_WPATTERN_COLOR2_COLUMN,  // Hidden, contains *GdkColor
	DIALOG_WPATTERN_ROWDATA_COLUMN, // Hidden, contains WpListRowData
	DIALOG_WPATTERN_NUM_COLS
};
#endif /* __GTK_H__ */


void gui_update( void );
#ifdef __GTK_H__
boolean gui_adjustment_widget_busy( GtkAdjustment *adj );
void gui_set_parent_child(GtkWidget *parent_w, GtkWidget *child_w);
GtkWidget *gui_hbox_add( GtkWidget *parent_w, int spacing );
GtkWidget *gui_vbox_add( GtkWidget *parent_w, int spacing );
void gui_box_set_packing( GtkWidget *box_w, boolean expand, boolean fill, boolean start );
GtkWidget *gui_button_add( GtkWidget *parent_w, const char *label, void (*callback)( ), void *callback_data );
GtkWidget *gui_button_with_pixbuf_xpm_add(GtkWidget *parent_w, const char **xpm_data, const char *label, void (*callback)( ), void *callback_data);
GtkWidget *gui_toggle_button_add( GtkWidget *parent_w, const char *label, boolean active, void (*callback)( ), void *callback_data );
GtkWidget *gui_wpattern_list_new(GtkWidget *parent_w);
GtkWidget *gui_colorpicker_add( GtkWidget *parent_w, RGBcolor *init_color, const char *title, void (*callback)( ), void *callback_data );
void gui_colorpicker_set_color( GtkWidget *colorpicker_w, RGBcolor *color );
GtkWidget *gui_filelist_new(GtkWidget *parent_w);
GtkWidget *gui_filelist_scan_new(GtkWidget *parent_w);
GtkWidget *gui_tree_add( GtkWidget *parent_w );
GtkTreePath *gui_tree_node_add( GtkWidget *tree_w, GtkTreePath *parent, Icon icon_pair[2], const char *text, boolean expanded, GNode *data );
void gui_cursor( GtkWidget *widget, int glyph );
GtkWidget *gui_dateedit_add( GtkWidget *parent_w, time_t the_time, void (*callback)( ), void *callback_data );
time_t gui_dateedit_get_time( GtkWidget *dateedit_w );
void gui_dateedit_set_time( GtkWidget *dateedit_w, time_t the_time );
GtkWidget *gui_entry_add( GtkWidget *parent_w, const char *init_text, void (*callback)( ), void *callback_data );
void gui_entry_set_text( GtkWidget *entry_w, const char *entry_text );
GtkWidget *gui_frame_add( GtkWidget *parent_w, const char *title );
GtkWidget *gui_gl_area_add( GtkWidget *parent_w );
void gui_keybind( GtkWidget *widget, char *keystroke );
GtkWidget *gui_label_add( GtkWidget *parent_w, const char *label_text );
GtkWidget *gui_menu_add( GtkWidget *parent_menu_w, const char *label );
GtkWidget *gui_menu_item_add( GtkWidget *menu_w, const char *label, void (*callback)( ), void *callback_data );
void gui_radio_menu_begin( int init_selected );
GtkWidget *gui_radio_menu_item_add( GtkWidget *menu_w, const char *label, void (*callback)( ), void *callback_data );
GtkWidget *gui_notebook_add( GtkWidget *parent_w );
void gui_notebook_page_add( GtkWidget *notebook_w, const char *tab_label, GtkWidget *content_w );
GtkWidget *gui_hpaned_add( GtkWidget *parent_w, int divider_x_pos );
GtkWidget *gui_vpaned_add( GtkWidget *parent_w, int divider_y_pos );
GtkWidget *gui_pixbuf_xpm_add(GtkWidget *parent_w, const char **xpm_data);
GtkWidget *gui_spectrum_new( GtkWidget *parent_w );
void gui_spectrum_fill( GtkWidget *spectrum_w, RGBcolor (*spectrum_func)( double x ) );
GtkWidget *gui_hscrollbar_add( GtkWidget *parent_w, GtkAdjustment *adjustment );
GtkWidget *gui_vscrollbar_add( GtkWidget *parent_w, GtkAdjustment *adjustment );
GtkWidget *gui_separator_add( GtkWidget *parent_w );
GtkWidget *gui_statusbar_add( GtkWidget *parent_w );
void gui_statusbar_message( GtkWidget *statusbar_w, const char *message );
GtkWidget *gui_table_add( GtkWidget *parent_w, int num_rows, int num_cols, boolean homog, int cell_padding );
void gui_table_attach( GtkWidget *table_w, GtkWidget *widget, int left, int right, int top, int bottom );
GtkWidget *gui_text_area_add( GtkWidget *parent_w, const char *init_text );
void gui_widget_packing( GtkWidget *widget, boolean expand, boolean fill, boolean start );
void gui_colorsel_window( const char *title, RGBcolor *init_color, void (*ok_callback)( ), void *ok_callback_data );
GtkWidget *gui_dialog_window( const char *title, void (*close_callback )( ) );
GtkWidget *gui_entry_window( const char *title, const char *init_text, void (*ok_callback)( ), void *ok_callback_data );
gchar *gui_dir_choose(const char *title, GtkWidget *parent, const char *init_dir);
void gui_window_icon_xpm(GtkWidget *window_w, const char **xpm_data);
void gui_window_modalize( GtkWidget *window_w, GtkWidget *parent_window_w );
#endif /* __GTK_H__ */


/* end gui.h */
