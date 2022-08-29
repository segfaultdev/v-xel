#version 430

precision mediump float;

const int vx_tile_air = 0;
const int vx_tile_water = 1;

const int vx_tile_dirt = 2;
const int vx_tile_grass = 3;
const int vx_tile_stone = 4;
const int vx_tile_sand = 5;

const int vx_tile_trunk = 6;
const int vx_tile_leaves = 7;
const int vx_tile_wood = 8;
const int vx_tile_light_wood = 9;
const int vx_tile_dark_wood = 10;

const int vx_tile_red_bricks = 11;
const int vx_tile_gray_bricks = 12;
const int vx_tile_glass = 13;

const int vx_tile_black_block = 14;
const int vx_tile_white_block = 15;
const int vx_tile_red_block = 16;
const int vx_tile_orange_block = 17;
const int vx_tile_yellow_block = 18;
const int vx_tile_green_block = 19;
const int vx_tile_cyan_block = 20;
const int vx_tile_blue_block = 21;
const int vx_tile_purple_block = 22;
const int vx_tile_magenta_block = 23;

const float PI = 3.14159265;
const float E = 2.71828;

const int vx_cloud_side = 512;

layout(std430, binding=2) buffer client_data {
  float client_array[];
};

layout(std430, binding=3) buffer chunk_data { 
  uint chunk_array[];
};

uniform vec2 screen_size;

uniform vec3 camera;

uniform float angle_ud;
uniform float angle_lr;

uniform float time;

uniform ivec3 mouse_pos;
uniform ivec3 mouse_step;
uniform int mouse_side;

uniform int selected;

uniform int vx_iterations;
uniform int vx_total_side;
uniform int vx_max_clients;

out vec4 finalColor;

float square(float x) {
  return x * x;
}

float _atan2(float x, float y) {
  if (x > 0) {
    return atan(y / x);
  } else if (x < 0) {
    if (y >= 0) return atan(y / x) + PI;
    else return atan(y / x) - PI;
  } else {
    if (y > 0) return PI / 2.0;
    else if (y < 0) return -PI / 2.0;
    else return 0;
  }
}

float atan2(float x, float y) {
  return mod(_atan2(x, y) + 2.0 * PI, 2.0 * PI);
}

float signed_angle_dist(float x, float y) {
  while (x < 0.0) x += 2.0 * PI;
  x = mod(x, 2.0 * PI);
  
  while (y < 0.0) y += 2.0 * PI;
  y = mod(y, 2.0 * PI);
  
  if (x >= PI && y < PI && x - y >= PI) return (x - (2.0 * PI)) - y;
  else if (x < PI && y >= PI && y - x >= PI) return (x + (2.0 * PI)) - y;
  else return x - y;
}

vec2 rotate(vec2 vector, float angle) {
  return vec2(vector.x * cos(angle) - vector.y * sin(angle), vector.x * sin(angle) + vector.y * cos(angle));
}

float lerp(float x, float y, float w) {
  if (w < 0) return x;
  if (w > 1) return y;
  
  return (y - x) * ((w * (w * 6.0 - 15.0) + 10.0) * w * w * w) + x;
}

bool in_cloud(float x, float z) {
  float j = floor(x / 8.0) / 2.0;
  float i = floor(z / 8.0) / 2.0;
  
  float value = 1.3 * (1 + sin(1.2 * j + 0.7 * i)) * (1 + sin(1.5 * j - 0.4 * i)) + 0.7 * (1 + sin(2.5 * i - 0.4 * j)) * (1 + sin(1.1 * i - 0.7 * j));
  return (value * 0.25 < 0.15);
}

bool in_leaves(float x, float z) {
  float j = floor(x) * 2.0;
  float i = floor(z) * 2.0;
  
  float value = 1.3 * (1 + sin(1.2 * j + 0.7 * i)) * (1 + sin(1.5 * j - 0.4 * i)) + 0.7 * (1 + sin(2.5 * i - 0.4 * j)) * (1 + sin(1.1 * i - 0.7 * j));
  return (value * 0.25 < 0.3);
}

bool in_star(float x, float y) {
  uvec2 mod = uvec2(floor(x * 0.8 + 1048576.0), floor(y * 0.8 + 1234567.0));
  
  uint mod_xy = mod.x ^ (mod.x << 4);
  mod_xy ^= mod_xy >> 3;
  mod_xy ^= mod.y << 1;
  mod_xy ^= mod_xy << 13;
  mod_xy ^= mod_xy >> 5;
  mod_xy ^= mod.y << 6;
  mod_xy ^= mod_xy >> 8;
  
  return (mod_xy >> 1) % 197 == 0;
}

uint get_tile(uint x, uint y, uint z) {
  if (y >= 128) return vx_tile_air;
  
  uint chunk_x = (x / 32) % vx_total_side;
  uint chunk_z = (z / 32) % vx_total_side;
  
  uint tile_x = x % 32;
  uint tile_z = z % 32;
  
  uint data_start = (chunk_x + chunk_z * vx_total_side) * 32 * 32 * 128;
  uint index = data_start + (tile_x + (tile_z + y * 32) * 32);
  
  return (chunk_array[index / 4] >> (8 * (index % 4))) & 255;
}

vec3 get_color(uint tile) {
  ivec3 color = ivec3(0, 0, 0);
  
  if (tile == vx_tile_grass) {
    color = ivec3(63, 223, 63);
  } else if (tile == vx_tile_dirt) {
    color = ivec3(159, 79, 15);
  } else if (tile == vx_tile_water) {
    color = ivec3(63, 95, 255);
  } else if (tile == vx_tile_stone) {
    color = ivec3(159, 159, 159);
  } else if (tile == vx_tile_sand) {
    color = ivec3(255, 255, 127);
  } else if (tile == vx_tile_trunk) {
    color = ivec3(79, 39, 7);
  } else if (tile == vx_tile_leaves) {
    color = ivec3(31, 111, 31);
  } else if (tile == vx_tile_wood) {
    color = ivec3(223, 159, 79);
  } else if (tile == vx_tile_light_wood) {
    color = ivec3(239, 207, 127);
  } else if (tile == vx_tile_dark_wood) {
    color = ivec3(159, 95, 39);
  } else if (tile == vx_tile_red_bricks) {
    color = ivec3(255, 95, 63);
  } else if (tile == vx_tile_gray_bricks) {
    color = ivec3(175, 175, 175);
  } else if (tile == vx_tile_glass) {
    color = ivec3(255, 255, 255);
  } else if (tile == vx_tile_black_block) {
    color = ivec3(23, 23, 23);
  } else if (tile == vx_tile_white_block) {
    color = ivec3(223, 223, 223);
  } else if (tile == vx_tile_red_block) {
    color = ivec3(223, 23, 23);
  } else if (tile == vx_tile_orange_block) {
    color = ivec3(223, 123, 23);
  } else if (tile == vx_tile_yellow_block) {
    color = ivec3(223, 223, 23);
  } else if (tile == vx_tile_green_block) {
    color = ivec3(123, 223, 23);
  } else if (tile == vx_tile_cyan_block) {
    color = ivec3(23, 223, 223);
  } else if (tile == vx_tile_blue_block) {
    color = ivec3(23, 23, 223);
  } else if (tile == vx_tile_purple_block) {
    color = ivec3(123, 23, 223);
  } else if (tile == vx_tile_magenta_block) {
    color = ivec3(223, 23, 223);
  }
  
  return vec3(color.x / 255.0, color.y / 255.0, color.z / 255.0);
}

bool is_solid(uint x, uint y, uint z) {
  uint tile = get_tile(x, y, z);
  return (tile != vx_tile_air && tile != vx_tile_water && tile != vx_tile_glass && tile != vx_tile_leaves);
}

float plot_shadow(vec3 start, vec3 ray) {
  uvec3 position = uvec3(floor(start.x), floor(start.y), floor(start.z));
  
  vec3 side = vec3(0.0, 0.0, 0.0);
  vec3 delta = abs(vec3(1.0, 1.0, 1.0) / ray);
  ivec3 step = ivec3(0, 0, 0);
  
  int hit_side = 0;
  
  if (ray.x < 0.0) {
    step.x = -1;
    side.x = (start.x - position.x) * delta.x;
  } else {
    step.x = 1;
    side.x = ((position.x + 1) - start.x) * delta.x;
  }
  
  if (ray.y < 0.0) {
    step.y = -1;
    side.y = (start.y - position.y) * delta.y;
  } else {
    step.y = 1;
    side.y = ((position.y + 1) - start.y) * delta.y;
  }
  
  if (ray.z < 0.0) {
    step.z = -1;
    side.z = (start.z - position.z) * delta.z;
  } else {
    step.z = 1;
    side.z = ((position.z + 1) - start.z) * delta.z;
  }
  
  for (int i = 0; i < vx_iterations * 0.75f; i++) {
    if (side.x <= side.y && side.x <= side.z) {
      side.x += delta.x;
      position.x += step.x;
      
      hit_side = 0;
    } else if (side.y <= side.x && side.y <= side.z) {
      side.y += delta.y;
      position.y += step.y;
      
      hit_side = 1;
    } else {
      side.z += delta.z;
      position.z += step.z;
      
      hit_side = 2;
    }
    
    if (position.y < 0 || position.y >= 128) break;
    if (hit_side == 1) i--;
    
    float dist;
    
    if (hit_side == 0) {
      dist = side.x - delta.x;
    } else if (hit_side == 1) {
      dist = side.y - delta.y;
    } else if (hit_side == 2) {
      dist = side.z - delta.z;
    }
    
    uint tile = get_tile(position.x, position.y, position.z);
    vec3 hit = start + (ray * dist);
    
    if (tile == vx_tile_leaves) {
      if (hit_side == 0) {
        if (in_leaves((hit.y - floor(hit.y)) * 16.0, (hit.z - floor(hit.z)) * 16.0)) continue;
      } else if (hit_side == 1) {
        if (in_leaves((hit.z - floor(hit.z)) * 16.0, (hit.x - floor(hit.x)) * 16.0)) continue;
      } else if (hit_side == 2) {
        if (in_leaves((hit.x - floor(hit.x)) * 16.0, (hit.y - floor(hit.y)) * 16.0)) continue;
      }
      
      return 0.5;
    } else if (tile != vx_tile_air && tile != vx_tile_glass) {
      return 0.5;
    }
  }
  
  float ceil_w = (240.0 - start.y) / ray.y;
  
  float ceil_x = mod((start.x + ray.x * ceil_w) + time * vx_cloud_side, vx_cloud_side);
  while (ceil_x < 0.0) ceil_x += vx_cloud_side;
  
  float ceil_z = mod((start.z + ray.z * ceil_w) - time * vx_cloud_side, vx_cloud_side);
  while (ceil_z < 0.0) ceil_z += vx_cloud_side;
  
  if (ray.y >= 0.000001 && in_cloud(ceil_x, ceil_z)) {
    return 0.75;
  }
  
  return 1.0;
}

vec4 plot_pixel(vec3 start, vec3 ray) {
  uvec3 position = uvec3(floor(start.x), floor(start.y), floor(start.z));
  
  vec3 side = vec3(0.0, 0.0, 0.0);
  vec3 delta = abs(vec3(1.0, 1.0, 1.0) / ray);
  ivec3 step = ivec3(0, 0, 0);
  
  vec3 final_color = vec3(0.0, 0.0, 0.0);
  float color_left = 1.0;
  
  int hit_side = 0;
  float depth = 1048576.0;
  
  if (ray.x < 0.0) {
    step.x = -1;
    side.x = (start.x - position.x) * delta.x;
  } else {
    step.x = 1;
    side.x = ((position.x + 1) - start.x) * delta.x;
  }
  
  if (ray.y < 0.0) {
    step.y = -1;
    side.y = (start.y - position.y) * delta.y;
  } else {
    step.y = 1;
    side.y = ((position.y + 1) - start.y) * delta.y;
  }
  
  if (ray.z < 0.0) {
    step.z = -1;
    side.z = (start.z - position.z) * delta.z;
  } else {
    step.z = 1;
    side.z = ((position.z + 1) - start.z) * delta.z;
  }
  
  for (int i = 0; i < vx_iterations; i++) {
    if (side.x <= side.y && side.x <= side.z) {
      side.x += delta.x;
      position.x += step.x;
      
      hit_side = 0;
    } else if (side.y <= side.x && side.y <= side.z) {
      side.y += delta.y;
      position.y += step.y;
      
      hit_side = 1;
    } else {
      side.z += delta.z;
      position.z += step.z;
      
      hit_side = 2;
    }
    
    if (position.y < 0 || position.y >= 128) break;
    if (hit_side == 1) i--;
    
    float dist;
    
    if (hit_side == 0) {
      dist = side.x - delta.x;
    } else if (hit_side == 1) {
      dist = side.y - delta.y;
    } else if (hit_side == 2) {
      dist = side.z - delta.z;
    }
    
    uint tile = get_tile(position.x, position.y, position.z);
    
    if (tile != vx_tile_air) {
      if (depth >= 1048576.0) depth = dist;
      vec3 hit = start + (ray * dist);
      
      float border_dist = 1.0;
      float ambient_dist = 1.0;
      
      if (hit_side == 0) {
        if (get_tile(position.x, position.y - 1, position.z) != tile) {
          border_dist = min(border_dist, hit.y - floor(hit.y));
        }
        
        if (get_tile(position.x, position.y + 1, position.z) != tile) {
          border_dist = min(border_dist, 1.0 - (hit.y - floor(hit.y)));
        }
        
        if (get_tile(position.x, position.y, position.z - 1) != tile) {
          border_dist = min(border_dist, hit.z - floor(hit.z));
        }
        
        if (get_tile(position.x, position.y, position.z + 1) != tile) {
          border_dist = min(border_dist, 1.0 - (hit.z - floor(hit.z)));
        }
        
        if (get_tile(position.x - step.x, position.y, position.z) == tile) {
          border_dist = 1.0;
        }
      } else if (hit_side == 1) {
        if (get_tile(position.x - 1, position.y, position.z) != tile) {
          border_dist = min(border_dist, hit.x - floor(hit.x));
        }
        
        if (get_tile(position.x + 1, position.y, position.z) != tile) {
          border_dist = min(border_dist, 1.0 - (hit.x - floor(hit.x)));
        }
        
        if (get_tile(position.x, position.y, position.z - 1) != tile) {
          border_dist = min(border_dist, hit.z - floor(hit.z));
        }
        
        if (get_tile(position.x, position.y, position.z + 1) != tile) {
          border_dist = min(border_dist, 1.0 - (hit.z - floor(hit.z)));
        }
        
        if (get_tile(position.x, position.y - step.y, position.z) == tile) {
          border_dist = 1.0;
        }
      } else if (hit_side == 2) {
        if (get_tile(position.x - 1, position.y, position.z) != tile) {
          border_dist = min(border_dist, hit.x - floor(hit.x));
        }
        
        if (get_tile(position.x + 1, position.y, position.z) != tile) {
          border_dist = min(border_dist, 1.0 - (hit.x - floor(hit.x)));
        }
        
        if (get_tile(position.x, position.y - 1, position.z) != tile) {
          border_dist = min(border_dist, hit.y - floor(hit.y));
        }
        
        if (get_tile(position.x, position.y + 1, position.z) != tile) {
          border_dist = min(border_dist, 1.0 - (hit.y - floor(hit.y)));
        }
        
        if (get_tile(position.x, position.y, position.z - step.z) == tile) {
          border_dist = 1.0;
        }
      }
      
      if (hit_side == 0) {
        if (is_solid(position.x - step.x, position.y - 1, position.z)) {
          border_dist = min(border_dist, hit.y - floor(hit.y));
        }
        
        if (is_solid(position.x - step.x, position.y + 1, position.z)) {
          border_dist = min(border_dist, 1.0 - (hit.y - floor(hit.y)));
        }
        
        if (is_solid(position.x - step.x, position.y, position.z - 1)) {
          border_dist = min(border_dist, hit.z - floor(hit.z));
        }
        
        if (is_solid(position.x - step.x, position.y, position.z + 1)) {
          border_dist = min(border_dist, 1.0 - (hit.z - floor(hit.z)));
        }
      } else if (hit_side == 1) {
        if (is_solid(position.x - 1, position.y - step.y, position.z)) {
          border_dist = min(border_dist, hit.x - floor(hit.x));
        }
        
        if (is_solid(position.x + 1, position.y - step.y, position.z)) {
          border_dist = min(border_dist, 1.0 - (hit.x - floor(hit.x)));
        }
        
        if (is_solid(position.x, position.y - step.y, position.z - 1)) {
          border_dist = min(border_dist, hit.z - floor(hit.z));
        }
        
        if (is_solid(position.x, position.y - step.y, position.z + 1)) {
          border_dist = min(border_dist, 1.0 - (hit.z - floor(hit.z)));
        }
      } else if (hit_side == 2) {
        if (is_solid(position.x - 1, position.y, position.z - step.z)) {
          border_dist = min(border_dist, hit.x - floor(hit.x));
        }
        
        if (is_solid(position.x + 1, position.y, position.z - step.z)) {
          border_dist = min(border_dist, 1.0 - (hit.x - floor(hit.x)));
        }
        
        if (is_solid(position.x, position.y - 1, position.z - step.z)) {
          border_dist = min(border_dist, hit.y - floor(hit.y));
        }
        
        if (is_solid(position.x, position.y + 1, position.z - step.z)) {
          border_dist = min(border_dist, 1.0 - (hit.y - floor(hit.y)));
        }
      }
      
      float border_size = 2.0;
      if (tile == vx_tile_glass) border_size = 5.0;
      
      if (border_dist < max((border_size / screen_size.x) * dist, border_size / 100.0)) {
        border_dist = 0.75;
      } else {
        border_dist = 1.0;
      }
      
      if (hit_side == 0) {
        if (is_solid(position.x - step.x, position.y - 1, position.z)) {
          ambient_dist = min(ambient_dist, hit.y - floor(hit.y));
        }
        
        if (is_solid(position.x - step.x, position.y + 1, position.z)) {
          ambient_dist = min(ambient_dist, 1.0 - (hit.y - floor(hit.y)));
        }
        
        if (is_solid(position.x - step.x, position.y, position.z - 1)) {
          ambient_dist = min(ambient_dist, hit.z - floor(hit.z));
        }
        
        if (is_solid(position.x - step.x, position.y, position.z + 1)) {
          ambient_dist = min(ambient_dist, 1.0 - (hit.z - floor(hit.z)));
        }
        
        if (is_solid(position.x - step.x, position.y - 1, position.z - 1)) {
          ambient_dist = min(ambient_dist, sqrt(square(hit.y - floor(hit.y)) + square(hit.z - floor(hit.z))));
        }
        
        if (is_solid(position.x - step.x, position.y + 1, position.z - 1)) {
          ambient_dist = min(ambient_dist, sqrt(square(1.0 - (hit.y - floor(hit.y))) + square(hit.z - floor(hit.z))));
        }
        
        if (is_solid(position.x - step.x, position.y - 1, position.z + 1)) {
          ambient_dist = min(ambient_dist, sqrt(square(hit.y - floor(hit.y)) + square(1.0 - (hit.z - floor(hit.z)))));
        }
        
        if (is_solid(position.x - step.x, position.y + 1, position.z + 1)) {
          ambient_dist = min(ambient_dist, sqrt(square(1.0 - (hit.y - floor(hit.y))) + square(1.0 - (hit.z - floor(hit.z)))));
        }
      } else if (hit_side == 1) {
        if (is_solid(position.x - 1, position.y - step.y, position.z)) {
          ambient_dist = min(ambient_dist, hit.x - floor(hit.x));
        }
        
        if (is_solid(position.x + 1, position.y - step.y, position.z)) {
          ambient_dist = min(ambient_dist, 1.0 - (hit.x - floor(hit.x)));
        }
        
        if (is_solid(position.x, position.y - step.y, position.z - 1)) {
          ambient_dist = min(ambient_dist, hit.z - floor(hit.z));
        }
        
        if (is_solid(position.x, position.y - step.y, position.z + 1)) {
          ambient_dist = min(ambient_dist, 1.0 - (hit.z - floor(hit.z)));
        }
        
        if (is_solid(position.x - 1, position.y - step.y, position.z - 1)) {
          ambient_dist = min(ambient_dist, sqrt(square(hit.x - floor(hit.x)) + square(hit.z - floor(hit.z))));
        }
        
        if (is_solid(position.x + 1, position.y - step.y, position.z - 1)) {
          ambient_dist = min(ambient_dist, sqrt(square(1.0 - (hit.x - floor(hit.x))) + square(hit.z - floor(hit.z))));
        }
        
        if (is_solid(position.x - 1, position.y - step.y, position.z + 1)) {
          ambient_dist = min(ambient_dist, sqrt(square(hit.x - floor(hit.x)) + square(1.0 - (hit.z - floor(hit.z)))));
        }
        
        if (is_solid(position.x + 1, position.y - step.y, position.z + 1)) {
          ambient_dist = min(ambient_dist, sqrt(square(1.0 - (hit.x - floor(hit.x))) + square(1.0 - (hit.z - floor(hit.z)))));
        }
      } else if (hit_side == 2) {
        if (is_solid(position.x - 1, position.y, position.z - step.z)) {
          ambient_dist = min(ambient_dist, hit.x - floor(hit.x));
        }
        
        if (is_solid(position.x + 1, position.y, position.z - step.z)) {
          ambient_dist = min(ambient_dist, 1.0 - (hit.x - floor(hit.x)));
        }
        
        if (is_solid(position.x, position.y - 1, position.z - step.z)) {
          ambient_dist = min(ambient_dist, hit.y - floor(hit.y));
        }
        
        if (is_solid(position.x, position.y + 1, position.z - step.z)) {
          ambient_dist = min(ambient_dist, 1.0 - (hit.y - floor(hit.y)));
        }
        
        if (is_solid(position.x - 1, position.y - 1, position.z - step.z)) {
          ambient_dist = min(ambient_dist, sqrt(square(hit.x - floor(hit.x)) + square(hit.y - floor(hit.y))));
        }
        
        if (is_solid(position.x + 1, position.y - 1, position.z - step.z)) {
          ambient_dist = min(ambient_dist, sqrt(square(1.0 - (hit.x - floor(hit.x))) + square(hit.y - floor(hit.y))));
        }
        
        if (is_solid(position.x - 1, position.y + 1, position.z - step.z)) {
          ambient_dist = min(ambient_dist, sqrt(square(hit.x - floor(hit.x)) + square(1.0 - (hit.y - floor(hit.y)))));
        }
        
        if (is_solid(position.x + 1, position.y + 1, position.z - step.z)) {
          ambient_dist = min(ambient_dist, sqrt(square(1.0 - (hit.x - floor(hit.x))) + square(1.0 - (hit.y - floor(hit.y)))));
        }
      }
      
      if (ambient_dist < 0.25) {
        ambient_dist = (sqrt(ambient_dist / 0.25)) * 0.67 + 0.33;
      } else {
        ambient_dist = 1.0;
      }
      
      vec3 color = get_color(tile);
      vec3 pre_hit = hit;
      
      pre_hit -= ray * 0.001;
      
      vec3 hit_mod = hit - floor(hit);
      
      if (tile == vx_tile_trunk) {
        if (hit_side == 1 && hit_mod.x >= 0.05 && hit_mod.x <= 0.95 && hit_mod.z >= 0.05 && hit_mod.z < 0.95) {
          color = get_color(vx_tile_wood);
        }
      } else if (tile == vx_tile_wood || tile == vx_tile_light_wood || tile == vx_tile_dark_wood) {
        vec2 hit_mod_2;
        
        if (hit_side == 0) hit_mod_2 = hit_mod.zy;
        if (hit_side == 1) hit_mod_2 = hit_mod.zx;
        if (hit_side == 2) hit_mod_2 = hit_mod.xy;
        
        hit_mod_2.y *= 4.0;
        hit_mod_2.x = mod(hit_mod_2.x + 0.5 * floor(hit_mod_2.y), 1.0);
        
        if (mod(hit_mod_2.y, 1.0) >= 0.8 || hit_mod_2.x <= 0.025 || hit_mod_2.x >= 0.975) {
          color *= 0.75;
        }
      } else if (tile == vx_tile_red_bricks || tile == vx_tile_gray_bricks) {
        vec2 hit_mod_2;
        
        if (hit_side == 0) hit_mod_2 = hit_mod.zy;
        if (hit_side == 1) hit_mod_2 = hit_mod.zx;
        if (hit_side == 2) hit_mod_2 = hit_mod.xy;
        
        hit_mod_2.y *= 4.0;
        hit_mod_2.x = mod(hit_mod_2.x * 2.0 + 0.5 * floor(hit_mod_2.y), 1.0);
        
        if (mod(hit_mod_2.y, 1.0) >= 0.8 || hit_mod_2.x <= 0.05 || hit_mod_2.x >= 0.95) {
          color = vec3(0.9, 0.9, 0.9);
        }
      }
      
      color *= border_dist * ambient_dist;
      color *= plot_shadow(pre_hit, vec3(cos(2 * PI * time), sin(2 * PI * time), 0.0));
      
      if (position == mouse_pos && hit_side == mouse_side) {
        color_left /= 2.0;
        final_color += vec3(1.0, 1.0, 1.0) * color_left;
      }
      
      if (tile == vx_tile_water) {
        vec3 new_start = pre_hit;
        
        float seconds = time * 1440.0;
        
        vec3 new_ray = vec3(
          ray.x * 0.97 + sin((hit.x + hit.y + hit.z - 0.21 * seconds) * 13.0 * 0.1) * 0.03,
          ray.y * 0.97 + sin((hit.x + hit.y - hit.z - 0.19 * seconds) * 15.0 * 0.1) * 0.03,
          ray.z * 0.97 + sin((hit.x - hit.y + hit.z + 0.17 * seconds) * 17.0 * 0.1) * 0.03
        );
        
        new_ray /= sqrt(square(new_ray.x) + square(new_ray.y) + square(new_ray.z));
        
        start = new_start;
        ray = vec3(hit_side == 0 ? -new_ray.x : new_ray.x, hit_side == 1 ? -new_ray.y : new_ray.y, hit_side == 2 ? -new_ray.z : new_ray.z);
        
        position = uvec3(floor(start.x), floor(start.y), floor(start.z));
        
        side = vec3(0.0, 0.0, 0.0);
        delta = abs(vec3(1.0, 1.0, 1.0) / ray);
        step = ivec3(0, 0, 0);
        
        if (ray.x < 0.0) {
          step.x = -1;
          side.x = (start.x - position.x) * delta.x;
        } else {
          step.x = 1;
          side.x = ((position.x + 1) - start.x) * delta.x;
        }
        
        if (ray.y < 0.0) {
          step.y = -1;
          side.y = (start.y - position.y) * delta.y;
        } else {
          step.y = 1;
          side.y = ((position.y + 1) - start.y) * delta.y;
        }
        
        if (ray.z < 0.0) {
          step.z = -1;
          side.z = (start.z - position.z) * delta.z;
        } else {
          step.z = 1;
          side.z = ((position.z + 1) - start.z) * delta.z;
        }
        
        color_left /= 2.0;
        final_color += color * color_left;
        
        i += vx_iterations / 5;
      } else if (tile == vx_tile_leaves) {
        if (hit_side == 0) {
          if (in_leaves((hit.y - floor(hit.y)) * 16.0, (hit.z - floor(hit.z)) * 16.0)) continue;
        } else if (hit_side == 1) {
          if (in_leaves((hit.z - floor(hit.z)) * 16.0, (hit.x - floor(hit.x)) * 16.0)) continue;
        } else if (hit_side == 2) {
          if (in_leaves((hit.x - floor(hit.x)) * 16.0, (hit.y - floor(hit.y)) * 16.0)) continue;
        }
        
        final_color += color * color_left;
        color_left = 0.0;
        
        break;
      } else if (tile == vx_tile_glass) {
        if (border_dist >= 1.0) {
          float glass_left = color_left / 5.0;
          
          final_color += color * glass_left;
          color_left -= glass_left;
          
          continue;
        }
        
        final_color += color * color_left;
        color_left = 0.0;
        
        break;
      } else {
        final_color += color * color_left;
        color_left = 0.0;
        
        break;
      }
    }
  }
  
  if (color_left <= 0.0) return vec4(final_color, depth);
  float ceil_w = (240.0 - start.y) / ray.y;
  
  float ceil_x = mod((start.x + ray.x * ceil_w) + time * vx_cloud_side, vx_cloud_side);
  while (ceil_x < 0.0) ceil_x += vx_cloud_side;
  
  float ceil_z = mod((start.z + ray.z * ceil_w) - time * vx_cloud_side, vx_cloud_side);
  while (ceil_z < 0.0) ceil_z += vx_cloud_side;
  
  float light = cos(PI * (time + 0.25)) * cos(PI * (time + 0.25));
  float k = 800.0 * light;
  
  k = k * k;
  k = k * k;
  
  float cr = k / 240100000000.0;
  float cg = k / 78904810000.0;
  float cb = k / 37480960000.0;
  
  float dist;
  
  if (hit_side == 0) {
    dist = side.x - delta.x;
  } else if (hit_side == 1) {
    dist = side.y - delta.y;
  } else if (hit_side == 2) {
    dist = side.z - delta.z;
  }
  
  float valid_x = ray.x;
  
  if (time >= 0.25 && time < 0.75) {
    valid_x *= -1.0;
  }
  
  float angle = atan(abs(ray.y) / abs(valid_x));
  
  if (valid_x < 0.0) {
    angle = PI - angle;
  }
  
  if (ray.y < 0.0) {
    angle *= -1.0;
  }
  
  if (angle < 0.0) {
    angle /= PI;
    angle = pow(angle, 3.0);
    angle *= PI;
  }
  
  ceil_w = max(abs(angle) * 32.0, 0.0);
  
  float sub = 32.0 * (0.5 - light);
  if (sub < -0.5) sub = -0.5;
  
  ceil_w += sub;
  
  float tr = pow(E, -cr * ceil_w);
  float tg = pow(E, -cg * ceil_w);
  float tb = pow(E, -cb * ceil_w);
  
  float r = tr * (1.0 - cr);
  float g = tg * (1.0 - cg);
  float b = tb * (1.0 - cb);
  
  float rem = (1.0 - light) * (1.0 - 1.0 / (1.0 + ceil_w / 2.0));
  
  r -= 0.50 * rem;
  g -= 0.25 * rem;
  
  float in_sun = 0.0;
  
  float sun_angle = 2.0 * PI * time;
  angle = atan2(ray.x, ray.y);
  
  vec2 sun = vec2(ray.z, signed_angle_dist(sun_angle, angle));
  sun = rotate(sun, time * 8.0 * PI);
  
  if (abs(sun.x) + abs(sun.y) < 0.1) {
    in_sun = 1.0;
  }
  
  if (ray.y > 0.0 && in_cloud(ceil_x, ceil_z)) {
    float cloud_force = 240.0 / sqrt(
      pow(ray.x * ((240.0 - start.y) / ray.y), 2.0) +
      pow(ray.z * ((240.0 - start.y) / ray.y), 2.0)
    );
    
    cloud_force = max(min(cloud_force, 1.0), 0.0);
    
    r *= lerp(1.0, 1.1, cloud_force);
    g *= lerp(1.0, 1.1, cloud_force);
    b *= lerp(1.0, 1.1, cloud_force);
    
    float a = (r + g + b) / 3.0;
    
    r += a * 0.2 * cloud_force;
    g += a * 0.2 * cloud_force;
    b += a * 0.2 * cloud_force;
    
    in_sun *= lerp(1.0, 0.5, cloud_force);
  }
  
  r = (r * (1.0 - in_sun)) + in_sun;
  g = (g * (1.0 - in_sun)) + in_sun;
  b = (b * (1.0 - in_sun)) + 0.85 * in_sun;
  
  float a = (r + g + b) / 3.0;
  
  if (ray.y > 0 && a < 0.01) {
    float star_force = 64.0 / sqrt(
      pow(ray.x * ((240.0 - start.y) / ray.y), 2.0) +
      pow(ray.z * ((240.0 - start.y) / ray.y), 2.0)
    );
    
    star_force *= 1.0 - (a / 0.01);
    
    if (in_star(ceil_x, ceil_z)) {
      r = (r * (1.0 - star_force)) + star_force;
      g = (g * (1.0 - star_force)) + star_force;
      b = (b * (1.0 - star_force)) + star_force;
    }
  }
  
  if (r < 0.0) r = 0.0;
  if (g < 0.0) g = 0.0;
  if (b < 0.0) b = 0.0;
  
  if (r > 1.0) r = 1.0;
  if (g > 1.0) g = 1.0;
  if (b > 1.0) b = 1.0;
  
  final_color += vec3(r, g, b) * color_left;
  return vec4(final_color, depth);
}

void main() {
  vec4 overlay_color = vec4(0.0, 0.0, 0.0, 0.0);
  
  if (gl_FragCoord.x >= screen_size.x - 24 && gl_FragCoord.x < screen_size.x - 4) {
    if (gl_FragCoord.y >= 4 && gl_FragCoord.y < 24) {
      vec2 raw_coords = vec2(gl_FragCoord.x - (screen_size.x - 24), gl_FragCoord.y - 4);
      
      vec2 hit = raw_coords / 20.0;
      vec2 hit_mod = hit;
      
      overlay_color = vec4(get_color(selected), 1.0);
      
      if (selected == vx_tile_wood || selected == vx_tile_light_wood || selected == vx_tile_dark_wood) {
        vec2 hit_mod_2 = hit_mod.xy;
        
        hit_mod_2.y *= 4.0;
        hit_mod_2.x = mod(hit_mod_2.x + 0.5 * floor(hit_mod_2.y), 1.0);
        
        if (mod(hit_mod_2.y, 1.0) >= 0.8 || hit_mod_2.x <= 0.025 || hit_mod_2.x >= 0.975) {
          overlay_color.xyz *= 0.75;
        }
      } else if (selected == vx_tile_red_bricks || selected == vx_tile_gray_bricks) {
        vec2 hit_mod_2 = hit_mod.xy;
        
        hit_mod_2.y *= 4.0;
        hit_mod_2.x = mod(hit_mod_2.x * 2.0 + 0.5 * floor(hit_mod_2.y), 1.0);
        
        if (mod(hit_mod_2.y, 1.0) >= 0.8 || hit_mod_2.x <= 0.05 || hit_mod_2.x >= 0.95) {
          overlay_color.xyz = vec3(0.9, 0.9, 0.9);
        }
      }
      
      float border_dist = min(min(hit.x, 1.0 - hit.x), min(hit.y, 1.0 - hit.y));
      
      if (border_dist < 0.04) {
        overlay_color.xyz *= 0.75;
      } else if (selected == vx_tile_glass) {
        overlay_color.w *= 0.2;
      }
      
      if (selected == vx_tile_leaves && in_leaves(floor(hit.x * 16.0), floor(hit.y * 16.0))) {
        overlay_color.w = 0.0;
      }
    }
  }
  
  vec2 coords = (gl_FragCoord.xy / screen_size.xy) * 2.0 - vec2(1.0, 1.0);
  float ratio_y = screen_size.y / screen_size.x;
  
  vec3 ray = vec3(coords.x, coords.y * ratio_y, 1.0);
  
  ray.zy = rotate(ray.zy, -angle_ud);
  ray.xz = rotate(ray.xz, -angle_lr);
  
  // float data = plot_shadow(camera + vec3(0.0, 0.5, 0.0), ray);
  // vec4 plot_data = vec4(data, data, data, 1.0);
  
  vec4 plot_data = plot_pixel(camera + vec3(0.0, 0.5, 0.0), ray);
  
  vec3 color = plot_data.xyz;
  float depth = plot_data.w;
  
  for (int i = 0; i < vx_max_clients; i++) {
    if (client_array[0 + 3 * i] < 0.0) continue;
    vec3 relative = vec3(client_array[0 + 3 * i], client_array[1 + 3 * i], client_array[2 + 3 * i]) - (camera + vec3(0.0, 1.0, 0.0));
    
    relative.xz = rotate(relative.xz, angle_lr);
    relative.zy = rotate(relative.zy, angle_ud);
    
    if (relative.z >= depth) continue;
    if (relative.z < 0.0) continue;
    
    relative.xy /= relative.z;
    
    vec2 screen = vec2((relative.x + 1.0) * (screen_size.x / 2.0), ((relative.y / ratio_y) + 1.0) * (screen_size.y / 2.0));
    float scale = (screen_size.x / 4.0) / relative.z;
    
    vec2 start = screen - vec2(scale, scale);
    vec2 end = screen + vec2(scale, scale * 3.0);
    
    if (gl_FragCoord.x >= start.x && gl_FragCoord.x < end.x && gl_FragCoord.y >= start.y && gl_FragCoord.y < end.y) {
      color = vec3(1.0, 0.0, 1.0);
      break;
    }
  }
  
  if (floor(gl_FragCoord.x) == floor(screen_size.x / 2.0) && abs(floor(gl_FragCoord.y) - floor(screen_size.y / 2.0)) < 6) {
    color = vec3(1.0, 1.0, 1.0) - color;
  } else if (floor(gl_FragCoord.y) == floor(screen_size.y / 2.0) && abs(floor(gl_FragCoord.x) - floor(screen_size.x / 2.0)) < 6) {
    color = vec3(1.0, 1.0, 1.0) - color;
  }
  
  if (gl_FragCoord.x >= screen_size.x - 28) {
    if (gl_FragCoord.y < 28) {
      color *= 0.5;
    }
  }
  
  finalColor = vec4(overlay_color.xyz * overlay_color.w + color.xyz * (1.0 - overlay_color.w), 1.0);
}
