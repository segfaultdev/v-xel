#include <stb_image.h>
#include <raylib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define VX_WIDTH       288
#define VX_HEIGHT      162
#define VX_ZOOM        6
#define VX_SIZE_X      (int64_t)(1024)
#define VX_SIZE_Y      (int64_t)(128)
#define VX_SIZE_Z      (int64_t)(1024)
#define VX_ITER        480
#define VX_SHADOW_ITER 480
#define VX_LIMIT       3

#define AIR_COLOR (Color){127, 127, 255, 0}

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define SQUARE(x) ((x) * (x))

#define MATH_E 2.71828f

#define fast_sin(num) (fast_cos((num) + 1.5f * PI))
#define fast_abs(num) ((num) < 0 ? -(num) : (num))

/*
static float fast_abs(float num) {
  return ((num) < 0 ? -(num) : (num));
}
*/

uint8_t *world = NULL;
uint8_t *height_map = NULL;

uint8_t *cloud_map = NULL;

float cam_x = VX_SIZE_X / 2;
float cam_y = 4;
float cam_z = VX_SIZE_Z / 2;

float vel_y = 0;
int on_ground = 0;

float angle_lr = 0;
float angle_ud = 0;

int selected = 1;

int sel_x = -1;
int sel_y = -1;
int sel_z = -1;

int sel_face = -1;

int do_borders = 1;
int do_shadows = 1;
int do_ambient = 1;

int seed = 0;

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

float lerp(float x, float y, float w) {
  if (w < 0) return x;
  if (w > 1) return y;
  
  return (y - x) * ((w * (w * 6.0 - 15.0) + 10.0) * w * w * w) + x;
}

uint8_t get_world(int64_t x, int64_t y, int64_t z) {
  x %= VX_SIZE_X;
  z %= VX_SIZE_Z;
  
  while (x < 0) x += VX_SIZE_X;
  while (z < 0) z += VX_SIZE_Z;
  
  if (y < 0 || y >= VX_SIZE_Y) {
    return 0;
  }
  
  return world[x + (y + z * VX_SIZE_Y) * VX_SIZE_X];
}

int is_solid(int64_t x, int64_t y, int64_t z) {
  int tile = get_world(x, y, z);
  return (tile != 0 && tile != 3 && tile != 8 && tile != 9 && tile != 10);
}

Color get_color(int x, int y, int z) {
  if (get_world(x, y, z) == 1) {
    return (Color){63, 223, 63, 255};
  } else if (get_world(x, y, z) == 2) {
    return (Color){159, 79, 15, 255};
  } else if (get_world(x, y, z) == 3) {
    return (Color){63, 95, 255, 255};
  } else if (get_world(x, y, z) == 4) {
    return (Color){255, 0, 0, 255};
  } else if (get_world(x, y, z) == 5) {
    return (Color){255, 255, 0, 255};
  } else if (get_world(x, y, z) == 6) {
    return (Color){0, 0, 255, 255};
  } else if (get_world(x, y, z) == 7) {
    return (Color){159, 159, 159, 255};
  } else if (get_world(x, y, z) == 8) {
    return (Color){255, 0, 0, 255};
  } else if (get_world(x, y, z) == 9) {
    return (Color){255, 255, 0, 255};
  } else if (get_world(x, y, z) == 10) {
    return (Color){0, 0, 255, 255};
  } else if (get_world(x, y, z) == 11) {
    return (Color){79, 39, 7, 255};
  } else if (get_world(x, y, z) == 12) {
    return (Color){31, 111, 31, 255};
  } else if (get_world(x, y, z) == 13) {
    return (Color){255, 255, 127, 255};
  }
  
  return BLACK;
}

void set_world(uint8_t value, int64_t x, int64_t y, int64_t z) {
  x %= VX_SIZE_X;
  z %= VX_SIZE_Z;
  
  while (x < 0) x += VX_SIZE_X;
  while (z < 0) z += VX_SIZE_Z;
  
  if (y < 0 || y >= VX_SIZE_Y) {
    return;
  }
  
  world[x + (y + z * VX_SIZE_Y) * VX_SIZE_X] = value;
  
  if (value) {
    if (y > height_map[x + z * VX_SIZE_X]) {
      height_map[x + z * VX_SIZE_X] = y;
    }
  } else {
    if (y == height_map[x + z * VX_SIZE_X]) {
      int64_t i = y;
      
      while (i && !world[x + (i + z * VX_SIZE_Y) * VX_SIZE_X]) {
        i--;
      }
      
      height_map[x + z * VX_SIZE_X] = i;
    }
  }
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
  int64_t pos_x = floorf(cam_x);
  int64_t pos_y = floorf(cam_y);
  int64_t pos_z = floorf(cam_z);
  
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
    
    if (pos_y > height_map[pos_x + pos_z * VX_SIZE_X]) {
      if (step_y > 0 || (side_y > side_x && side_y > side_z)) {
        continue;
      }
    }
        
    if (pos_y < 0 || pos_y >= VX_SIZE_Y) break;
    uint8_t tile = get_world(pos_x, pos_y, pos_z);
    
    if (tile == 3 && !get_world(pos_x, pos_y + 1, pos_z)) {
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
  
  return (Color){255, 255, 255, 255};
}

Color plot_pixel(float *depth, float cam_x, float cam_y, float cam_z, int scr_x, int scr_y, float dir_x, float dir_y, float dir_z, int limit) {
  int64_t pos_x = floorf(cam_x);
  int64_t pos_y = floorf(cam_y);
  int64_t pos_z = floorf(cam_z);
  
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
    
    if (pos_y > height_map[pos_x + pos_z * VX_SIZE_X]) {
      if (step_y > 0 || (side_y > side_x && side_y > side_z)) {
        continue;
      }
    }
        
    if (pos_y < 0 || pos_y >= VX_SIZE_Y) break;
    uint8_t tile = get_world(pos_x, pos_y, pos_z);
    
    if (tile == 3 && !get_world(pos_x, pos_y + 1, pos_z)) {
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
      
      if (do_borders) {
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
      
      if (do_ambient) {
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
      
      if (tile == 3) {
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
      } else if (tile >= 8 && tile <= 10) {
        mask = (Color){((float)(c.r) * (float)(mask.r)) / 255.0f, ((float)(c.g) * (float)(mask.g)) / 255.0f, ((float)(c.b) * (float)(mask.b)) / 255.0f, mask.a};
        continue;
      }
      
      if (tile >= 8 && tile <= 10) {
        Color org = get_color(pos_x, pos_y, pos_z);
        
        c.r = c.r * border_dist + ((org.r + 255) / 2) * (1.0f - border_dist);
        c.g = c.g * border_dist + ((org.g + 255) / 2) * (1.0f - border_dist);
        c.b = c.b * border_dist + ((org.b + 255) / 2) * (1.0f - border_dist);
      } else {
        c.r *= border_dist;
        c.g *= border_dist;
        c.b *= border_dist;
      }
      
      float ref_x = fast_cos(2 * PI * daytime);
      float ref_y = fast_sin(2 * PI * daytime);
      float ref_z = 0.0f;
      
      float temp = 1.0f - fast_cos(PI * (daytime + 0.25f)) * fast_cos(PI * (daytime + 0.25f));
      
      float light_r = 0.35f * temp + 0.65f;
      float light_g = 0.40f * temp + 0.60f;
      float light_b = 0.15f * temp + 0.80f;
      
      if (do_shadows) {
        Color top = plot_shadow(hit_x - dir_x * 0.001f, hit_y - dir_z * 0.001f, hit_z - dir_z * 0.001f, ref_x, ref_y, ref_z, MIN(limit - 1, 1));
        
        light_r = MIN(light_r, (float)(top.r) / 255.0f);
        light_g = MIN(light_g, (float)(top.g) / 255.0f);
        light_b = MIN(light_b, (float)(top.b) / 255.0f);
      }
      
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
  
  float ceil_x = fmodf((cam_x + dir_x * ceil_w) + daytime * VX_SIZE_X, VX_SIZE_X);
  while (ceil_x < 0.0f) ceil_x += VX_SIZE_X;
  
  float ceil_z = fmodf((cam_z + dir_z * ceil_w) - daytime * VX_SIZE_X, VX_SIZE_Z);
  while (ceil_z < 0.0f) ceil_z += VX_SIZE_Z;
  
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
  
  if (old_y >= 0.000001f && cloud_map[(int)(ceil_x) + (int)(ceil_z) * VX_SIZE_X]) {
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
  
  if (depth) *depth = -1.0f;
  return (Color){255 * r, 255 * g, 255 * b, alpha};
}  

unsigned int seed_3a = 1;
unsigned int seed_3b = 1;
unsigned int seed_3c = 1;

unsigned int rand_3() {
  seed_3a = (seed_3a * 1664525 + 1013904223) % 1431655765;
  seed_3b = (seed_3b * 16843019 + 826366249) % 1431655765;
  seed_3c = (seed_3c * 16843031 + 826366237) % 1431655765;
  
  return seed_3a + seed_3b + seed_3c;
}

float grad_1(int x) {
  x %= 16;
  while (x < 0) x += 16;
  
  x += 7 * seed;
  
  seed_3a = (x * 1664525 + 1013904223) % 1431655765;
  seed_3b = (x * 16843019 + 826366249) % 1431655765;
  seed_3c = ((seed_3a + seed_3b) * 16843031 + 826366237) % 1431655765;
  
  int count = rand_3() % 2;
  
  for (int i = 0; i < count; i++) {
    rand_3();
  }
  
  return fast_abs(rand_3() % 65537) / 65537.0;
}

float grad_2(int x, int y) {
  x %= 8;
  y %= 16;
  
  while (x < 0) x += 8;
  while (y < 0) y += 16;
  
  x +=  3 * seed;
  y += 17 * seed;
  
  seed_3a = (x * 1664525 + 1013904223) % 1431655765;
  seed_3b = (y * 16843019 + 826366249) % 1431655765;
  seed_3c = ((seed_3a + seed_3b) * 16843031 + 826366237) % 1431655765;
  
  int count = rand_3() % 2;
  
  for (int i = 0; i < count; i++) {
    rand_3();
  }
  
  return fast_abs(rand_3() % 65537) / 65537.0;
}

float eval_1(float x) {
  x = x - (int)(x / 16) * 16;
  while (x < 0) x += 16;
  
  float dx = x - floorf(x);
  return lerp(grad_1(floorf(x + 0)), grad_1(floorf(x + 1)), dx);
}

float eval_2(float x, float y) {
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

void set_tree(int64_t x, int64_t y, int64_t z) {
  int64_t length = 4 + (rand_3() % 3);
  
  for (int64_t i = 0; i < length; i++) {
    set_world(11, x, y + i, z);
  }
  
  for (int64_t i = -2; i <= 2; i++) {
    for (int64_t j = -2; j <= 1; j++) {
      for (int64_t k = -2; k <= 2; k++) {
        if (i * i + j * j + k * k > 5) {
          continue;
        }
        
        if (!get_world(x + k, y + (length - 1) + j, z + i)) {
          set_world(12, x + k, y + (length - 1) + j, z + i);
        }
      }
    }
  }
  
  /*
  int64_t length = 11 + (rand_3() % 3);
  
  for (int64_t i = 0; i < length; i++) {
    set_world(11, x, y + i, z);
  }
  
  for (int64_t i = -3; i <= 3; i++) {
    for (int64_t j = 0; j <= 8; j++) {
      for (int64_t k = -3; k <= 3; k++) {
        if (j % 2 || i * i + k * k >= (3.5f - j / 3.0f) * (3.5f - j / 3.0f)) {
          continue;
        }
        
        if (!get_world(x + k, y + (length - 1) + j - 7, z + i)) {
          set_world(12, x + k, y + (length - 1) + j - 7, z + i);
        }
      }
    }
  }
  */
}

void handle_xz(float old_x, float old_z) {
  int tile_lo = get_world(cam_x, cam_y - 1.0f, cam_z);
  int tile_hi = get_world(cam_x, cam_y + 0.0f, cam_z);
  
  if (tile_lo || tile_hi) {
    if ((int)(cam_x) != (int)(old_x) && (int)(cam_z) == (int)(old_z)) {
      cam_x = (cam_x > old_x) ? ((int)(cam_x) - 0.01f) : ((int)(old_x) + 0.01f);
    } else if ((int)(cam_x) == (int)(old_x) && (int)(cam_z) != (int)(old_z)) {
      cam_z = (cam_z > old_z) ? ((int)(cam_z) - 0.01f) : ((int)(old_z) + 0.01f);
    } else if ((int)(cam_x) != (int)(old_x) && (int)(cam_z) != (int)(old_z)) {
      
      float col_x = fast_abs(old_x - ((cam_x > old_x) ? ((int)(cam_x) - 0.01f) : (int)(old_x) + 0.01f));
      float col_z = fast_abs(old_z - ((cam_z > old_z) ? ((int)(cam_z) - 0.01f) : (int)(old_z) + 0.01f));
      
      if (col_x < col_z) {
        cam_x = (cam_x > old_x) ? ((int)(cam_x) - 0.01f) : ((int)(old_x) + 0.01f);
      } else {
        cam_z = (cam_z > old_z) ? ((int)(cam_z) - 0.01f) : ((int)(old_z) + 0.01f);
      }
    }
  }
  
  tile_lo = get_world(cam_x, cam_y - 1.0f, cam_z);
  tile_hi = get_world(cam_x, cam_y + 0.0f, cam_z);
  
  if (tile_lo || tile_hi) {
    if ((int)(cam_x) != (int)(old_x) && (int)(cam_z) == (int)(old_z)) {
      cam_x = (cam_x > old_x) ? ((int)(cam_x) - 0.01f) : ((int)(old_x) + 0.01f);
    } else if ((int)(cam_x) == (int)(old_x) && (int)(cam_z) != (int)(old_z)) {
      cam_z = (cam_z > old_z) ? ((int)(cam_z) - 0.01f) : ((int)(old_z) + 0.01f);
    } else if ((int)(cam_x) != (int)(old_x) && (int)(cam_z) != (int)(old_z)) {
      
      float col_x = fast_abs(old_x - ((cam_x > old_x) ? ((int)(cam_x) - 0.01f) : (int)(old_x) + 0.01f));
      float col_z = fast_abs(old_z - ((cam_z > old_z) ? ((int)(cam_z) - 0.01f) : (int)(old_z) + 0.01f));
      
      if (col_x < col_z) {
        cam_x = (cam_x > old_x) ? ((int)(cam_x) - 0.01f) : ((int)(old_x) + 0.01f);
      } else {
        cam_z = (cam_z > old_z) ? ((int)(cam_z) - 0.01f) : ((int)(old_z) + 0.01f);
      }
    }
  }
}

void handle_y(float old_y) {
  int tile_lo = get_world(cam_x, cam_y - 1.0f, cam_z);
  int tile_hi = get_world(cam_x, cam_y + 0.5f, cam_z);
  
  on_ground = 0;
  
  if (tile_lo) {
    cam_y = (int)(cam_y) + 1.0f;
    if (vel_y < 0.0f) vel_y = 0.0f;
    
    on_ground = 1;
  } else if (tile_hi) {
    cam_y = (int)(cam_y) + 0.49f;
    if (vel_y > 0.0f) vel_y = 0.0f;
  }
}

int diff_x(float y) {
  return 3.0f * eval_1(12.3f + y / 14.5f);
}

int diff_z(float y) {
  return 3.0f * eval_1(45.6f + y / 12.3f);
}

float lerp_3(float a, float b, float c, float w) {
  float w1 = w * 2.0f;
  float w2 = w1 - 1.0f;
  
  if (w1 > 1.0f) w1 = 1.0f;
  if (w2 < 0.0f) w2 = 0.0f;
  
  return lerp(lerp(a, b, w1), c, w2);
}

int main(int argc, const char **argv) {
  InitWindow(VX_WIDTH * VX_ZOOM, VX_HEIGHT * VX_ZOOM, "v!xel");
  srand(time(0));
  
  seed = rand() % 1048576;
  
  world = calloc(VX_SIZE_X * VX_SIZE_Y * VX_SIZE_Z, 1);
  height_map = calloc(VX_SIZE_X * VX_SIZE_Z, 2);
  
  cloud_map = calloc(VX_SIZE_X * VX_SIZE_Z, 1);
  
  for (int64_t i = 0; i < VX_SIZE_X; i++) {
    for (int64_t j = 0; j < VX_SIZE_Z; j++) {
      if (eval_2((int)(j / 8) / 4.0f + 11.2f, (int)(i / 8) / 4.0f + 6.5f) < 0.3f) cloud_map[i + j * VX_SIZE_X] = 1;
    }
  }
  
  int diffs_x[VX_SIZE_Y];
  int diffs_z[VX_SIZE_Y];
  
  for (int64_t i = 0; i < VX_SIZE_Y; i++) {
    diffs_x[i] = diff_x(i);
    diffs_z[i] = diff_z(i);
  }
  
  for (int64_t i = 0; i < VX_SIZE_X; i++) {
    for (int64_t j = 0; j < VX_SIZE_Z; j++) {
      float value_1 = eval_2(45.6f + j / 64.0f, i / 64.0f);
      float value_2 = roundf(value_1 * 5.0f) / 5.0f;
      float value_3 = eval_2(j / 32.0f + value_2 * 91.1f, i / 32.0f - value_2 * 33.6f);
      float value_4 = 1.0f - fabs(1.0f - eval_2(78.9f + i / 16.0f, j / 16.0f) * 2.0f);
      float value_5 = eval_2(i / 32.0f, j / 32.0f);
      float value_6 = eval_2(j / 64.0f, i / 64.0f);
      
      int64_t height = 32.0f + floorf(80.0f * value_6 * value_6 * lerp(value_3, value_4, value_5 * value_5));
      
      if (i == (int)(cam_x) % VX_SIZE_X && j == (int)(cam_z) % VX_SIZE_Z) {
        cam_y = MAX(36, height) + 3.0f;
      }
      
      for (int64_t k = 0; k <= height; k++) {
        set_world(2, i + diffs_x[k], k, j + diffs_z[k]);
      }
      
      for (int64_t k = height + 1; k <= 36; k++) {
        set_world(3, i + diffs_x[k], k, j + diffs_z[k]);
      }
    }
    
    printf("terrain: %.2f%% done\n", (i * 100.0f) / VX_SIZE_X);
  }
  
  for (int64_t i = 0; i < VX_SIZE_Z; i++) {
    for (int64_t j = 0; j < 112; j++) {
      for (int64_t k = 0; k < VX_SIZE_X; k++) {
        if (j < 96) {
          if (get_world(k, j, i) == 2 && get_world(k, j + 1, i) == 0) {
            if (j <= 37) {
              for (int64_t l = MAX(j - 2, 0); l <= j; l++) {
                set_world(13, k, l, i);
              }
            } else {
              set_world(1, k, j, i);
            }
            
            continue;
          }
        }
        
        int total = 0;
        int count = 0;
        
        for (int dz = -1; dz <= 1; dz++) {
          for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
              int x = k + dx;
              int y = j + dy;
              int z = i + dz;
              
              if (y >= 0 && y < VX_SIZE_Y) {
                total++;
                
                if (get_world(x, y, z)) {
                  count++;
                }
              }
            }
          }
        }
        
        if (count == total) {
          set_world(7, k, j, i);
        }
      }
    }
    
    printf("details: %.2f%% done\n", (i * 100.0f) / VX_SIZE_Z);
  }
  
  int tree_count = VX_SIZE_X * VX_SIZE_Z * 0.15f * 0.015f;
  
  for (int i = 0; i < tree_count; i++) {
    int64_t x = rand() % VX_SIZE_X;
    int64_t z = rand() % VX_SIZE_Z;
    
    if (eval_2(14.1 + x / 20.0, 42.5 + z / 20.0) < 0.65f) {
      i--;
      continue;
    }
    
    int64_t y = VX_SIZE_Y - 1;
    
    while (y >= 0 && !get_world(x, y - 1, z)) {
      y--;
    }
    
    if (y < 0) {
      i--;
      continue;
    }
    
    if (get_world(x, y - 1, z) == 3 || get_world(x, y - 1, z) == 11 || get_world(x, y - 1, z) == 12) {
      i--;
      continue;
    }
    
    set_tree(x, y, z);
    printf("trees: %.2f%% done\n", (i * 100.0f) / tree_count);
  }
  
  Image screen = GenImageColor(VX_WIDTH, VX_HEIGHT, BLACK);
  Texture texture = LoadTextureFromImage(screen);
  
  while (!WindowShouldClose()) {
    BeginDrawing();
    ImageClearBackground(&screen, (Color){AIR_COLOR.r, AIR_COLOR.g, AIR_COLOR.b, 255});
    
    float ratio_y = (float)(VX_HEIGHT) / VX_WIDTH;
    
    for (int64_t y = 0; y < VX_HEIGHT; y++) {
      for (int64_t x = 0; x < VX_WIDTH; x++) {
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
        
        Color color = plot_pixel(NULL, cam_x, cam_y + 0.5f, cam_z, x, y, dir_x, dir_y, dir_z, VX_LIMIT);
        color.a = 255;
        
        ImageDrawPixel(&screen, x, y, color);
      }
    }
    
    UpdateTexture(texture, screen.data);
    DrawTextureEx(texture, (Vector2){0, 0}, 0, VX_ZOOM, WHITE);
    
    float old_x = cam_x;
    float old_y = cam_y;
    float old_z = cam_z;
    
    if (IsKeyDown(KEY_W)) {
      cam_x += 10.00 * fast_sin(angle_lr) * GetFrameTime();
      cam_z += 10.00 * fast_cos(angle_lr) * GetFrameTime();
      
      handle_xz(old_x, old_z);
    } else if (IsKeyDown(KEY_S)) {
      cam_x -= 10.00 * fast_sin(angle_lr) * GetFrameTime();
      cam_z -= 10.00 * fast_cos(angle_lr) * GetFrameTime();
      
      handle_xz(old_x, old_z);
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
      cam_y += 7.50 * GetFrameTime();
    } else if (IsKeyDown(KEY_LEFT_SHIFT)) {
      cam_y -= 7.50 * GetFrameTime();
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
    
    cam_y += delta_y;
    */
    
    handle_y(old_y);
    
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
    
    sprintf(buffer, "X: %.02f", cam_x);
    DrawText(buffer, 10, 30, 20, WHITE);
    
    sprintf(buffer, "Y: %.02f", cam_y);
    DrawText(buffer, 10, 50, 20, WHITE);
    
    sprintf(buffer, "Z: %.02f", cam_z);
    DrawText(buffer, 10, 70, 20, WHITE);
    
    sprintf(buffer, "time: %.02f", daytime);
    DrawText(buffer, 10, 90, 20, WHITE);
    
    daytime += GetFrameTime() / 1440.0f;
    while (daytime >= 1.0f) daytime -= 1.0f;
    
    DrawFPS(10, 10);
    EndDrawing();
  }
  
  free(world);
  
  free(cloud_map);
  
  CloseWindow();
}
