// __constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;


float4 clampedValue(float4 color);
float4 gammaCorrect(float4 color, float gamma);
float getLuminance(float4 color);
float4 adjustColor(float4 color, float L, float Ld);

__kernel void tm_reinhard_extended( __global const float* red_channel,
                                    __global const float* green_channel, 
                                    __global const float* blue_channel,
                                    __write_only image2d_t outputImage,
                                    float exposure,
                                    float gamma,
                                    float Lwhite, 
                                    int width)
{
    // get the current position
    const int2 pos = {get_global_id(0), get_global_id(1)};
    int index = pos.x + width * pos.y;

    // read corresponding pixels
    float r = red_channel[index];
    float g = green_channel[index]; 
    float b = blue_channel[index]; 

    float4 color = {r, g, b, 1.0f};

    float Lw = getLuminance(color);
    float L = exposure * Lw;
    float Ld = (L * (1.0f + L / (Lwhite * Lwhite))) / (1.0f + L);
    color = Ld * color / Lw;
    color = clamp(color, 0.0f, 1.0f);
    color = gammaCorrect(color, gamma);

    // correct alpha
    color.w = 1.0f;

    color = color * 255.0f;

    // write back
    write_imageui(outputImage, pos, convert_uint4_sat(color));
}

float4 gammaCorrect(float4 color, float gamma)
{
    return native_powr(color, (float4)(1.f/gamma));
}

float getLuminance(float4 color)
{
    return 0.212671 * color.x + 0.71516 * color.y + 0.072169 * color.z;
}

//     // compute coordinates for opengl point coordinates
//     gl_Position = vec4(position.x*2-1, position.y*2-1, 0.0, 1.0);
//     uv = vec2(position.x, 1-position.y);

__kernel void float_to_uint_RGBA( __read_only image2d_t inputImage,
                                  __write_only image2d_t outputImage)
{
    // get the current position
    const int2 pos = {get_global_id(0), get_global_id(1)};

    // read corresponding pixel
    float4 color = read_imagef(inputImage, sampler, pos);

    // color saturation
    color = clamp(color, 0.0f, 1.0f);

    write_imageui(outputImage, pos, convert_uint4_sat(color));
}