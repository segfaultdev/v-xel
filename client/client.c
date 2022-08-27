#include <external/glad.h>
#include <GLFW/glfw3.h>
#include <client.h>
#include <raylib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <rlgl.h>
#include <time.h>

#define VX_WIDTH  288
#define VX_HEIGHT 162
#define VX_ZOOM   4

#define SQUARE(x) ((x) * (x))

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define fast_abs(num) ((num) < 0 ? -(num) : (num))

char vx_name[20];

vx_client_t vx_player = (vx_client_t){
  .pos_x = 16384.0f,
  .pos_y = 96.0f,
  .pos_z = 16384.0f,
};

float last_x = 0.0f;
float last_y = 0.0f;
float last_z = 0.0f;

float vel_y = 0;
int on_ground = 0;

float angle_lr = 0;
float angle_ud = 0;

float daytime = 0.25f;

GLuint ssbo;
uint8_t sent[VX_TOTAL_SIDE * VX_TOTAL_SIDE];

uint8_t get_world(uint32_t x, uint32_t y, uint32_t z) {
  return vx_chunk_get(x, y, z);
}

int is_solid(uint32_t x, uint32_t y, uint32_t z) {
  int tile = get_world(x, y, z);
  return (tile != vx_tile_air && tile != vx_tile_water);
}

void set_world(uint8_t value, uint32_t x, uint32_t y, uint32_t z) {
  uint32_t chunk_x = (x / VX_CHUNK_X) % VX_TOTAL_SIDE;
  uint32_t chunk_z = (z / VX_CHUNK_Z) % VX_TOTAL_SIDE;
  
  sent[chunk_x + chunk_z * VX_TOTAL_SIDE] = 0;
  vx_loader_place(value, x, y, z);
}

void handle_collision(void) {
  on_ground = 0;
  
  int tile_lo = get_world(vx_player.pos_x, vx_player.pos_y - 1.0f, vx_player.pos_z);
  int tile_hi = get_world(vx_player.pos_x, vx_player.pos_y + 0.5f, vx_player.pos_z);
  
  if (tile_lo) {
    vx_player.pos_y = (int)(vx_player.pos_y) + 1.0f;
    if (vel_y < 0.0f) vel_y = 0.0f;
    
    on_ground = 1;
  } else if (tile_hi) {
    vx_player.pos_y = (int)(vx_player.pos_y) + 0.49f;
    if (vel_y > 0.0f) vel_y = 0.0f;
  }
  
  float new_x = vx_player.pos_x;
  float new_z = vx_player.pos_z;
  
  for (int y = (int)(vx_player.pos_y) - 1; y <= (int)(vx_player.pos_y); y++) {
    for (int z = (int)(vx_player.pos_z) - 1; z <= (int)(vx_player.pos_z) + 1; z++) {
      for (int x = (int)(vx_player.pos_x) - 1; x <= (int)(vx_player.pos_x) + 1; x++) {
        int delta_x = x - (int)(vx_player.pos_x);
        int delta_z = z - (int)(vx_player.pos_z);
        
        int move_x = (delta_x != 0);
        int move_z = (delta_z != 0);
        
        if (move_x + move_z != 1) continue;
        
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
  
  vx_player.pos_x = new_x;
  vx_player.pos_z = new_z;
}

void check_jesus(float cam_x, float cam_y, float cam_z, float dir_x, float dir_y, float dir_z, int *mouse_pos, int *mouse_step, int *mouse_side) {
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
    
    if ((pos_y < 0 && step_y < 0) || (pos_y >= VX_CHUNK_Y && step_y > 0)) break;
    uint8_t tile = get_world(pos_x, pos_y, pos_z);
    
    if (tile) {
      mouse_pos[0] = pos_x;
      mouse_pos[1] = pos_y;
      mouse_pos[2] = pos_z;
      
      *mouse_side = hit_side;
      
      if (hit_side == 0) {
        mouse_step[0] = step_x;
        mouse_step[1] = 0;
        mouse_step[2] = 0;
      } else if (hit_side == 1) {
        mouse_step[0] = 0;
        mouse_step[1] = step_y;
        mouse_step[2] = 0;
      } else if (hit_side == 2) {
        mouse_step[0] = 0;
        mouse_step[1] = 0;
        mouse_step[2] = step_z;
      }
      
      return;
    }
  }
  
  *mouse_side = -1;
}

int main(int argc, const char **argv) {
  if (argc != 3) {
    printf("usage: %s [NAME] [SERVER]\n", argv[0]);
    exit(1);
  }
  
  strcpy(vx_name, argv[1]);
  
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(VX_WIDTH * VX_ZOOM, VX_HEIGHT * VX_ZOOM, vx_name);
  
  srand(time(0));
  
  SetTraceLogLevel(LOG_NONE);
  
  Shader shader = LoadShader(0, "render.fs");
  int flying = 0;
  
  last_x = vx_player.pos_x;
  last_y = vx_player.pos_y;
  last_z = vx_player.pos_z;
  
  int has_sent_size = 0;
  int has_sent_angle = 0;
  int has_sent_camera = 0;
  
  int shader_vx_iterations = VX_ITER;
  int shader_vx_total_side = VX_TOTAL_SIDE;
  
  int selected = 1;
  int paused = 0;
  
  SetShaderValue(shader, GetShaderLocation(shader, "vx_iterations"), &shader_vx_iterations, SHADER_UNIFORM_INT);
  SetShaderValue(shader, GetShaderLocation(shader, "vx_total_side"), &shader_vx_total_side, SHADER_UNIFORM_INT);
  
  glGenBuffers(1, &ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, VX_CHUNK_X * VX_CHUNK_Z * VX_CHUNK_Y * VX_TOTAL_SIDE * VX_TOTAL_SIDE, NULL, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo);
  
  DisableCursor();
  
  vx_chunk_init();
  vx_loader_init(argv[2]);
  
  for (int i = 0; i < VX_TOTAL_SIDE * VX_TOTAL_SIDE; i++) {
    sent[i] = 1;
  }
  
  RenderTexture2D target;
  
  while (!WindowShouldClose()) {
    if (IsWindowResized()) {
      UnloadRenderTexture(target);
      has_sent_size = 0;
    }
    
    if (!IsWindowFocused()) {
      EnableCursor();
      paused = 1;
    }
    
    if (!has_sent_size) {
      float shader_screen_size[2] = {GetScreenWidth() / VX_ZOOM, GetScreenHeight() / VX_ZOOM};
      SetShaderValue(shader, GetShaderLocation(shader, "screen_size"), shader_screen_size, SHADER_UNIFORM_VEC2);
      
      target = LoadRenderTexture(shader_screen_size[0], shader_screen_size[1]);
      has_sent_size = 1;
    }
    
    if (!has_sent_angle) {
      float shader_angle_ud = angle_ud;
      float shader_angle_lr = angle_lr;
      
      SetShaderValue(shader, GetShaderLocation(shader, "angle_ud"), &shader_angle_ud, SHADER_UNIFORM_FLOAT);
      SetShaderValue(shader, GetShaderLocation(shader, "angle_lr"), &shader_angle_lr, SHADER_UNIFORM_FLOAT);
      
      has_sent_angle = 1;
    }
    
    if (!has_sent_camera) {
      float shader_camera[3] = {vx_player.pos_x, vx_player.pos_y, vx_player.pos_z};
      SetShaderValue(shader, GetShaderLocation(shader, "camera"), shader_camera, SHADER_UNIFORM_VEC3);
      
      has_sent_camera = 1;
    }
    
    for (int i = 0; i < VX_TOTAL_SIDE; i++) {
      for (int j = 0; j < VX_TOTAL_SIDE; j++) {
        if (!sent[j + i * VX_TOTAL_SIDE] && vx_chunks[j + i * VX_TOTAL_SIDE].loaded) {
          glBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            (j + i * VX_TOTAL_SIDE) * VX_CHUNK_X * VX_CHUNK_Z * VX_CHUNK_Y,
            VX_CHUNK_X * VX_CHUNK_Z * VX_CHUNK_Y,
            vx_chunks[j + i * VX_TOTAL_SIDE].data
          );
          
          sent[j + i * VX_TOTAL_SIDE] = 1;
        }
      }
    }
    
    float shader_time = daytime;
    SetShaderValue(shader, GetShaderLocation(shader, "time"), &shader_time, SHADER_UNIFORM_FLOAT);
    
    {
      float ray_x = 0.0f;
      float ray_y = 0.0f;
      float ray_z = 1.0f;
      
      float dir_x = ray_x;
      float dir_y = ray_y * cosf(-angle_ud) + ray_z * sinf(-angle_ud);
      float dir_z = ray_z * cosf(-angle_ud) - ray_y * sinf(-angle_ud);
      
      ray_x = dir_x;
      ray_y = dir_y;
      ray_z = dir_z;
      
      dir_x = ray_x * cosf(-angle_lr) - ray_z * sinf(-angle_lr);
      dir_y = ray_y;
      dir_z = ray_z * cosf(-angle_lr) + ray_x * sinf(-angle_lr);
      
      int mouse_pos[3];
      int mouse_step[3];
      int mouse_side;
      
      check_jesus(vx_player.pos_x, vx_player.pos_y + 0.5f, vx_player.pos_z, dir_x, dir_y, dir_z, mouse_pos, mouse_step, &mouse_side);
      
      if (mouse_side >= 0) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
          set_world(0, mouse_pos[0], mouse_pos[1], mouse_pos[2]);
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          int place_x = mouse_pos[0] - mouse_step[0];
          int place_y = mouse_pos[1] - mouse_step[1];
          int place_z = mouse_pos[2] - mouse_step[2];
          
          set_world(selected, place_x, place_y, place_z);
        }
      }
      
      SetShaderValue(shader, GetShaderLocation(shader, "mouse_pos"), mouse_pos, SHADER_UNIFORM_IVEC3);
      SetShaderValue(shader, GetShaderLocation(shader, "mouse_step"), mouse_step, SHADER_UNIFORM_IVEC3);
      SetShaderValue(shader, GetShaderLocation(shader, "mouse_side"), &mouse_side, SHADER_UNIFORM_INT);
    }
    
    BeginTextureMode(target);
    BeginShaderMode(shader);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), WHITE);
    EndShaderMode();
    EndTextureMode();
    
    BeginDrawing();
    
    DrawTexturePro(
      target.texture,
      (Rectangle){0, 0, GetScreenWidth() / VX_ZOOM, -GetScreenHeight() / VX_ZOOM},
      (Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()},
      (Vector2){0, 0},
      0.0f,
      WHITE
    );
    
    if (paused) {
      DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 127});
    } else {
      float mouse_delta_x = GetMouseDelta().x;
      float mouse_delta_y = GetMouseDelta().y;
      
      if (fast_abs(mouse_delta_x) > 0.01f) {
        angle_lr += mouse_delta_x * 0.0033f;
        has_sent_angle = 0;
      }
      
      if (fast_abs(mouse_delta_y) > 0.01f) {
        angle_ud += mouse_delta_y * 0.0033f;
        
        if (angle_ud < -PI / 2.0f) angle_ud = -PI / 2.0f;
        if (angle_ud > PI / 2.0f) angle_ud = PI / 2.0f;
        
        has_sent_angle = 0;
      }
      
      if (IsKeyDown(KEY_W)) {
        vx_player.pos_x += 10.00 * sinf(angle_lr) * GetFrameTime();
        vx_player.pos_z += 10.00 * cosf(angle_lr) * GetFrameTime();
      } else if (IsKeyDown(KEY_S)) {
        vx_player.pos_x -= 10.00 * sinf(angle_lr) * GetFrameTime();
        vx_player.pos_z -= 10.00 * cosf(angle_lr) * GetFrameTime();
      } else if (IsKeyDown(KEY_A)) {
        vx_player.pos_x -= 10.00 * sinf(angle_lr + PI / 2.0f) * GetFrameTime();
        vx_player.pos_z -= 10.00 * cosf(angle_lr + PI / 2.0f) * GetFrameTime();
      } else if (IsKeyDown(KEY_D)) {
        vx_player.pos_x += 10.00 * sinf(angle_lr + PI / 2.0f) * GetFrameTime();
        vx_player.pos_z += 10.00 * cosf(angle_lr + PI / 2.0f) * GetFrameTime();
      }
      
      if (IsKeyPressed(KEY_X)) {
        flying = !flying;
      }
      
      if (flying) {
        if (IsKeyDown(KEY_SPACE)) {
          vx_player.pos_y += 7.50 * GetFrameTime();
        } else if (IsKeyDown(KEY_LEFT_SHIFT)) {
          vx_player.pos_y -= 7.50 * GetFrameTime();
        }
      } else {
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
      
      if (IsKeyDown(KEY_Z)) {
        daytime += 0.15f * GetFrameTime();
        while (daytime >= 1.0f) daytime -= 1.0f;
      } else if (IsKeyDown(KEY_C)) {
        daytime -= 0.15f * GetFrameTime();
        while (daytime < 0.0f) daytime += 1.0f;
      }
    }
    
    if (IsKeyPressed(KEY_Q)) {
      paused = !paused;
      
      if (paused) EnableCursor();
      else DisableCursor();
    }
    
    float move_x = vx_player.pos_x - last_x;
    float move_y = vx_player.pos_y - last_y;
    float move_z = vx_player.pos_z - last_z;
    
    float move_dist = sqrtf(SQUARE(move_x) + SQUARE(move_y) + SQUARE(move_z));
    
    move_x /= move_dist;
    move_y /= move_dist;
    move_z /= move_dist;
    
    vx_player.pos_x = last_x;
    vx_player.pos_y = last_y;
    vx_player.pos_z = last_z;
    
    while (move_dist >= 0.0001f) {
      float max_move = move_dist;
      if (max_move > 0.2f) max_move = 0.2f;
      
      vx_player.pos_x += move_x * max_move;
      vx_player.pos_y += move_y * max_move;
      vx_player.pos_z += move_z * max_move;
      
      handle_collision();
      move_dist -= max_move;
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
    
    if (vx_player.pos_x != last_x || vx_player.pos_y != last_y || vx_player.pos_z != last_z) {
      has_sent_camera = 0;
    }
    
    if (fast_abs(last_x - vx_player.pos_x) + fast_abs(last_y - vx_player.pos_y) + fast_abs(last_z - vx_player.pos_z) >= 0.05f) {
      vx_loader_update(vx_player.pos_x, vx_player.pos_y, vx_player.pos_z);
    }
    
    last_x = vx_player.pos_x;
    last_y = vx_player.pos_y;
    last_z = vx_player.pos_z;
  }
  
  CloseWindow();
}

void on_chunk_update(uint32_t chunk_x, uint32_t chunk_z) {
  sent[chunk_x + chunk_z * VX_TOTAL_SIDE] = 0;
}
