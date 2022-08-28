#ifndef __SERVER_H__
#define __SERVER_H__

#include <protocol.h>
#include <stdint.h>

#define VX_MAX_CHUNKS 1024

#define VX_TOTAL_X 1024
#define VX_TOTAL_Z 1024

typedef struct vx_client_t vx_client_t;

struct vx_client_t {
  char name[20];
  float pos_x, pos_y, pos_z;
  
  void *connection;
};

extern uint32_t vx_seed;

extern vx_chunk_t **vx_chunks;

extern vx_chunk_t **vx_loaded_chunks;
extern int vx_loaded_count;

extern vx_client_t *vx_clients;

void        vx_chunk_init(void);
vx_chunk_t *vx_chunk_load(uint32_t chunk_x, uint32_t chunk_z);
void        vx_chunk_unload(int count);

void vx_generate(vx_chunk_t *chunk);

void         vx_client_init(void);
vx_client_t *vx_client_join(const char *name, void *connection);
void         vx_client_exit(vx_client_t *client);
vx_client_t *vx_client_find(void *connection);
vx_client_t *vx_client_find_name(const char *name);

void vx_command(vx_client_t *client, const char *command);

void vx_server_send(vx_client_t *client, const char *text);
void vx_server_tell(vx_client_t *sender, vx_client_t *client, const char *text);
void vx_server_post(const char *text);
void vx_server_move(vx_client_t *client, float pos_x, float pos_y, float pos_z);

#endif
