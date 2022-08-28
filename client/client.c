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

float vx_time = 0.25f;

float last_x = 0.0f;
float last_y = 0.0f;
float last_z = 0.0f;

float vel_y = 0;
int on_ground = 0;

float angle_lr = 0;
float angle_ud = 0;

GLuint chunk_ssbo;
GLuint client_ssbo;

uint8_t sent[VX_TOTAL_SIDE * VX_TOTAL_SIDE];

int has_sent_size = 0;
int has_sent_angle = 0;
int has_sent_camera = 0;
int has_sent_clients = 0;
int has_sent_selected = 0;

int single_spinlock = 0;

uint32_t single_updates[4 * 64];
int single_count = 0;

uint8_t get_world(uint32_t x, uint32_t y, uint32_t z) {
  if (y >= 1048576) return vx_tile_stone;
  return vx_chunk_get(x, y, z);
}

int is_solid(uint32_t x, uint32_t y, uint32_t z) {
  int tile = get_world(x, y, z);
  return (tile != vx_tile_air && tile != vx_tile_water);
}

void set_world(uint8_t value, uint32_t x, uint32_t y, uint32_t z) {
  uint32_t chunk_x = (x / VX_CHUNK_X) % VX_TOTAL_SIDE;
  uint32_t chunk_z = (z / VX_CHUNK_Z) % VX_TOTAL_SIDE;
  
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
  if (argc != 3 && argc != 4) {
    printf("usage: %s [NAME] [SERVER] (SHADER PATH)\n", argv[0]);
    exit(1);
  }
  
  const char *shader_path = "render.fs";
  if (argc == 4) shader_path = argv[3];
  
  strcpy(vx_name, argv[1]);
  
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(VX_WIDTH * VX_ZOOM, VX_HEIGHT * VX_ZOOM, vx_name);
  
  srand(time(0));
  
  SetTraceLogLevel(LOG_WARNING);
  SetTargetFPS(60);
  
  SetExitKey(KEY_NULL);
  
  Shader shader = LoadShader(0, shader_path);
  int flying = 0;
  
  while (vx_player.pos_x < 0) vx_player.pos_x += 16777216.0f;
  while (vx_player.pos_z < 0) vx_player.pos_z += 16777216.0f;
  
  vx_player.pos_x = fmodf(vx_player.pos_x, 16777216.0f);
  vx_player.pos_z = fmodf(vx_player.pos_z, 16777216.0f);
  
  last_x = vx_player.pos_x;
  last_y = vx_player.pos_y;
  last_z = vx_player.pos_z;
  
  int shader_vx_iterations = VX_ITER;
  int shader_vx_total_side = VX_TOTAL_SIDE;
  int shader_vx_max_clients = VX_MAX_CLIENTS;
  
  int selected = 1;
  int paused = 0;
  int in_chat = 0;
  
  int was_paused = 0;
  
  SetShaderValue(shader, GetShaderLocation(shader, "vx_iterations"), &shader_vx_iterations, SHADER_UNIFORM_INT);
  SetShaderValue(shader, GetShaderLocation(shader, "vx_total_side"), &shader_vx_total_side, SHADER_UNIFORM_INT);
  SetShaderValue(shader, GetShaderLocation(shader, "vx_max_clients"), &shader_vx_max_clients, SHADER_UNIFORM_INT);
  
  GLuint ssbos[2];
  glGenBuffers(2, ssbos);
  
  client_ssbo = ssbos[0];
  chunk_ssbo = ssbos[1];
  
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, client_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * 3 * VX_MAX_CLIENTS, NULL, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, client_ssbo);
  
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, VX_CHUNK_X * VX_CHUNK_Z * VX_CHUNK_Y * VX_TOTAL_SIDE * VX_TOTAL_SIDE, NULL, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, chunk_ssbo);
  
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  DisableCursor();
  
  vx_chunk_init();
  vx_loader_init(argv[2]);
  
  for (int i = 0; i < VX_TOTAL_SIDE * VX_TOTAL_SIDE; i++) {
    sent[i] = 1;
  }
  
  RenderTexture2D target;
  
  char current_message[1024];
  int current_length = 0;
  
  float blank_start = GetTime();
  
  while (!WindowShouldClose()) {
    while (vx_player.pos_x < 0) vx_player.pos_x += 16777216.0f;
    while (vx_player.pos_z < 0) vx_player.pos_z += 16777216.0f;
    
    vx_player.pos_x = fmodf(vx_player.pos_x, 16777216.0f);
    vx_player.pos_z = fmodf(vx_player.pos_z, 16777216.0f);
    
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
    
    if (!has_sent_selected) {
      SetShaderValue(shader, GetShaderLocation(shader, "selected"), &selected, SHADER_UNIFORM_INT);
      has_sent_selected = 1;
    }
    
    for (int i = 0; i < VX_TOTAL_SIDE; i++) {
      for (int j = 0; j < VX_TOTAL_SIDE; j++) {
        if (!sent[j + i * VX_TOTAL_SIDE] && vx_chunks[j + i * VX_TOTAL_SIDE].loaded) {
          glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_ssbo);
          
          glBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            (j + i * VX_TOTAL_SIDE) * VX_CHUNK_X * VX_CHUNK_Z * VX_CHUNK_Y,
            VX_CHUNK_X * VX_CHUNK_Z * VX_CHUNK_Y,
            vx_chunks[j + i * VX_TOTAL_SIDE].data
          );
          
          glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
          sent[j + i * VX_TOTAL_SIDE] = 1;
        }
      }
    }
    
    while (single_spinlock);
    single_spinlock = 1;
    
    for (int i = 0; i < single_count; i++) {
      uint32_t x = single_updates[0 + 4 * i];
      uint32_t y = single_updates[1 + 4 * i];
      uint32_t z = single_updates[2 + 4 * i];
      uint8_t tile = single_updates[3 + 4 * i];
      
      uint32_t chunk_x = (x / 32) % VX_TOTAL_SIDE;
      uint32_t chunk_z = (z / 32) % VX_TOTAL_SIDE;
      
      uint32_t tile_x = x % 32;
      uint32_t tile_z = z % 32;
      
      uint32_t data_start = (chunk_x + chunk_z * VX_TOTAL_SIDE) * 32 * 32 * 128;
      
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_ssbo);
      
      glBufferSubData(
        GL_SHADER_STORAGE_BUFFER,
        data_start + (tile_x + (tile_z + y * 32) * 32),
        1,
        &tile
      );
      
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    
    single_count = 0;
    single_spinlock = 0;
    
    if (strcmp(shader_path, "render_faster.fs")) {
      float shader_time = vx_time;
      SetShaderValue(shader, GetShaderLocation(shader, "time"), &shader_time, SHADER_UNIFORM_FLOAT);
    }
    
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
      // SetShaderValue(shader, GetShaderLocation(shader, "mouse_step"), mouse_step, SHADER_UNIFORM_IVEC3);
      SetShaderValue(shader, GetShaderLocation(shader, "mouse_side"), &mouse_side, SHADER_UNIFORM_INT);
    }
    
    if (!has_sent_clients) {
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, client_ssbo);
      float client_array[3 * VX_MAX_CLIENTS];
      
      for (int i = 0; i < VX_MAX_CLIENTS; i++) {
        if (i >= vx_client_count) {
          client_array[0 + 3 * i] = -1.0f;
          client_array[1 + 3 * i] = -1.0f;
          client_array[2 + 3 * i] = -1.0f;
        } else if (vx_clients) {
          client_array[0 + 3 * i] = vx_clients[i].pos_x;
          client_array[1 + 3 * i] = vx_clients[i].pos_y;
          client_array[2 + 3 * i] = vx_clients[i].pos_z;
        }
      }
      
      glBufferSubData(
        GL_SHADER_STORAGE_BUFFER,
        0,
        sizeof(float) * 3 * VX_MAX_CLIENTS,
        client_array
      );
      
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
      has_sent_clients = 1;
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
    
    float ratio_y = (float)(VX_HEIGHT) / VX_WIDTH;
    
    for (int i = 0; i < vx_client_count; i++) {
      float rel_x = vx_clients[i].pos_x - vx_player.pos_x;
      float rel_y = vx_clients[i].pos_y - vx_player.pos_y;
      float rel_z = vx_clients[i].pos_z - vx_player.pos_z;
      
      float dir_x;
      float dir_y;
      float dir_z;
      
      dir_x = rel_x * cosf(angle_lr) - rel_z * sinf(angle_lr);
      dir_y = rel_y;
      dir_z = rel_z * cosf(angle_lr) + rel_x * sinf(angle_lr);
      
      rel_x = dir_x;
      rel_y = dir_y;
      rel_z = dir_z;
      
      dir_x = rel_x;
      dir_y = rel_y * cosf(angle_ud) + rel_z * sinf(angle_ud);
      dir_z = rel_z * cosf(angle_ud) - rel_y * sinf(angle_ud);
      
      if (dir_z < 0.0f) continue;
      
      dir_x /= dir_z;
      dir_y /= dir_z;
      
      float scr_x = ((dir_x + 1.0f) / 2.0f) * GetScreenWidth();
      float scr_y = (((-dir_y / ratio_y) + 1.0f) / 2.0f) * GetScreenHeight();
      
      float scale = (GetScreenWidth() * 0.25f) / dir_z;
      
      int start_x = scr_x - scale;
      int end_x = scr_x + scale;
      
      int start_y = scr_y - scale;
      int end_y = scr_y + scale * 3.0f;
      
      int length = MeasureText(vx_clients[i].name, 30) + 6;
      
      DrawRectangle((int)(scr_x) - (length / 2), start_y - 40, length, 36, (Color){0, 0, 0, 127});
      DrawText(vx_clients[i].name, ((int)(scr_x) - (length / 2)) + 3, start_y - 33, 30, WHITE);
    }
    
    for (int i = vx_message_count - 1; i >= 0; i--) {
      int index = (vx_message_count - 1) - i;
      
      int alpha = 255 - index * 32;
      if (in_chat) alpha = 255;
      
      if (alpha <= 0) break;
      
      int y_start = GetScreenHeight() - ((index + 1) * 24 + 32);
      int length = MeasureText(vx_messages[i], 20) + 4;
      
      DrawRectangle(0, y_start, length, 24, (Color){0, 0, 0, alpha * 0.5f});
      DrawText(vx_messages[i], 2, y_start + 2, 20, (Color){255, 255, 255, alpha});
    }
    
    if (paused) {
      if (in_chat) {
        current_message[current_length] = '\0';
        
        DrawRectangle(0, GetScreenHeight() - 24, GetScreenWidth() - 32 * VX_ZOOM, 24, (Color){0, 0, 0, 127});
        DrawText(current_message, 2, GetScreenHeight() - 22, 20, WHITE);
        
        if ((int)((GetTime() - blank_start) * 3.0f) % 2 == 0) {
          DrawRectangle(4 + MeasureText(current_message, 20), GetScreenHeight() - 22, 2, 20, WHITE);
        }
        
        if (IsKeyPressed(KEY_ENTER)) {
          vx_loader_chat(current_message);
          vx_chat_add(vx_name, current_message);
          
          paused = 0;
          in_chat = 0;
          
          DisableCursor();
        } else if (IsKeyPressed(KEY_BACKSPACE)) {
          if (current_length) current_length--;
        }
        
        for (;;) {
          char chr = GetCharPressed();
          if (!chr) break;
          
          current_message[current_length++] = chr;
          blank_start = GetTime();
        }
      } else {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 127});
      }
      
      was_paused = 1;
    } else if (!paused) {
      float mouse_delta_x = GetMouseDelta().x;
      float mouse_delta_y = GetMouseDelta().y;
      
      if (!was_paused && fast_abs(mouse_delta_x) > 0.01f) {
        angle_lr += mouse_delta_x * 0.0033f;
        has_sent_angle = 0;
      }
      
      if (!was_paused && fast_abs(mouse_delta_y) > 0.01f) {
        angle_ud += mouse_delta_y * 0.0033f;
        
        if (angle_ud < -PI / 2.0f) angle_ud = -PI / 2.0f;
        if (angle_ud > PI / 2.0f) angle_ud = PI / 2.0f;
        
        has_sent_angle = 0;
      }
      
      if (IsKeyDown(KEY_W)) {
        vx_player.pos_x += 10.00 * sinf(angle_lr) * GetFrameTime();
        vx_player.pos_z += 10.00 * cosf(angle_lr) * GetFrameTime();
      }
      
      if (IsKeyDown(KEY_S)) {
        vx_player.pos_x -= 10.00 * sinf(angle_lr) * GetFrameTime();
        vx_player.pos_z -= 10.00 * cosf(angle_lr) * GetFrameTime();
      }
      
      if (IsKeyDown(KEY_A)) {
        vx_player.pos_x -= 10.00 * sinf(angle_lr + PI / 2.0f) * GetFrameTime();
        vx_player.pos_z -= 10.00 * cosf(angle_lr + PI / 2.0f) * GetFrameTime();
      }
      
      if (IsKeyDown(KEY_D)) {
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
      }
      
      if (fast_abs(GetMouseWheelMove()) > 0.01f) {
        if (GetMouseWheelMove() < 0.0f) {
          selected = (((selected - 1) + 1) % (vx_tile_count - 1)) + 1;
        } else {
          selected = (((selected - 1) + (vx_tile_count - 2)) % (vx_tile_count - 1)) + 1;
        }
        
        has_sent_selected = 0;
      }
      
      if (IsKeyDown(KEY_Z)) {
        vx_time += 0.15f * GetFrameTime();
        while (vx_time >= 1.0f) vx_time -= 1.0f;
      } else if (IsKeyDown(KEY_C)) {
        vx_time -= 0.15f * GetFrameTime();
        while (vx_time < 0.0f) vx_time += 1.0f;
      }
      
      if (IsKeyPressed(KEY_T)) {
        current_length = 0;
        blank_start = GetTime();
        
        paused = 1;
        in_chat = 1;
        
        EnableCursor();
      }
      
      was_paused = 0;
    }
    
    if (!flying) {
      vel_y -= 20.0f * GetFrameTime();
      
      if (vel_y < -20.0f) vel_y = -20.0f;
      if (vel_y > 20.0f) vel_y = 20.0f;
      
      float delta_y = vel_y * GetFrameTime();
      
      if (delta_y <= -1.0f) delta_y = -0.99f;
      if (delta_y >= 1.0f) delta_y = 0.99f;
      
      vx_player.pos_y += delta_y;
    }
    
    if (IsKeyPressed(KEY_ESCAPE)) {
      paused = !paused;
      
      if (paused) EnableCursor();
      else DisableCursor();
      
      if (!paused) in_chat = 0;
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
    
    sprintf(buffer, "Time: %02d:%02d", ((int)(vx_time * 24.0f) + 10) % 24, (int)(vx_time * 24.0f * 60.0f) % 60);
    DrawText(buffer, 10, 50, 20, WHITE);
    
    vx_time += GetFrameTime() / 1440.0f;
    while (vx_time >= 1.0f) vx_time -= 1.0f;
    
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

void on_place_update(uint8_t tile, uint32_t x, uint32_t y, uint32_t z) {
  while (single_spinlock);
  single_spinlock = 1;
  
  single_updates[0 + 4 * single_count] = x;
  single_updates[1 + 4 * single_count] = y;
  single_updates[2 + 4 * single_count] = z;
  single_updates[3 + 4 * single_count] = tile;
  
  single_count++;
  single_spinlock = 0;
}

void on_client_update(void) {
  has_sent_clients = 0;
}
