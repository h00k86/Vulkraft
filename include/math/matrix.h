#pragma once

/* Column-major 4x4 matrix: m[col * 4 + row]. */
typedef struct { float m[16]; } mat4;

mat4 mat4_identity(void);
/* Orthographic projection with OpenGL depth range [-1, 1]. */
mat4 mat4_ortho(float left, float right, float bottom, float top, float near, float far);
/* Vulkan depth range [0, 1]: zBack maps to 1, zFront maps to 0. */
mat4 mat4_ortho_world_z(float left, float right, float bottom, float top,
                        float zBack, float zFront);
/* Add an offset to the translation components; not a general matrix product. */
mat4 mat4_translate(mat4 m, float x, float y, float z);
/* Centered scale of 1/zoom: values above 1 zoom out. zoom must be positive. */
mat4 mat4_scale_around(float zoom, float centerX, float centerY);
/* a * b: applies b first, then a. */
mat4 mat4_multiply(mat4 a, mat4 b);
mat4 mat4_translation(float x, float y, float z);
mat4 mat4_scaling(float x, float y, float z);
/* Right-handed rotations in degrees. */
mat4 mat4_rotation_x(float degrees);
mat4 mat4_rotation_y(float degrees);
mat4 mat4_rotation_z(float degrees);
/* Transform (x,y,z,1), without perspective division. */
void mat4_transform_point(const mat4 *m, const float in[3], float out[3]);
