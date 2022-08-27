#include <msgbox.h>
#include <server.h>
#include <string.h>
#include <stdio.h>

uint32_t vx_seed = 123456789;

static void server_update(msg_Conn *conn, msg_Event event, msg_Data data) {
  if (event == msg_message || event == msg_request) {
    vx_packet_t *packet = (vx_packet_t *)(data.bytes);
    vx_client_t *client = vx_client_find(conn);
    
    if (packet->type == vx_packet_welcome && !client) {
      packet->welcome[19] = '\0';
      if (vx_client_find_name(packet->welcome)) return;
      
      client = vx_client_join(packet->welcome, conn);
      printf("client '%s' connected\n", client->name);
      
      for (int i = 0; i < VX_MAX_CLIENTS; i++) {
        if (!vx_clients[i].connection) continue;
        
        msg_Data msg_data = msg_new_data_space(vx_packet_size(vx_packet_update));
        vx_packet_t *response = (vx_packet_t *)(msg_data.bytes);
        
        response->type = vx_packet_update;
        strcpy(response->update.name, client->name);
        
        response->update.pos_x = client->pos_x;
        response->update.pos_y = client->pos_y;
        response->update.pos_z = client->pos_z;
        
        msg_send(vx_clients[i].connection, msg_data);
        msg_delete_data(msg_data);
      }
    }
    
    if (!client) return;
    
    if (packet->type == vx_packet_request) {
      if (packet->request[0] < VX_TOTAL_X && packet->request[1] < VX_TOTAL_Z) {
        // printf("player issued chunk (%u, %u)\n", packet->request[0], packet->request[1]);
        
        msg_Data msg_data = msg_new_data_space(vx_packet_size(vx_packet_chunk));
        vx_packet_t *response = (vx_packet_t *)(msg_data.bytes);
        
        response->type = vx_packet_chunk;
        response->chunk = *vx_chunk_load(packet->request[0], packet->request[1]);
        
        msg_send(conn, msg_data);
        msg_delete_data(msg_data);
      }
    } else if (packet->type == vx_packet_place) {
      if (packet->place.x < VX_TOTAL_X * VX_CHUNK_X &&
          packet->place.y < VX_CHUNK_Y &&
          packet->place.z < VX_TOTAL_Z * VX_CHUNK_Z &&
          packet->place.tile < vx_tile_count) {
        uint32_t chunk_x = packet->place.x / VX_CHUNK_X;
        uint32_t tile_x = packet->place.x % VX_CHUNK_X;
        
        uint32_t chunk_z = packet->place.z / VX_CHUNK_Z;
        uint32_t tile_z = packet->place.z % VX_CHUNK_Z;
        
        if (!vx_chunks[chunk_x + chunk_z * VX_TOTAL_X]) {
          vx_chunk_load(chunk_x, chunk_z);
        }
        
        if (vx_chunks[chunk_x + chunk_z * VX_TOTAL_X]) {
          vx_chunks[chunk_x + chunk_z * VX_TOTAL_X]->data[tile_x + (tile_z + packet->place.y * VX_CHUNK_Z) * VX_CHUNK_X] = packet->place.tile;
          vx_chunks[chunk_x + chunk_z * VX_TOTAL_X]->dirty = 1;
          
          for (int i = 0; i < VX_MAX_CLIENTS; i++) {
            if (vx_clients[i].connection == conn) continue;
            if (!vx_clients[i].connection) continue;
            
            msg_Data msg_data = msg_new_data_space(vx_packet_size(vx_packet_place));
            vx_packet_t *response = (vx_packet_t *)(msg_data.bytes);
            
            memcpy(response, packet, vx_packet_size(vx_packet_place));
            msg_send(vx_clients[i].connection, msg_data);
            
            msg_delete_data(msg_data);
          }
        }
      }
    } else if (packet->type == vx_packet_update) {
      if (vx_client_find_name(packet->update.name) != client) return;
      
      client->pos_x = packet->update.pos_x;
      client->pos_y = packet->update.pos_y;
      client->pos_z = packet->update.pos_z;
      
      for (int i = 0; i < VX_MAX_CLIENTS; i++) {
        if (vx_clients[i].connection == conn) continue;
        if (!vx_clients[i].connection) continue;
        
        msg_Data msg_data = msg_new_data_space(vx_packet_size(vx_packet_update));
        vx_packet_t *response = (vx_packet_t *)(msg_data.bytes);
        
        memcpy(response, packet, vx_packet_size(vx_packet_update));
        msg_send(vx_clients[i].connection, msg_data);
        
        msg_delete_data(msg_data);
      }
    } else if (packet->type == vx_packet_chat) {
      printf("[%s] %s\n", client->name, packet->chat);
      
      for (int i = 0; i < VX_MAX_CLIENTS; i++) {
        if (vx_clients[i].connection == conn) continue;
        if (!vx_clients[i].connection) continue;
        
        msg_Data msg_data = msg_new_data_space(vx_packet_size(vx_packet_chat) + strlen(packet->chat) + strlen(client->name) + 4);
        vx_packet_t *response = (vx_packet_t *)(msg_data.bytes);
        
        response->type = vx_packet_chat;
        sprintf(response->chat, "[%s] %s", client->name, packet->chat);
        
        msg_send(vx_clients[i].connection, msg_data);
        msg_delete_data(msg_data);
      }
    }
  } else if (event == msg_connection_closed || event == msg_connection_lost || event == msg_error) {
    vx_client_t *client = vx_client_find(conn);
    
    if (client) {
      client->connection = NULL;
      printf("client '%s' disconnected\n", client->name);
      
      for (int i = 0; i < VX_MAX_CLIENTS; i++) {
        if (!vx_clients[i].connection) continue;
        
        msg_Data msg_data = msg_new_data_space(vx_packet_size(vx_packet_bye));
        vx_packet_t *response = (vx_packet_t *)(msg_data.bytes);
        
        response->type = vx_packet_bye; 
        strcpy(response->bye, client->name);
        
        msg_send(vx_clients[i].connection, msg_data);
        msg_delete_data(msg_data);
      }
    }
  }
}

int main(void) {
  vx_chunk_init();
  vx_client_init();
  
  msg_listen("tcp://*:14142", server_update);
  
  for (;;) {
    msg_runloop(10);
  }
  
  vx_chunk_unload(vx_loaded_count);
  return 0;
}
