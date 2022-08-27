#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define VX_CHUNK_X 32
#define VX_CHUNK_Y 128
#define VX_CHUNK_Z 32

#define VX_MAX_CLIENTS 32

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
  vx_tile_wood,
  vx_tile_light_wood,
  vx_tile_dark_wood,
  
  vx_tile_red_bricks,
  vx_tile_gray_bricks,
  vx_tile_glass,
  
  vx_tile_black_block,
  vx_tile_white_block,
  vx_tile_red_block,
  vx_tile_orange_block,
  vx_tile_yellow_block,
  vx_tile_green_block,
  vx_tile_cyan_block,
  vx_tile_blue_block,
  vx_tile_purple_block,
  vx_tile_magenta_block,
  
  vx_tile_count,
};

typedef struct vx_chunk_t vx_chunk_t;

struct vx_chunk_t {
  uint32_t chunk_x, chunk_z;
  uint8_t data[VX_CHUNK_X * VX_CHUNK_Y * VX_CHUNK_Z];
  
  uint8_t requested, loaded, dirty, padding;
} __attribute__((packed));

enum {
  vx_packet_request,
  vx_packet_chunk,
  vx_packet_place,
  vx_packet_welcome,
  vx_packet_bye,
  vx_packet_update,
  vx_packet_chat,
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
    
    char chat[65536];
  };
} __attribute__((packed));

#endif
