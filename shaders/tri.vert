#version 450
layout(set = 0, binding = 0) uniform CameraData { mat4 projection; mat4 view; } camera;
layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inUV;
layout(location = 4) in int inTextureID;
layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;
layout(location = 2) out flat int fragTextureID;
void main() {
    gl_Position = camera.projection * camera.view * vec4(inPosition, 1.0);
    fragUV = inUV;
    fragColor = inColor;
    fragTextureID = inTextureID;
}
