__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

__kernel void tm_reinhard_extended(__read_only image2d_t inputImage,
                                    __write_only image2d_t outputImage,
                                    __constant float exposure,
                                    __constant float gamma,
                                    __constant float Lwhite,
                                    int imageWidth)
{
    // get the current position
    const int2 pos = {get_global_id(0), get_global_id(1)};

    // read corresponding pixel
    float4 fColor = read_imagef(inputImage, sampler, pos);

    // do the trick 
    fColor = exposure * fColor;
    float L = getLuminance(fColor);
    float Ld = (L * (1.0 + L / (Lwhite * Lwhite))) / (1.0 + L);
    fColor = adjustColor(fColor, L, Ld);
    // fColor = clampedValue(fColor);
    fColor = gammaCorrect(fColor, gamma);

    // color saturation
    fColor = clamp(fColor, 0.0, 1.0);

    // write back
    write_imagef(inputImage, pos, fColor);
}

float4 clampedValue(float4 fColor)
{
    fColor.x = 1.0;
    return clamp(fColor, 0.0, 1.0);
}

float4 gammaCorrect(float4 fColor, float gamma)
{
    return pow(fColor, vec4(1.0/gamma));
}

float getLuminance(float4 fColor)
{
    return 0.212671 * fColor.x + 0.71516 * fColor.y + 0.072169 * fColor.z;
}

float4 adjustColor(float4 color, float L, float Ld)
{
    return Ld * color / L;
}

//     // compute coordinates for opengl point coordinates
//     gl_Position = vec4(position.x*2-1, position.y*2-1, 0.0, 1.0);
//     uv = vec2(position.x, 1-position.y);
