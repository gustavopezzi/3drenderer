#include <stdio.h>
#include <string.h>
#include "array.h"
#include "mesh.h"

#define MAX_NUM_MESHES 100
static mesh_t meshes[MAX_NUM_MESHES];
static int mesh_count = 0;

void load_mesh_obj_data(mesh_t* mesh, char* obj_filename) {
    FILE* file = fopen(obj_filename, "r");
    char line[1024];
    tex2_t* texcoords = NULL;

    while (fgets(line, 1024, file)) {
        // Vertex information
        if (strncmp(line, "v ", 2) == 0) {
            vec3_t vertex;
            sscanf(line, "v %f %f %f", &vertex.x, &vertex.y, &vertex.z);
            array_push(meshes[mesh_count].vertices, vertex);
        }
        // Texture coordinate information
        else if (strncmp(line, "vt ", 3) == 0) {
            tex2_t texcoord;
            sscanf(line, "vt %f %f", &texcoord.u, &texcoord.v);
            array_push(texcoords, texcoord);
        }
        // Face information
        else if (strncmp(line, "f ", 2) == 0) {
            int v[4], vt[4], vn[4];
            int count = sscanf(
                line, "f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d",
                &v[0], &vt[0], &vn[0],
                &v[1], &vt[1], &vn[1],
                &v[2], &vt[2], &vn[2],
                &v[3], &vt[3], &vn[3]
            );

            if (count == 9) {
                // Triangle
                face_t face = {
                    .a = v[0],
                    .b = v[1],
                    .c = v[2],
                    .a_uv = texcoords[vt[0] - 1],
                    .b_uv = texcoords[vt[1] - 1],
                    .c_uv = texcoords[vt[2] - 1],
                    .color = 0xFFFFFFFF
                };
                array_push(meshes[mesh_count].faces, face);
            } else if (count == 12) {
                // Quad split into two triangles: [0,1,2] and [0,2,3]
                face_t face1 = {
                    .a = v[0],
                    .b = v[1],
                    .c = v[2],
                    .a_uv = texcoords[vt[0] - 1],
                    .b_uv = texcoords[vt[1] - 1],
                    .c_uv = texcoords[vt[2] - 1],
                    .color = 0xFFFFFFFF
                };
                face_t face2 = {
                    .a = v[0],
                    .b = v[2],
                    .c = v[3],
                    .a_uv = texcoords[vt[0] - 1],
                    .b_uv = texcoords[vt[2] - 1],
                    .c_uv = texcoords[vt[3] - 1],
                    .color = 0xFFFFFFFF
                };
                array_push(meshes[mesh_count].faces, face1);
                array_push(meshes[mesh_count].faces, face2);
            }
        }
    }
    array_free(texcoords);
    fclose(file);
}

void load_mesh_png_data(mesh_t* mesh, char* png_filename) {
    upng_t* png_image = upng_new_from_file(png_filename);
    if (png_image != NULL) {
        upng_decode(png_image);
        if (upng_get_error(png_image) == UPNG_EOK) {
            mesh->texture = png_image;
        }
    }
}

void load_mesh(char* obj_filename, char* png_filename, vec3_t scale, vec3_t translation, vec3_t rotation) {
    load_mesh_obj_data(&meshes[mesh_count], obj_filename);
    load_mesh_png_data(&meshes[mesh_count], png_filename);

    meshes[mesh_count].scale = scale;
    meshes[mesh_count].translation = translation;
    meshes[mesh_count].rotation = rotation;

    mesh_count++;
}

mesh_t* get_mesh(int mesh_index) {
    return &meshes[mesh_index];
}

int get_num_meshes(void) {
    return mesh_count;
}

inline void rotate_mesh_x(int mesh_index, float angle) {
    meshes[mesh_index].rotation.x += angle;
}

inline void rotate_mesh_y(int mesh_index, float angle) {
    meshes[mesh_index].rotation.y += angle;
}

inline void rotate_mesh_z(int mesh_index, float angle) {
    meshes[mesh_index].rotation.z += angle;
}

void free_meshes(void) {
    for (int i = 0; i < mesh_count; i++) {
        array_free(meshes[i].faces);
        array_free(meshes[i].vertices);
        upng_free(meshes[i].texture);
    }
}
