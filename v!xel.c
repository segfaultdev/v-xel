#include <stb_image.h>
#include <raylib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define VX_WIDTH  288
#define VX_HEIGHT 162
#define VX_ZOOM   5
#define VX_SIZE_X (int64_t)(1024)
#define VX_SIZE_Y (int64_t)(128)
#define VX_SIZE_Z (int64_t)(1024)
#define VX_ITER   128
#define VX_LIMIT  3

#define AIR_COLOR (Color){0, 0, 0, 144}

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define fast_sin(num) (fast_cos((num) + 1.5f * PI))
#define fast_abs(num) ((num) < 0 ? -(num) : (num))

uint8_t *world = NULL;

float cam_x = VX_SIZE_X / 2;
float cam_y = 4;
float cam_z = VX_SIZE_Z / 2;

float angle_lr = 0;
float angle_ud = 0;

int selected = 1;

int sel_x = -1;
int sel_y = -1;
int sel_z = -1;

int sel_face = -1;

int do_borders = 1;
int do_shadows = 1;

int seed = 0;

float fast_cos(float x) {
  float tp = 1 / (2 * PI);

  x = fast_abs(x * tp);
  x -= 0.25f + (int)(x + 0.25f);
  x *= 16.0f * (fast_abs(x) - 0.5f);

  // optional
  x += 0.225f * x * (fast_abs(x) - 1.0f);

  return x;
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

Color get_color(int64_t x, int64_t y, int64_t z) {
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
}

Color plot_pixel(float cam_x, float cam_y, float cam_z, int scr_x, int scr_y, float dir_x, float dir_y, float dir_z, int limit) {
  if (!limit) return AIR_COLOR;
  
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
  
  int iter = VX_ITER;
  
  if (limit < VX_LIMIT) {
    iter /= 3;
  }
  
  for (int i = 0; i < iter; i++) {
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
    
    uint8_t tile = get_world(pos_x, pos_y, pos_z);
    
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
      
      float dist;
      
      if (hit_side == 0) {
        dist = side_x - delta_x;
      } else if (hit_side == 1) {
        dist = side_y - delta_y;
      } else if (hit_side == 2) {
        dist = side_z - delta_z;
      }
      
      float hit_x = cam_x + dir_x * dist;
      float hit_y = cam_y + dir_y * dist;
      float hit_z = cam_z + dir_z * dist; 
      
      float border_dist = 1.0f;
      
      if (do_borders) {
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
            if (get_world(pos_x - step_x, pos_y - 1, pos_z)) {
              border_dist = MIN(border_dist, hit_y - (int)(hit_y));
            }
            
            if (get_world(pos_x - step_x, pos_y + 1, pos_z)) {
              border_dist = MIN(border_dist, 1.0f - (hit_y - (int)(hit_y)));
            }
            
            if (get_world(pos_x - step_x, pos_y, pos_z - 1)) {
              border_dist = MIN(border_dist, hit_z - (int)(hit_z));
            }
            
            if (get_world(pos_x - step_x, pos_y, pos_z + 1)) {
              border_dist = MIN(border_dist, 1.0f - (hit_z - (int)(hit_z)));
            }
          } else if (hit_side == 1) {
            if (get_world(pos_x - 1, pos_y - step_y, pos_z)) {
              border_dist = MIN(border_dist, hit_x - (int)(hit_x));
            }
            
            if (get_world(pos_x + 1, pos_y - step_y, pos_z)) {
              border_dist = MIN(border_dist, 1.0f - (hit_x - (int)(hit_x)));
            }
            
            if (get_world(pos_x, pos_y - step_y, pos_z - 1)) {
              border_dist = MIN(border_dist, hit_z - (int)(hit_z));
            }
            
            if (get_world(pos_x, pos_y - step_y, pos_z + 1)) {
              border_dist = MIN(border_dist, 1.0f - (hit_z - (int)(hit_z)));
            }
          } else if (hit_side == 2) {
            if (get_world(pos_x - 1, pos_y, pos_z - step_z)) {
              border_dist = MIN(border_dist, hit_x - (int)(hit_x));
            }
            
            if (get_world(pos_x + 1, pos_y, pos_z - step_z)) {
              border_dist = MIN(border_dist, 1.0f - (hit_x - (int)(hit_x)));
            }
            
            if (get_world(pos_x, pos_y - 1, pos_z - step_z)) {
              border_dist = MIN(border_dist, hit_y - (int)(hit_y));
            }
            
            if (get_world(pos_x, pos_y + 1, pos_z - step_z)) {
              border_dist = MIN(border_dist, 1.0f - (hit_y - (int)(hit_y)));
            }
          }
          
          if (border_dist < (2.0f / VX_WIDTH) * dist) {
            border_dist = 0.75f;
          } else {
            border_dist = 1.0f;
          }
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
        
        float new_x = dir_x * 0.98f + fast_sin((hit_x + hit_y + hit_z - 0.17f * GetTime()) * 13.0f) * 0.02f;
        float new_y = dir_y * 0.98f + fast_sin((hit_x + hit_y - hit_z - 0.15f * GetTime()) * 15.0f) * 0.02f;
        float new_z = dir_z * 0.98f + fast_sin((hit_x - hit_y + hit_z + 0.13f * GetTime()) * 17.0f) * 0.02f;
        
        float length = sqrtf(new_x * new_x + new_y * new_y + new_z * new_z);
        
        new_x /= length;
        new_y /= length;
        new_z /= length;
        
        if (hit_side == 0) {
          new_color = plot_pixel(hit_x - dir_x * 0.001f, hit_y, hit_z, -1, -1, -new_x, new_y, new_z, limit - 1);
        } else if (hit_side == 1) {
          new_color = plot_pixel(hit_x, hit_y - dir_y * 0.001f, hit_z, -1, -1, new_x, -new_y, new_z, limit - 1);
        } else if (hit_side == 2) {
          new_color = plot_pixel(hit_x, hit_y, hit_z - dir_z * 0.001f, -1, -1, new_x, new_y, -new_z, limit - 1);
        }
        
        c = (Color){(c.r + new_color.r) / 2, (c.g + new_color.g) / 2, (c.b + new_color.b) / 2, 255};
      } else if (tile >= 8 && tile <= 10) {
        if (1 || limit > 1) {
          Color new_color;
          
          float new_x = dir_x * 0.998f + fast_sin((hit_x + hit_y + hit_z) * 13.0f) * 0.002f;
          float new_y = dir_y * 0.998f + fast_sin((hit_x + hit_y - hit_z) * 15.0f) * 0.002f;
          float new_z = dir_z * 0.998f + fast_sin((hit_x - hit_y + hit_z) * 17.0f) * 0.002f;
          
          float length = sqrtf(new_x * new_x + new_y * new_y + new_z * new_z);
          
          new_x /= length;
          new_y /= length;
          new_z /= length;
          
          new_color = plot_pixel(hit_x + dir_x * 0.001f, hit_y + dir_y * 0.001f, hit_z + dir_z * 0.001f, -1, -1, new_x, new_y, new_z, limit);
          c = (Color){(c.r * new_color.r) / 255, (c.g * new_color.g) / 255, (c.b * new_color.b) / 255, new_color.a};
        } else {
          return AIR_COLOR;
        }
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
      
      float ref_x = 1.0f / sqrtf(3.0f);
      float ref_y = 1.0f / sqrtf(3.0f);
      float ref_z = 1.0f / sqrtf(3.0f);
      
      if (do_shadows) {
        Color top = plot_pixel(hit_x - dir_x * 0.001f, hit_y - dir_z * 0.001f, hit_z - dir_z * 0.001f, -1, -1, ref_x, ref_y, ref_z, MIN(limit - 1, 1));
        
        if (!(tile >= 8 && tile <= 10) && (memcmp(&top, &AIR_COLOR, sizeof(Color)) || (hit_side == 1 && step_y > 0))) {
          c.r = c.r * 0.49f + top.r * 0.01f;
          c.g = c.g * 0.49f + top.g * 0.01f;
          c.b = c.b * 0.49f + top.b * 0.01f;
        }
      }
      
      if (sel_x == pos_x && sel_y == pos_y && sel_z == pos_z && sel_face == hit_side) {
        c.r = c.r * 0.8f + 255 * 0.2f;
        c.g = c.g * 0.8f + 255 * 0.2f;
        c.b = c.b * 0.8f + 255 * 0.2f;
      }
      
      return c;
    }
  }
  
  return AIR_COLOR;
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

float lerp(float x, float y, float w) {
  if (w < 0) return x;
  if (w > 1) return y;
  
  return (y - x) * ((w * (w * 6.0 - 15.0) + 10.0) * w * w * w) + x;
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
  int tile_lo = get_world(cam_x, cam_y - 1.00f, cam_z);
  int tile_hi = get_world(cam_x, cam_y + 0.50f, cam_z);
  
  if (tile_lo) {
    cam_y = (int)(cam_y) + 1.00f;
  } else if (tile_hi) {
    cam_y = (int)(cam_y) + 0.49f;
  }
}

int diff_x(float x, float y, float z) {
  return 4.0 * eval_2((y / 16.0 - x / 64.0), (z / 128.0) - 31.4);
}

int diff_z(float x, float y, float z) {
  return 4.0 * eval_2((x / 128.0 - z / 64.0) + 15.9, y / 16.0);
}

int main(void) {
  InitWindow(VX_WIDTH * VX_ZOOM, VX_HEIGHT * VX_ZOOM, "v!xel test");
  srand(time(0));
  
  seed = rand() % 1048576;
  world = calloc(VX_SIZE_X * VX_SIZE_Y * VX_SIZE_Z, 1);
  
  for (int64_t i = 0; i < VX_SIZE_X; i++) {
    for (int64_t j = 0; j < VX_SIZE_Z; j++) {
      float value_1 = (1.0 - fabs(1.0 - eval_2(12.3 + i / 16.0, j / 16.0) * 2.0));
      float value_2 = roundf(eval_2(45.6 + j / 32.0, i / 32.0) * 8.0f) / 8.0f;
      float value_3 = eval_2(78.9 + i / 64.0, j / 64.0);
      float value_4 = eval_2(1.2 + j / 64.0, i / 64.0);
      
      int64_t height = 32.0 + floorf(80.0f * lerp(0.1, 1.0, value_4) * lerp(value_1, value_2, value_3));
      
      if (i == (int)(cam_x) % VX_SIZE_X && j == (int)(cam_z) % VX_SIZE_Z) {
        cam_y = height + 3.0f;
      }
      
      for (int64_t k = 0; k <= height; k++) {
        set_world(2, i + diff_x(i, k, j), k, j + diff_z(i, k, j));
      }
    }
    
    printf("terrain: %.2f%% done\n", (i * 100.0f) / VX_SIZE_X);
  }
  
  for (int64_t i = 0; i < VX_SIZE_Z; i++) {
    for (int64_t j = 0; j < VX_SIZE_Y; j++) {
      for (int64_t k = 0; k < VX_SIZE_X; k++) {
        if (j < VX_SIZE_Y - 1) {
          if (get_world(k, j, i) == 2 && get_world(k, j + 1, i) == 0) {
            set_world(1, k, j, i);
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
  
  int tree_count = VX_SIZE_X * VX_SIZE_Z * 0.35f * 0.015f;
  
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
    
    if (get_world(x, y - 1, z) == 11 || get_world(x, y - 1, z) == 12) {
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
    ImageClearBackground(&screen, BLACK);
    
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
        
        Color color = plot_pixel(cam_x, cam_y, cam_z, x, y, dir_x, dir_y, dir_z, VX_LIMIT);
        
        if (memcmp(&color, &AIR_COLOR, sizeof(Color))) {
          ImageDrawPixel(&screen, x, y, color);
        }
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
      
      handle_y(old_y);
    } else if (IsKeyDown(KEY_LEFT_SHIFT)) {
      cam_y -= 7.50 * GetFrameTime();
      
      handle_y(old_y);
    }
    
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
    
    char buffer[100];
    
    sprintf(buffer, "X: %.02f", cam_x);
    DrawText(buffer, 10, 30, 20, WHITE);
    
    sprintf(buffer, "Y: %.02f", cam_y);
    DrawText(buffer, 10, 50, 20, WHITE);
    
    sprintf(buffer, "Z: %.02f", cam_z);
    DrawText(buffer, 10, 70, 20, WHITE);
    
    DrawFPS(10, 10);
    EndDrawing();
  }
  
  free(world);
  CloseWindow();
}
