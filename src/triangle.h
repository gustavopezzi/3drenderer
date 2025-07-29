#ifndef TRIANGLE_H
#define TRIANGLE_H

#include <stdint.h>
#include "texture.h"
#include "vector.h"
#include "upng.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

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
    vec2_t* v0, // Vertex 0 (screen point)
    vec2_t* v1, // Vertex 1 (screen point)
    vec2_t* v2, // Vertex 2 (screen point)
    uint32_t color
);

void draw_filled_triangle(
    vec4_t* v0, // Vertex 0
    vec4_t* v1, // Vertex 1
    vec4_t* v2, // Vertex 2
    uint32_t color
);

void draw_textured_triangle(
    vec4_t* v0, float v0u, float v0v, // Vertex 0, followed by its UV texture coord.
    vec4_t* v1, float v1u, float v1v, // Vertex 1, followed by its UV texture coord.
    vec4_t* v2, float v2u, float v2v, // Vertex 0, followed by its UV texture coord.
    upng_t* texture
);

#endif
