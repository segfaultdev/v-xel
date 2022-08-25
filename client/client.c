#include <client.h>
#include <raylib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define VX_WIDTH       288
#define VX_HEIGHT      162
#define VX_ZOOM        4
#define VX_ITER        256
#define VX_SHADOW_ITER 768
#define VX_LIMIT       3

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define SQUARE(x) ((x) * (x))

#define MATH_E 2.71828f

#define fast_sin(num) (fast_cos((num) + 1.5f * PI))
#define fast_abs(num) ((num) < 0 ? -(num) : (num))

char vx_name[20];

uint8_t *cloud_map = NULL;

vx_client_t vx_player = (vx_client_t){
  .pos_x = 16384.0f,
  .pos_y = 16384.0f,
  .pos_z = 16384.0f,
};

float last_x = 0.0f;
float last_y = 0.0f;
float last_z = 0.0f;

float vel_y = 0;
int on_ground = 0;

float angle_lr = 0;
float angle_ud = 0;

int selected = 1;

int sel_x = -1;
int sel_y = -1;
int sel_z = -1;

int sel_face = -1;

float daytime = 0.25f;

float fast_cos(float x) {
  float tp = 1 / (2 * PI);

  x = fast_abs(x * tp);
  x -= 0.25f + (int)(x + 0.25f);
  x *= 16.0f * (fast_abs(x) - 0.5f);

  // optional
  x += 0.225f * x * (fast_abs(x) - 1.0f);

  return x;
}

uint32_t seed_3a = 1;
uint32_t seed_3b = 1;
uint32_t seed_3c = 1;

static uint32_t rand_3() {
  seed_3a = (seed_3a * 1664525 + 1013904223) % 1431655765;
  seed_3b = (seed_3b * 16843019 + 826366249) % 1431655765;
  seed_3c = (seed_3c * 16843031 + 826366237) % 1431655765;
  
  return seed_3a + seed_3b + seed_3c;
}

static float grad_2(int x, int y) {
  x %= 8;
  y %= 16;
  
  while (x < 0) x += 8;
  while (y < 0) y += 16;
  
  seed_3a = (x * 1664525 + 1013904223) % 1431655765;
  seed_3b = (y * 16843019 + 826366249) % 1431655765;
  seed_3c = ((seed_3a + seed_3b) * 16843031 + 826366237) % 1431655765;
  
  int count = rand_3() % 2;
  
  for (int i = 0; i < count; i++) {
    rand_3();
  }
  
  return fabs(rand_3() % 65537) / 65537.0;
}

static float lerp(float x, float y, float w) {
  if (w < 0) return x;
  if (w > 1) return y;
  
  return (y - x) * ((w * (w * 6.0f - 15.0f) + 10.0f) * w * w * w) + x;
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

uint8_t get_world(uint32_t x, uint32_t y, uint32_t z) {
  return vx_chunk_get(x, y, z);
}

int is_solid(uint32_t x, uint32_t y, uint32_t z) {
  int tile = get_world(x, y, z);
  return (tile != vx_tile_air && tile != vx_tile_water);
}

Color get_color(int x, int y, int z) {
  uint8_t tile = get_world(x, y, z);
  
  if (tile == vx_tile_grass) {
    return (Color){63, 223, 63, 255};
  } else if (tile == vx_tile_dirt) {
    return (Color){159, 79, 15, 255};
  } else if (tile == vx_tile_water) {
    return (Color){63, 95, 255, 255};
  } else if (tile == vx_tile_stone) {
    return (Color){159, 159, 159, 255};
  } else if (tile == vx_tile_sand) {
    return (Color){255, 255, 127, 255};
  } else if (tile == vx_tile_trunk) {
    return (Color){79, 39, 7, 255};
  } else if (tile == vx_tile_leaves) {
    return (Color){31, 111, 31, 255};
  }
  
  return BLACK;
}

void set_world(uint8_t value, uint32_t x, uint32_t y, uint32_t z) {
  vx_loader_place(value, x, y, z);
}

float get_hue(float r, float g, float b) {
  float min = r;
  if (g < min) min = g;
  if (b < min) min = b;
  
  float hue = 0.0f;
  
  if (r >= g && r >= b) {
    hue = 1.0f + (g - b) / (r - min);
  } else if (g >= r && g >= b) {
    hue = 3.0f + (b - r) / (g - min);
  } else {
    hue = 5.0f + (r - g) / (b - min);
  }
  
  return hue / 6.0f;
}

Color plot_shadow(float cam_x, float cam_y, float cam_z, float dir_x, float dir_y, float dir_z, int limit) {
  uint32_t pos_x = floorf(cam_x);
  uint32_t pos_y = floorf(cam_y);
  uint32_t pos_z = floorf(cam_z);
  
  float side_x = 0;
  float side_y = 0;
  float side_z = 0;
  
  float delta_x = fast_abs(1 / dir_x);
  float delta_y = fast_abs(1 / dir_y);
  float delta_z = fast_abs(1 / dir_z);
  
  int step_x = 0;
  int step_y = 0;
  int step_z = 0;
  
  int hit_side = 0;
  
  if (dir_x < 0) {
    step_x = -1;
    side_x = (cam_x - pos_x) * delta_x;
  } else {
    step_x = 1;
    side_x = ((pos_x + 1) - cam_x) * delta_x;
  }
  
  if (dir_y < 0) {
    step_y = -1;
    side_y = (cam_y - pos_y) * delta_y;
  } else {
    step_y = 1;
    side_y = ((pos_y + 1) - cam_y) * delta_y;
  }
  
  if (dir_z < 0) {
    step_z = -1;
    side_z = (cam_z - pos_z) * delta_z;
  } else {
    step_z = 1;
    side_z = ((pos_z + 1) - cam_z) * delta_z;
  }
  
  for (int i = 0; limit && i < VX_ITER; i++) {
    if (side_x <= side_y && side_x <= side_z) {
      side_x += delta_x;
      pos_x += step_x;
      
      hit_side = 0;
    } else if (side_y <= side_x && side_y <= side_z) {
      side_y += delta_y;
      pos_y += step_y;
      
      hit_side = 1;
    } else {
      side_z += delta_z;
      pos_z += step_z;
      
      hit_side = 2;
    }
    
    float dist;
    
    if (hit_side == 0) {
      dist = side_x - delta_x;
    } else if (hit_side == 1) {
      dist = side_y - delta_y;
    } else if (hit_side == 2) {
      dist = side_z - delta_z;
    }
    
    if (pos_y < 0 || pos_y >= VX_CHUNK_Y) break;
    uint8_t tile = get_world(pos_x, pos_y, pos_z);
    
    if (tile == vx_tile_water && !get_world(pos_x, pos_y + 1, pos_z)) {
      float hit_x = cam_x + dir_x * dist;
      float hit_y = cam_y + dir_y * dist;
      float hit_z = cam_z + dir_z * dist; 
      
      if (hit_y - pos_y >= 0.75f) {
        float w = (0.75f - (hit_y - pos_y)) / dir_y;
        
        if (w * dir_x + (hit_x - pos_x) < 0.0f || w * dir_x + (hit_x - pos_x) >= 1.0f ||
            w * dir_z + (hit_z - pos_z) < 0.0f || w * dir_z + (hit_z - pos_z) >= 1.0f) {
          tile = 0;
        } else {
          hit_side = 1;
          dist += w;
        }
      }
    }
    
    if (tile) {
      return (Color){127, 127, 127, 255};
    }
  }
  
  
  float old_y = dir_y;
  
  if (dir_y < 0.000001f) dir_y = 0.000001f;
  float ceil_w = (160.0f - cam_y) / dir_y;
  
  float ceil_x = fmodf((cam_x + dir_x * ceil_w) + daytime * VX_CLOUD_SIDE, VX_CLOUD_SIDE);
  while (ceil_x < 0.0f) ceil_x += VX_CLOUD_SIDE;
  
  float ceil_z = fmodf((cam_z + dir_z * ceil_w) - daytime * VX_CLOUD_SIDE, VX_CLOUD_SIDE);
  while (ceil_z < 0.0f) ceil_z += VX_CLOUD_SIDE;
  
  ceil_w = ((240.0f - cam_y) / dir_y) / 100.0f;
  ceil_w = 32.0f / ceil_w;
  
  if (old_y >= 0.000001f && cloud_map[(int)(ceil_x) + (int)(ceil_z) * VX_CLOUD_SIDE]) {
    return (Color){191, 191, 191, 255};
  }
  
  return (Color){255, 255, 255, 255};
}

Color plot_pixel(float *depth, float cam_x, float cam_y, float cam_z, int scr_x, int scr_y, float dir_x, float dir_y, float dir_z, int limit) {
  uint32_t pos_x = floorf(cam_x);
  uint32_t pos_y = floorf(cam_y);
  uint32_t pos_z = floorf(cam_z);
  
  float side_x = 0;
  float side_y = 0;
  float side_z = 0;
  
  float delta_x = fast_abs(1 / dir_x);
  float delta_y = fast_abs(1 / dir_y);
  float delta_z = fast_abs(1 / dir_z);
  
  int step_x = 0;
  int step_y = 0;
  int step_z = 0;
  
  int hit_side = 0;
  
  if (dir_x < 0) {
    step_x = -1;
    side_x = (cam_x - pos_x) * delta_x;
  } else {
    step_x = 1;
    side_x = ((pos_x + 1) - cam_x) * delta_x;
  }
  
  if (dir_y < 0) {
    step_y = -1;
    side_y = (cam_y - pos_y) * delta_y;
  } else {
    step_y = 1;
    side_y = ((pos_y + 1) - cam_y) * delta_y;
  }
  
  if (dir_z < 0) {
    step_z = -1;
    side_z = (cam_z - pos_z) * delta_z;
  } else {
    step_z = 1;
    side_z = ((pos_z + 1) - cam_z) * delta_z;
  }
  
  Color mask = (Color){255, 255, 255, 255};
  int iter = VX_ITER;
  
  if (limit < VX_LIMIT) {
    iter /= 3;
  }
  
  for (int i = 0; limit && i < iter; i++) {
    if (side_x <= side_y && side_x <= side_z) {
      side_x += delta_x;
      pos_x += step_x;
      
      hit_side = 0;
    } else if (side_y <= side_x && side_y <= side_z) {
      side_y += delta_y;
      pos_y += step_y;
      
      hit_side = 1;
    } else {
      side_z += delta_z;
      pos_z += step_z;
      
      hit_side = 2;
    }
    
    float dist;
    
    if (hit_side == 0) {
      dist = side_x - delta_x;
    } else if (hit_side == 1) {
      dist = side_y - delta_y;
    } else if (hit_side == 2) {
      dist = side_z - delta_z;
    }
     
    if (pos_y < 0 || pos_y >= VX_CHUNK_Y) break;
    uint8_t tile = get_world(pos_x, pos_y, pos_z);
    
    if (tile == vx_tile_water && !get_world(pos_x, pos_y + 1, pos_z)) {
      float hit_x = cam_x + dir_x * dist;
      float hit_y = cam_y + dir_y * dist;
      float hit_z = cam_z + dir_z * dist; 
      
      if (hit_y - pos_y >= 0.75f) {
        float w = (0.75f - (hit_y - pos_y)) / dir_y;
        
        if (w * dir_x + (hit_x - pos_x) < 0.0f || w * dir_x + (hit_x - pos_x) >= 1.0f ||
            w * dir_z + (hit_z - pos_z) < 0.0f || w * dir_z + (hit_z - pos_z) >= 1.0f) {
          tile = 0;
        } else {
          hit_side = 1;
          dist += w;
        }
      }
    }
    
    if (tile) {
      Color c = get_color(pos_x, pos_y, pos_z);
      
      if (!(tile >= 8 && tile <= 10)) {
        if (hit_side == 0) {
          c.r = (int)(c.r * 0.85);
          c.g = (int)(c.g * 0.85);
          c.b = (int)(c.b * 0.85);
        } else if (hit_side == 2) {
          c.r = (int)(c.r * 0.70);
          c.g = (int)(c.g * 0.70);
          c.b = (int)(c.b * 0.70);
        }
      }
      
      float hit_x = cam_x + dir_x * dist;
      float hit_y = cam_y + dir_y * dist;
      float hit_z = cam_z + dir_z * dist; 
      
      float border_dist = 1.0f;
      float ambient_dist = 1.0f;
      
      if (1) {
        if (hit_side == 0) {
          if (get_world(pos_x, pos_y - 1, pos_z) != tile) {
            border_dist = MIN(border_dist, hit_y - (int)(hit_y));
          }
          
          if (get_world(pos_x, pos_y + 1, pos_z) != tile) {
            border_dist = MIN(border_dist, 1.0f - (hit_y - (int)(hit_y)));
          }
          
          if (get_world(pos_x, pos_y, pos_z - 1) != tile) {
            border_dist = MIN(border_dist, hit_z - (int)(hit_z));
          }
          
          if (get_world(pos_x, pos_y, pos_z + 1) != tile) {
            border_dist = MIN(border_dist, 1.0f - (hit_z - (int)(hit_z)));
          }
          
          if (get_world(pos_x - step_x, pos_y, pos_z) == tile) {
            border_dist = 1.0f;
          }
        } else if (hit_side == 1) {
          if (get_world(pos_x - 1, pos_y, pos_z) != tile) {
            border_dist = MIN(border_dist, hit_x - (int)(hit_x));
          }
          
          if (get_world(pos_x + 1, pos_y, pos_z) != tile) {
            border_dist = MIN(border_dist, 1.0f - (hit_x - (int)(hit_x)));
          }
          
          if (get_world(pos_x, pos_y, pos_z - 1) != tile) {
            border_dist = MIN(border_dist, hit_z - (int)(hit_z));
          }
          
          if (get_world(pos_x, pos_y, pos_z + 1) != tile) {
            border_dist = MIN(border_dist, 1.0f - (hit_z - (int)(hit_z)));
          }
          
          if (get_world(pos_x, pos_y - step_y, pos_z) == tile) {
            border_dist = 1.0f;
          }
        } else if (hit_side == 2) {
          if (get_world(pos_x - 1, pos_y, pos_z) != tile) {
            border_dist = MIN(border_dist, hit_x - (int)(hit_x));
          }
          
          if (get_world(pos_x + 1, pos_y, pos_z) != tile) {
            border_dist = MIN(border_dist, 1.0f - (hit_x - (int)(hit_x)));
          }
          
          if (get_world(pos_x, pos_y - 1, pos_z) != tile) {
            border_dist = MIN(border_dist, hit_y - (int)(hit_y));
          }
          
          if (get_world(pos_x, pos_y + 1, pos_z) != tile) {
            border_dist = MIN(border_dist, 1.0f - (hit_y - (int)(hit_y)));
          }
          
          if (get_world(pos_x, pos_y, pos_z - step_z) == tile) {
            border_dist = 1.0f;
          }
        }
        
        if (hit_side == 0) {
          if (is_solid(pos_x - step_x, pos_y - 1, pos_z)) {
            border_dist = MIN(border_dist, hit_y - (int)(hit_y));
          }
          
          if (is_solid(pos_x - step_x, pos_y + 1, pos_z)) {
            border_dist = MIN(border_dist, 1.0f - (hit_y - (int)(hit_y)));
          }
          
          if (is_solid(pos_x - step_x, pos_y, pos_z - 1)) {
            border_dist = MIN(border_dist, hit_z - (int)(hit_z));
          }
          
          if (is_solid(pos_x - step_x, pos_y, pos_z + 1)) {
            border_dist = MIN(border_dist, 1.0f - (hit_z - (int)(hit_z)));
          }
        } else if (hit_side == 1) {
          if (is_solid(pos_x - 1, pos_y - step_y, pos_z)) {
            border_dist = MIN(border_dist, hit_x - (int)(hit_x));
          }
          
          if (is_solid(pos_x + 1, pos_y - step_y, pos_z)) {
            border_dist = MIN(border_dist, 1.0f - (hit_x - (int)(hit_x)));
          }
          
          if (is_solid(pos_x, pos_y - step_y, pos_z - 1)) {
            border_dist = MIN(border_dist, hit_z - (int)(hit_z));
          }
          
          if (is_solid(pos_x, pos_y - step_y, pos_z + 1)) {
            border_dist = MIN(border_dist, 1.0f - (hit_z - (int)(hit_z)));
          }
        } else if (hit_side == 2) {
          if (is_solid(pos_x - 1, pos_y, pos_z - step_z)) {
            border_dist = MIN(border_dist, hit_x - (int)(hit_x));
          }
          
          if (is_solid(pos_x + 1, pos_y, pos_z - step_z)) {
            border_dist = MIN(border_dist, 1.0f - (hit_x - (int)(hit_x)));
          }
          
          if (is_solid(pos_x, pos_y - 1, pos_z - step_z)) {
            border_dist = MIN(border_dist, hit_y - (int)(hit_y));
          }
          
          if (is_solid(pos_x, pos_y + 1, pos_z - step_z)) {
            border_dist = MIN(border_dist, 1.0f - (hit_y - (int)(hit_y)));
          }
        }
        
        if (border_dist < (2.0f / VX_WIDTH) * dist) {
          border_dist = 0.75f;
        } else {
          border_dist = 1.0f;
        }
      }
      
      if (1) {
        if (hit_side == 0) {
          if (is_solid(pos_x - step_x, pos_y - 1, pos_z)) {
            ambient_dist = MIN(ambient_dist, hit_y - (int)(hit_y));
          }
          
          if (is_solid(pos_x - step_x, pos_y + 1, pos_z)) {
            ambient_dist = MIN(ambient_dist, 1.0f - (hit_y - (int)(hit_y)));
          }
          
          if (is_solid(pos_x - step_x, pos_y, pos_z - 1)) {
            ambient_dist = MIN(ambient_dist, hit_z - (int)(hit_z));
          }
          
          if (is_solid(pos_x - step_x, pos_y, pos_z + 1)) {
            ambient_dist = MIN(ambient_dist, 1.0f - (hit_z - (int)(hit_z)));
          }
          
          if (is_solid(pos_x - step_x, pos_y - 1, pos_z - 1)) {
            ambient_dist = MIN(ambient_dist, sqrtf(SQUARE(hit_y - (int)(hit_y)) + SQUARE(hit_z - (int)(hit_z))));
          }
          
          if (is_solid(pos_x - step_x, pos_y + 1, pos_z - 1)) {
            ambient_dist = MIN(ambient_dist, sqrtf(SQUARE(1.0f - (hit_y - (int)(hit_y))) + SQUARE(hit_z - (int)(hit_z))));
          }
          
          if (is_solid(pos_x - step_x, pos_y - 1, pos_z + 1)) {
            ambient_dist = MIN(ambient_dist, sqrtf(SQUARE(hit_y - (int)(hit_y)) + SQUARE(1.0f - (hit_z - (int)(hit_z)))));
          }
          
          if (is_solid(pos_x - step_x, pos_y + 1, pos_z + 1)) {
            ambient_dist = MIN(ambient_dist, sqrtf(SQUARE(1.0f - (hit_y - (int)(hit_y))) + SQUARE(1.0f - (hit_z - (int)(hit_z)))));
          }
        } else if (hit_side == 1) {
          if (is_solid(pos_x - 1, pos_y - step_y, pos_z)) {
            ambient_dist = MIN(ambient_dist, hit_x - (int)(hit_x));
          }
          
          if (is_solid(pos_x + 1, pos_y - step_y, pos_z)) {
            ambient_dist = MIN(ambient_dist, 1.0f - (hit_x - (int)(hit_x)));
          }
          
          if (is_solid(pos_x, pos_y - step_y, pos_z - 1)) {
            ambient_dist = MIN(ambient_dist, hit_z - (int)(hit_z));
          }
          
          if (is_solid(pos_x, pos_y - step_y, pos_z + 1)) {
            ambient_dist = MIN(ambient_dist, 1.0f - (hit_z - (int)(hit_z)));
          }
          
          if (is_solid(pos_x - 1, pos_y - step_y, pos_z - 1)) {
            ambient_dist = MIN(ambient_dist, sqrtf(SQUARE(hit_x - (int)(hit_x)) + SQUARE(hit_z - (int)(hit_z))));
          }
          
          if (is_solid(pos_x + 1, pos_y - step_y, pos_z - 1)) {
            ambient_dist = MIN(ambient_dist, sqrtf(SQUARE(1.0f - (hit_x - (int)(hit_x))) + SQUARE(hit_z - (int)(hit_z))));
          }
          
          if (is_solid(pos_x - 1, pos_y - step_y, pos_z + 1)) {
            ambient_dist = MIN(ambient_dist, sqrtf(SQUARE(hit_x - (int)(hit_x)) + SQUARE(1.0f - (hit_z - (int)(hit_z)))));
          }
          
          if (is_solid(pos_x + 1, pos_y - step_y, pos_z + 1)) {
            ambient_dist = MIN(ambient_dist, sqrtf(SQUARE(1.0f - (hit_x - (int)(hit_x))) + SQUARE(1.0f - (hit_z - (int)(hit_z)))));
          }
        } else if (hit_side == 2) {
          if (is_solid(pos_x - 1, pos_y, pos_z - step_z)) {
            ambient_dist = MIN(ambient_dist, hit_x - (int)(hit_x));
          }
          
          if (is_solid(pos_x + 1, pos_y, pos_z - step_z)) {
            ambient_dist = MIN(ambient_dist, 1.0f - (hit_x - (int)(hit_x)));
          }
          
          if (is_solid(pos_x, pos_y - 1, pos_z - step_z)) {
            ambient_dist = MIN(ambient_dist, hit_y - (int)(hit_y));
          }
          
          if (is_solid(pos_x, pos_y + 1, pos_z - step_z)) {
            ambient_dist = MIN(ambient_dist, 1.0f - (hit_y - (int)(hit_y)));
          }
          
          if (is_solid(pos_x - 1, pos_y - 1, pos_z - step_z)) {
            ambient_dist = MIN(ambient_dist, sqrtf(SQUARE(hit_x - (int)(hit_x)) + SQUARE(hit_y - (int)(hit_y))));
          }
          
          if (is_solid(pos_x + 1, pos_y - 1, pos_z - step_z)) {
            ambient_dist = MIN(ambient_dist, sqrtf(SQUARE(1.0f - (hit_x - (int)(hit_x))) + SQUARE(hit_y - (int)(hit_y))));
          }
          
          if (is_solid(pos_x - 1, pos_y + 1, pos_z - step_z)) {
            ambient_dist = MIN(ambient_dist, sqrtf(SQUARE(hit_x - (int)(hit_x)) + SQUARE(1.0f - (hit_y - (int)(hit_y)))));
          }
          
          if (is_solid(pos_x + 1, pos_y + 1, pos_z - step_z)) {
            ambient_dist = MIN(ambient_dist, sqrtf(SQUARE(1.0f - (hit_x - (int)(hit_x))) + SQUARE(1.0f - (hit_y - (int)(hit_y)))));
          }
        }
        
        if (ambient_dist < 0.25f) {
          ambient_dist = sqrtf(ambient_dist / 0.25f);
        } else {
          ambient_dist = 1.0f;
        }
      }
      
      if (scr_x == GetMouseX() / VX_ZOOM && scr_y == GetMouseY() / VX_ZOOM) {
        sel_x = pos_x;
        sel_y = pos_y;
        sel_z = pos_z;
        
        sel_face = hit_side;
        
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
          set_world(0, pos_x, pos_y, pos_z);
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          if (hit_side == 0) {
            set_world(selected, pos_x - step_x, pos_y, pos_z);
          } else if (hit_side == 1) {
            set_world(selected, pos_x, pos_y - step_y, pos_z);
          } else if (hit_side == 2) {
            set_world(selected, pos_x, pos_y, pos_z - step_z);
          }
        }
      }
      
      if (tile == vx_tile_water) {
        Color new_color;
        
        float new_x = dir_x * 0.99f + fast_sin((hit_x + hit_y + hit_z - 0.17f * GetTime()) * 13.0f) * 0.01f;
        float new_y = dir_y * 0.99f + fast_sin((hit_x + hit_y - hit_z - 0.15f * GetTime()) * 15.0f) * 0.01f;
        float new_z = dir_z * 0.99f + fast_sin((hit_x - hit_y + hit_z + 0.13f * GetTime()) * 17.0f) * 0.01f;
        
        float length = sqrtf(new_x * new_x + new_y * new_y + new_z * new_z);
        
        new_x /= length;
        new_y /= length;
        new_z /= length;
        
        if (hit_side == 0) {
          new_color = plot_pixel(NULL, hit_x - dir_x * 0.001f, hit_y, hit_z, -1, -1, -new_x, new_y, new_z, limit - 1);
        } else if (hit_side == 1) {
          new_color = plot_pixel(NULL, hit_x, hit_y - dir_y * 0.001f, hit_z, -1, -1, new_x, -new_y, new_z, limit - 1);
        } else if (hit_side == 2) {
          new_color = plot_pixel(NULL, hit_x, hit_y, hit_z - dir_z * 0.001f, -1, -1, new_x, new_y, -new_z, limit - 1);
        }
        
        c = (Color){(c.r + new_color.r) / 2, (c.g + new_color.g) / 2, (c.b + new_color.b) / 2, 255};
      }
      
      c.r *= border_dist;
      c.g *= border_dist;
      c.b *= border_dist;
      
      float ref_x = fast_cos(2 * PI * daytime);
      float ref_y = fast_sin(2 * PI * daytime);
      float ref_z = 0.0f;
      
      float temp = 1.0f - fast_cos(PI * (daytime + 0.25f)) * fast_cos(PI * (daytime + 0.25f));
      
      float light_r = 0.35f * temp + 0.65f;
      float light_g = 0.40f * temp + 0.60f;
      float light_b = 0.15f * temp + 0.80f;
      
      Color top = plot_shadow(hit_x - dir_x * 0.001f, hit_y - dir_z * 0.001f, hit_z - dir_z * 0.001f, ref_x, ref_y, ref_z, MIN(limit - 1, 1));
      
      light_r = MIN(light_r, (float)(top.r) / 255.0f);
      light_g = MIN(light_g, (float)(top.g) / 255.0f);
      light_b = MIN(light_b, (float)(top.b) / 255.0f);
      
      light_r = MIN(light_r, ambient_dist * 0.5f + 0.5f);
      light_g = MIN(light_g, ambient_dist * 0.5f + 0.5f);
      light_b = MIN(light_b, ambient_dist * 0.5f + 0.5f);
      
      c.r *= light_r;
      c.g *= light_g;
      c.b *= light_b;
      
      c.r = (c.r * (float)(mask.r)) / 255.0f;
      c.g = (c.g * (float)(mask.g)) / 255.0f;
      c.b = (c.b * (float)(mask.b)) / 255.0f;
      
      if (sel_x == pos_x && sel_y == pos_y && sel_z == pos_z && sel_face == hit_side) {
        c.r = c.r * 0.8f + 255 * 0.2f;
        c.g = c.g * 0.8f + 255 * 0.2f;
        c.b = c.b * 0.8f + 255 * 0.2f;
      }
      
      if (depth) *depth = dist;
      return c;
    }
  }
  
  float old_y = dir_y;
  
  if (dir_y < 0.000001f) dir_y = 0.000001f;
  float ceil_w = (160.0f - cam_y) / dir_y;
  
  float ceil_x = fmodf((cam_x + dir_x * ceil_w) + daytime * VX_CLOUD_SIDE, VX_CLOUD_SIDE);
  while (ceil_x < 0.0f) ceil_x += VX_CLOUD_SIDE;
  
  float ceil_z = fmodf((cam_z + dir_z * ceil_w) - daytime * VX_CLOUD_SIDE, VX_CLOUD_SIDE);
  while (ceil_z < 0.0f) ceil_z += VX_CLOUD_SIDE;
  
  int alpha = 0;
  
  float light = fast_cos(PI * (daytime + 0.25f)) * fast_cos(PI * (daytime + 0.25f));
  float k = 800.0f * light;
  
  k = k * k;
  k = k * k;
  
  float cr = k / 240100000000.0f;
  float cg = k / 78904810000.0f;
  float cb = k / 37480960000.0f;
  
  ceil_w = ((240.0f - cam_y) / dir_y) / 100.0f;
  ceil_w = 32.0f / ceil_w;
  
  if (old_y >= 0.000001f && cloud_map[(int)(ceil_x) + (int)(ceil_z) * VX_CLOUD_SIDE]) {
    ceil_w = lerp(0.0f, ceil_w, light);
    alpha = 127;
  }
  
  float tr = powf(MATH_E, -cr * ceil_w);
  float tg = powf(MATH_E, -cg * ceil_w);
  float tb = powf(MATH_E, -cb * ceil_w);
  
  float r = tr * (1.0f - cr);
  float g = tg * (1.0f - cg);
  float b = tb * (1.0f - cb);
  
  float rem = (1.0f - light) * (1.0f - 1.0f / (1.0f + ceil_w / 2.0f));
  
  r -= 0.50f * rem;
  g -= 0.25f * rem;
  
  r = (r * (float)(mask.r)) / 255.0f;
  g = (g * (float)(mask.g)) / 255.0f;
  b = (b * (float)(mask.b)) / 255.0f;
  
  if (r < 0.0f) r = 0.0f;
  if (g < 0.0f) g = 0.0f;
  if (b < 0.0f) b = 0.0f;
  
  if (r > 1.0f) r = 1.0f;
  if (g > 1.0f) g = 1.0f;
  if (b > 1.0f) b = 1.0f;
  
  return (Color){255 * r, 255 * g, 255 * b, alpha};
}

void handle_collision(void) {
  float new_x = vx_player.pos_x;
  float new_y = vx_player.pos_y;
  float new_z = vx_player.pos_z;
  
  for (int y = (int)(vx_player.pos_y) - 2; y <= (int)(vx_player.pos_y) + 1; y += 3) {
    if (!get_world((int)(vx_player.pos_x), y, (int)(vx_player.pos_z))) continue;
    int delta_y = y - (int)(vx_player.pos_y);
    
    if (delta_y > 0) {
      new_y = MIN(new_y, y - 1.3f);
    } else if (delta_y < 0) {
      new_y = MAX(new_y, y + 2.3f);
    }
  }
  
  for (int y = (int)(vx_player.pos_y) - 1; y <= (int)(vx_player.pos_y); y++) {
    for (int z = (int)(vx_player.pos_z) - 1; z <= (int)(vx_player.pos_z) + 1; z++) {
      for (int x = (int)(vx_player.pos_x) - 1; x <= (int)(vx_player.pos_x) + 1; x++) {
        int delta_x = x - (int)(vx_player.pos_x);
        int delta_y = y - (int)(vx_player.pos_y);
        int delta_z = z - (int)(vx_player.pos_z);
        
        if (!delta_x && !delta_z) continue;
        if (delta_x && delta_z) continue;
        
        if (get_world(x, y, z)) {
          if (delta_x > 0) {
            new_x = MIN(new_x, x - 0.3f);
          } else if (delta_x < 0) {
            new_x = MAX(new_x, x + 1.3f);
          }
          
          if (delta_z > 0) {
            new_z = MIN(new_z, z - 0.3f);
          } else if (delta_z < 0) {
            new_z = MAX(new_z, z + 1.3f);
          }
        }
      }
    }
  }
  
  for (int y = (int)(vx_player.pos_y) - 2; y <= (int)(vx_player.pos_y) + 1; y += 3) {
    if (!get_world((int)(vx_player.pos_x), y, (int)(vx_player.pos_z))) continue;
    int delta_y = y - (int)(vx_player.pos_y);
    
    if (delta_y > 0) {
      new_y = MIN(new_y, y - 1.3f);
    } else if (delta_y < 0) {
      new_y = MAX(new_y, y + 2.3f);
    }
  }
  
  vx_player.pos_x = new_x;
  vx_player.pos_y = new_y;
  vx_player.pos_z = new_z;
}

int main(int argc, const char **argv) {
  if (argc != 3) {
    printf("usage: %s [NAME] [SERVER]\n", argv[0]);
    exit(1);
  }
  
  strcpy(vx_name, argv[1]);
  
  InitWindow(VX_WIDTH * VX_ZOOM, VX_HEIGHT * VX_ZOOM, vx_name);
  srand(time(0));
  
  cloud_map = calloc(VX_CLOUD_SIDE * VX_CLOUD_SIDE, 1);
  
  for (uint32_t i = 0; i < VX_CLOUD_SIDE; i++) {
    for (uint32_t j = 0; j < VX_CLOUD_SIDE; j++) {
      if (eval_2((int)(j / 8) / 4.0f + 11.2f, (int)(i / 8) / 4.0f + 6.5f) < 0.3f) cloud_map[i + j * VX_CLOUD_SIDE] = 1;
    }
  }
  
  Image screen = GenImageColor(VX_WIDTH, VX_HEIGHT, BLACK);
  Texture texture = LoadTextureFromImage(screen);
  
  float *screen_depths = malloc(VX_WIDTH * VX_HEIGHT * sizeof(float)); // more cum
  
  vx_chunk_init();
  vx_loader_init(argv[2]);
  
  while (!WindowShouldClose()) {
    BeginDrawing();
    ImageClearBackground(&screen, BLACK);
    
    float ratio_y = (float)(VX_HEIGHT) / VX_WIDTH;
    
    for (uint32_t y = 0; y < VX_HEIGHT; y++) {
      for (uint32_t x = 0; x < VX_WIDTH; x++) {
        screen_depths[x + y * VX_WIDTH] = 6942000.0f;
      }
    }
    
    handle_collision();
    
    #pragma omp parallel for
    for (uint32_t y = 0; y < VX_HEIGHT; y++) {
      for (uint32_t x = 0; x < VX_WIDTH; x++) {
        float scr_x = (((float)(x) / VX_WIDTH) * 2 - 1) * 1.0f;
        float scr_y = (((float)(y) / VX_HEIGHT) * 2 - 1) * 1.0f;
        
        float ray_x = scr_x;
        float ray_y = -scr_y * ratio_y;
        float ray_z = 1.0f;
        
        float dir_x = ray_x;
        float dir_y = ray_y * fast_cos(-angle_ud) + ray_z * fast_sin(-angle_ud);
        float dir_z = ray_z * fast_cos(-angle_ud) - ray_y * fast_sin(-angle_ud);
        
        ray_x = dir_x;
        ray_y = dir_y;
        ray_z = dir_z;
        
        dir_x = ray_x * fast_cos(-angle_lr) - ray_z * fast_sin(-angle_lr);
        dir_y = ray_y;
        dir_z = ray_z * fast_cos(-angle_lr) + ray_x * fast_sin(-angle_lr);
        
        float depth; // cum
        
        Color color = plot_pixel(&depth, vx_player.pos_x, vx_player.pos_y + 0.5f, vx_player.pos_z, x, y, dir_x, dir_y, dir_z, VX_LIMIT);
        color.a = 255;
        
        ImageDrawPixel(&screen, x, y, color);
        screen_depths[x + y * VX_WIDTH] = depth;
      }
    }
    
    UpdateTexture(texture, screen.data);
    DrawTextureEx(texture, (Vector2){0, 0}, 0, VX_ZOOM, WHITE);
    
    for (int i = 0; i < vx_client_count; i++) {
      float rel_x = vx_clients[i].pos_x - vx_player.pos_x;
      float rel_y = vx_clients[i].pos_y - vx_player.pos_y;
      float rel_z = vx_clients[i].pos_z - vx_player.pos_z;
      
      float dir_x;
      float dir_y;
      float dir_z;
      
      dir_x = rel_x * fast_cos(angle_lr) - rel_z * fast_sin(angle_lr);
      dir_y = rel_y;
      dir_z = rel_z * fast_cos(angle_lr) + rel_x * fast_sin(angle_lr);
      
      rel_x = dir_x;
      rel_y = dir_y;
      rel_z = dir_z;
      
      dir_x = rel_x;
      dir_y = rel_y * fast_cos(angle_ud) + rel_z * fast_sin(angle_ud);
      dir_z = rel_z * fast_cos(angle_ud) - rel_y * fast_sin(angle_ud);
      
      if (dir_z < 0.0f) continue;
      
      dir_x /= dir_z;
      dir_y /= dir_z;
      
      float scr_x = ((dir_x + 1.0f) / 2.0f) * VX_WIDTH;
      float scr_y = (((-dir_y / ratio_y) + 1.0f) / 2.0f) * VX_HEIGHT;
      
      float scale = (VX_WIDTH * 0.25f) / dir_z;
      
      int start_x = scr_x - scale;
      int end_x = scr_x + scale;
      
      int start_y = scr_y - scale;
      int end_y = scr_y + scale * 3.0f;
      
      for (int j = start_y; j < end_y; j++) {
        for (int k = start_x; k < end_x; k++) {
          if (k < 0 || k >= VX_WIDTH || j < 0 || j >= VX_HEIGHT) continue;
          
          if (dir_z < screen_depths[k + j * VX_WIDTH]) {
            DrawRectangle(k * VX_ZOOM, j * VX_ZOOM, VX_ZOOM, VX_ZOOM, MAGENTA);
            screen_depths[k + j * VX_WIDTH] = dir_z;
          }
        }
      }
      
      int length = MeasureText(vx_clients[i].name, 30) + 6;
      
      DrawRectangle((int)(scr_x * VX_ZOOM) - (length / 2), start_y * VX_ZOOM - 40, length, 36, (Color){0, 0, 0, 127});
      DrawText(vx_clients[i].name, ((int)(scr_x * VX_ZOOM) - (length / 2)) + 3, start_y * VX_ZOOM - 33, 30, WHITE);
    }
    
    float old_x = vx_player.pos_x;
    float old_y = vx_player.pos_y;
    float old_z = vx_player.pos_z;
    
    if (IsKeyDown(KEY_W)) {
      vx_player.pos_x += 10.00 * fast_sin(angle_lr) * GetFrameTime();
      vx_player.pos_z += 10.00 * fast_cos(angle_lr) * GetFrameTime();
    } else if (IsKeyDown(KEY_S)) {
      vx_player.pos_x -= 10.00 * fast_sin(angle_lr) * GetFrameTime();
      vx_player.pos_z -= 10.00 * fast_cos(angle_lr) * GetFrameTime();
    }
    
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
      angle_lr -= 1.50 * GetFrameTime();
    } else if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
      angle_lr += 1.50 * GetFrameTime();
    }
    
    if (IsKeyDown(KEY_Q)) {
      angle_ud -= 1.50 * GetFrameTime();
    } else if (IsKeyDown(KEY_E)) {
      angle_ud += 1.50 * GetFrameTime();
    }
    
    if (IsKeyDown(KEY_SPACE)) {
      vx_player.pos_y += 7.50 * GetFrameTime();
    } else if (IsKeyDown(KEY_LEFT_SHIFT)) {
      vx_player.pos_y -= 7.50 * GetFrameTime();
    }
    
    /*
    
    if (IsKeyPressed(KEY_SPACE) && on_ground) {
      vel_y = 10.0f;
    }
    
    vel_y -= 20.0f * GetFrameTime();
    
    if (vel_y < -20.0f) vel_y = -20.0f;
    if (vel_y > 20.0f) vel_y = 20.0f;
    
    float delta_y = vel_y * GetFrameTime();
    
    if (delta_y <= -1.0f) delta_y = -0.99f;
    if (delta_y >= 1.0f) delta_y = 0.99f;
    
    vx_player.pos_y += delta_y;
    */
    
    if (IsKeyDown(KEY_ONE)) {
      selected = 1;
    } else if (IsKeyDown(KEY_TWO)) {
      selected = 2;
    } else if (IsKeyDown(KEY_THREE)) {
      selected = 3;
    } else if (IsKeyDown(KEY_FOUR)) {
      selected = 4;
    } else if (IsKeyDown(KEY_FIVE)) {
      selected = 5;
    } else if (IsKeyDown(KEY_SIX)) {
      selected = 6;
    } else if (IsKeyDown(KEY_SEVEN)) {
      selected = 7;
    } else if (IsKeyDown(KEY_EIGHT)) {
      selected = 8;
    } else if (IsKeyDown(KEY_NINE)) {
      selected = 9;
    } else if (IsKeyDown(KEY_ZERO)) {
      selected = 10;
    }
    
    if (IsKeyDown(KEY_Z)) {
      daytime += 0.15f * GetFrameTime();
      while (daytime >= 1.0f) daytime -= 1.0f;
    } else if (IsKeyDown(KEY_C)) {
      daytime -= 0.15f * GetFrameTime();
      while (daytime < 0.0f) daytime += 1.0f;
    }
    
    char buffer[100];
    
    sprintf(buffer, "(%.02f, %.02f, %.02f)", vx_player.pos_x, vx_player.pos_y, vx_player.pos_z);
    DrawText(buffer, 10, 30, 20, WHITE);
    
    sprintf(buffer, "Time: %02d:%02d", ((int)(daytime * 24.0f) + 10) % 24, (int)(daytime * 24.0f * 60.0f) % 60);
    DrawText(buffer, 10, 50, 20, WHITE);
    
    daytime += GetFrameTime() / 1440.0f;
    while (daytime >= 1.0f) daytime -= 1.0f;
    
    DrawFPS(10, 10);
    EndDrawing();
    
    if (fast_abs(last_x - vx_player.pos_x) + fast_abs(last_y - vx_player.pos_y) + fast_abs(last_z - vx_player.pos_z) >= 0.05f) {
      vx_loader_update(vx_player.pos_x, vx_player.pos_y, vx_player.pos_z);
    }
    
    last_x = vx_player.pos_x;
    last_y = vx_player.pos_y;
    last_z = vx_player.pos_z;
  }
  
  free(cloud_map);
  CloseWindow();
}
