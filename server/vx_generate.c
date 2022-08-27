#include <server.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>

uint32_t seed_3a = 1;
uint32_t seed_3b = 1;
uint32_t seed_3c = 1;

static uint32_t rand_3() {
  seed_3a = (seed_3a * 1664525 + 1013904223) % 1431655765;
  seed_3b = (seed_3b * 16843019 + 826366249) % 1431655765;
  seed_3c = (seed_3c * 16843031 + 826366237) % 1431655765;
  
  return seed_3a + seed_3b + seed_3c;
}

static float lerp(float x, float y, float w) {
  if (w < 0) return x;
  if (w > 1) return y;
  
  return (y - x) * ((w * (w * 6.0f - 15.0f) + 10.0f) * w * w * w) + x;
}

static float grad_1(int x) {
  x %= 16;
  while (x < 0) x += 16;
  
  x += 7 * vx_seed;
  
  seed_3a = (x * 1664525 + 1013904223) % 1431655765;
  seed_3b = (x * 16843019 + 826366249) % 1431655765;
  seed_3c = ((seed_3a + seed_3b) * 16843031 + 826366237) % 1431655765;
  
  int count = rand_3() % 2;
  
  for (int i = 0; i < count; i++) {
    rand_3();
  }
  
  return fabs(rand_3() % 65537) / 65537.0f;
}

static float grad_2(int x, int y) {
  x %= 8;
  y %= 16;
  
  while (x < 0) x += 8;
  while (y < 0) y += 16;
  
  x +=  3 * vx_seed;
  y += 17 * vx_seed;
  
  seed_3a = (x * 1664525 + 1013904223) % 1431655765;
  seed_3b = (y * 16843019 + 826366249) % 1431655765;
  seed_3c = ((seed_3a + seed_3b) * 16843031 + 826366237) % 1431655765;
  
  int count = rand_3() % 2;
  
  for (int i = 0; i < count; i++) {
    rand_3();
  }
  
  return fabs(rand_3() % 65537) / 65537.0;
}

static float eval_1(float x) {
  x = x - (int)(x / 16) * 16;
  while (x < 0) x += 16;
  
  float dx = x - floorf(x);
  return lerp(grad_1(floorf(x + 0)), grad_1(floorf(x + 1)), dx);
}

static float eval_2(float x, float y) {
  x = x - (int)(x / 8) * 8;
  y = y - (int)(y / 8) * 16;
  
  while (x < 0) x += 8;
  while (y < 0) y += 16;
  
  x -= (y / 2);
  
  float dx = x - floorf(x);
  float dy = y - floorf(y);
  
  if (1 - dx < dy) {
    float tmp = dx;
    
    dx = 1 - dy;
    dy = 1 - tmp;
    
    float s = ((dx - dy) + 1) / 2;
    float h = dx + dy;
    
    float t = lerp(grad_2(floorf(x + 0), floorf(y + 1)), grad_2(floorf(x + 1), floorf(y + 0)), s);
    return lerp(grad_2(floorf(x + 1), floorf(y + 1)), t, h);
  } else {
    float s = ((dx - dy) + 1) / 2;
    float h = dx + dy;
    
    float t = lerp(grad_2(floorf(x + 0), floorf(y + 1)), grad_2(floorf(x + 1), floorf(y + 0)), s);
    return lerp(grad_2(floorf(x + 0), floorf(y + 0)), t, h);
  }
}

static uint8_t get_chunk_tile(vx_chunk_t *chunk, uint32_t x, uint32_t y, uint32_t z) {
  return chunk->data[x + (z + y * VX_CHUNK_Z) * VX_CHUNK_X];
}

static void set_chunk_tile(vx_chunk_t *chunk, uint8_t tile, uint32_t x, uint32_t y, uint32_t z) {
  chunk->data[x + (z + y * VX_CHUNK_Z) * VX_CHUNK_X] = tile;
}

void vx_tree(vx_chunk_t *chunk) {
  uint32_t x, y, z;
  int attempt;
  
  for (attempt = 0; attempt < 12; attempt++) {
    x = (rand_3() % (VX_CHUNK_X - 4)) + 2;
    z = (rand_3() % (VX_CHUNK_Z - 4)) + 2;
    y = 1;
    
    while (y < VX_CHUNK_Y - 20 && get_chunk_tile(chunk, x, y, z) != vx_tile_air) {
      y++;
    }
    
    if (get_chunk_tile(chunk, x, y - 1, z) == vx_tile_grass || get_chunk_tile(chunk, x, y - 1, z) == vx_tile_sand) {
      break;
    }
  }
  
  if (attempt == 12) return;
  int variant = 0;
  
  if (get_chunk_tile(chunk, x, y - 1, z) == vx_tile_sand) {
    if (rand_3() % 8 == 0 && x >= 4 && z >= 4 && x < VX_CHUNK_X - 5 && z < VX_CHUNK_Z - 5) {
      variant = 2;
    } else {
      return;
    }
  }
  
  if (variant == 0 && y >= 64 && rand_3() % 2 == 0 && x >= 3 && z >= 3 && x < VX_CHUNK_X - 4 && z < VX_CHUNK_Z - 4) {
    variant = 1;
  }
  
  // 0 = unbranded tree uwu
  // 1 = pine tree
  // 2 = palm tree
  
  if (variant == 0) {
    int length = 4 + (rand_3() % 6);
    
    for (int i = 0; i < length; i++) {
      set_chunk_tile(chunk, vx_tile_trunk, x, y + i, z);
    }
    
    for (int i = -2; i <= 2; i++) {
      for (int j = -2; j <= 1; j++) {
        for (int k = -2; k <= 2; k++) {
          if (i * i + j * j + k * k > 5) continue;
          
          if (get_chunk_tile(chunk, x + k, y + (length - 1) + j, z + i) == vx_tile_air) {
            set_chunk_tile(chunk, vx_tile_leaves, x + k, y + (length - 1) + j, z + i);
          }
        }
      }
    }
  } else if (variant == 1) {
    int length = 10 + (rand_3() % 6);
    
    for (int i = 0; i < length; i++) {
      set_chunk_tile(chunk, vx_tile_trunk, x, y + i, z);
    }
    
    for (int i = -3; i <= 3; i++) {
      for (int j = 0; j <= 8; j++) {
        for (int k = -3; k <= 3; k++) {
          if (j % 2 || i * i + k * k >= (3.5f - j / 3.0f) * (3.5f - j / 3.0f)) {
            continue;
          }
          
          if (get_chunk_tile(chunk, x + k, y + (length - 1) + j - 7, z + i) == vx_tile_air) {
            set_chunk_tile(chunk, vx_tile_leaves, x + k, y + (length - 1) + j - 7, z + i);
          }
        }
      }
    }
  } else if (variant == 2) {
    int length = 4 + (rand_3() % 4);
    
    float angle = 2.0f * 3.14159f * (fabs(rand_3() % 65537) / 65537.0f);
    
    float pos_x = x + 0.5f;
    float pos_z = z + 0.5f;
    
    float delta_x = 0.0f;
    float delta_z = 0.0f;
    
    float accel_x = cosf(angle) * 0.067f;
    float accel_z = sinf(angle) * 0.067f;
    
    for (int i = 0; i < length; i++) {
      pos_x += delta_x;
      pos_z += delta_z;
      
      delta_x += accel_x;
      delta_z += accel_z;
      
      set_chunk_tile(chunk, vx_tile_trunk, (int)(pos_x), y + i, (int)(pos_z));
    }
    
    for (int i = -3; i <= 3; i++) {
      for (int k = -3; k <= 3; k++) {
        if (i && k) continue;
        
        int d = i + k;
        if (d < 0) d = -d;
        
        int j = -((d - 0) * (d - 3)) / 2;
        
        if (get_chunk_tile(chunk, (int)(pos_x) + k, y + length + j, (int)(pos_z) + i) == vx_tile_air) {
          set_chunk_tile(chunk, vx_tile_leaves, (int)(pos_x) + k, y + length + j, (int)(pos_z) + i);
        }
      }
    }
  }
}

static float magic_function(float x) {
  x = x - 0.125f;
  x = cosf(x) * cosf(x) - 0.4f;
  
  return x * x * x * 3.85f;
}

void vx_generate(vx_chunk_t *chunk) {
  for (uint32_t i = 0; i < VX_CHUNK_Z; i++) {
    for (uint32_t j = 0; j < VX_CHUNK_X; j++) {
      uint32_t x = j + chunk->chunk_x * VX_CHUNK_X;
      uint32_t z = i + chunk->chunk_z * VX_CHUNK_Z;
      
      const float scale_factor = 1.075f;
      
      float value_1 = eval_2(45.6f + x / 64.0f, z / 64.0f) * scale_factor;
      float value_2 = roundf(value_1 * 5.0f) / 5.0f;
      float value_3 = eval_2(x / 32.0f + value_2 * 91.1f, z / 32.0f - value_2 * 33.6f) * scale_factor;
      float value_4 = 1.0f - fabs(1.0f - eval_2(78.9f + z / 16.0f, x / 16.0f) * scale_factor * 2.0f);
      float value_5 = eval_2(z / 32.0f, x / 32.0f) * scale_factor;
      float value_6 = eval_2(x / 64.0f, z / 64.0f) * scale_factor;
      float value_7 = eval_2(z / 20.0f, x / 20.0f) * scale_factor;
      
      uint32_t height = 32.0f + floorf(80.0f * value_6 * value_6 * lerp(value_3, value_4, value_5 * value_5));
      
      for (uint32_t y = 0; y <= height && y < VX_CHUNK_Y; y++) {
        if (y < height - roundf(lerp(8.0f, 2.0f, height / 114.0f))) {
          chunk->data[j + (i + y * VX_CHUNK_Z) * VX_CHUNK_X] = vx_tile_stone;
        } else if (height < 38) {
          if (y <= height) {
            chunk->data[j + (i + y * VX_CHUNK_Z) * VX_CHUNK_X] = vx_tile_sand;
          }
        } else {
          if (y == height && y < 80.0f + value_7 * 48.0f) {
            chunk->data[j + (i + y * VX_CHUNK_Z) * VX_CHUNK_X] = vx_tile_grass;
          } else if (y < height) {
            chunk->data[j + (i + y * VX_CHUNK_Z) * VX_CHUNK_X] = vx_tile_dirt;
          }
        }
      }
      
      for (int64_t y = height + 1; y < VX_CHUNK_Y; y++) {
        chunk->data[j + (i + y * VX_CHUNK_Z) * VX_CHUNK_X] = (y <= 36 ? vx_tile_water : vx_tile_air);
      }
    }
  }
  
  float tree_x = ((chunk->chunk_x + 0.5f) * VX_CHUNK_X) / 64.0f;
  float tree_z = ((chunk->chunk_z + 0.5f) * VX_CHUNK_Z) / 64.0f;
  
  float value = eval_2(tree_x, tree_z);
  
  int count = (int)(magic_function(value) * 64.0f - eval_2(tree_z + 55.17f, tree_x - 3.13f) * 12.0f);
  
  for (int i = 0; i < count; i++) {
    vx_tree(chunk);
  }
}
