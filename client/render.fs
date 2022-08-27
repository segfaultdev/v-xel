#version 430

precision mediump float;

#extension GL_OES_standard_derivatives : enable
#extension GL_NV_gpu_shader5 : enable

const int vx_tile_air = 0;
const int vx_tile_water = 1;

const int vx_tile_dirt = 2;
const int vx_tile_grass = 3;
const int vx_tile_stone = 4;
const int vx_tile_sand = 5;

const int vx_tile_trunk = 6;
const int vx_tile_leaves = 7;
const int vx_tile_wood = 8;

const float PI = 3.14159265;
const float E = 2.71828;

const int vx_cloud_side = 512;

layout(std430, binding=3) buffer chunk_data { 
  uint8_t chunk_array[];
};

uniform vec2 screen_size;

uniform vec3 camera;

uniform float angle_ud;
uniform float angle_lr;

uniform float time;

uniform ivec3 mouse_pos;
uniform ivec3 mouse_step;
uniform int mouse_side;

uniform int vx_iterations;
uniform int vx_total_side;

uint32_t seed_3a = 1;
uint32_t seed_3b = 1;
uint32_t seed_3c = 1;

float square(float x) {
  return x * x;
}

vec2 rotate(vec2 vector, float angle) {
  return vec2(vector.x * cos(angle) - vector.y * sin(angle), vector.x * sin(angle) + vector.y * cos(angle));
}

uint32_t rand_3() {
  seed_3a = (seed_3a * 1664525 + 1013904223) % 1431655765;
  seed_3b = (seed_3b * 16843019 + 826366249) % 1431655765;
  seed_3c = (seed_3c * 16843031 + 826366237) % 1431655765;
  
  return seed_3a + seed_3b + seed_3c;
}

float grad_2(int x, int y) {
  x %= 8;
  y %= 16;
  
  while (x < 0) x += 8;
  while (y < 0) y += 16;
  
  seed_3a = (x * 1664525 + 1013904223) % 1431655765;
  seed_3b = (y * 16843019 + 826366249) % 1431655765;
  seed_3c = ((seed_3a + seed_3b) * 16843031 + 826366237) % 1431655765;
  
  uint count = rand_3() % 2;
  
  for (uint i = 0; i < count; i++) {
    rand_3();
  }
  
  return abs(mod((float)(rand_3()), 65537.0)) / 65537.0;
}

float lerp(float x, float y, float w) {
  if (w < 0) return x;
  if (w > 1) return y;
  
  return (y - x) * ((w * (w * 6.0 - 15.0) + 10.0) * w * w * w) + x;
}

float eval_2(float x, float y) {
  x = x - (int)(x / 8) * 8;
  y = y - (int)(y / 8) * 16;
  
  while (x < 0) x += 8;
  while (y < 0) y += 16;
  
  x -= (y / 2);
  
  float dx = x - (int)(x);
  float dy = y - (int)(y);
  
  if (1 - dx < dy) {
    float tmp = dx;
    
    dx = 1 - dy;
    dy = 1 - tmp;
    
    float s = ((dx - dy) + 1) / 2;
    float h = dx + dy;
    
    float t = lerp(grad_2((int)(x + 0), (int)(y + 1)), grad_2((int)(x + 1), (int)(y + 0)), s);
    return lerp(grad_2((int)(x + 1), (int)(y + 1)), t, h);
  } else {
    float s = ((dx - dy) + 1) / 2;
    float h = dx + dy;
    
    float t = lerp(grad_2((int)(x + 0), (int)(y + 1)), grad_2((int)(x + 1), (int)(y + 0)), s);
    return lerp(grad_2((int)(x + 0), (int)(y + 0)), t, h);
  }
}

bool in_cloud(float x, float z) {
  return eval_2((int)((int)(x) / 8) / 4.0 + 11.2, (int)((int)(z) / 8) / 4.0 + 6.5) < 0.3;
}

bool in_leaves(float x, float z) {
  return eval_2((int)(x), (int)(z)) < 0.4;
}

uint get_tile(uint x, uint y, uint z) {
  if (y >= 128) return 0;
  
  uint chunk_x = (x / 32) % vx_total_side;
  uint chunk_z = (z / 32) % vx_total_side;
  
  uint tile_x = x % 32;
  uint tile_z = z % 32;
  
  uint data_start = (chunk_x + chunk_z * vx_total_side) * 32 * 32 * 128;
  return (uint)(chunk_array[data_start + (tile_x + (tile_z + y * 32) * 32)]);
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
  }
  
  return vec3((float)(color.x) / 255.0, (float)(color.y) / 255.0, (float)(color.z) / 255.0);
}

bool is_solid(uint x, uint y, uint z) {
  uint tile = get_tile(x, y, z);
  return (tile != vx_tile_air && tile != vx_tile_water);
}

float plot_shadow(vec3 start, vec3 ray) {
  uvec3 position = uvec3((uint)(start.x), (uint)(start.y), (uint)(start.z));
  
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
        if (in_leaves((hit.y - (int)(hit.y)) * 16.0, (hit.z - (int)(hit.z)) * 16.0)) continue;
      } else if (hit_side == 1) {
        if (in_leaves((hit.z - (int)(hit.z)) * 16.0, (hit.x - (int)(hit.x)) * 16.0)) continue;
      } else if (hit_side == 2) {
        if (in_leaves((hit.x - (int)(hit.x)) * 16.0, (hit.y - (int)(hit.y)) * 16.0)) continue;
      }
      
      return 0.5;
    } else if (tile != vx_tile_air) {
      return 0.5;
    }
  }
  
  float old_y = ray.y;
  
  if (ray.y < 0.000001) ray.y = 0.000001;
  float ceil_w = (160.0 - start.y) / ray.y;
  
  float ceil_x = mod((start.x + ray.x * ceil_w) + time * vx_cloud_side, vx_cloud_side);
  while (ceil_x < 0.0) ceil_x += vx_cloud_side;
  
  float ceil_z = mod((start.z + ray.z * ceil_w) - time * vx_cloud_side, vx_cloud_side);
  while (ceil_z < 0.0) ceil_z += vx_cloud_side;
  
  ceil_w = ((240.0 - start.y) / ray.y) / 100.0;
  ceil_w = 32.0 / ceil_w;
  
  if (old_y >= 0.000001 && in_cloud(ceil_x, ceil_z)) {
    return 0.75;
  }
  
  return 1.0;
}

vec3 plot_pixel(vec3 start, vec3 ray) {
  uvec3 position = uvec3((uint)(start.x), (uint)(start.y), (uint)(start.z));
  
  vec3 side = vec3(0.0, 0.0, 0.0);
  vec3 delta = abs(vec3(1.0, 1.0, 1.0) / ray);
  ivec3 step = ivec3(0, 0, 0);
  
  vec3 final_color = vec3(0.0, 0.0, 0.0);
  float color_left = 1.0;
  
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
      vec3 hit = start + (ray * dist);
      
      float border_dist = 1.0;
      float ambient_dist = 1.0;
      
      if (hit_side == 0) {
        if (get_tile(position.x, position.y - 1, position.z) != tile) {
          border_dist = min(border_dist, hit.y - (int)(hit.y));
        }
        
        if (get_tile(position.x, position.y + 1, position.z) != tile) {
          border_dist = min(border_dist, 1.0 - (hit.y - (int)(hit.y)));
        }
        
        if (get_tile(position.x, position.y, position.z - 1) != tile) {
          border_dist = min(border_dist, hit.z - (int)(hit.z));
        }
        
        if (get_tile(position.x, position.y, position.z + 1) != tile) {
          border_dist = min(border_dist, 1.0 - (hit.z - (int)(hit.z)));
        }
        
        if (get_tile(position.x - step.x, position.y, position.z) == tile) {
          border_dist = 1.0;
        }
      } else if (hit_side == 1) {
        if (get_tile(position.x - 1, position.y, position.z) != tile) {
          border_dist = min(border_dist, hit.x - (int)(hit.x));
        }
        
        if (get_tile(position.x + 1, position.y, position.z) != tile) {
          border_dist = min(border_dist, 1.0 - (hit.x - (int)(hit.x)));
        }
        
        if (get_tile(position.x, position.y, position.z - 1) != tile) {
          border_dist = min(border_dist, hit.z - (int)(hit.z));
        }
        
        if (get_tile(position.x, position.y, position.z + 1) != tile) {
          border_dist = min(border_dist, 1.0 - (hit.z - (int)(hit.z)));
        }
        
        if (get_tile(position.x, position.y - step.y, position.z) == tile) {
          border_dist = 1.0;
        }
      } else if (hit_side == 2) {
        if (get_tile(position.x - 1, position.y, position.z) != tile) {
          border_dist = min(border_dist, hit.x - (int)(hit.x));
        }
        
        if (get_tile(position.x + 1, position.y, position.z) != tile) {
          border_dist = min(border_dist, 1.0 - (hit.x - (int)(hit.x)));
        }
        
        if (get_tile(position.x, position.y - 1, position.z) != tile) {
          border_dist = min(border_dist, hit.y - (int)(hit.y));
        }
        
        if (get_tile(position.x, position.y + 1, position.z) != tile) {
          border_dist = min(border_dist, 1.0 - (hit.y - (int)(hit.y)));
        }
        
        if (get_tile(position.x, position.y, position.z - step.z) == tile) {
          border_dist = 1.0;
        }
      }
      
      if (hit_side == 0) {
        if (is_solid(position.x - step.x, position.y - 1, position.z)) {
          border_dist = min(border_dist, hit.y - (int)(hit.y));
        }
        
        if (is_solid(position.x - step.x, position.y + 1, position.z)) {
          border_dist = min(border_dist, 1.0 - (hit.y - (int)(hit.y)));
        }
        
        if (is_solid(position.x - step.x, position.y, position.z - 1)) {
          border_dist = min(border_dist, hit.z - (int)(hit.z));
        }
        
        if (is_solid(position.x - step.x, position.y, position.z + 1)) {
          border_dist = min(border_dist, 1.0 - (hit.z - (int)(hit.z)));
        }
      } else if (hit_side == 1) {
        if (is_solid(position.x - 1, position.y - step.y, position.z)) {
          border_dist = min(border_dist, hit.x - (int)(hit.x));
        }
        
        if (is_solid(position.x + 1, position.y - step.y, position.z)) {
          border_dist = min(border_dist, 1.0 - (hit.x - (int)(hit.x)));
        }
        
        if (is_solid(position.x, position.y - step.y, position.z - 1)) {
          border_dist = min(border_dist, hit.z - (int)(hit.z));
        }
        
        if (is_solid(position.x, position.y - step.y, position.z + 1)) {
          border_dist = min(border_dist, 1.0 - (hit.z - (int)(hit.z)));
        }
      } else if (hit_side == 2) {
        if (is_solid(position.x - 1, position.y, position.z - step.z)) {
          border_dist = min(border_dist, hit.x - (int)(hit.x));
        }
        
        if (is_solid(position.x + 1, position.y, position.z - step.z)) {
          border_dist = min(border_dist, 1.0 - (hit.x - (int)(hit.x)));
        }
        
        if (is_solid(position.x, position.y - 1, position.z - step.z)) {
          border_dist = min(border_dist, hit.y - (int)(hit.y));
        }
        
        if (is_solid(position.x, position.y + 1, position.z - step.z)) {
          border_dist = min(border_dist, 1.0 - (hit.y - (int)(hit.y)));
        }
      }
      
      if (border_dist < max((2.0 / screen_size.x) * dist, 0.02)) {
        border_dist = 0.75;
      } else {
        border_dist = 1.0;
      }
      
      if (hit_side == 0) {
        if (is_solid(position.x - step.x, position.y - 1, position.z)) {
          ambient_dist = min(ambient_dist, hit.y - (int)(hit.y));
        }
        
        if (is_solid(position.x - step.x, position.y + 1, position.z)) {
          ambient_dist = min(ambient_dist, 1.0 - (hit.y - (int)(hit.y)));
        }
        
        if (is_solid(position.x - step.x, position.y, position.z - 1)) {
          ambient_dist = min(ambient_dist, hit.z - (int)(hit.z));
        }
        
        if (is_solid(position.x - step.x, position.y, position.z + 1)) {
          ambient_dist = min(ambient_dist, 1.0 - (hit.z - (int)(hit.z)));
        }
        
        if (is_solid(position.x - step.x, position.y - 1, position.z - 1)) {
          ambient_dist = min(ambient_dist, sqrt(square(hit.y - (int)(hit.y)) + square(hit.z - (int)(hit.z))));
        }
        
        if (is_solid(position.x - step.x, position.y + 1, position.z - 1)) {
          ambient_dist = min(ambient_dist, sqrt(square(1.0 - (hit.y - (int)(hit.y))) + square(hit.z - (int)(hit.z))));
        }
        
        if (is_solid(position.x - step.x, position.y - 1, position.z + 1)) {
          ambient_dist = min(ambient_dist, sqrt(square(hit.y - (int)(hit.y)) + square(1.0 - (hit.z - (int)(hit.z)))));
        }
        
        if (is_solid(position.x - step.x, position.y + 1, position.z + 1)) {
          ambient_dist = min(ambient_dist, sqrt(square(1.0 - (hit.y - (int)(hit.y))) + square(1.0 - (hit.z - (int)(hit.z)))));
        }
      } else if (hit_side == 1) {
        if (is_solid(position.x - 1, position.y - step.y, position.z)) {
          ambient_dist = min(ambient_dist, hit.x - (int)(hit.x));
        }
        
        if (is_solid(position.x + 1, position.y - step.y, position.z)) {
          ambient_dist = min(ambient_dist, 1.0 - (hit.x - (int)(hit.x)));
        }
        
        if (is_solid(position.x, position.y - step.y, position.z - 1)) {
          ambient_dist = min(ambient_dist, hit.z - (int)(hit.z));
        }
        
        if (is_solid(position.x, position.y - step.y, position.z + 1)) {
          ambient_dist = min(ambient_dist, 1.0 - (hit.z - (int)(hit.z)));
        }
        
        if (is_solid(position.x - 1, position.y - step.y, position.z - 1)) {
          ambient_dist = min(ambient_dist, sqrt(square(hit.x - (int)(hit.x)) + square(hit.z - (int)(hit.z))));
        }
        
        if (is_solid(position.x + 1, position.y - step.y, position.z - 1)) {
          ambient_dist = min(ambient_dist, sqrt(square(1.0 - (hit.x - (int)(hit.x))) + square(hit.z - (int)(hit.z))));
        }
        
        if (is_solid(position.x - 1, position.y - step.y, position.z + 1)) {
          ambient_dist = min(ambient_dist, sqrt(square(hit.x - (int)(hit.x)) + square(1.0 - (hit.z - (int)(hit.z)))));
        }
        
        if (is_solid(position.x + 1, position.y - step.y, position.z + 1)) {
          ambient_dist = min(ambient_dist, sqrt(square(1.0 - (hit.x - (int)(hit.x))) + square(1.0 - (hit.z - (int)(hit.z)))));
        }
      } else if (hit_side == 2) {
        if (is_solid(position.x - 1, position.y, position.z - step.z)) {
          ambient_dist = min(ambient_dist, hit.x - (int)(hit.x));
        }
        
        if (is_solid(position.x + 1, position.y, position.z - step.z)) {
          ambient_dist = min(ambient_dist, 1.0 - (hit.x - (int)(hit.x)));
        }
        
        if (is_solid(position.x, position.y - 1, position.z - step.z)) {
          ambient_dist = min(ambient_dist, hit.y - (int)(hit.y));
        }
        
        if (is_solid(position.x, position.y + 1, position.z - step.z)) {
          ambient_dist = min(ambient_dist, 1.0 - (hit.y - (int)(hit.y)));
        }
        
        if (is_solid(position.x - 1, position.y - 1, position.z - step.z)) {
          ambient_dist = min(ambient_dist, sqrt(square(hit.x - (int)(hit.x)) + square(hit.y - (int)(hit.y))));
        }
        
        if (is_solid(position.x + 1, position.y - 1, position.z - step.z)) {
          ambient_dist = min(ambient_dist, sqrt(square(1.0 - (hit.x - (int)(hit.x))) + square(hit.y - (int)(hit.y))));
        }
        
        if (is_solid(position.x - 1, position.y + 1, position.z - step.z)) {
          ambient_dist = min(ambient_dist, sqrt(square(hit.x - (int)(hit.x)) + square(1.0 - (hit.y - (int)(hit.y)))));
        }
        
        if (is_solid(position.x + 1, position.y + 1, position.z - step.z)) {
          ambient_dist = min(ambient_dist, sqrt(square(1.0 - (hit.x - (int)(hit.x))) + square(1.0 - (hit.y - (int)(hit.y)))));
        }
      }
      
      if (ambient_dist < 0.25) {
        ambient_dist = (sqrt(ambient_dist / 0.25)) * 0.67 + 0.33;
      } else {
        ambient_dist = 1.0;
      }
      
      vec3 color = get_color(tile);
      color *= border_dist * ambient_dist;
      
      vec3 pre_hit = hit;
      
      if (hit_side == 0) pre_hit.x -= step.x * 0.001;
      if (hit_side == 1) pre_hit.y -= step.y * 0.001;
      if (hit_side == 2) pre_hit.z -= step.z * 0.001;
      
      if (tile == vx_tile_trunk) {
        vec3 mod = hit - floor(hit);
        
        if (hit_side == 1 && mod.x >= 0.05 && mod.x <= 0.95 && mod.z >= 0.05 && mod.z < 0.95) {
          color = get_color(vx_tile_wood);
        }
      }
      
      color *= plot_shadow(pre_hit, vec3(cos(2 * PI * time), sin(2 * PI * time), 0.0));
      
      if (position == mouse_pos && hit_side == mouse_side) {
        color = color * 0.5 + vec3(1.0, 1.0, 1.0) * 0.5;
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
        
        position = uvec3((uint)(start.x), (uint)(start.y), (uint)(start.z));
        
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
      } else if (tile == vx_tile_leaves) {
        if (hit_side == 0) {
          if (in_leaves((hit.y - (int)(hit.y)) * 16.0, (hit.z - (int)(hit.z)) * 16.0)) continue;
        } else if (hit_side == 1) {
          if (in_leaves((hit.z - (int)(hit.z)) * 16.0, (hit.x - (int)(hit.x)) * 16.0)) continue;
        } else if (hit_side == 2) {
          if (in_leaves((hit.x - (int)(hit.x)) * 16.0, (hit.y - (int)(hit.y)) * 16.0)) continue;
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
  
  if (color_left <= 0.0) return final_color;
  
  float old_y = ray.y;
  
  if (ray.y < 0.000001) ray.y = 0.000001;
  float ceil_w = (160.0 - start.y) / ray.y;
  
  float ceil_x = mod((start.x + ray.x * ceil_w) + time * vx_cloud_side, vx_cloud_side);
  while (ceil_x < 0.0) ceil_x += vx_cloud_side;
  
  float ceil_z = mod((start.z + ray.z * ceil_w) - time * vx_cloud_side, vx_cloud_side);
  while (ceil_z < 0.0) ceil_z += vx_cloud_side;
  
  float light = cos(PI * (time + 0.25f)) * cos(PI * (time + 0.25f));
  float k = 800.0 * light;
  
  k = k * k;
  k = k * k;
  
  float cr = k / 240100000000.0;
  float cg = k / 78904810000.0;
  float cb = k / 37480960000.0;
  
  ceil_w = ((240.0 - start.y) / ray.y) / 100.0;
  ceil_w = 32.0 / ceil_w;
  
  if (old_y >= 0.000001 && in_cloud(ceil_x, ceil_z)) {
    ceil_w = lerp(0.0, ceil_w, light);
  }
  
  float tr = pow(E, -cr * ceil_w);
  float tg = pow(E, -cg * ceil_w);
  float tb = pow(E, -cb * ceil_w);
  
  float r = tr * (1.0 - cr);
  float g = tg * (1.0 - cg);
  float b = tb * (1.0 - cb);
  
  float rem = (1.0 - light) * (1.0 - 1.0 / (1.0 + ceil_w / 2.0));
  
  r -= 0.50 * rem;
  g -= 0.25 * rem;
  
  if (r < 0.0) r = 0.0;
  if (g < 0.0) g = 0.0;
  if (b < 0.0) b = 0.0;
  
  if (r > 1.0) r = 1.0;
  if (g > 1.0) g = 1.0;
  if (b > 1.0) b = 1.0;
  
  final_color += vec3(r, g, b) * color_left;
  return final_color;
}

void main() {
  vec2 coords = (gl_FragCoord.xy / screen_size.xy) * 2.0 - vec2(1.0, 1.0);
  
  vec3 ray = vec3(coords.x, coords.y * (screen_size.y / screen_size.x), 1.0);
  
  ray.zy = rotate(ray.zy, -angle_ud);
  ray.xz = rotate(ray.xz, -angle_lr);
  
  vec3 color = plot_pixel(camera + vec3(0.0, 0.5, 0.0), ray);
  
  if ((int)(gl_FragCoord.x) == (int)(screen_size.x / 2.0) && abs((int)(gl_FragCoord.y) - (int)(screen_size.y / 2.0)) < 6) {
    color = vec3(1.0, 1.0, 1.0) - color;
  } else if ((int)(gl_FragCoord.y) == (int)(screen_size.y / 2.0) && abs((int)(gl_FragCoord.x) - (int)(screen_size.x / 2.0)) < 6) {
    color = vec3(1.0, 1.0, 1.0) - color;
  }
  
  gl_FragColor = vec4(color, 1.0);
}
