#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

// The viewer's own sharpening (the button at the bottom right): FidelityFX CAS (AMD, MIT), applied to what is on the
// screen - the picture as it is drawn at the current zoom - not to the picture's own pixels. It is the same filter as
// core/edit/Cas.h and Detail.frag; here `strength` is the 0..100 % slider as 0..1. The window layer it reads is
// premultiplied, and so is the maths (a transparent neighbour counts for nothing).
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float strength;
    vec2 texel; // 1 / the layer's size in pixels
};

layout(binding = 1) uniform sampler2D source;

vec3 cas(vec3 a, vec3 b, vec3 c, vec3 d, vec3 e, vec3 f, vec3 g, vec3 h, vec3 i, float s)
{
    vec3 mn = min(min(min(d, e), f), min(b, h));
    mn += min(min(min(mn, a), c), min(g, i));
    vec3 mx = max(max(max(d, e), f), max(b, h));
    mx += max(max(max(mx, a), c), max(g, i));
    vec3 amp = vec3(0.0);
    for (int k = 0; k < 3; ++k)
        if (mx[k] > 0.0)
            amp[k] = clamp(min(mn[k], 2.0 - mx[k]) / mx[k], 0.0, 1.0);
    amp = sqrt(amp);
    vec3 w = -amp / (8.0 + (5.0 - 8.0) * s);
    vec3 sharpened = clamp(((b + d + f + h) * w + e) / (1.0 + 4.0 * w), 0.0, 1.0);
    return e + s * (sharpened - e);
}

void main()
{
    vec4 center = texture(source, qt_TexCoord0);
    if (strength <= 0.0001) {
        fragColor = center * qt_Opacity;
        return;
    }
    vec2 t = texel;
    vec3 rgb = cas(texture(source, qt_TexCoord0 + vec2(-t.x, -t.y)).rgb, texture(source, qt_TexCoord0 + vec2(0.0, -t.y)).rgb,
                   texture(source, qt_TexCoord0 + vec2(t.x, -t.y)).rgb,  texture(source, qt_TexCoord0 + vec2(-t.x, 0.0)).rgb,
                   center.rgb,                                           texture(source, qt_TexCoord0 + vec2(t.x, 0.0)).rgb,
                   texture(source, qt_TexCoord0 + vec2(-t.x, t.y)).rgb,  texture(source, qt_TexCoord0 + vec2(0.0, t.y)).rgb,
                   texture(source, qt_TexCoord0 + vec2(t.x, t.y)).rgb, strength);
    // a premultiplied colour cannot exceed its alpha
    fragColor = vec4(min(rgb, vec3(center.a)), center.a) * qt_Opacity;
}
