#ifndef __PHOTON_BUFFER_H__
#define __PHOTON_BUFFER_H__

typedef struct photon_editor photon_editor_t;
typedef struct photon_buffer photon_buffer_t;
typedef struct photon_buf_options photon_buf_options_t;

photon_buffer_t *photon_create_buffer(photon_editor_t *editor, const photon_buf_options_t *options);
void photon_delete_buffer(photon_editor_t *editor, photon_buffer_t *buffer);

#endif//__PHOTON_BUFFER_H__
