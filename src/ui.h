#ifndef __UI_H__
#define __UI_H__
#include <stddef.h>

typedef struct photon_editor photon_editor_t;

int photon_ui_init(photon_editor_t *editor);

void photon_move_ui_cursor(int y, int x);
void photon_draw_str(photon_editor_t *editor, const char *str);
void photon_draw_nstr(photon_editor_t *editor, const char *str, size_t sz);
void photon_draw_box(photon_editor_t *editor, int rows, int cols);
void photon_tint_line(photon_editor_t *editor, int y, int x, int n);
void photon_ui_refresh(void);
void photon_ui_clear(void);
int photon_ui_width(void);
int photon_ui_height(void);

#if PHOTON_DEBUG
int photon_ui_snapshot(const char *fpath, const char *bpath);
int photon_ui_frame_number(void);

#define PHOTON_UI_CALLS_NUM 0
#define PHOTON_UI_REFRESH_NUM 1
#endif

void photon_ui_end(void);

#endif
