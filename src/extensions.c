#include "extensions.h"
#include "photon.h"

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

