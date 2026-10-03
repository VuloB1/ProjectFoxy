#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

// The checkerboard drawn behind transparent pictures in modo pixel. `cells` is how many
// squares fit across and down the item (the item is as big as the picture on screen, so
// the squares keep one size however far it is zoomed).
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 cells;
    vec4 colorA;
    vec4 colorB;
};

void main()
{
    vec2 c = floor(qt_TexCoord0 * cells);
    vec4 col = mix(colorA, colorB, mod(c.x + c.y, 2.0));
    fragColor = vec4(col.rgb * col.a, col.a) * qt_Opacity;
}
