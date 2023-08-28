#ifndef TRIANGLE_H
#define TRIANGLE_H

#include <stdint.h>
#include "texture.h"
#include "vector.h"
#include "upng.h"

typedef struct {
    int a;
    int b;
    int c;
    tex2_t a_uv;
    tex2_t b_uv;
    tex2_t c_uv;
    uint32_t color;
} face_t;

typedef struct {
    vec4_t points[3];
    tex2_t texcoords[3];
    uint32_t color;
    upng_t* texture;
} triangle_t;

vec3_t get_triangle_normal(vec4_t vertices[3]);

void draw_wire_triangle(
    float x0, float y0, float x1, float y1, float x2, float y2,
    uint32_t color
);

void draw_filled_triangle(
    float x0, float y0, float z0, float w0,
    float x1, float y1, float z1, float w1,
    float x2, float y2, float z2, float w2,
    uint32_t color
);

void draw_textured_triangle(
    float x0, float y0, float z0, float w0, float tex_u0, float tex_v0,
    float x1, float y1, float z1, float w1, float tex_u1, float tex_v1,
    float x2, float y2, float z2, float w2, float tex_u2, float tex_v2,
    upng_t* texture
);

#endif
