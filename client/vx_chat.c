#include <client.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char **vx_messages = NULL;
int vx_message_count = 0;

void vx_chat_add(const char *name, const char *data) {
  char *buffer;
  
  if (name) {
    if (data[0] == '/') return;
    
    buffer = malloc(strlen(name) + strlen(data) + 4);
    sprintf(buffer, "[%s] %s", name, data);
  } else {
    buffer = malloc(strlen(data) + 1);
    sprintf(buffer, "%s", data);
  }
  
  vx_messages = realloc(vx_messages, (vx_message_count + 1) * sizeof(char *));
  vx_messages[vx_message_count++] = buffer;
}
