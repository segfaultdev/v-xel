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

void vx_tree(vx_chunk_t *chunk) {
  uint32_t x, y, z;
  int attempt;
  
  for (attempt = 0; attempt < 10; attempt++) {
    x = (rand_3() % (VX_CHUNK_X - 4)) + 2;
    z = (rand_3() % (VX_CHUNK_Z - 4)) + 2;
    y = 1;
    
    while (chunk->data[x + (z + y * VX_CHUNK_Z) * VX_CHUNK_X]) {
      y++;
    }
    
    if (chunk->data[x + (z + (y - 1) * VX_CHUNK_Z) * VX_CHUNK_X] == vx_tile_grass) {
      break;
    }
  }
  
  if (attempt == 10) return;
  uint32_t length = 4 + (rand_3() % 3);
  
  for (int i = 0; i < length; i++) {
    chunk->data[x + (z + (y + i) * VX_CHUNK_Z) * VX_CHUNK_X] = vx_tile_trunk;
  }
  
  for (int i = -2; i <= 2; i++) {
    for (int j = -2; j <= 1; j++) {
      for (int k = -2; k <= 2; k++) {
        if (i * i + j * j + k * k > 5) {
          continue;
        }
        
        if (chunk->data[(x + k) + ((z + i) + (y + (length - 1) + j) * VX_CHUNK_Z) * VX_CHUNK_X] == vx_tile_air) {
          chunk->data[(x + k) + ((z + i) + (y + (length - 1) + j) * VX_CHUNK_Z) * VX_CHUNK_X] = vx_tile_leaves;
        }
      }
    }
  }
}

void vx_generate(vx_chunk_t *chunk) {
  for (uint32_t i = 0; i < VX_CHUNK_Z; i++) {
    for (uint32_t j = 0; j < VX_CHUNK_X; j++) {
      uint32_t x = j + chunk->chunk_x * VX_CHUNK_X;
      uint32_t z = i + chunk->chunk_z * VX_CHUNK_Z;
      
      float value_1 = eval_2(45.6f + x / 64.0f, z / 64.0f);
      float value_2 = roundf(value_1 * 5.0f) / 5.0f;
      float value_3 = eval_2(x / 32.0f + value_2 * 91.1f, z / 32.0f - value_2 * 33.6f);
      float value_4 = 1.0f - fabs(1.0f - eval_2(78.9f + z / 16.0f, x / 16.0f) * 2.0f);
      float value_5 = eval_2(z / 32.0f, x / 32.0f);
      float value_6 = eval_2(x / 64.0f, z / 64.0f);
      
      uint32_t height = 32.0f + floorf(80.0f * value_6 * value_6 * lerp(value_3, value_4, value_5 * value_5));
      
      for (uint32_t y = 0; y <= height && y < VX_CHUNK_Y; y++) {
        if (y < height - 6) {
          chunk->data[j + (i + y * VX_CHUNK_Z) * VX_CHUNK_X] = vx_tile_stone;
        } else if (height < 38) {
          if (y <= height) {
            chunk->data[j + (i + y * VX_CHUNK_Z) * VX_CHUNK_X] = vx_tile_sand;
          }
        } else {
          if (y < height) {
            chunk->data[j + (i + y * VX_CHUNK_Z) * VX_CHUNK_X] = vx_tile_dirt;
          } else if (y == height) {
            chunk->data[j + (i + y * VX_CHUNK_Z) * VX_CHUNK_X] = vx_tile_grass;
          }
        }
      }
      
      for (int64_t y = height + 1; y < VX_CHUNK_Y; y++) {
        chunk->data[j + (i + y * VX_CHUNK_Z) * VX_CHUNK_X] = (y <= 36 ? vx_tile_water : vx_tile_air);
      }
    }
  }
  
  int count = (int)((eval_2(14.1 + chunk->chunk_x / 6.0f, 42.5 + chunk->chunk_z / 6.0f) * 4.5f) - 1.5f);
  
  for (int i = 0; i < count; i++) {
    vx_tree(chunk);
  }
}
