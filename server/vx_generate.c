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
  uint32_t length = 4 + (rand_3() % 6);
  
  uint32_t x, y, z;
  int attempt;
  
  for (attempt = 0; attempt < 12; attempt++) {
    x = (rand_3() % (VX_CHUNK_X - 4)) + 2;
    z = (rand_3() % (VX_CHUNK_Z - 4)) + 2;
    y = 1;
    
    while (y < VX_CHUNK_Y - 20 && chunk->data[x + (z + y * VX_CHUNK_Z) * VX_CHUNK_X] != vx_tile_air) {
      y++;
    }
    
    if (chunk->data[x + (z + (y - 1) * VX_CHUNK_Z) * VX_CHUNK_X] == vx_tile_grass ||
        chunk->data[x + (z + (y - 1) * VX_CHUNK_Z) * VX_CHUNK_X] == vx_tile_sand) {
      break;
    }
  }
  
  if (chunk->data[x + (z + (y - 1) * VX_CHUNK_Z) * VX_CHUNK_X] == vx_tile_sand) return;
  if (attempt == 12) return;
  
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

static float magic_function(float x) {
  x = x - 0.125f;
  x = cosf(x) * cosf(x) - 0.4f;
  
  return x * x * x * 3.85f;
}

int vx_house(vx_chunk_t *chunk) {
  uint32_t x, y, z;

  const int floor_height = 4;

  uint32_t floors = rand() % (VX_CHUNK_Y / floor_height) + 1;
  uint32_t height = floors * floor_height;
  height = height >= VX_CHUNK_Y ? VX_CHUNK_Y - 1 : height;
  uint32_t width = 8 + (rand() % VX_CHUNK_X);
  width = width >= VX_CHUNK_X ? VX_CHUNK_X - 1 : width;

  int attempt;
  for (attempt = 0; attempt < 10; attempt++) {
    x = (rand() % (VX_CHUNK_X - width));
    z = (rand() % (VX_CHUNK_Z - width));
    y = 0;
    
    for(uint32_t i = 0; i < width; i++) {
      for(uint32_t j = 0; j < width; j++) {
        if(chunk->data[x + i + (z + j + y * VX_CHUNK_Z) * VX_CHUNK_X] == vx_tile_air) {
          goto next_attempt;
        }
      }
    }
    break;
  next_attempt: ;
  }
  if (attempt == 10) return 0;

  if (x + width >= VX_CHUNK_X) return 0;
  if (z + width >= VX_CHUNK_Z) return 0;
  if (y + height >= VX_CHUNK_Y) return 0;

  int materials[] = {
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
  };

  int material = materials[rand() % (sizeof(materials) / sizeof(int))];

  uint32_t secure_height = (rand() % height);
  secure_height = secure_height > (height / 3) ? height / 3 : secure_height;

  /* Fill the walls with the material */
  if(rand() % 2 == 0) {
    /* Blocky walls */
    for(uint32_t i = 0; i < width; i++) {
      int rand_seize = 1024;
      for(uint32_t j = 0; j < height; j++) {
        if(j > secure_height) {
          if(rand() % rand_seize == 0) {
            break;
          }
          rand_seize -= (rand_seize / 4);
          rand_seize = rand_seize ? rand_seize : 1;
        }

        chunk->data[x + i + (z + (y + j) * VX_CHUNK_Z) * VX_CHUNK_X] = material;
        chunk->data[x + (z + i + (y + j) * VX_CHUNK_Z) * VX_CHUNK_X] = material;
        chunk->data[x + i + (z + width + (y + j) * VX_CHUNK_Z) * VX_CHUNK_X] = material;
        chunk->data[x + width + (z + i + (y + j) * VX_CHUNK_Z) * VX_CHUNK_X] = material;

        /* Windows with water */
        if(i % 2 == 0 && i) {
          if(j % floor_height == 0 && j) {
            int glass_material = (rand_seize != 1024 && rand() % rand_seize != 0) ? vx_tile_air : vx_tile_glass;
            chunk->data[x + i + (z + (y + j) * VX_CHUNK_Z) * VX_CHUNK_X] = glass_material;
            chunk->data[x + (z + i + (y + j) * VX_CHUNK_Z) * VX_CHUNK_X] = glass_material;
            chunk->data[x + i + (z + width + (y + j) * VX_CHUNK_Z) * VX_CHUNK_X] = glass_material;
            chunk->data[x + width + (z + i + (y + j) * VX_CHUNK_Z) * VX_CHUNK_X] = glass_material;
          }
        }
      }
    }

    /* Filler */
    for(uint32_t i = 1; i < width - 1; i++) {
      for(uint32_t j = 1; j < width - 1; j++) {
        int rand_seize = 512;
        for(uint32_t k = 0; k < height; k++) {
          if(k > secure_height) {
            if(rand() % rand_seize == 0) {
              break;
            }
            rand_seize -= (rand_seize / 2);
            rand_seize = rand_seize ? rand_seize : 1;
          }
          chunk->data[x + i + (z + j + (y + k) * VX_CHUNK_Z) * VX_CHUNK_X] = vx_tile_grass;
        }
      }
    }
  } else {
    /* TODO: Circular walls */
  }
  return 1;
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
  
  int count = (int)((eval_2(14.1 + chunk->chunk_x / 6.0f, 42.5 + chunk->chunk_z / 6.0f) * 4.5f) - 1.5f);
  for (int i = 0; i < count; i++) {
    vx_tree(chunk);
  }
}
