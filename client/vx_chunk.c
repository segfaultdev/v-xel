#include <client.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

vx_chunk_t *vx_chunks = NULL;

void vx_chunk_init(void) {
  vx_chunks = malloc(VX_TOTAL_SIDE * VX_TOTAL_SIDE * sizeof(vx_chunk_t));
  
  for (uint32_t z = 0; z < VX_TOTAL_SIDE; z++) {
    for (uint32_t x = 0; x < VX_TOTAL_SIDE; x++) {
      vx_chunks[x + z * VX_TOTAL_SIDE].loaded = 0;
      vx_chunks[x + z * VX_TOTAL_SIDE].requested = 0;
    }
  }
}

uint8_t vx_chunk_get(uint32_t x, uint32_t y, uint32_t z) {
  if (y >= VX_CHUNK_Y) return 0;
  
  uint32_t chunk_x = (x / VX_CHUNK_X);
  uint32_t chunk_z = (z / VX_CHUNK_Z);
  
  uint32_t mod_x = (chunk_x % VX_TOTAL_SIDE);
  uint32_t mod_z = (chunk_z % VX_TOTAL_SIDE);
  
  uint32_t tile_x = (x % VX_CHUNK_X);
  uint32_t tile_z = (z % VX_CHUNK_Z);
  
  vx_chunk_t *chunk = (vx_chunks + (mod_x + mod_z * VX_TOTAL_SIDE));
  
  if (chunk->chunk_x == chunk_x && chunk->chunk_z == chunk_z && chunk->loaded) {
    return chunk->data[tile_x + (tile_z + y * VX_CHUNK_Z) * VX_CHUNK_X];
  } else if (!chunk->requested) {
    chunk->chunk_x = chunk_x;
    chunk->chunk_z = chunk_z;
    
    chunk->loaded = 0;
    chunk->requested = 1;
    
    return vx_tile_air;
  } else {
    return vx_tile_air;
  }
}
