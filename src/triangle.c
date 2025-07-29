#include "display.h"
#include "swap.h"
#include "triangle.h"

///////////////////////////////////////////////////////////////////////////////
// Return the normal vector of a triangle face
///////////////////////////////////////////////////////////////////////////////
vec3_t get_triangle_normal(vec4_t vertices[3]) {
    // Get individual vectors from A, B, and C vertices to compute normal
    vec3_t vector_a = vec3_from_vec4(vertices[0]); /*   A   */
    vec3_t vector_b = vec3_from_vec4(vertices[1]); /*  / \  */
    vec3_t vector_c = vec3_from_vec4(vertices[2]); /* C---B */

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
// Checks if a triangle edge is top-left
///////////////////////////////////////////////////////////////////////////////
bool is_top_left(vec2_t* start, vec2_t* end) {
    vec2_t edge = { end->x - start->x, end->y - start->y };
    bool is_top_edge = edge.y == 0 && edge.x > 0;
    bool is_left_edge = edge.y < 0;
    return is_left_edge || is_top_edge;
}

///////////////////////////////////////////////////////////////////////////////
// Performs the 2D edge-cross between 2 vertices and a point
///////////////////////////////////////////////////////////////////////////////
float edge_cross(vec2_t* a, vec2_t* b, vec2_t* p) {
    vec2_t ab = { b->x - a->x, b->y - a->y };
    vec2_t ap = { p->x - a->x, p->y - a->y };
    return ab.x * ap.y - ab.y * ap.x;
}

///////////////////////////////////////////////////////////////////////////////
// Draw a triangle using three raw line calls
///////////////////////////////////////////////////////////////////////////////
void draw_wire_triangle(
    vec2_t* v0,
    vec2_t* v1,
    vec2_t* v2,
    uint32_t color
) {
    draw_line(v0->x, v0->y, v1->x, v1->y, color);
    draw_line(v1->x, v1->y, v2->x, v2->y, color);
    draw_line(v2->x, v2->y, v0->x, v0->y, color);
}

///////////////////////////////////////////////////////////////////////////////
// Draw a textured triangle based on a texture array of colors.
// The parameters are the vertices v0, v1, v3, and their UV texture coords.
///////////////////////////////////////////////////////////////////////////////
//
//        v0
//        /\
//       /  \
//      /    \
//     /      \
//   v1        \
//     \_       \
//        \_     \
//           \_   \
//              \_ \
//                 \\
//                   \
//                    v2
//
///////////////////////////////////////////////////////////////////////////////
void draw_textured_triangle(
    vec4_t* v0, float v0u, float v0v,
    vec4_t* v1, float v1u, float v1v,
    vec4_t* v2, float v2u, float v2v,
    upng_t* texture
) {
    // Flip the V component to account for inverted UV-coordinates (V grows downwards)
    v0v = 1.0 - v0v;
    v1v = 1.0 - v1v;
    v2v = 1.0 - v2v;

    // Finds the bounding box with all candidate pixels
    int x_min = floor(MIN(MIN(v0->x, v1->x), v2->x));
    int y_min = floor(MIN(MIN(v0->y, v1->y), v2->y));
    int x_max = ceil(MAX(MAX(v0->x, v1->x), v2->x));
    int y_max = ceil(MAX(MAX(v0->y, v1->y), v2->y));

    // Screen 2D points from vertices v0, v1, and v2
    vec2_t sv0 = { v0->x, v0->y };
    vec2_t sv1 = { v1->x, v1->y };
    vec2_t sv2 = { v2->x, v2->y };

    // Compute the area of the entire triangle/parallelogram
    float area = edge_cross(&sv0, &sv1, &sv2);

    // Back-face culling using the signed-area
    if (area <= 0) {
        return;
    }

    // Compute the constant delta_s that will be used for the horizontal and vertical steps
    float delta_w0_col = (v1->y - v2->y);
    float delta_w1_col = (v2->y - v0->y);
    float delta_w2_col = (v0->y - v1->y);
    float delta_w0_row = (v2->x - v1->x);
    float delta_w1_row = (v0->x - v2->x);
    float delta_w2_row = (v1->x - v0->x);

    // Rasterization fill rule, not 100% precise due to floating point innacuracy
    float bias0 = 0; // is_top_left(&sv1, &sv2) ? 0 : -0.0001;
    float bias1 = 0; // is_top_left(&sv2, &sv0) ? 0 : -0.0001;
    float bias2 = 0; // is_top_left(&sv0, &sv1) ? 0 : -0.0001;

    // Compute the edge functions for the fist (top-left) point
    vec2_t p0 = { x_min + 0.5f , y_min + 0.5f };
    float w0_row = edge_cross(&sv1, &sv2, &p0) + bias0;
    float w1_row = edge_cross(&sv2, &sv0, &p0) + bias1;
    float w2_row = edge_cross(&sv0, &sv1, &p0) + bias2;

    // Loop all candidate pixels inside the bounding box
    for (int y = y_min; y <= y_max; y++) {
        float w0 = w0_row;
        float w1 = w1_row;
        float w2 = w2_row;
        for (int x = x_min; x <= x_max; x++) {
            bool is_inside = w0 >= 0 && w1 >= 0 && w2 >= 0;
            if (is_inside) {
                float alpha = w0 / area;
                float beta  = w1 / area;
                float gamma = w2 / area;
                
                // Variables to store the interpolated values of U, V, and also 1/w for the current pixel
                float interpolated_u;
                float interpolated_v;
                float interpolated_reciprocal_w;

                // Perform the interpolation of all U/w and V/w values using barycentric weights and a factor of 1/w
                interpolated_u = (v0u / v0->w) * alpha + (v1u / v1->w) * beta + (v2u / v2->w) * gamma;
                interpolated_v = (v0v / v0->w) * alpha + (v1v / v1->w) * beta + (v2v / v2->w) * gamma;

                // Also interpolate the value of 1/w for the current pixel
                interpolated_reciprocal_w = (1 / v0->w) * alpha + (1 / v1->w) * beta + (1 / v2->w) * gamma;

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

                // Only draw the pixel if the depth value is less than the one previously stored in the z-buffer
                if (interpolated_reciprocal_w < get_zbuffer_at(x, y)) {
                    // Draw a pixel at position (x,y) with the color that comes from the mapped texture
                    uint32_t* texture_buffer = (uint32_t*)upng_get_buffer(texture);
                    draw_pixel(x, y, texture_buffer[(texture_width * tex_y) + tex_x]);

                    // Update the z-buffer value with the 1/w of this current pixel
                    update_zbuffer_at(x, y, interpolated_reciprocal_w);
                }
            }
            w0 += delta_w0_col;
            w1 += delta_w1_col;
            w2 += delta_w2_col;
        }
        w0_row += delta_w0_row;
        w1_row += delta_w1_row;
        w2_row += delta_w2_row;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Draw a flat-shaded fill triangle
///////////////////////////////////////////////////////////////////////////////
//
//        v0
//        /\
//       /  \
//      /    \
//     /      \
//   v1        \
//     \_       \
//        \_     \
//           \_   \
//              \_ \
//                 \\
//                   \
//                    v2
//
///////////////////////////////////////////////////////////////////////////////
void draw_filled_triangle(
    vec4_t* v0,
    vec4_t* v1,
    vec4_t* v2,
    uint32_t color
) {
    // Finds the bounding box with all candidate pixels
    int x_min = floor(MIN(MIN(v0->x, v1->x), v2->x));
    int y_min = floor(MIN(MIN(v0->y, v1->y), v2->y));
    int x_max = ceil(MAX(MAX(v0->x, v1->x), v2->x));
    int y_max = ceil(MAX(MAX(v0->y, v1->y), v2->y));

    // Screen 2D points from vertices v0, v1, and v2
    vec2_t sv0 = { v0->x, v0->y };
    vec2_t sv1 = { v1->x, v1->y };
    vec2_t sv2 = { v2->x, v2->y };

    // Compute the area of the entire triangle/parallelogram
    float area = edge_cross(&sv0, &sv1, &sv2);

    // Back-face culling using the signed-area
    if (area <= 0) {
        return;
    }

    // Compute the constant delta_s that will be used for the horizontal and vertical steps
    float delta_w0_col = (v1->y - v2->y);
    float delta_w1_col = (v2->y - v0->y);
    float delta_w2_col = (v0->y - v1->y);
    float delta_w0_row = (v2->x - v1->x);
    float delta_w1_row = (v0->x - v2->x);
    float delta_w2_row = (v1->x - v0->x);

    // Rasterization fill rule, not 100% precise due to floating point innacuracy
    float bias0 = 0; // is_top_left(&sv1, &sv2) ? 0 : -0.0001;
    float bias1 = 0; // is_top_left(&sv2, &sv0) ? 0 : -0.0001;
    float bias2 = 0; // is_top_left(&sv0, &sv1) ? 0 : -0.0001;

    // Compute the edge functions for the fist (top-left) point
    vec2_t p0 = { x_min + 0.5f , y_min + 0.5f };
    float w0_row = edge_cross(&sv1, &sv2, &p0) + bias0;
    float w1_row = edge_cross(&sv2, &sv0, &p0) + bias1;
    float w2_row = edge_cross(&sv0, &sv1, &p0) + bias2;

    // Loop all candidate pixels inside the bounding box
    for (int y = y_min; y <= y_max; y++) {
        float w0 = w0_row;
        float w1 = w1_row;
        float w2 = w2_row;
        for (int x = x_min; x <= x_max; x++) {
            bool is_inside = w0 >= 0 && w1 >= 0 && w2 >= 0;
            if (is_inside) {
                float alpha = w0 / area;
                float beta  = w1 / area;
                float gamma = w2 / area;
                
                // Interpolate the value of 1/w for the current pixel
                float interpolated_reciprocal_w = (1 / v0->w) * alpha + (1 / v1->w) * beta + (1 / v2->w) * gamma;

                // Adjust 1/w so the pixels that are closer to the camera have smaller values
                interpolated_reciprocal_w = 1.0 - interpolated_reciprocal_w;

                // Only draw the pixel if the depth value is less than the one previously stored in the z-buffer
                if (interpolated_reciprocal_w < get_zbuffer_at(x, y)) {
                    // Draw a pixel at position (x,y) with a solid color
                    draw_pixel(x, y, color);

                    // Update the z-buffer value with the 1/w of this current pixel
                    update_zbuffer_at(x, y, interpolated_reciprocal_w);
                }
            }
            w0 += delta_w0_col;
            w1 += delta_w1_col;
            w2 += delta_w2_col;
        }
        w0_row += delta_w0_row;
        w1_row += delta_w1_row;
        w2_row += delta_w2_row;
    }
}