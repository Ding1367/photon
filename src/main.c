#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <dlfcn.h>
#include <limits.h>
#include "photon.h"
#include "photon_debug.h"
#include "extensions.h"
#include "input.h"
#include "buffer.h"
#include "ui.h"

static const char *errorMessages[] = {
    NULL,
    "Invalid parameters",
    "Not enough memory"
};

photon_buffer_t *ctx = NULL;

static int predraw(photon_draw_req_t *req){
    if (!ctx) return 1;
    int y = req->y, x = req->x;
    int p_y = ctx->y, p_x = ctx->x;
    int rows = ctx->rows, cols = ctx->cols;
    if (y < p_y || y - p_y >= rows) {
        if (ctx->y + 1 == rows) return 0;
        ctx->y++;
        ctx->x = p_x;
        return 0;
    }
    if (x < p_x || x - p_x >= cols) return 0;
    return 1;
}

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
    editor.pre_draw = &predraw;

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

    editor.api.ui.width = photon_ui_width();
    editor.api.ui.height = photon_ui_height();

    PHOTON_DEBUG_OPT(int capture = 0);
    // logic...
    while (!editor.should_quit){
        photon_ui_clear();
        editor.first_buf->draw(&editor.api, editor.first_buf);
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
