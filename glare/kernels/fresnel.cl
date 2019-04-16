__constant const float PI = 3.14159265f;

__constant const float RINGING = 1.25f;
__constant const float ROTATE  = 2.75f;
__constant const float BLUR    = 3.5f;

const sampler_t fsampler = CLK_ADDRESS_CLAMP | CLK_NORMALIZED_COORDS_TRUE |
                       CLK_FILTER_LINEAR;

#define RADIAN(x) (x * (PI / 180.0f))

#define ROUNDS 4

void renew(ulong4 *state, ulong4 seed)
{
    /* Retain the PRNG's state. */
    ulong4 block = *state + seed;

    #pragma unroll
    for (ulong t = 0; t < ROUNDS; t++)
    {
        /* Round 2t + 0 (×4 mix & permutation). */
        block.lo += block.hi; block.hi = rotate(block.hi, (ulong2)(14, 16));
        block.hi ^= block.lo; block = block.xywz;
        block.lo += block.hi; block.hi = rotate(block.hi, (ulong2)(52, 57));
        block.hi ^= block.lo; block = block.xywz;
        block.lo += block.hi; block.hi = rotate(block.hi, (ulong2)(23, 40));
        block.hi ^= block.lo; block = block.xywz;
        block.lo += block.hi; block.hi = rotate(block.hi, (ulong2)( 5, 37));
        block.hi ^= block.lo; block = block.xywz;

        /* Key addition. */
        block += seed;

        /* Round 2t + 1 (×4 mix & permutation). */
        block.lo += block.hi; block.hi = rotate(block.hi, (ulong2)(25, 33));
        block.hi ^= block.lo; block = block.xywz;
        block.lo += block.hi; block.hi = rotate(block.hi, (ulong2)(46, 12));
        block.hi ^= block.lo; block = block.xywz;
        block.lo += block.hi; block.hi = rotate(block.hi, (ulong2)(58, 22));
        block.hi ^= block.lo; block = block.xywz;
        block.lo += block.hi; block.hi = rotate(block.hi, (ulong2)(32, 32));
        block.hi ^= block.lo; block = block.xywz;

        /* Key addition. */
        block += seed;
    }

    /* Feed-forward. */
    *state ^= block;
}

/** @struct PRNG
  * @brief PRNG internal state.
  *
  * This structure contains an instance of PRNG, which is enough information to
  * generate essentially infinitely many unbiased pseudorandom numbers.
**/
typedef struct PRNG
{
    /** @brief The 256-bit internal state. **/
    ulong4 state;
    /** @brief An integer indicating how much of the state has been used. **/
    uint pointer;
    /** @brief A pointer to the PRNG's seed, common to all instances. **/
    ulong4 seed;
} PRNG;

/** This function creates a new PRNG instance, and initializes it to zero.
  * @param ID The ID to create the PRNG instance with, must be unique.
  * @param seed A pointer to the PRNG's seed.
  * @returns The PRNG instance, ready for use.
**/
PRNG init(ulong ID, ulong seed)
{
    PRNG instance;
    instance.state = (ulong4)(ID);
    instance.pointer = 0;
    instance.seed = (ulong4)(seed, 0, 0, 0);
    return instance;
}

/* This function will return a uniform pseudorandom number in [0..1). */
/** This function returns a uniform pseudorandom number in [0..1).
  * @param prng A pointer to the PRNG instance to use.
  * @returns An unbiased uniform pseudorandom number between 0 and 1 exclusive.
**/
float rand(PRNG *prng)
{
    /* Do we need to renew? */
    if (prng->pointer == 0)
    {
         renew(&prng->state, prng->seed);
         prng->pointer = 4;
    }

    /* Return a uniform number in the desired interval. */
    --prng->pointer;
    if (prng->pointer == 3) return (float)(prng->state.w);
    if (prng->pointer == 2) return (float)(prng->state.z);
    if (prng->pointer == 1) return (float)(prng->state.y);
    return (float)(prng->state.x);
}



// we are storing complex numbers in this matrix
__kernel void generate_complex_exp( __global float* output,
                               		float lambda, 
									float d,
									int width, 
									int height,
									float resolution)	// px / mm
{
	// global id0 -> width, global id1 -> height
	int xi = get_global_id(0);
	int yi = get_global_id(1);
    int index = xi + yi * width; 
	index = 2*index;

	// center the coordinates to get a proper fresnel term
 
	// float xp = ((float)xi/width - 0.5f ) / resolution / (1.0)  / d / lambda;	// mm 
	// float yp = ((float)yi/height- 0.5f ) / resolution / (1.0)  / d / lambda;	// mm

	// xp = xp * 2.25;
	// yp = yp * 2.25;	


	// rad / mm^2 * px^2 / px^2 * mm^2 => rad 
	float value = PI / (d * lambda) * (xp*xp + yp*yp); // (d * d * lambda * lambda);

	output[index]  	= cos(value);
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
	// index = 2*index;

	// we have info only on the 1st channel
	// float4 colour = convert_float4(read_imageui(inputImage, sampler, pos)) / 255.0f;
	uint4 colour = read_imageui(inputImage, sampler, pos);

	float p = 0;

	if (colour.x > 200)
		p = 1;
	
	p = colour.x / 255.0f;

	// fresnel (multiplication with complex exp)
	output[index] = input_exponential[index] * p;
	output[index+1] = input_exponential[index+1] * p;

}

__kernel void compute_magnitude_kernel(	__global float* inputImage,
							   	__write_only image2d_t outputImage,
								__global float* outputMono,
							   	int width,
								int height,
								float lambda, 
								float distance
							)
{
	int xp = get_global_id(0);
	int yp = get_global_id(1);
	const int2 pos = {xp, yp};

	int indexOutput = xp + yp * width; 
	int indexInput = indexOutput * 2;

	float K = lambda*lambda*distance*distance;

	float color = inputImage[indexInput]*inputImage[indexInput] + inputImage[indexInput+1]*inputImage[indexInput+1];
	
	// normalisation as part of Fresnel equation
	color = sqrt(color)/K;

	// fft normalisation
	color = color / width / height;


	if (indexOutput < width*height)
		outputMono[indexOutput] = color;

	write_imagef(outputImage, pos, (float4){color, color, color, 1.0f});
}

__kernel void spectral_blur(__read_only image2d_t inputImage, 
							__global float* outputRed,
							__global float* outputGreen, 
							__global float* outputBlue,
							__global const float* spectrumMapping, 
							int width,
							int height,
							float lambda, 
							float distance,
							float normFactor
							)
{
	// global id0 -> width, global id1 -> height
	int xp = get_global_id(0);
	int yp = get_global_id(1);
	const int2 pos = {xp, yp};

	int indexOutput = xp + yp * width; 

	ulong seed = 0;
	PRNG prng = init(indexOutput, seed);



	// int samples = 32; 
	// float3 color = (float3)(0, 0, 0);

	// // if (pos.x < width / 2) { pos.x = width - pos.x; pos.y = height - pos.y; }


	// for (size_t i = 0; i < samples; ++i)
	// {
    // 	float wavelength = (float)i / samples;
	// 	wavelength = (380 + wavelength * 390); // from lambda_i formula 


	// 	//TODO: add some blur and random noise
	// 	float x = (float)(pos.x) / width;
	// 	float y = (float)(pos.y) / height;
	// 	x -= 0.5f; y -= 0.5f;

	// 	float xi = x * lambda / wavelength;
	// 	float yi = y * lambda / wavelength;

	// 	// float r = (rand(&prng) > 0.5f) ? 1.0f : -1.0f;
	// 	// float angle = r * (1.0f - pow(rand(&prng), RINGING)) * RADIAN(ROTATE);

	// 	// float rx = xi;
	// 	// float ry = yi;

	// 	// xi = rx * cos(angle) + ry * sin(angle);
	// 	// yi = ry * cos(angle) - rx * sin(angle);

	// 	xi = xi + x;
	// 	yi = yi + y;

	// 	xi += 0.5f; yi += 0.5f;
		
		
	// 	float intensity = read_imagef(inputImage, fsampler, (float2)(xi, yi)).x;
	//  	intensity = intensity / normFactor;
		
	// 	int idx = (wavelength - 380);
		
	// 	float X = spectrumMapping[idx*3];
	// 	float Y = spectrumMapping[idx*3+1];
	// 	float Z = spectrumMapping[idx*3+2];

	// 	// XYZ to sRGB
	// 	float R =  3.2404542*X - 1.5371385*Y - 0.4985314*Z;
	// 	float G = -0.9692660*X + 1.8760108*Y + 0.0415560*Z;
	// 	float B =  0.0556434*X - 0.2040259*Y + 1.0572252*Z;


	// 	// TODO: clamp it if it's the case

	// 	float3 rgb = {R, G, B};
		
	// 	color += rgb * intensity;
	// }
	
	float4 color = read_imagef(inputImage, sampler, pos);

	outputRed[indexOutput] 	= color.x / normFactor;
	outputGreen[indexOutput]= color.y / normFactor;
	outputBlue[indexOutput] = color.z / normFactor;

	// outputRed[indexOutput]  = color.x / samples;
	// outputGreen[indexOutput]= color.y / samples; 
	// outputBlue[indexOutput] = color.z / samples; 

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

