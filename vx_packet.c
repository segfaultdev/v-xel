#include <protocol.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

size_t vx_packet_size(uint16_t type) {
  if (type == vx_packet_request) {
    return (sizeof(uint16_t) + 2 * sizeof(uint32_t));
  } else if (type == vx_packet_chunk) {
    return (sizeof(uint16_t) + sizeof(vx_chunk_t));
  } else if (type == vx_packet_place) {
    return (sizeof(uint16_t) + 3 * sizeof(uint32_t) + sizeof(uint8_t));
  } else if (type == vx_packet_welcome || type == vx_packet_bye) {
    return (sizeof(uint16_t) + 20 * sizeof(char));
  } else if (type == vx_packet_update) {
    return (sizeof(uint16_t) + 20 * sizeof(char) + 3 * sizeof(float));
  } else if (type == vx_packet_chat) {
    return (sizeof(uint16_t));
  } else if (type == vx_packet_chunk_rle) {
    return (sizeof(uint16_t) + 2 * sizeof(uint32_t));
  }
  
  return 0;
}

void vx_packet_encode(void *input, size_t input_size, void *output, size_t *output_size, int is_output_file) {
  *output_size = 0;
  
  uint32_t total = 0;
  
  uint32_t count = 0;
  uint8_t last = 0;
  
  for (uint32_t i = 0; i < input_size; i++) {
    uint8_t curr = ((uint8_t *)(input))[i];
    if (!i) last = curr;
    
    if (curr != last) {
      total += count;
      count <<= 1; // bottom bit is 1 when raw encoding, 0 when run-length encoding
      
      while (count) {
        uint8_t byte = count & 0x7F;
        count >>= 7;
        
        if (count) byte |= 0x80;
        
        if (is_output_file) {
          fwrite(&byte, 1, 1, (FILE *)(output));
        } else if (output) {
          ((uint8_t *)(output))[(*output_size)++] = byte;
        } else {
          (*output_size)++;
        }
      }
      
      if (is_output_file) {
        fwrite(&last, 1, 1, (FILE *)(output));
      } else if (output) {
        ((uint8_t *)(output))[(*output_size)++] = last;
      } else {
        (*output_size)++;
      }
    }
    
    last = curr;
    count++;
  }
  
  if (count) {
    total += count;
    count <<= 1; // bottom bit is 1 when raw encoding, 0 when run-length encoding
    
    while (count) {
      uint8_t byte = count & 0x7F;
      count >>= 7;
      
      if (count) byte |= 0x80;
      
      if (is_output_file) {
        fwrite(&byte, 1, 1, (FILE *)(output));
      } else if (output) {
        ((uint8_t *)(output))[(*output_size)++] = byte;
      } else {
        (*output_size)++;
      }
    }
    
    if (is_output_file) {
      fwrite(&last, 1, 1, (FILE *)(output));
    } else if (output) {
      ((uint8_t *)(output))[(*output_size)++] = last;
    } else {
      (*output_size)++;
    }
  }
}

void vx_packet_decode(void *input, size_t input_size, void *output, size_t *output_size, int is_input_file) {
  *output_size = 0;
    
  uint32_t input_offset = 0;
  
  for (;;) {
    if (is_input_file) {
      size_t current = ftell((FILE *)(input));
      fseek((FILE *)(input), 0, SEEK_END);
      
      if (ftell((FILE *)(input)) == current) break;
      fseek((FILE *)(input), current, SEEK_SET);
    } else {
      if (input_offset >= input_size) break;
    }
    
    uint32_t count = 0;
    int shift = 0;
    
    for (;;) {
      uint8_t byte;
      
      if (is_input_file) {
        fread(&byte, 1, 1, (FILE *)(input));
      } else {
        byte = ((uint8_t *)(input))[input_offset++];
      }
      
      count |= ((uint32_t)(byte & 0x7F) << (shift * 7));
      shift++;
      
      if (!(byte & 0x80)) break;
    }
    
    if (count % 2) {
      // TODO
    } else {
      uint8_t byte;
      
      if (is_input_file) {
        fread(&byte, 1, 1, (FILE *)(input));
      } else {
        byte = ((uint8_t *)(input))[input_offset++];
      }
      
      memset(output + *output_size, byte, count >> 1);
    }
    
    *output_size += (count >> 1);
  }
}
