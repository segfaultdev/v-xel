#include <server.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

vx_client_t *vx_clients = NULL;

void vx_client_init(void) {
  vx_clients = malloc(VX_MAX_CLIENTS * sizeof(vx_client_t));
  
  for (int i = 0; i < VX_MAX_CLIENTS; i++) {
    vx_clients[i].connection = NULL;
  }
}

vx_client_t *vx_client_join(const char *name, void *connection) {
  for (int i = 0; i < VX_MAX_CLIENTS; i++) {
    if (vx_clients[i].connection) continue;
    
    vx_clients[i].connection = connection;
    strcpy(vx_clients[i].name, name);
    
    vx_clients[i].pos_x = VX_TOTAL_X * VX_CHUNK_X * 0.5f;
    vx_clients[i].pos_y = 1;
    vx_clients[i].pos_z = VX_TOTAL_Z * VX_CHUNK_Z * 0.5f;
    
    return (vx_clients + i);
  }
  
  return NULL;
}

void vx_client_exit(vx_client_t *client) {
  client->connection = NULL;
}

vx_client_t *vx_client_find(void *connection) {
  for (int i = 0; i < VX_MAX_CLIENTS; i++) {
    if (vx_clients[i].connection == connection) {
      return (vx_clients + i);
    }
  }
  
  return NULL;
}

vx_client_t *vx_client_find_name(const char *name) {
  for (int i = 0; i < VX_MAX_CLIENTS; i++) {
    if (!vx_clients[i].connection) continue;
    
    if (!strcmp(vx_clients[i].name, name)) {
      return (vx_clients + i);
    }
  }
  
  return NULL;
}

