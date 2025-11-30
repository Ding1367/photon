#ifndef __EXTENSIONS_H__
#define __EXTENSIONS_H__
#include <stdint.h>

typedef struct photon_editor photon_editor_t;
typedef struct photon_extension photon_extension_t;

void photon_setup_api(photon_editor_t *editor, photon_extension_t *ext);
int photon_trigger_hook(photon_editor_t *editor, int id, uintptr_t data);

#endif//__EXTENSIONS_H__
