#include <server.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

void vx_command(vx_client_t *client, const char *command) {
  const char *old_command = command;
  
  char **args = NULL;
  const char **starts = NULL;
  int count = 0;
  
  char *curr = NULL;
  int length = 0;
  
  while (*command) {
    if (*command == ' ') {
      if (length) {
        curr = realloc(curr, length + 1);
        curr[length] = '\0';
        
        args = realloc(args, (count + 1) * sizeof(char *));
        args[count] = curr;
        
        starts = realloc(starts, (count + 1) * sizeof(char *));
        starts[count] = command - length;
        
        count++;
      }
      
      curr = NULL;
      length = 0;
    } else {
      curr = realloc(curr, length + 1);
      curr[length++] = *command;
    }
    
    command++;
  }
  
  if (length) {
    curr = realloc(curr, length + 1);
    curr[length] = '\0';
    
    args = realloc(args, (count + 1) * sizeof(char *));
    args[count] = curr;
    
    starts = realloc(starts, (count + 1) * sizeof(char *));
    starts[count] = command - length;
    
    count++;
  }
  
  if (!strcmp(args[0], "/ping") && count == 1) {
    vx_server_send(client, "pong!");
    return;
  } else if (!strcmp(args[0], "/help")) {
    if (count == 1) {
      vx_server_send(client, "available commands: /ping /help /save /tp /tell /place /fill /time");
      return;
    } else if (count == 2) {
      if (!strcmp(args[1], "ping")) {
        vx_server_send(client, "/ping: send a 'pong!' to the client");
        return;
      } else if (!strcmp(args[1], "help")) {
        vx_server_send(client, "/help [command]: shows the usage of a command if specified, or a list of commands otherwise");
        return;
      } else if (!strcmp(args[1], "save")) {
        vx_server_send(client, "/save: saves the current world");
        return;
      } else if (!strcmp(args[1], "tp")) {
        vx_server_send(client, "/tp [player] or /tp [x] [y] [z]: teleports to a player or to some coords");
        return;
      } else if (!strcmp(args[1], "tell")) {
        vx_server_send(client, "/tell [player] [message]: sends a private message to a player");
        return;
      } else if (!strcmp(args[1], "place")) {
        vx_server_send(client, "/place [x] [y] [z] [tile]: places a tile at some coords given its id (air = 0, water = 1, etc.)");
        return;
      } else if (!strcmp(args[1], "fill")) {
        vx_server_send(client, "/fill [x1] [y1] [z1] [x2] [y2] [z2] [tile]: fills an area with a tile id (air = 0, water = 1, etc.)");
        return;
      } else if (!strcmp(args[1], "time")) {
        vx_server_send(client, "/time [hours]:[minutes] or /time [time]: sets the time to an specified time, either in standard format or "
                               "as a number from 0 to 1");
        return;
      }
    }
  } else if (!strcmp(args[0], "/save") && count == 1) {
    char buffer[100];
    sprintf(buffer, "client '%s' requested world save", client->name);
    
    vx_server_post(buffer);
    vx_chunk_unload(vx_loaded_count);
    
    return;
  } else if (!strcmp(args[0], "/tp")) {
    if (count == 2) {
      vx_client_t *dest = vx_client_find_name(args[1]);
      char buffer[100];
      
      if (dest) {
        sprintf(buffer, "teleported to client '%s'", args[1]);
        vx_server_send(client, buffer);
        
        sprintf(buffer, "client '%s' teleported to you", client->name);
        vx_server_send(dest, buffer);
        
        vx_server_move(client, dest->pos_x, dest->pos_y, dest->pos_z);
      } else {
        sprintf(buffer, "cannot find client '%s'", args[1]);
        vx_server_send(client, buffer);
      }
      
      return;
    } else if (count == 4) {
      char *end_x;
      float pos_x = strtof(args[1], &end_x);
      
      char *end_y;
      float pos_y = strtof(args[2], &end_y);
      
      char *end_z;
      float pos_z = strtof(args[3], &end_z);
      
      if (!(*end_x) && !(*end_y) && !(*end_z)) {
        char buffer[100];
        
        sprintf(buffer, "teleported to (%.2f, %.2f, %.2f)", pos_x, pos_y, pos_z);
        vx_server_send(client, buffer);
        
        vx_server_move(client, pos_x, pos_y, pos_z);
        return;
      }
    }
  } else if (!strcmp(args[0], "/tell") && count > 2) {
    vx_client_t *dest = vx_client_find_name(args[1]);
    
    if (dest) {
      vx_server_tell(client, dest, starts[2]);
    } else {
      char buffer[100];
      
      sprintf(buffer, "cannot find client '%s'", args[1]);
      vx_server_send(client, buffer);
    }
    
    return;
  } else if (!strcmp(args[0], "/place")) {
    
  } else if (!strcmp(args[0], "/fill")) {
    
  } else if (!strcmp(args[0], "/time") && count == 2) {
    char *end;
    float time = strtof(args[1], &end);
    
    int valid = 0;
    
    if (!(*end)) {
      valid = 1;
    } else if (*end == ':') {
      float minutes = strtof(end + 1, &end);
      time = fmodf(time + 14.0f, 24.0f);
      
      if (!(*end)) {
        time = ((time * 60.0f) + fmodf(minutes, 60.0f)) / 1440.0f;
        valid = 1;
      }
    }
    
    if (valid) {
      char buffer[100];
      
      vx_time = time;
      vx_server_time(NULL);
      
      sprintf(buffer, "client '%s' set the time to %02d:%02d", client->name,
        ((int)(vx_time * 24.0f) + 10) % 24, (int)(vx_time * 24.0f * 60.0f) % 60);
      
      vx_server_post(buffer);
      return;
    }
  }
  
  char buffer[strlen(old_command) + 100];
  sprintf(buffer, "unknown command: '%s'", old_command);
  
  vx_server_send(client, buffer);
}
