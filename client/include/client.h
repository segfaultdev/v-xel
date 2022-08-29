#ifndef __SERVER_H__
#define __SERVER_H__

#include <protocol.h>
#include <stdint.h>

#define VX_TOTAL_SIDE 32

typedef struct vx_client_t vx_client_t;

struct vx_client_t {
  char name[20];
  float pos_x, pos_y, pos_z;
};

typedef struct vx_config_t vx_config_t;

struct vx_config_t {
  // settings passed to the render.fs shader, 4-byte aligned to make my life a whole lot easier
  uint32_t do_shadows;
  uint32_t do_occlusion;
  uint32_t do_borders;
  uint32_t do_reflections;
  uint32_t do_sky_decoration;
  uint32_t do_atmosphere;
};

extern vx_chunk_t *vx_chunks;
extern char vx_name[];

extern vx_client_t *vx_clients;
extern int vx_client_count;

extern char **vx_messages;
extern int vx_message_count;

extern vx_client_t vx_player;
extern float vx_time;

extern int vx_iterations;

void    vx_chunk_init(void);
uint8_t vx_chunk_get(uint32_t x, uint32_t y, uint32_t z);

void vx_loader_init(const char *server_ip);
void vx_loader_place(uint8_t tile, uint32_t x, uint32_t y, uint32_t z);
void vx_loader_update(float pos_x, float pos_y, float pos_z);
void vx_loader_chat(const char *data);

void vx_client_add(const char *name);
void vx_client_remove(const char *name);
void vx_client_update(const char *name, float pos_x, float pos_y, float pos_z);

void vx_chat_add(const char *name, const char *data);

// on client implementation

void on_chunk_update(uint32_t chunk_x, uint32_t chunk_z);
void on_place_update(uint8_t tile, uint32_t x, uint32_t y, uint32_t z);
void on_client_update(void);

#endif
