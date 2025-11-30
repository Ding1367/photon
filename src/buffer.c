#include "buffer.h"
#include "ui.h"
#include "photon.h"
#include "extensions.h"
#include <stdlib.h>
#include <string.h>

#define err_and_ret(edit, err, val) edit->error = err; return val;

extern photon_buffer_t *ctx;

#define GROUP_SIZE 16

typedef struct alloc_group {
    void *ptrs[GROUP_SIZE];
    unsigned char n;
    unsigned char fail;
} alloc_group_t;

void *group_alloc(alloc_group_t *group, size_t n, int zero){
    if (group->n >= GROUP_SIZE) return NULL;
    void *p = zero ? calloc(n, 1) : malloc(n);
    if (p != NULL)
        group->ptrs[group->n++] = p;
    else
        group->fail = 1;
    return p;
}

void group_free(alloc_group_t *group){
    for (int i = 0; i < group->n; i++)
        free(group->ptrs);
}

static void photon_draw_buf(const photon_api_t *api, photon_buffer_t *buf){
    photon_buffer_t *old_ctx = ctx;
    ctx = buf;
    photon_editor_t *editor = api->editor;
    editor->ui_hints = editor->theme.normal;
    photon_move_ui_cursor(buf->y, buf->x);
    photon_draw_box(editor, buf->rows, buf->cols);
    int y = buf->y;
    int i = buf->scroll;
    while (y - buf->y < buf->rows){
        if (i < buf->num_line) break;
        photon_move_ui_cursor(y, buf->x);
        photon_draw_str(editor, buf->lines[i].line);
        photon_ui_cursor_loc(&y, NULL);
        i++;
    }
    ctx = old_ctx;
}

photon_buffer_t *photon_create_buffer(photon_editor_t *editor, const photon_buf_options_t *options){
    const char *name = options->name;
    char type = options->type;
    if (type != BUF_FILE && type != BUF_SCRATCH){
        err_and_ret(editor, PHOTON_BAD_PARAM, NULL);
    }

    alloc_group_t ag = {0};

    photon_buffer_t *buf = group_alloc(&ag, sizeof(photon_buffer_t), 0);
    photon_line_t *lines = group_alloc(&ag, 8 * sizeof(photon_line_t), 1);
    char *emptyLine = group_alloc(&ag, 16, 0);
    char *nameCopy = NULL;
    size_t nameLen = 0;
    if (name){
        nameLen = strlen(name);
        nameCopy = group_alloc(&ag, nameLen + 1, 0);
    }
    if (ag.fail){
        group_free(&ag);
        err_and_ret(editor, PHOTON_NO_MEM, NULL);
    }
    if (nameLen)
        memcpy(nameCopy, name, nameLen + 1);
    emptyLine[0] = 0;
    
    buf->x = options->x;
    buf->y = options->y;
    buf->rows = options->rows;
    buf->cols = options->cols;
    buf->next = editor->first_buf;
    buf->prev = NULL;
    buf->num_line = 1;
    buf->cap_line = 8;
    buf->lines = lines;
    lines[0].line = emptyLine;
    lines[0].capacity = 16;
    buf->type = type;
    buf->name = nameCopy;
    buf->draw = photon_draw_buf;
    buf->userdata = NULL;
    editor->first_buf = buf;
    photon_trigger_hook(editor, PHOTON_HOOK_NEWBUF, (uintptr_t)buf);
    return buf;
}

void photon_delete_buffer(photon_editor_t *editor, photon_buffer_t *buffer){
    if (buffer->prev == NULL){
        editor->first_buf = buffer->next;
        if (editor->first_buf)
            editor->first_buf = NULL;
    } else {
        buffer->prev = buffer->next;
        if (buffer->prev)
            buffer->next->prev = buffer->prev;
    }
    for (int i = 0; i < buffer->num_line; i++){
        free(buffer->lines[i].line);
    }
    free(buffer->lines);
    free(buffer->name);
    free(buffer);
}
