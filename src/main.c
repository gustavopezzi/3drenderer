#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>
#include "upng.h"
#include "array.h"
#include "display.h"
#include "triangle.h"
#include "clipping.h"
#include "vector.h"
#include "matrix.h"
#include "light.h"
#include "camera.h"
#include "texture.h"
#include "mesh.h"

///////////////////////////////////////////////////////////////////////////////
// Global variables for execution status and game loop
///////////////////////////////////////////////////////////////////////////////
bool is_running = false;
int previous_frame_time = 0;
float delta_time = 0;

uint32_t fps, last_fps;

///////////////////////////////////////////////////////////////////////////////
// Array to store triangles that should be rendered each frame
///////////////////////////////////////////////////////////////////////////////
#define MAX_TRIANGLES 10000
triangle_t triangles_to_render[MAX_TRIANGLES];
int triangles_to_render_count = 0;

///////////////////////////////////////////////////////////////////////////////
// Declaration of our global transformation matrices
///////////////////////////////////////////////////////////////////////////////
mat4_t world_matrix;
mat4_t proj_matrix;
mat4_t view_matrix;

///////////////////////////////////////////////////////////////////////////////
// Setup function to initialize variables and game objects
///////////////////////////////////////////////////////////////////////////////
void setup(void) {
    // Initialize render mode and triangle culling method
    set_render_method(RENDER_TEXTURED);
    set_cull_method(CULL_BACKFACE);

    // Initialize the scene light direction
    init_light(vec3_new(0, 0, 1));

    // Initialize the perspective projection matrix
    float aspect_y = (float)get_window_height() / (float)get_window_width();
    float aspect_x = (float)get_window_width() / (float)get_window_height();
    float fov_y = 3.141592 / 3.0; // the same as 180/3, or 60deg
    float fov_x = atan(tan(fov_y / 2) * aspect_x) * 2;
    float znear = 1.0;
    float zfar = 50.0;
    proj_matrix = mat4_make_perspective(fov_y, aspect_y, znear, zfar);

    // Initialize frustum planes with a point and a normal
    init_frustum_planes(fov_x, fov_y, znear, zfar);

    // Loads mesh entities
    // load_mesh("./assets/cat.obj", "./assets/cat.png", vec3_new(1, 1, 1), vec3_new(0, 0, +5), vec3_new(0, 0, 0));

    // Loads mesh entities
    load_mesh("./assets/runway.obj", "./assets/runway.png", vec3_new(1, 1, 1), vec3_new(0, -1.5, +23), vec3_new(0, 0, 0));
    load_mesh("./assets/f22.obj", "./assets/f22.png", vec3_new(1, 1, 1), vec3_new(0, -1.3, +5), vec3_new(0, -M_PI/2, 0));
    load_mesh("./assets/efa.obj", "./assets/efa.png", vec3_new(1, 1, 1), vec3_new(-2, -1.3, +9), vec3_new(0, -M_PI/2, 0));
    load_mesh("./assets/f117.obj", "./assets/f117.png", vec3_new(1, 1, 1), vec3_new(+2, -1.3, +9), vec3_new(0, -M_PI/2, 0));
}

///////////////////////////////////////////////////////////////////////////////
// Poll system events and handle keyboard input
///////////////////////////////////////////////////////////////////////////////
void process_input(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT: {
                is_running = false;
                break;
            }
            case SDL_MOUSEMOTION: {
                float sensitivity = 0.07f;
                float yaw = event.motion.xrel * sensitivity;
                float pitch = event.motion.yrel * sensitivity;
                rotate_camera_yaw(yaw * delta_time);
                rotate_camera_pitch(pitch * delta_time);
                break;
            }
            case SDL_MOUSEWHEEL: {
                float speed = 20.0f;
                if (event.wheel.y > 0) {
                    update_camera_forward_velocity(vec3_mul(get_camera_direction(), speed * delta_time));
                    update_camera_position(vec3_add(get_camera_position(), get_camera_forward_velocity()));
                } else if (event.wheel.y < 0) {
                    update_camera_forward_velocity(vec3_mul(get_camera_direction(), speed * delta_time));
                    update_camera_position(vec3_sub(get_camera_position(), get_camera_forward_velocity()));
                }
                break;
            }
            case SDL_KEYDOWN: {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    is_running = false;
                }
                if (event.key.keysym.sym == SDLK_1) {
                    set_render_method(RENDER_WIRE_VERTEX);
                }
                if (event.key.keysym.sym == SDLK_2) {
                    set_render_method(RENDER_WIRE);
                }
                if (event.key.keysym.sym == SDLK_3) {
                    set_render_method(RENDER_FILL_TRIANGLE);
                }
                if (event.key.keysym.sym == SDLK_4) {
                    set_render_method(RENDER_FILL_TRIANGLE_WIRE);
                }
                if (event.key.keysym.sym == SDLK_5) {
                    set_render_method(RENDER_TEXTURED);
                }
                if (event.key.keysym.sym == SDLK_6) {
                    set_render_method(RENDER_TEXTURED_WIRE);
                }
                if (event.key.keysym.sym == SDLK_c) {
                    set_cull_method(CULL_BACKFACE);
                }
                if (event.key.keysym.sym == SDLK_x) {
                    set_cull_method(CULL_NONE);
                }
                break;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Process the graphics pipeline stages for all the mesh triangles
///////////////////////////////////////////////////////////////////////////////
// +-------------+
// | Model space |  <-- original mesh vertices
// +-------------+
// |   +-------------+
// `-> | World space |  <-- multiply by world matrix
//     +-------------+
//     |   +--------------+
//     `-> | Camera space |  <-- multiply by view matrix
//         +--------------+
//         |    +------------+
//         `--> |  Clipping  |  <-- clip against the six frustum planes
//              +------------+
//              |    +------------+
//              `--> | Projection |  <-- multiply by projection matrix
//                   +------------+
//                   |    +-------------+
//                   `--> | Image space |  <-- apply perspective divide
//                        +-------------+
//                        |    +--------------+
//                        `--> | Screen space |  <-- ready to render
//                             +--------------+
///////////////////////////////////////////////////////////////////////////////
void process_graphics_pipeline_stages(mesh_t* mesh) {
    // Create scale, rotation, and translation matrices that will be used to multiply the mesh vertices
    mat4_t scale_matrix = mat4_make_scale(mesh->scale.x, mesh->scale.y, mesh->scale.z);
    mat4_t translation_matrix = mat4_make_translation(mesh->translation.x, mesh->translation.y, mesh->translation.z);
    mat4_t rotation_matrix_x = mat4_make_rotation_x(mesh->rotation.x);
    mat4_t rotation_matrix_y = mat4_make_rotation_y(mesh->rotation.y);
    mat4_t rotation_matrix_z = mat4_make_rotation_z(mesh->rotation.z);

    // Update camera look at target to create view matrix
    vec3_t target = get_camera_lookat_target();
    vec3_t up_direction = vec3_new(0, 1, 0);
    view_matrix = mat4_look_at(get_camera_position(), target, up_direction);

    // Loop all triangle faces of our mesh
    int num_faces = array_length(mesh->faces);
    for (int face_index = 0; face_index < num_faces; face_index++) {
        face_t mesh_face = mesh->faces[face_index];

        vec3_t face_vertices[3];
        face_vertices[0] = mesh->vertices[mesh_face.a - 1];
        face_vertices[1] = mesh->vertices[mesh_face.b - 1];
        face_vertices[2] = mesh->vertices[mesh_face.c - 1];

        vec4_t transformed_vertices[3];

        // Loop all three vertices of this current face and apply transformations
        for (int v = 0; v < 3; v++) {
            vec4_t transformed_vertex = vec4_from_vec3(face_vertices[v]);

            // Create a World Matrix combining scale, rotation, and translation matrices
            world_matrix = mat4_identity();

            // Order matters: First scale, then rotate, then translate. [T]*[R]*[S]*v
            world_matrix = mat4_mul_mat4(scale_matrix, world_matrix);
            world_matrix = mat4_mul_mat4(rotation_matrix_z, world_matrix);
            world_matrix = mat4_mul_mat4(rotation_matrix_y, world_matrix);
            world_matrix = mat4_mul_mat4(rotation_matrix_x, world_matrix);
            world_matrix = mat4_mul_mat4(translation_matrix, world_matrix);

            // Multiply the world matrix by the original vector
            transformed_vertex = mat4_mul_vec4(world_matrix, transformed_vertex);

            // Multiply the view matrix by the vector to transform the scene to camera space
            transformed_vertex = mat4_mul_vec4(view_matrix, transformed_vertex);

            // Save transformed vertex in the array of transformed vertices
            transformed_vertices[v] = transformed_vertex;
        }

        // Calculate the triangle face normal
        vec3_t face_normal = get_triangle_normal(transformed_vertices);

        // Backface culling test to see if the current face should be projected
        if (should_cull_backface()) {
            // Find the vector between vertex A in the triangle and the camera origin
            vec3_t camera_ray = vec3_sub(vec3_new(0, 0, 0), vec3_from_vec4(transformed_vertices[0]));

            // Calculate how aligned the camera ray is with the face normal (using dot product)
            float dot_normal_camera = vec3_dot(face_normal, camera_ray);

            // Backface culling, bypassing triangles that are looking away from the camera
            if (dot_normal_camera < 0) {
                continue;
            }
        }
        
        // Create a polygon from the original transformed triangle to be clipped
        polygon_t polygon = polygon_from_triangle(
            vec3_from_vec4(transformed_vertices[0]),
            vec3_from_vec4(transformed_vertices[1]),
            vec3_from_vec4(transformed_vertices[2]),
            mesh_face.a_uv,
            mesh_face.b_uv,
            mesh_face.c_uv
        );
        
        // Clip the polygon and returns a new polygon with potential new vertices
        clip_polygon(&polygon);

        // Break the clipped polygon apart back into a list of triangles
        triangle_t triangles_after_clipping[MAX_NUM_POLY_TRIANGLES];
        int num_triangles_after_clipping = 0;

        triangles_from_polygon(&polygon, triangles_after_clipping, &num_triangles_after_clipping);

        // Loops all the assembled triangles after clipping
        for (int triangle_index = 0; triangle_index < num_triangles_after_clipping; triangle_index++) {
            triangle_t triangle_after_clipping = triangles_after_clipping[triangle_index];

            vec4_t projected_points[3];

            // Loop all three vertices to perform projection and conversion to screen space
            for (int v = 0; v < 3; v++) {
                // Project the current vertex using a perspective projection matrix
                projected_points[v] = mat4_mul_vec4(proj_matrix, triangle_after_clipping.points[v]);

                // Perform perspective divide
                if (projected_points[v].w != 0) {
                    projected_points[v].x /= projected_points[v].w;
                    projected_points[v].y /= projected_points[v].w;
                    projected_points[v].z /= projected_points[v].w;
                }

                // Flip vertically since the y values of the 3D mesh grow bottom->up and in screen space y values grow top->down
                projected_points[v].y *= -1;

                // Scale into the view
                projected_points[v].x *= (get_window_width() / 2.0);
                projected_points[v].y *= (get_window_height() / 2.0);

                // Translate the projected points to the middle of the screen
                projected_points[v].x += (get_window_width() / 2.0);
                projected_points[v].y += (get_window_height() / 2.0);
            }

            // Calculate the shade intensity based on how aliged is the normal with the flipped light direction ray
            float light_intensity_factor = -vec3_dot(face_normal, get_light_direction());

            // Calculate the triangle color based on the light angle
            uint32_t triangle_color = apply_light_intensity(mesh_face.color, light_intensity_factor);

            // Create the final projected triangle that will be rendered in screen space
            triangle_t triangle_to_render = {
                .points = {
                    { projected_points[0].x, projected_points[0].y, projected_points[0].z, projected_points[0].w },
                    { projected_points[1].x, projected_points[1].y, projected_points[1].z, projected_points[1].w },
                    { projected_points[2].x, projected_points[2].y, projected_points[2].z, projected_points[2].w }
                },
                .texcoords = {
                    { triangle_after_clipping.texcoords[0].u, triangle_after_clipping.texcoords[0].v },
                    { triangle_after_clipping.texcoords[1].u, triangle_after_clipping.texcoords[1].v },
                    { triangle_after_clipping.texcoords[2].u, triangle_after_clipping.texcoords[2].v }
                },
                .color = triangle_color,
                .texture = mesh->texture
            };

            // Save the projected triangle in the array of triangles to render
            if (triangles_to_render_count < MAX_TRIANGLES) {
                triangles_to_render[triangles_to_render_count++] = triangle_to_render;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Update function frame by frame with a fixed time step
///////////////////////////////////////////////////////////////////////////////
void update(void) {
    // Get a delta time factor converted to seconds to be used to update our game objects
    delta_time = (SDL_GetTicks() - previous_frame_time) / 1000.0;

    previous_frame_time = SDL_GetTicks();

    fps++;
    if (previous_frame_time - last_fps >= 1000) {
        //printf("FPS: %u\n", fps);
        fps = 0;
        last_fps = SDL_GetTicks();
    }

    // Initialize the counter of triangles to render for the current frame
    triangles_to_render_count = 0;

    // Loop all scene meshes
    for (int mesh_index = 0; mesh_index < get_num_meshes(); mesh_index++) {
        mesh_t* mesh = get_mesh(mesh_index);

        // Process graphics pipeline stages for each mesh
        process_graphics_pipeline_stages(mesh);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Render function to draw objects on the display
///////////////////////////////////////////////////////////////////////////////
void render(void) {
    // Clear all the arrays to get ready for the next frame
    clear_color_buffer(0xFF000000);
    clear_z_buffer();
    
    draw_grid();

    // Loop all triangles from the triangles_to_render array
    for (int i = 0; i < triangles_to_render_count; i++) {
        triangle_t triangle = triangles_to_render[i];

        // Draw filled triangle
        if (should_render_filled_triangle()) {
            vec4_t v0 = { triangle.points[0].x, triangle.points[0].y, triangle.points[0].z, triangle.points[0].w }; // vertex A
            vec4_t v1 = { triangle.points[1].x, triangle.points[1].y, triangle.points[1].z, triangle.points[1].w }; // vertex B
            vec4_t v2 = { triangle.points[2].x, triangle.points[2].y, triangle.points[2].z, triangle.points[2].w }; // vertex C
            draw_filled_triangle(&v0, &v1, &v2, triangle.color);
        }

        // Draw textured triangle
        if (should_render_textured_triangle()) {
            vec4_t v0 = { triangle.points[0].x, triangle.points[0].y, triangle.points[0].z, triangle.points[0].w }; // vertex A
            vec4_t v1 = { triangle.points[1].x, triangle.points[1].y, triangle.points[1].z, triangle.points[1].w }; // vertex B
            vec4_t v2 = { triangle.points[2].x, triangle.points[2].y, triangle.points[2].z, triangle.points[2].w }; // vertex C
            draw_textured_triangle(
                &v0, triangle.texcoords[0].u, triangle.texcoords[0].v,
                &v1, triangle.texcoords[1].u, triangle.texcoords[1].v,
                &v2, triangle.texcoords[2].u, triangle.texcoords[2].v,
                triangle.texture
            );
        }

        // Draw triangle wireframe
        if (should_render_wire()) {
            vec2_t v0 = { triangle.points[0].x, triangle.points[0].y }; // Vertex A
            vec2_t v1 = { triangle.points[1].x, triangle.points[1].y }; // Vertex B
            vec2_t v2 = { triangle.points[2].x, triangle.points[2].y }; // Vertex C
            draw_wire_triangle(&v0, &v1, &v2, 0xFFFFFFFF);
        }

        // Draw triangle vertex points
        if (should_render_wire_vertex()) {
            draw_rect(triangle.points[0].x - 3, triangle.points[0].y - 3, 6, 6, 0xFF0000FF); // vertex A
            draw_rect(triangle.points[1].x - 3, triangle.points[1].y - 3, 6, 6, 0xFF0000FF); // vertex B
            draw_rect(triangle.points[2].x - 3, triangle.points[2].y - 3, 6, 6, 0xFF0000FF); // vertex C
        }
    }

    // Finally draw the color buffer to the SDL window
    render_color_buffer();
}

///////////////////////////////////////////////////////////////////////////////
// Free the memory that was dynamically allocated by the program
///////////////////////////////////////////////////////////////////////////////
void free_resources(void) {
    free_meshes();
    destroy_window();
}

///////////////////////////////////////////////////////////////////////////////
// Main function
///////////////////////////////////////////////////////////////////////////////
int main(void) {
    is_running = init_window();

    setup();

    while (is_running) {
        process_input();
        update();
        render();
    }

    free_resources();

    return 0;
}
