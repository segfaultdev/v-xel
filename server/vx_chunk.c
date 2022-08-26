#include <server.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

vx_chunk_t **vx_chunks = NULL;

vx_chunk_t **vx_loaded_chunks = NULL;
int vx_loaded_count = 0;

void vx_chunk_init(void) {
  vx_chunks = malloc(VX_TOTAL_X * VX_TOTAL_Z * sizeof(vx_chunk_t *));
  vx_loaded_chunks = malloc((VX_MAX_CHUNKS * 2) * sizeof(vx_chunk_t *));
  
  for (uint32_t z = 0; z < VX_TOTAL_Z; z++) {
    for (uint32_t x = 0; x < VX_TOTAL_X; x++) {
      vx_chunks[x + z * VX_TOTAL_X] = NULL;
    }
  }
}

vx_chunk_t *vx_chunk_load(uint32_t chunk_x, uint32_t chunk_z) {
  chunk_x %= VX_TOTAL_X;
  chunk_z %= VX_TOTAL_Z;
  
  if (vx_chunks[chunk_x + chunk_z * VX_TOTAL_X]) {
    return vx_chunks[chunk_x + chunk_z * VX_TOTAL_X];
  }
  
  if (vx_loaded_count > VX_MAX_CHUNKS) {
    vx_chunk_unload(vx_loaded_count - VX_MAX_CHUNKS);
  }
  
  vx_chunk_t *chunk = malloc(sizeof(vx_chunk_t));
  chunk->chunk_x = chunk_x;
  chunk->chunk_z = chunk_z;
  
  char path[40];
  sprintf(path, "chunks/%u-%u.bin", chunk_x, chunk_z);
  
  FILE *file = fopen(path, "rb");
  
  if (file) {
    printf("loading chunk (%u, %u)\n", chunk_x, chunk_z);
    uint32_t offset = 0;
    
    while (!feof(file)) {
      uint32_t count = 0;
      int shift = 0;
      
      for (;;) {
        uint8_t byte;
        fread(&byte, 1, 1, file);
        
        count |= ((uint32_t)(byte & 0x7F) << (shift * 7));
        shift++;
        
        if (!(byte & 0x80)) break;
      }
      
      if (count % 2) {
        fread(chunk->data + offset, 1, count >> 1, file);
      } else {
        uint8_t byte;
        fread(&byte, 1, 1, file);
        
        if (count >> 1 > VX_CHUNK_X * VX_CHUNK_Z * VX_CHUNK_Y - offset) {
          count = (VX_CHUNK_X * VX_CHUNK_Z * VX_CHUNK_Y - offset) << 1;
        }
        
        memset(chunk->data + offset, byte, count >> 1);
      }
      
      offset += (count >> 1);
    }
    
    fclose(file);
    chunk->dirty = 0;
  } else {
    printf("generating chunk (%u, %u)\n", chunk_x, chunk_z);
    
    vx_generate(chunk);
    chunk->dirty = 1;
  }
  
  vx_chunks[chunk_x + chunk_z * VX_TOTAL_X] = chunk;
  vx_loaded_chunks[vx_loaded_count++] = chunk;
  
  return chunk;
}

static void vx_chunk_unload_single(int index) {
  if (!vx_loaded_chunks[index]) return;
  vx_chunks[vx_loaded_chunks[index]->chunk_x + vx_loaded_chunks[index]->chunk_z * VX_TOTAL_X] = NULL;
  
  if (vx_loaded_chunks[index]->dirty) {
    char path[40];
    sprintf(path, "chunks/%u-%u.bin", vx_loaded_chunks[index]->chunk_x, vx_loaded_chunks[index]->chunk_z);
    
    FILE *file = fopen(path, "w+b");
    
    if (file) {
      uint32_t count = 0;
      uint8_t last = 0;
      
      for (uint32_t i = 0; i < VX_CHUNK_X * VX_CHUNK_Z * VX_CHUNK_Y; i++) {
        uint8_t curr = vx_loaded_chunks[index]->data[i];
        if (!i) last = curr;
        
        if (curr != last) {
          count <<= 1; // bottom bit is 1 when raw encoding, 0 when run-length encoding
          
          while (count) {
            uint8_t byte = count & 0x7F;
            count >>= 7;
            
            if (count) byte |= 0x80;
            fwrite(&byte, 1, 1, file);
          }
          
          fwrite(&last, 1, 1, file);
        }
        
        last = curr;
        count++;
      }
      
      if (count) {
        count <<= 1; // bottom bit is 1 when raw encoding, 0 when run-length encoding
        
        while (count) {
          uint8_t byte = count & 0x7F;
          count >>= 7;
          
          if (count) byte |= 0x80;
          fwrite(&byte, 1, 1, file);
        }
        
        fwrite(&last, 1, 1, file);
      }
      
      fclose(file);
    } else {
      vx_fatal("cannot save chunk (%u, %u)\n", vx_loaded_chunks[index]->chunk_x, vx_loaded_chunks[index]->chunk_z);
    }
  }
  
  free(vx_loaded_chunks[index]);
  vx_loaded_chunks[index] = NULL;
}

void vx_chunk_unload(int count) {
  printf("unloading %d chunks out of %d\n", count, vx_loaded_count);
  
  for (int i = 0; i < count; i++) {
    vx_chunk_unload_single(i);
  }
  
  vx_loaded_count -= count;
  
  for (int i = 0; i < vx_loaded_count; i++) {
    vx_loaded_chunks[i] = vx_loaded_chunks[i + count];
  }
}
