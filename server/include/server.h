#ifndef __SERVER_H__
#define __SERVER_H__

#include <stdint.h>
#include <stdlib.h>

#define VX_MAX_CHUNKS  64
#define VX_MAX_CLIENTS 32

#define VX_TOTAL_X 1024
#define VX_TOTAL_Z 1024

#define VX_CHUNK_X 32
#define VX_CHUNK_Y 128
#define VX_CHUNK_Z 32

#define vx_fatal(...) do {printf("error: " __VA_ARGS__); exit(1);} while (0)

enum {
  vx_tile_air,
  vx_tile_water,
  
  vx_tile_dirt,
  vx_tile_grass,
  vx_tile_stone,
  vx_tile_sand,
  
  vx_tile_trunk,
  vx_tile_leaves,
  
  vx_tile_count,
};

typedef struct vx_chunk_t vx_chunk_t;

struct vx_chunk_t {
  uint32_t chunk_x, chunk_z;
  uint8_t data[VX_CHUNK_X * VX_CHUNK_Y * VX_CHUNK_Z];
  
  uint8_t requested, loaded, dirty;
};

typedef struct vx_client_t vx_client_t;

struct vx_client_t {
  char name[20];
  float pos_x, pos_y, pos_z;
  
  void *connection; // connection using any library :p
};

enum {
  vx_packet_request,
  vx_packet_chunk,
  vx_packet_place,
  vx_packet_welcome, // lets the server and other clients know a client just connected
  vx_packet_bye,     // sent server -> client, lets other clients know a client disconnected
  vx_packet_update,  // updates position of a client on server or other clients
};

typedef struct vx_packet_t vx_packet_t;

struct vx_packet_t {
  uint16_t type;
  
  union {
    uint32_t request[2];
    vx_chunk_t chunk;
    
    struct {
      uint32_t x, y, z;
      uint8_t tile;
    } __attribute__((packed)) place;
    
    char welcome[20];
    char bye[20];
    
    struct {
      char name[20];
      float pos_x, pos_y, pos_z;
    } __attribute__((packed)) update;
  };
} __attribute__((packed));

extern uint32_t vx_seed;

extern vx_chunk_t **vx_chunks;

extern vx_chunk_t **vx_loaded_chunks;
extern int vx_loaded_count;

extern vx_client_t *vx_clients;

size_t vx_packet_size(uint16_t type);

void        vx_chunk_init(void);
vx_chunk_t *vx_chunk_load(uint32_t chunk_x, uint32_t chunk_z);
void        vx_chunk_unload(int count);

void vx_generate(vx_chunk_t *chunk);

void         vx_client_init(void);
vx_client_t *vx_client_join(const char *name, void *connection);
void         vx_client_exit(vx_client_t *client);
vx_client_t *vx_client_find(void *connection);
vx_client_t *vx_client_find_name(const char *name);

#endif
