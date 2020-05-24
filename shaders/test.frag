#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragPos;

void main() {
    float x_c = fragPos.x;
    float y_c = fragPos.y;
    float x_n = x_c;
    float y_n = y_c;
    float x_n1;
    float y_n1;
    int count = 0;
    for (; count < 20; count++) {
        x_n1 = x_n*x_n - y_n*y_n + y_c;
        y_n1 = 2*x_n*y_n + x_c;

        x_n = x_n1;
        y_n = y_n1;

        if (x_n1*x_n1 + y_n1*y_n1 > 10) {
            break;
        }
    }
    outColor = vec4(0.0, (x_n+1.0)/10.0, (y_n+1.0)/10.0, 1.0);
}
