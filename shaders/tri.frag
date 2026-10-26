#version 450
layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in flat int fragTextureID;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 1) uniform sampler2D texSamplers[16];
float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}
void main() {
    if (fragTextureID < 0) { outColor = fragColor; return; }
    if (fragTextureID == 0) {
        vec3 msdf = texture(texSamplers[0], fragUV).rgb;
        float sigDist = median(msdf.r, msdf.g, msdf.b) - 0.5;
        float alpha = clamp(sigDist / max(fwidth(sigDist), 0.0001) + 0.5, 0.0, 1.0);
        outColor = vec4(fragColor.rgb, fragColor.a * alpha);
        return;
    }
    vec4 sampled;
    switch (fragTextureID) {
    case 1: sampled = texture(texSamplers[1], fragUV); break;
    case 2: sampled = texture(texSamplers[2], fragUV); break;
    case 3: sampled = texture(texSamplers[3], fragUV); break;
    case 4: sampled = texture(texSamplers[4], fragUV); break;
    case 5: sampled = texture(texSamplers[5], fragUV); break;
    case 6: sampled = texture(texSamplers[6], fragUV); break;
    case 7: sampled = texture(texSamplers[7], fragUV); break;
    case 8: sampled = texture(texSamplers[8], fragUV); break;
    case 9: sampled = texture(texSamplers[9], fragUV); break;
    case 10: sampled = texture(texSamplers[10], fragUV); break;
    case 11: sampled = texture(texSamplers[11], fragUV); break;
    case 12: sampled = texture(texSamplers[12], fragUV); break;
    case 13: sampled = texture(texSamplers[13], fragUV); break;
    case 14: sampled = texture(texSamplers[14], fragUV); break;
    case 15: sampled = texture(texSamplers[15], fragUV); break;
    default: sampled = vec4(1.0); break;
    }
    outColor = sampled * fragColor;
}
