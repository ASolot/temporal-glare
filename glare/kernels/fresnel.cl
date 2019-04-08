__constant const float PI = 3.14159265f;




// we are storing complex numbers in this matrix
__kernel void generate_complex_exp( __global float* output,
                               		float lambda, 
									float d,
									int width)
{
	// global id0 -> width, global id1 -> height
	int xp = get_global_id(0);
	int yp = get_global_id(1);
    int index = xp + yp * width; 
	index = 2*index;

	float value = PI / d / lambda * (xp*xp + yp*yp);

	output[index] = cos(value);
	output[index+1] = sin(value);
}

// we now take the image, the texture, and we multiply pointwise
__kernel void multiply_with_complex_exp(__read_only image2d_t inputImage, 
										__global const float* input_exponential,
										__global float* output,
										int width)
{
	// global id0 -> width, global id1 -> height
	int xp = get_global_id(0);
	int yp = get_global_id(1);
	const int2 pos = {xp, yp};

    int index = xp + yp * width; 
	index = 2*index;

	// we have info only on the 1st channel
	float4 colour = convert_float4(read_imageui(inputImage, sampler, pos));


	output[index] = input_exponential[index] * colour.x;
	output[index+1] = input_exponential[index+1] * colour.x;
}