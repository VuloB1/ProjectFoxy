#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

// A thin grid between the picture's own pixels (modo pixel, zoomed in far enough that
// every pixel is a block on screen). `pixels` is the picture's size in pixels and
// `lineWidth` one screen pixel expressed in picture pixels, so the lines stay thin.
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 pixels;
    float lineWidth;
    vec4 lineColor;
};

void main()
{
    vec2 g = qt_TexCoord0 * pixels;
    vec2 d = abs(fract(g + 0.5) - 0.5); // distance to the nearest pixel border, in pixels
    float ax = 1.0 - smoothstep(0.0, lineWidth, d.x);
    float ay = 1.0 - smoothstep(0.0, lineWidth, d.y);
    float a = max(ax, ay) * lineColor.a;
    fragColor = vec4(lineColor.rgb * a, a) * qt_Opacity;
}
