#include <pthread.h>
#include <msgbox.h>
#include <client.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static pthread_t loader_thread;
static int loader_waiting = 0;

static msg_Conn *connection = NULL;
static const char *connection_ip = NULL;

static void client_update(msg_Conn *conn, msg_Event event, msg_Data data) {
  if (event == msg_connection_ready) {
    connection = conn;
    
    msg_Data msg_data = msg_new_data_space(vx_packet_size(vx_packet_welcome));
    vx_packet_t *packet = msg_data.bytes;
    
    packet->type = vx_packet_welcome;
    strcpy(packet->welcome, vx_name);
    
    msg_send(connection, msg_data);
    msg_delete_data(msg_data);
  } else if (event == msg_message || event == msg_reply) {
    vx_packet_t *packet = data.bytes;
    
    if (packet->type == vx_packet_chunk) {
      uint32_t mod_x = packet->chunk.chunk_x % VX_TOTAL_SIDE;
      uint32_t mod_z = packet->chunk.chunk_z % VX_TOTAL_SIDE;
      
      vx_chunks[mod_x + mod_z * VX_TOTAL_SIDE] = packet->chunk;
      vx_chunks[mod_x + mod_z * VX_TOTAL_SIDE].loaded = 1;
      vx_chunks[mod_x + mod_z * VX_TOTAL_SIDE].requested = 0;
      
      loader_waiting = 0;
    } else if (packet->type == vx_packet_place) {
      if (packet->place.y >= VX_CHUNK_Y) return;
      
      uint32_t chunk_x = (packet->place.x / VX_CHUNK_X);
      uint32_t chunk_z = (packet->place.z / VX_CHUNK_Z);
      
      uint32_t mod_x = (chunk_x % VX_TOTAL_SIDE);
      uint32_t mod_z = (chunk_z % VX_TOTAL_SIDE);
      
      uint32_t tile_x = (packet->place.x % VX_CHUNK_X);
      uint32_t tile_z = (packet->place.z % VX_CHUNK_Z);
      
      if (vx_chunks[mod_x + mod_z * VX_TOTAL_SIDE].chunk_x == chunk_x &&
          vx_chunks[mod_x + mod_z * VX_TOTAL_SIDE].chunk_z == chunk_z &&
          vx_chunks[mod_x + mod_z * VX_TOTAL_SIDE].loaded) {
        vx_chunks[mod_x + mod_z * VX_TOTAL_SIDE].data[tile_x + (tile_z + packet->place.y * VX_CHUNK_Z) * VX_CHUNK_X] = packet->place.tile;
      }
    } else if (packet->type == vx_packet_update) {
      if (!strcmp(packet->update.name, vx_name)) {
        vx_player.pos_x = packet->update.pos_x;
        vx_player.pos_y = packet->update.pos_y;
        vx_player.pos_z = packet->update.pos_z;
      } else {
        vx_client_update(packet->update.name, packet->update.pos_x, packet->update.pos_y, packet->update.pos_z);
      }
    } else if (packet->type == vx_packet_bye) {
      vx_client_remove(packet->bye);
    }
  } else if (event == msg_error) {
    vx_fatal("%s\n", msg_as_str(data));
  }
}

static void *loader_function(void *) {
  char buffer[128];
  sprintf(buffer, "tcp://%s:14142", connection_ip);
  
  msg_connect(buffer, client_update, msg_no_context);
  
  for (;;) {
    msg_runloop(10);
    
    if (loader_waiting) continue;
    if (!connection) continue;
    
    for (uint32_t z = 0; z < VX_TOTAL_SIDE; z++) {
      for (uint32_t x = 0; x < VX_TOTAL_SIDE; x++) {
        if (vx_chunks[x + z * VX_TOTAL_SIDE].requested && !vx_chunks[x + z * VX_TOTAL_SIDE].loaded) {
          uint32_t chunk_x = vx_chunks[x + z * VX_TOTAL_SIDE].chunk_x;
          uint32_t chunk_z = vx_chunks[x + z * VX_TOTAL_SIDE].chunk_z;
          
          loader_waiting = 1;
          
          msg_Data msg_data = msg_new_data_space(vx_packet_size(vx_packet_request));
          vx_packet_t *packet = msg_data.bytes;
          
          packet->type = vx_packet_request;
          packet->request[0] = chunk_x;
          packet->request[1] = chunk_z;
          
          msg_send(connection, msg_data);
          msg_delete_data(msg_data);
          
          // printf("loading chunk (%u, %u)\n", chunk_x, chunk_z);
        }
      }
    }
  }
  
  pthread_exit(NULL);
}

void vx_loader_init(const char *server_ip) {
  connection_ip = server_ip;
  pthread_create(&loader_thread, NULL, loader_function, NULL);
}

void vx_loader_place(uint8_t tile, uint32_t x, uint32_t y, uint32_t z) {
  if (y >= VX_CHUNK_Y) return;
  if (!connection) return;
  
  uint32_t chunk_x = (x / VX_CHUNK_X);
  uint32_t chunk_z = (z / VX_CHUNK_Z);
  
  uint32_t mod_x = (chunk_x % VX_TOTAL_SIDE);
  uint32_t mod_z = (chunk_z % VX_TOTAL_SIDE);
  
  uint32_t tile_x = (x % VX_CHUNK_X);
  uint32_t tile_z = (z % VX_CHUNK_Z);
  
  vx_chunks[mod_x + mod_z * VX_TOTAL_SIDE].data[tile_x + (tile_z + y * VX_CHUNK_Z) * VX_CHUNK_X] = tile;
  
  msg_Data msg_data = msg_new_data_space(vx_packet_size(vx_packet_place));
  vx_packet_t *packet = msg_data.bytes;
  
  packet->type = vx_packet_place;
  
  packet->place.x = x;
  packet->place.y = y;
  packet->place.z = z;
  
  packet->place.tile = tile;
  
  msg_send(connection, msg_data);
  msg_delete_data(msg_data);
}

void vx_loader_update(float pos_x, float pos_y, float pos_z) {
  if (!connection) return;
  
  msg_Data msg_data = msg_new_data_space(vx_packet_size(vx_packet_update));
  vx_packet_t *packet = msg_data.bytes;
  
  packet->type = vx_packet_update;
  
  strcpy(packet->update.name, vx_name);
  
  packet->update.pos_x = pos_x;
  packet->update.pos_y = pos_y;
  packet->update.pos_z = pos_z;
  
  msg_send(connection, msg_data);
  msg_delete_data(msg_data);
}
