#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include "photon.h"
#include "photon_debug.h"
#include "input.h"
#include "ui.h"

static const char *errorMessages[] = {
    NULL,
    "Invalid parameters",
    "Not enough memory"
};

#define err_and_ret(edit, err, val) edit->error = err; return val;

static void photon_draw_buf(const photon_api_t *api, photon_buffer_t *buf){
    photon_editor_t *editor = api->editor;
    photon_draw_box(editor, buf->y, buf->x, buf->rows, buf->cols);
    for (int i = 0; i < buf->num_line; i++){
        photon_draw_str(editor, i, 0, buf->lines[i].line);
    }
}

void photon_setup_api(photon_editor_t *editor, photon_extension_t *ext){
    editor->api.hooks = &ext->hooks;
    editor->error = ext->errorValue;
}

int photon_trigger_hook(photon_editor_t *editor, int id, uintptr_t data){
    photon_event_t event;
    event.cancelled = 0;
    event.data = data;
    photon_extension_t *it = editor->first_ext;
    while (it){
        if (it->hooks.hooks[id]){
            photon_setup_api(editor, it);
            it->hooks.hooks[id](&editor->api, &event);
        }
        it = it->next;
    }
    return event.cancelled;
}

photon_buffer_t *photon_create_buffer(photon_editor_t *editor, const photon_buf_options_t *options){
    const char *name = options->name;
    char type = options->type;
    if (type != BUF_FILE && type != BUF_SCRATCH){
        err_and_ret(editor, PHOTON_BAD_PARAM, NULL);
    }
    photon_buffer_t *buf = malloc(sizeof(photon_buffer_t));
    char *name_copy = NULL;
    if (name != NULL){
        name_copy = strdup(name);
    }
    if (buf == NULL || (!name_copy && name)){
        free(name_copy);
        free(buf);
        err_and_ret(editor, PHOTON_NO_MEM, NULL);
    }
    buf->x = options->x;
    buf->y = options->y;
    buf->rows = options->rows;
    buf->cols = options->cols;
    buf->next = editor->first_buf;
    buf->prev = NULL;
    buf->num_line = 0;
    buf->cap_line = 0;
    buf->lines = NULL;
    buf->type = type;
    buf->name = name_copy;
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

static int counter;

void photon_handle_keypress(photon_editor_t *editor, int key){
    if (key == PHOTON_INVALID_KEY) return;
    if (photon_trigger_hook(editor, PHOTON_HOOK_KEYPRESS, key)) return;
    if (key == 17){ // ^Q
        editor->should_quit = 1;
    } else if (key == 7) { // ^G
        putchar(7);
        fflush(stdout);
    }
}

void photon_editor_cleanup(photon_editor_t *editor){
    while (editor->first_buf){
        photon_delete_buffer(editor, editor->first_buf);
    }
    while (editor->first_ext){
        photon_extension_t *next = editor->first_ext->next;
        if (editor->first_ext->on_unload){
            photon_setup_api(editor, editor->first_ext);
            editor->first_ext->on_unload(&editor->api);
        }
        dlclose(editor->first_ext->handle);
        free(editor->first_ext);
        editor->first_ext = next;
    }
}

const char *photon_editor_error_msg(photon_editor_t *editor){
    if (editor->error < 1 || editor->error > PHOTON_NO_MEM){
        return "Undefined error";
    }
    return errorMessages[editor->error];
}

#define LOAD_FATAL_ERR (-1)
#define LOAD_ERROR 0
#define LOAD_OK 1

static int load_extensions(photon_editor_t *editor){
    // load extensions
    char extDirPath[PATH_MAX] = {0};
    const char *home = getenv("HOME");
    if (!home){
        return LOAD_OK;
    }
    snprintf(extDirPath, PATH_MAX, "%s/.config/photon/extensions", home);
    DIR *dir = opendir(extDirPath);
    if (!dir){
        if (errno != ENOENT){
            fprintf(stderr, "failed to open directory %s: %s\r\n", extDirPath, strerror(errno));
            return LOAD_ERROR;
        }
        return LOAD_OK;
    }
    int err = LOAD_OK;

    struct dirent *ent;
    while ((ent = readdir(dir))){
        const char *name = ent->d_name;
        uint16_t len = ent->d_namlen;
        if (len >= 3 && strcmp(name + len - 3, ".so") == 0 && strncmp(name, "--", 2) != 0){
            char extPath[PATH_MAX] = {0};
            snprintf(extPath, PATH_MAX, "%s/%s", extDirPath, name);

            void *handle = dlopen(extPath, RTLD_LAZY);
            if (handle == NULL){
                fprintf(stderr, "failed to load extension %s: %s\r\n", name, dlerror());
                err = LOAD_ERROR;
                continue;
            }

            photon_extension_t *ext = malloc(sizeof(photon_extension_t));
            if (!ext){
                fprintf(stderr, "failed to allocate extension %s: %s\r\n", name, strerror(errno));
                photon_editor_cleanup(editor);
                err = LOAD_FATAL_ERR;
                break;
            }
            
            // get the functions
            ext->on_load = (void (*)(const photon_api_t *api))dlsym(handle, "photon_on_load");
            ext->on_unload = (void (*)(const photon_api_t *api))dlsym(handle, "photon_on_unload");
            ext->pre_frame = (void (*)(const photon_api_t *api))dlsym(handle, "photon_pre_frame");

            ext->next = editor->first_ext;
            ext->errorValue = PHOTON_OK;
            ext->handle = handle;
            memset(&ext->hooks, 0, sizeof(photon_hooks_t));
            editor->first_ext = ext;

            if (ext->on_load){
                photon_setup_api(editor, ext);
                ext->on_load(&editor->api);
            }
        }
    }

    closedir(dir);
    return err;
}

int main(void){
    atexit(&photon_ui_end);

    photon_editor_t editor = {0};

    if (!photon_ui_init(&editor)){
        fprintf(stderr, "failed to initialize library: %s\n", photon_editor_error_msg(&editor));
        return EXIT_FAILURE;
    }
    editor.api.editor = &editor;
    editor.api.buffer.create = &photon_create_buffer;
    editor.api.buffer.delete = &photon_delete_buffer;
    editor.api.ui.draw_str = &photon_draw_str;
    editor.api.ui.draw_nstr = &photon_draw_nstr;
    editor.api.ui.tint_line = &photon_tint_line;
    editor.api.get_error_msg = &photon_editor_error_msg;
    editor.theme.normal = (photon_theme_attr_t){ .bg = 0x1c1c1c, .fg = 0xebdbb2, .style = 0 };
    editor.ui_hints = editor.theme.normal;

    switch (load_extensions(&editor)){
        case LOAD_FATAL_ERR: return EXIT_FAILURE;
        case LOAD_ERROR: {
            // TODO: improve and use UI instead of stdio
            printf("\x1b[7mPress any key to continue");
            fflush(stdout);
            getchar();
            printf("\x1b[0m\x1b[2J\x1b[H");
        } break;
        case LOAD_OK: break;
        default: break;
    }

    photon_buffer_t *buf;
    photon_buf_options_t options;
    options.x = options.y = 0;
    options.rows = photon_ui_height();
    options.cols = photon_ui_width();
    options.name = NULL;
    options.type = BUF_FILE;
    if ((buf = photon_create_buffer(&editor, &options)) == NULL){
        return 1;
    }
    buf->num_line = 1;
    buf->lines = malloc(sizeof(photon_line_t));
    buf->lines[0].line = strdup("int main my_type_t float cha unsigned char");
    buf->lines[0].length = strlen(buf->lines[0].line);
    buf->lines[0].capacity = strlen(buf->lines[0].line) + 1;

    editor.api.ui.width = photon_ui_width();
    editor.api.ui.height = photon_ui_height();

    PHOTON_DEBUG_OPT(int capture = 0);
    // logic...
    while (!editor.should_quit){
        photon_ui_clear();
        editor.first_buf->draw(&editor.api, editor.first_buf);

        char buf[16] = {0};
        sprintf(buf, "%d", counter++);
        photon_draw_str(&editor, 23, 0, buf);

        photon_extension_t *it = editor.first_ext;
        while (it){
            if (it->pre_frame){
                photon_setup_api(&editor, it);
                it->pre_frame(&editor.api);
            }
            it = it->next;
        }
        PHOTON_DEBUG_OPT(if (capture) {
            char nameBack[64] = {0};
            char nameFront[64] = {0};
            sprintf(nameFront, "front-%d.bin", photon_ui_frame_number());
            sprintf(nameBack, "back-%d.bin", photon_ui_frame_number());
            photon_ui_snapshot(nameFront, nameBack);
            capture = 0;
        })
        photon_ui_refresh();

        int key = photon_input_read_key();
    all_good:
        PHOTON_DEBUG_OPT(if (key == 19) capture = 1); // ^S
        photon_handle_keypress(&editor, key);
    }

    photon_editor_cleanup(&editor);
    return EXIT_SUCCESS;
}
