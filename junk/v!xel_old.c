// #define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

// #undef STB_IMAGE_IMPLEMENTATION

#include <raylib.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define VX_WIDTH  320
#define VX_HEIGHT 200
#define VX_ZOOM   4
#define VX_SIZE_X 40030
#define VX_SIZE_Y 256
#define VX_SIZE_Z 20015
#define VX_ITER   128

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define fast_sin(num) (fast_cos((num) + 1.5f * PI))
#define fast_abs(num) ((num) < 0 ? -(num) : (num))

uint8_t *world = NULL;
uint32_t *color = NULL;

int world_width = 0;
int world_height = 0;

float cam_x = VX_SIZE_X / 2;
float cam_y = 64;
float cam_z = VX_SIZE_Z / 2;

float angle_lr = 0;
float angle_ud = 0;

int selected = 1;

float fast_cos(float x) {
  float tp = 1 / (2 * PI);

  x = fast_abs(x * tp);
  x -= 0.25f + (int)(x + 0.25f);
  x *= 16.0f * (fast_abs(x) - 0.5f);

  // optional
  x += 0.225f * x * (fast_abs(x) - 1.0f);

  return x;
}

uint8_t get_world(int x, int y, int z) {
  if (x < 0 || z < 0 || x >= world_width || z >= world_height) {
    return 0;
  }
  
  if (y <= world[x + z * world_width]) {
    return 2;
  }
  
  return 0;
}

Color get_color(int x, int y, int z) {
  if (x < 0 || z < 0 || x >= world_width || z >= world_height) {
    return BLACK;
  }
  
  x = (x * 21600) / VX_SIZE_X;
  z = (z * 10800) / VX_SIZE_Z;
  
  uint32_t c = color[x + z * 21600];
  return (Color){c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF, 0xFF};
}

void set_world(uint8_t value, int x, int y, int z) {
  return;
  
  /*
  if (x < 0 || y < 0 || z < 0 || x >= VX_SIZE_X || y >= VX_SIZE_Y || z >= VX_SIZE_Z) {
    return;
  }
  
  world[x + (y + z * VX_SIZE_Y) * VX_SIZE_X] = value;
  */
}

void plot_pixel(int scr_x, int scr_y, float dir_x, float dir_y, float dir_z) {
  int pos_x = cam_x;
  int pos_y = cam_y;
  int pos_z = cam_z;
  
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
  
  for (int i = 0; i < VX_ITER; i++) {
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
      
      if (hit_side == 0) {
        c.r = (int)(c.r * 0.75);
        c.g = (int)(c.g * 0.75);
        c.b = (int)(c.b * 0.75);
      } else if (hit_side == 2) {
        c.r = (int)(c.r * 0.50);
        c.g = (int)(c.g * 0.50);
        c.b = (int)(c.b * 0.50);
      }
      
      if (scr_x == GetMouseX() / VX_ZOOM && scr_y == GetMouseY() / VX_ZOOM) {
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
      
      DrawRectangle(scr_x * VX_ZOOM, scr_y * VX_ZOOM, VX_ZOOM, VX_ZOOM, c);
      return;
    }
  }
}

int main(void) {
  InitWindow(VX_WIDTH * VX_ZOOM, VX_HEIGHT * VX_ZOOM, "v!xel test");
  
  int channels = 0;
  
  srand(time(0));
  
  
  
  world = stbi_load("h_map_40030.png", &world_width, &world_height, &channels, 1);
  if (channels != 1) return 1;
  
  color = (uint32_t *)(stbi_load("c_map.png", &world_width, &world_height, &channels, 4));
  // if (channels != 4) return 1;
  
  world_width = 40030;
  world_height = 20015;
  
  printf("%d, %d\n", world_width, world_height);
  
  cam_x = rand() % world_width;
  cam_z = rand() % world_height;
  
  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLACK);
    
    if (get_world((int)(cam_x), (int)(cam_y) - 2, (int)(cam_z))) {
      cam_y = (int)(cam_y) + 1.0f;
    } else {
      cam_y -= 0.1f;
    }
    
    float ratio_y = (float)(VX_HEIGHT) / VX_WIDTH;
    
    for (int y = 0; y < VX_HEIGHT; y++) {
      for (int x = 0; x < VX_WIDTH; x++) {
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
        
        plot_pixel(x, y, dir_x, dir_y, dir_z);
      }
    }
    
    if (IsKeyDown(KEY_W)) {
      cam_x += fast_sin(angle_lr);
      cam_z += fast_cos(angle_lr);
    } else if (IsKeyDown(KEY_S)) {
      cam_x -= fast_sin(angle_lr);
      cam_z -= fast_cos(angle_lr);
    }
    
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
      angle_lr -= 0.05;
    } else if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
      angle_lr += 0.05;
    }
    
    if (IsKeyDown(KEY_Q)) {
      angle_ud -= 0.05;
    } else if (IsKeyDown(KEY_E)) {
      angle_ud += 0.05;
    }
    
    if (IsKeyDown(KEY_SPACE)) {
      cam_y++;
    } else if (IsKeyDown(KEY_LEFT_SHIFT)) {
      cam_y--;
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
