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

	float value = PI / (d * d * d * lambda * lambda * lambda) * (xp*xp + yp*yp);

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
	float4 colour = convert_float4(read_imageui(inputImage, sampler, pos)) / 255.0f;


	output[index] = input_exponential[index] * colour.x;
	output[index+1] = input_exponential[index+1] * colour.x;
}

__kernel void spectral_blur(__global const float* inputImage, 
							__global float* outputRed,
							__global float* outputGreen, 
							__global float* outputBlue,
							__global float* outputMono,
							__global const float* spectrumMapping, 
							int width,
							float lambda, 
							float distance
							)
{
	// global id0 -> width, global id1 -> height
	int xp = get_global_id(0);
	int yp = get_global_id(1);
	const int2 pos = {xp, yp};

    int indexOutput = xp + yp * width; 
	int indexInput = indexOutput * 2;
	float K = lambda*lambda*distance*distance;
	// K = 1.0f;

	// modulus and norm
	outputMono[indexOutput] = inputImage[indexInput]*inputImage[indexInput] + inputImage[indexInput+1]*inputImage[indexInput+1];
	outputMono[indexOutput] /= K;

	// float4 color = (float4)(0, 0, 0, 255);

	// if (pos.x < width / 2) { pos.x = width - pos.x; pos.y = height - pos.y; }

	// for (size_t t = 0; t < samples; ++t)
	// {
    // 	float wavelength = (float)t / samples;
	// 	float dx = (float)(pos.x + BLUR * (rand(&prng) - 0.5f)) / width;
	// 	float dy = (float)(pos.y + BLUR * (rand(&prng) - 0.5f)) / height;
	// 	dx -= 0.5f; dy -= 0.5f;

	// 	float sx = dx * ((wavelength * 400 + 390) / LAMBDA);
	// 	float sy = dy * ((wavelength * 400 + 390) / LAMBDA);

	// 	float r = (rand(&prng) > 0.5f) ? 1.0f : -1.0f;
	// 	float angle = r * (1.0f - pow(rand(&prng), RINGING)) * RADIAN(ROTATE);

	// 	float rx = sx, ry = sy;
	// 	sx = rx * cos(angle) + ry * sin(angle);
	// 	sy = ry * cos(angle) - rx * sin(angle);

	// 	sx += 0.5f; sy += 0.5f;
	// 	float intensity = read_imagef(spectrum, sampler, (float2)(sx, sy)).x;
	// 	float4 rgba = read_imagef(spectrum, sampler, (float2)(wavelength, 0)).xyz;
	// 	color += rgba * intensity;
	// }

	// write_imageui(outputImage, pos, convert_uint4_sat(color))
}


__kernel void conv_of_ffts(	__global const float* input2,
							__global const float* input3, 
							__global float* output1, 
							int width)
{
	int xp = get_global_id(0);
	int yp = get_global_id(1);

	int index = (xp + yp*width)*2;


	// x1 + y1*j = (x2 + y2*j)(x3 + y3*j)
	//			 = x2*x3 + x2*y3*j + y2*x3*j -y2*y3
	// x1 = x2*x3 - y2*y3
	// y1 = x2*y3 + y2*x3
	// where x1 = output[index], y1 = output[index+1] etc. 

	// x1 		   =     x2        *     x3        -     y2          *     y3
	output1[index] = input2[index] * input3[index] - input2[index+1] * input3[index+1];

	// y1            =     x2        *     y3          +     y2          *     x3 
	output1[index+1] = input2[index] * input3[index+1] + input2[index+1] * input3[index];


}