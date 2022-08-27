#include <client.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

vx_client_t *vx_clients = NULL;
int vx_client_count = 0;

void vx_client_add(const char *name) {
  vx_clients = realloc(vx_clients, (vx_client_count + 1) * sizeof(vx_client_t));
  strcpy(vx_clients[vx_client_count].name, name);
  
  vx_clients[vx_client_count].pos_x = 16384.0f;
  vx_clients[vx_client_count].pos_y = 16384.0f;
  vx_clients[vx_client_count].pos_z = 16384.0f;
  
  char buffer[256];
  sprintf(buffer, "client '%s' connected\n", name);
  
  vx_loader_update(vx_player.pos_x, vx_player.pos_y, vx_player.pos_z);
  vx_chat_add(NULL, buffer);
  
  vx_client_count++;
}

void vx_client_remove(const char *name) {
  for (int i = 0; i < vx_client_count; i++) {
    if (!strcmp(vx_clients[i].name, name)) {
      vx_client_count--;
      
      for (int j = i; j < vx_client_count; i++) {
        vx_clients[j] = vx_clients[j + 1];
      }
      
      vx_clients = realloc(vx_clients, vx_client_count * sizeof(vx_client_t));
      
      char buffer[256];
      sprintf(buffer, "client '%s' disconnected\n", name);
      
      vx_chat_add(NULL, buffer);
      
      on_client_update();
      return;
    }
  }
}

void vx_client_update(const char *name, float pos_x, float pos_y, float pos_z) {
  for (int i = 0; i < vx_client_count; i++) {
    if (!strcmp(vx_clients[i].name, name)) {
      vx_clients[i].pos_x = pos_x;
      vx_clients[i].pos_y = pos_y;
      vx_clients[i].pos_z = pos_z;
      
      on_client_update();
      return;
    }
  }
  
  vx_client_add(name);
  vx_client_update(name, pos_x, pos_y, pos_z);
}
