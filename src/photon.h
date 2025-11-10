#ifndef __PHOTON_H__
#define __PHOTON_H__
#include <stdint.h>
#include <stddef.h>

#define PHOTON_OK 0
#define PHOTON_BAD_PARAM 1
#define PHOTON_NO_MEM 2

#define PHOTON_BOLD 1
#define PHOTON_ITALIC 2
#define PHOTON_UNDERLINE 4
#define PHOTON_STRIKETHROUGH 8

typedef struct photon_buf_options {
    char type;
    int x, y, rows, cols;
    const char *name;
} photon_buf_options_t;

typedef struct photon_line {
    char *line;
    int length;
    int capacity;
} photon_line_t;

#define BUF_FILE 0
#define BUF_SCRATCH 1

typedef struct photon_api photon_api_t;
typedef struct photon_editor photon_editor_t;
typedef struct photon_buffer photon_buffer_t;

typedef void (*photon_buf_draw_t)(const photon_api_t *api, photon_buffer_t *self);

struct photon_buffer {
    char type;
    photon_line_t *lines;
    int num_line;
    int cap_line;
    char *name;

    int scroll;

    photon_buf_draw_t draw;
    void *userdata;

    photon_buffer_t *prev;
    photon_buffer_t *next;

    int y, x, rows, cols;
};

typedef struct photon_event {
    int cancelled;
    union {
        uintptr_t data;
        photon_buffer_t *buffer;
    };
} photon_event_t;

typedef union photon_hooks {
    struct {
        void (*on_keypress)(const photon_api_t *api, photon_event_t *event);
        void (*on_new_buf)(const photon_api_t *api, photon_event_t *event);
    };
    void (*hooks[2])(const photon_api_t *api, photon_event_t *event);
} photon_hooks_t;

#define PHOTON_HOOK_KEYPRESS 0
#define PHOTON_HOOK_NEWBUF 1

typedef struct photon_extension {
    int errorValue;

    void  *handle;
    void (*on_load)(const photon_api_t *);
    void (*pre_frame)(const photon_api_t *);
    void (*on_unload)(const photon_api_t *);

    struct photon_extension *next;

    photon_hooks_t hooks;
} photon_extension_t;

typedef struct {
    int y, x;
    int fg, bg;
    char style;
    char ch;
} photon_draw_req_t;

typedef int (*photon_pre_draw_t)(photon_draw_req_t *req);

struct photon_api {
    photon_editor_t *editor;
    photon_hooks_t *hooks;
    int error;

    const char *(*get_error_msg)(photon_editor_t *editor);
    struct {
        photon_buffer_t *(*create)(photon_editor_t *editor, const photon_buf_options_t *options);
#if __cplusplus
        void (*delete_buf)(photon_editor_t *editor, photon_buffer_t *buffer);
#else
        void (*delete)(photon_editor_t *editor, photon_buffer_t *buffer);
#endif
    } buffer;
    struct {
        void (*draw_str)(photon_editor_t *editor, int y, int x, const char *str);
        void (*draw_nstr)(photon_editor_t *editor, int y, int x, const char *str, size_t sz);
        void (*draw_box)(photon_editor_t *editor, int y, int x, int rows, int cols);
        void (*tint_line)(photon_editor_t *editor, int y, int x, int cols);
        int width;
        int height;
    } ui;
};

typedef struct photon_theme_attr {
    int bg, fg;
    char style;
} photon_theme_attr_t;

typedef struct photon_editor {
    photon_buffer_t *first_buf;
    photon_extension_t *first_ext;
    photon_api_t api;

    char should_quit;

    struct {
        photon_theme_attr_t normal;
    } theme;

    photon_theme_attr_t ui_hints;

    int error;
    
    photon_pre_draw_t pre_draw;
} photon_editor_t;

#endif//__PHOTON_H__
