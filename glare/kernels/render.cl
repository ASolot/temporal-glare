const sampler_t sampler = CLK_ADDRESS_CLAMP | CLK_NORMALIZED_COORDS_FALSE |
                       CLK_FILTER_LINEAR;

constant uint4 blackColor = {0, 0, 0, 255};
constant uint4 whiteColor = {255, 255, 255, 255};

__kernel void glr_render_pupil(__write_only image2d_t outputImage,
                               float radius, 
                               int width, 
                               int height,
                               float2 ctr)

{
    const int2 pos = {get_global_id(0), get_global_id(1)}; 
    // if(distance(ctr, convert_float2(pos)) > radius)
    if(pown(ctr.x - pos.x, 2) + pown(ctr.y - pos.y, 2) > pown(radius,2) )
        write_imageui(outputImage, pos, blackColor);
    else
        write_imageui(outputImage, pos, whiteColor);
}


// this kernel puts a white spot in the middle of a grating texture
__kernel void glr_render_gratings(__read_only image2d_t inputImage,
                                  __write_only image2d_t outputImage,
                                  float radius, 
                                  int width, 
                                  int height, 
                                  float2 ctr)
{
    const int2 pos = {get_global_id(0), get_global_id(1)}; 
    if(pown(ctr.x - pos.x, 2) + pown(ctr.y - pos.y, 2) > pown(radius,2) )
    {
        uint4 color = read_imageui(inputImage, sampler, pos);
        write_imageui(outputImage, pos, color);
    }
    else
        write_imageui(outputImage, pos, whiteColor);
}


// we distort the original points coordinates based on 
// the variation of the lens size
__kernel void glr_render_lens_points(__global const float* points, 
                                     __write_only image2d_t outputImage, 
                                     int width, 
                                     int height, 
                                     float distort )
{
    int index = get_global_id(0);
    index = index * 4;

    float xnew = points[index] + distort * points[index+1];
    xnew = xnew * width/2.0f + width/2.0f;

    float ynew = points[index+2] - distort * points[index+3];
    ynew = ynew * height/2.0f + height/2.0f;

    int2 pos; 

    int i, j;
    for(i = -1; i <=1; i++)
        for(j = -1; j<=1; j++)
        {
            pos = (int2){(int)(xnew+i), (int)(ynew+j)};
            write_imageui(outputImage, pos, whiteColor);
        }
}

// merge images 
// __kernel void glr_merge_images(__read_only image2d_t inputImage1,
//                                 __read_only image2d_t inputImage2,
//                                 __read_only image2d_t inputImage3,
//                                 __write_only image2d_t outputImage)
// {
//     const int2 pos = {get_global_id(0), get_global_id(1)}; 
    
// }
