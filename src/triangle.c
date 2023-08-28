#include "display.h"
#include "swap.h"
#include "triangle.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

///////////////////////////////////////////////////////////////////////////////
// Return the normal vector of a triangle face
///////////////////////////////////////////////////////////////////////////////
vec3_t get_triangle_normal(vec4_t vertices[3]) {
    // Get individual vectors from A, B, and C vertices to compute normal
    vec3_t vector_a = vec3_from_vec4(vertices[0]);  /*   A   */
    vec3_t vector_b = vec3_from_vec4(vertices[1]);  /*  / \  */
    vec3_t vector_c = vec3_from_vec4(vertices[2]);  /* C---B */

    // Get the vector subtraction of B-A and C-A
    vec3_t vector_ab = vec3_sub(vector_b, vector_a);
    vec3_t vector_ac = vec3_sub(vector_c, vector_a);
    vec3_normalize(&vector_ab);
    vec3_normalize(&vector_ac);

    // Compute the face normal (using cross product to find perpendicular)
    vec3_t normal = vec3_cross(vector_ab, vector_ac);
    vec3_normalize(&normal);

    return normal;
}

///////////////////////////////////////////////////////////////////////////////
// Compute the edge function between three vec2s
///////////////////////////////////////////////////////////////////////////////
float edge_cross(vec2_t* a, vec2_t* b, vec2_t* p) {
    vec2_t ab = { b->x - a->x, b->y - a->y };
    vec2_t ap = { p->x - a->x, p->y - a->y };
    return ab.x * ap.y - ab.y * ap.x;
}

///////////////////////////////////////////////////////////////////////////////
// Draw a triangle using three raw line calls
///////////////////////////////////////////////////////////////////////////////
void draw_wire_triangle(float x0, float y0, float x1, float y1, float x2, float y2, uint32_t color) {
    draw_line(x0, y0, x1, y1, color);
    draw_line(x1, y1, x2, y2, color);
    draw_line(x2, y2, x0, y0, color);
}

///////////////////////////////////////////////////////////////////////////////
// Draw a filled triangle with a solid color
///////////////////////////////////////////////////////////////////////////////
void draw_filled_triangle(
    float x0, float y0, float z0, float w0,
    float x1, float y1, float z1, float w1,
    float x2, float y2, float z2, float w2,
    uint32_t color
) {
    vec2_t v0 = {x0, y0};
    vec2_t v1 = {x1, y1};
    vec2_t v2 = {x2, y2};
    
    float area = edge_cross(&v0, &v1, &v2);
    
    // Perform back-face culling testing the signed area of the full triangle edge function
    if (area < 0) {
        return;
    }
    
    // Compute the bounding box with candidate pixels to be rendered
    int x_min = floor(MIN(MIN(v0.x, v1.x), v2.x));
    int y_min = floor(MIN(MIN(v0.y, v1.y), v2.y));
    int x_max = ceil(MAX(MAX(v0.x, v1.x), v2.x));
    int y_max = ceil(MAX(MAX(v0.y, v1.y), v2.y));

    // Compute the constant delta_s that will be used for the horizontal and vertical steps
    float delta_edge0_col = (v1.y - v2.y);
    float delta_edge1_col = (v2.y - v0.y);
    float delta_edge2_col = (v0.y - v1.y);
    float delta_edge0_row = (v2.x - v1.x);
    float delta_edge1_row = (v0.x - v2.x);
    float delta_edge2_row = (v1.x - v0.x);

    // Compute the edge functions for the first (top-left) point (point p0 is in the middle of the pixel)
    vec2_t p0 = { x_min + 0.5f, y_min + 0.5f };
    float edge0_row = edge_cross(&v1, &v2, &p0);
    float edge1_row = edge_cross(&v2, &v0, &p0);
    float edge2_row = edge_cross(&v0, &v1, &p0);

    // Loop all candidate pixels inside the bounding box
    for (int y = y_min; y <= y_max; y++) {
        float edge0 = edge0_row;
        float edge1 = edge1_row;
        float edge2 = edge2_row;
        for (int x = x_min; x <= x_max; x++) {
            bool is_inside = edge0 >= 0 && edge1 >= 0 && edge2 >= 0;
            if (is_inside) {
                float alpha = edge0 / area;
                float beta  = edge1 / area;
                float gamma = edge2 / area;
                float interpolated_reciprocal_w = (1 / w0) * alpha + (1 / w1) * beta + (1 / w2) * gamma;
                interpolated_reciprocal_w = 1.0 - interpolated_reciprocal_w;
                if (interpolated_reciprocal_w < get_zbuffer_at(x, y)) {
                    draw_pixel(x, y, color);
                    update_zbuffer_at(x, y, interpolated_reciprocal_w);
                }
            }
            edge0 += delta_edge0_col;
            edge1 += delta_edge1_col;
            edge2 += delta_edge2_col;
        }
        edge0_row += delta_edge0_row;
        edge1_row += delta_edge1_row;
        edge2_row += delta_edge2_row;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Draw a textured triangle based on a texture array of colors.
///////////////////////////////////////////////////////////////////////////////
void draw_textured_triangle(
    float x0, float y0, float z0, float w0, float tex_u0, float tex_v0,
    float x1, float y1, float z1, float w1, float tex_u1, float tex_v1,
    float x2, float y2, float z2, float w2, float tex_u2, float tex_v2,
    upng_t* texture
) {
    vec2_t v0 = {x0, y0};
    vec2_t v1 = {x1, y1};
    vec2_t v2 = {x2, y2};

    // Flip the V component to account for inverted UV-coordinates (V grows downwards)
    tex_v0 = 1.0 - tex_v0;
    tex_v1 = 1.0 - tex_v1;
    tex_v2 = 1.0 - tex_v2;

    float area = edge_cross(&v0, &v1, &v2);
    
    // Perform back-face culling testing the signed area of the full triangle edge function
    if (area < 0) {
        return;
    }

    // Compute the bounding box with candidate pixels to be rendered
    int x_min = floor(MIN(MIN(v0.x, v1.x), v2.x));
    int y_min = floor(MIN(MIN(v0.y, v1.y), v2.y));
    int x_max = ceil(MAX(MAX(v0.x, v1.x), v2.x));
    int y_max = ceil(MAX(MAX(v0.y, v1.y), v2.y));

    // Compute the constant delta_s that will be used for the horizontal and vertical steps
    float delta_edge0_col = (v1.y - v2.y);
    float delta_edge1_col = (v2.y - v0.y);
    float delta_edge2_col = (v0.y - v1.y);
    float delta_edge0_row = (v2.x - v1.x);
    float delta_edge1_row = (v0.x - v2.x);
    float delta_edge2_row = (v1.x - v0.x);

    // Compute the edge functions for the first (top-left) point (point p0 is in the middle of the pixel)
    vec2_t p0 = { x_min + 0.5f, y_min + 0.5f };
    float edge0_row = edge_cross(&v1, &v2, &p0);
    float edge1_row = edge_cross(&v2, &v0, &p0);
    float edge2_row = edge_cross(&v0, &v1, &p0);

    // Loop all candidate pixels inside the bounding box
    for (int y = y_min; y <= y_max; y++) {
        float edge0 = edge0_row;
        float edge1 = edge1_row;
        float edge2 = edge2_row;
        for (int x = x_min; x <= x_max; x++) {
            bool is_inside = edge0 >= 0 && edge1 >= 0 && edge2 >= 0;
            if (is_inside) {
                float alpha = edge0 / area;
                float beta  = edge1 / area;
                float gamma = edge2 / area;

                // Perform the interpolation of all U/w and V/w values using barycentric weights and a factor of 1/w
                float interpolated_u = (tex_u0 / w0) * alpha + (tex_u1 / w1) * beta + (tex_u2 / w2) * gamma;
                float interpolated_v = (tex_v0 / w0) * alpha + (tex_v1 / w1) * beta + (tex_v2 / w2) * gamma;

                // Also interpolate the value of 1/w for the current pixel
                float interpolated_reciprocal_w = (1 / w0) * alpha + (1 / w1) * beta + (1 / w2) * gamma;

                // Now we can divide back both interpolated values by 1/w
                interpolated_u /= interpolated_reciprocal_w;
                interpolated_v /= interpolated_reciprocal_w;

                // Get the mesh texture width and height dimensions
                int texture_width = upng_get_width(texture);
                int texture_height = upng_get_height(texture);

                // Map the UV coordinate to the full texture width and height
                int tex_x = abs((int)(interpolated_u * texture_width)) % texture_width;
                int tex_y = abs((int)(interpolated_v * texture_height)) % texture_height;

                // Adjust 1/w so the pixels that are closer to the camera have smaller values
                interpolated_reciprocal_w = 1.0 - interpolated_reciprocal_w;

                if (interpolated_reciprocal_w < get_zbuffer_at(x, y)) {
                    uint32_t* texture_buffer = (uint32_t*)upng_get_buffer(texture);
                    draw_pixel(x, y, texture_buffer[(texture_width * tex_y) + tex_x]);
                    update_zbuffer_at(x, y, interpolated_reciprocal_w);
                }
            }
            edge0 += delta_edge0_col;
            edge1 += delta_edge1_col;
            edge2 += delta_edge2_col;
        }
        edge0_row += delta_edge0_row;
        edge1_row += delta_edge1_row;
        edge2_row += delta_edge2_row;
    }
}