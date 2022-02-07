// #define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

// #undef STB_IMAGE_IMPLEMENTATION

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
#define VX_SIZE_Y (int64_t)(256)
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
  if (x < 0 || y < 0 || z < 0 || x >= VX_SIZE_X || y >= VX_SIZE_Y || z >= VX_SIZE_Z) {
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
  }
  
  return BLACK;
}

void set_world(uint8_t value, int64_t x, int64_t y, int64_t z) {
  if (x < 0 || y < 0 || z < 0 || x >= VX_SIZE_X || y >= VX_SIZE_Y || z >= VX_SIZE_Z) {
    return;
  }
  
  world[x + (y + z * VX_SIZE_Y) * VX_SIZE_X] = value;
}

Color plot_pixel(float cam_x, float cam_y, float cam_z, int scr_x, int scr_y, float dir_x, float dir_y, float dir_z, int limit) {
  if (!limit) return AIR_COLOR;
  
  int64_t pos_x = cam_x;
  int64_t pos_y = cam_y;
  int64_t pos_z = cam_z;
  
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
      
      if (tile >= 8 && tile <= 10) {
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
        
        if (border_dist < 0.05f) {
          border_dist = 0.0f;
        } else {
          border_dist = 1.0f;
        }
      } else {
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
        
        if (border_dist < 0.1f) {
          border_dist *= 10.0f;
        } else {
          border_dist = 1.0f;
        }
        
        border_dist = border_dist * 0.2f + 0.8f;
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
      
      float ref_x = 0.0f;
      float ref_y = 1.0f;
      float ref_z = 0.0f;
      
      if (tile == 1) {
        float value = 0.0f;
        
        value += fast_sin(hit_x * 47.02f + hit_z * 33.37f) * fast_sin(hit_x * 30.0f) * 6.0f;
        value += fast_sin(hit_x * 31.11f - hit_z * 24.25f) * fast_sin(hit_y * 49.1f) * 5.0f;
        value += fast_sin(hit_y * 55.89f - hit_x * 12.73f) * fast_sin(hit_z *  8.2f) * 4.0f;
        
        c.r = MAX(0, MIN(255, (int)(c.r) + value));
        c.g = MAX(0, MIN(255, (int)(c.g) + value));
        c.b = MAX(0, MIN(255, (int)(c.b) + value));
      } else if (tile == 2 || tile == 7) {
        float value = 0.0f;
        
        value += fast_sin(hit_x * 7.02f + hit_z * 15.37f) * fast_sin(hit_y * 30.0f) * 6.0f;
        value += fast_sin(hit_x * 19.11f - hit_z * 3.25f) * fast_sin(hit_y * 49.1f) * 5.0f;
        value += fast_sin(hit_y * 1.89f - hit_x * 8.73f)  * fast_sin(hit_z *  8.2f) * 4.0f;
        
        c.r = MAX(0, MIN(255, (int)(c.r) + value));
        c.g = MAX(0, MIN(255, (int)(c.g) + value));
        c.b = MAX(0, MIN(255, (int)(c.b) + value));
      } else if (tile == 3) {
        ref_x += fast_sin((hit_x + hit_y + hit_z - 0.17f * GetTime()) * 13.0f) * 0.02f;
        ref_y += fast_sin((hit_x + hit_y - hit_z - 0.15f * GetTime()) * 15.0f) * 0.02f;
        ref_z += fast_sin((hit_x - hit_y + hit_z + 0.13f * GetTime()) * 17.0f) * 0.02f;
        
        if (hit_side == 0) {
          ref_x = 0.0f;
        } else if (hit_side == 1) {
          ref_y = 1.0f;
        } else if (hit_side == 2) {
          ref_z = 0.0f;
        }
      }
      
      Color top = plot_pixel(hit_x - dir_x * 0.001f, hit_y - dir_z * 0.001f, hit_z - dir_z * 0.001f, -1, -1, ref_x, ref_y, ref_z, MIN(limit - 1, 1));
      
      if (!(tile >= 8 && tile <= 10) && (memcmp(&top, &AIR_COLOR, sizeof(Color)) || (hit_side == 1 && step_y > 0))) {
        c.r = c.r * 0.4f + top.r * 0.1f;
        c.g = c.g * 0.4f + top.g * 0.1f;
        c.b = c.b * 0.4f + top.b * 0.1f;
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

int main(void) {
  InitWindow(VX_WIDTH * VX_ZOOM, VX_HEIGHT * VX_ZOOM, "v!xel test");
  srand(time(0));
  
  world = calloc(VX_SIZE_X * VX_SIZE_Y * VX_SIZE_Z, 1);
  
  for (int64_t i = 0; i < VX_SIZE_X; i++) {
    for (int64_t j = 0; j < VX_SIZE_Z; j++) {
      set_world(1, i, 1, j);
      set_world(2, i, 0, j);
    }
  }
  
  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLACK);
    
    if (get_world((int64_t)(cam_x), (int64_t)(cam_y) - 1, (int64_t)(cam_z))) {
      cam_y = (int64_t)(cam_y) + 1.0f;
    }
    
    float ratio_y = (float)(VX_HEIGHT) / VX_WIDTH;
    
    for (int64_t y = 0; y < VX_HEIGHT; y++) {
      for (int64_t x = 0; x < VX_WIDTH; x++) {
        float scr_x = ((float)(x) / VX_WIDTH) * 2 - 1;
        float scr_y = ((float)(y) / VX_HEIGHT) * 2 - 1;
        
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
          DrawRectangle(x * VX_ZOOM, y * VX_ZOOM, VX_ZOOM, VX_ZOOM, color);
        }
      }
    }
    
    if (IsKeyDown(KEY_W)) {
      cam_x += 5.00 * fast_sin(angle_lr) * GetFrameTime();
      cam_z += 5.00 * fast_cos(angle_lr) * GetFrameTime();
    } else if (IsKeyDown(KEY_S)) {
      cam_x -= 5.00 * fast_sin(angle_lr) * GetFrameTime();
      cam_z -= 5.00 * fast_cos(angle_lr) * GetFrameTime();
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
