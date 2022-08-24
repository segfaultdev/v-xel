#ifndef __SERVER_H__
#define __SERVER_H__

#include <stdint.h>
#include <stdlib.h>

#define VX_CLOUD_SIDE 512
#define VX_TOTAL_SIDE 20

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

extern vx_chunk_t *vx_chunks;
extern char vx_name[];

extern vx_client_t *vx_clients;
extern int vx_client_count;

extern vx_client_t vx_player;

size_t vx_packet_size(uint16_t type);

void    vx_chunk_init(void);
uint8_t vx_chunk_get(uint32_t x, uint32_t y, uint32_t z);

void vx_loader_init(void);
void vx_loader_place(uint8_t tile, uint32_t x, uint32_t y, uint32_t z);
void vx_loader_update(float pos_x, float pos_y, float pos_z);

void vx_client_add(const char *name);
void vx_client_remove(const char *name);
void vx_client_update(const char *name, float pos_x, float pos_y, float pos_z);

#endif
