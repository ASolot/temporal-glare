#include <QPainter>
#include <QPaintEvent>
#include <QWidget>
#include <QImage>
#include <iostream>
#include <fstream>
#include <QtWidgets>
#include <string>
#include <assert.h>

#include "TemporalGlareRenderer.h"

#include "ocl_utils.hpp"
#include "spectrumMap.h"


// Auto adjust exposure with a key value proposed in
// "Perceptual Effects in Real-time Tone Mapping" by Krawczyk et al.

TemporalGlareRenderer::TemporalGlareRenderer() :
	m_imgWidth(0), m_imgHeight(0), m_maxPupilSize(9.0f), ncols(0), nrows(0), 
    m_pupilRadiusPx(0), m_fieldLuminance(0.5), m_nPoints(2000), 
    m_lambda(575.0f/1000.0f/1000.0f), m_distance(20), m_gamma(5.0f), m_alpha(1.0f),
    m_Lwhite(5.0f), m_autoExposure(true), m_autoExposureValue(1.0f), m_distort(0.0f),
    m_slidRadiusDeformedPx(0), m_slidRadiusPx(0)
{
    m_apertureTexture = nullptr;
    m_slidTexture = nullptr;

    m_imageChanged = false;

    float tmp = (m_alpha - 0.5f) * 20.f;
    m_exposure = std::pow(2.f, tmp);

    initOpenCL();
    
    srand (time(NULL));
}

void TemporalGlareRenderer::updatePupilDiameter()
{
    // TODO: limit variation based on time 
    // TODO: auto link m_fieldLuminance with the LWhite from the HDR image
    float p = 4.9 - 3*tanh(0.4 * (log(m_fieldLuminance) + 1));
    m_apperture = p + noise() * m_maxPupilSize / p * sqrt( 1 - p / m_maxPupilSize);
    m_pupilRadiusPx = (float)m_imgHeight / m_maxPupilSize * m_apperture / 2.0f;
    // m_pupilRadiusPx = (float)m_imgHeight / 2.0f * m_apperture / m_maxPupilSize;
    m_slidRadiusPx  = (float)m_imgHeight / m_maxPupilSize * 3.7f / 2.0f ;
}

void TemporalGlareRenderer::updateLensDeformation()
{
    // TODO: Smooth out the noise function 
    m_distort = noise() * 100;
    m_slidRadiusDeformedPx = m_slidRadiusPx + m_distort * deformationCoeff(2*m_slidRadiusPx/m_imgHeight);
}

void TemporalGlareRenderer::updateApertureTexture()
{
    // if image is changed, we just realloc another apperture size
    if(m_imageChanged == true)
    {
        if (m_apertureTexture != nullptr)
            delete [] m_apertureTexture;

        m_apertureTexture = new float [m_imgHeight * m_imgWidth];
        m_pupilCenter = { m_imgWidth/2.0f, m_imgHeight/2.0f};
        m_apperture = m_maxPupilSize;

        m_imageChanged = false;

        // generate the complex exponential
        int test_size = m_imgWidth * m_imgHeight * 2;
        m_complexExponential = new float[test_size];
        m_complexAperture = new float[test_size];

        // We compute E for a specific m_lambda
        cl::Buffer buffer_complex(context, CL_MEM_READ_WRITE, sizeof(float) * test_size);

        compExpKernel.setArg(0, buffer_complex);
        compExpKernel.setArg(1, m_lambda);                              // mm
        compExpKernel.setArg(2, m_distance);                            // mm
        compExpKernel.setArg(3, m_imgWidth);                            // px
        compExpKernel.setArg(4, m_imgHeight);
        compExpKernel.setArg(5, (float)m_imgHeight / m_maxPupilSize);   // px / mm

        queue.enqueueNDRangeKernel(
            compExpKernel, 
            cl::NullRange, 
            cl::NDRange(m_imgWidth, m_imgHeight, 1), 
            cl::NullRange
        );
        queue.finish();

		queue.enqueueReadBuffer(buffer_complex, CL_TRUE, 0, sizeof(float) * test_size, m_complexExponential);
		queue.finish();

    }
   
}


void TemporalGlareRenderer::paint(QPainter *painter, QPaintEvent *event, int elapsed, const QSize &destSize)
{
    if( image == nullptr ) // No HDR image available
        return;

    unsigned char* data = NULL;
    float* raw = NULL;
    float* magnitude = NULL; 
    float* r = NULL;
    float* g = NULL;
    float* b = NULL;

    float normFactor = 1.0f;
    try {
        
        // TODO Remove all additional buffers
        // TODO Check the spectral blur 
        // TODO Fix PSF
        // TODO Smooth particle random movement
        magnitude = new float[m_imgWidth * m_imgHeight];
        raw  = new float[m_imgWidth*m_imgHeight*4];
        data = new unsigned char[m_imgWidth*m_imgHeight*4];
        memset(data, 255, m_imgWidth * m_imgHeight * 4);

        cl::size_t<3> origin;
        origin[0] = 0; origin[1] = 0, origin[2] = 0;
        cl::size_t<3> region;
        region[0] = m_imgWidth; region[1] = m_imgHeight; region[2] = 1;


        std::cout<<"Frame Updated\n";

        // make the neccessary updates 
        updateApertureTexture();
        updatePupilDiameter(); 
        updateLensDeformation();

        //STEP: GENERATING THE PUPIL
        cl::Image2D pupilBuffer(context, 
                    CL_MEM_READ_WRITE, 
                    cl::ImageFormat(CL_RGBA, CL_UNSIGNED_INT8),
                    m_imgWidth,
                    m_imgHeight,
                    0,
                    NULL);

        
        pupilKernel = cl::Kernel(program, "glr_render_pupil");
        pupilKernel.setArg(0, pupilBuffer);
        pupilKernel.setArg(1, m_pupilRadiusPx);
        pupilKernel.setArg(2, m_imgWidth);
        pupilKernel.setArg(3, m_imgHeight);
        pupilKernel.setArg(4, m_pupilCenter);

        queue.enqueueNDRangeKernel(
            pupilKernel, 
            cl::NullRange, 
            cl::NDRange(m_imgWidth, m_imgHeight, 1), 
            cl::NullRange
        );
        queue.finish();

        // queue.enqueueReadImage(pupilBuffer, CL_TRUE, origin, region, 0, 0 , data,  NULL, NULL);
        // queue.finish();

        //STEP: GRATINGS RENDERING 
        cl::Image2D slidBufferIn(context, 
                    CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
                    cl::ImageFormat(CL_RGBA, CL_UNSIGNED_INT8),
                    m_slidWidth,
                    m_slidHeight,
                    0,
                    m_slidTexture);

        cl::Image2D slidBufferOut(context, 
                    CL_MEM_READ_WRITE, 
                    cl::ImageFormat(CL_RGBA, CL_UNSIGNED_INT8),
                    m_slidWidth,
                    m_slidHeight,
                    0,
                    NULL);

        gratingsKernel = cl::Kernel(program, "glr_render_gratings");
        gratingsKernel.setArg(0, slidBufferIn);
        gratingsKernel.setArg(1, slidBufferOut);
        gratingsKernel.setArg(2, m_slidRadiusDeformedPx);
        gratingsKernel.setArg(3, m_imgWidth);
        gratingsKernel.setArg(4, m_imgHeight);
        gratingsKernel.setArg(5, m_pupilCenter);

        queue.enqueueNDRangeKernel(
            gratingsKernel, 
            cl::NullRange, 
            cl::NDRange(m_imgWidth, m_imgHeight, 1), 
            cl::NullRange
        );
        queue.finish();
        
        //STEP: GENERATE LENS POINTS  
        cl::Buffer coordinatesBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * m_nPoints * 4);
        queue.enqueueWriteBuffer(coordinatesBuffer, CL_TRUE, 0, sizeof(float) * m_nPoints * 4, m_pointCoordinates);
        
        cl::Buffer pointsBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned char) * m_imgWidth * m_imgHeight * 4);
        queue.enqueueWriteBuffer(pointsBuffer, CL_TRUE, 0, sizeof(unsigned char) * m_imgWidth * m_imgHeight * 4, data);

        queue.finish();

        lensDotsKernel = cl::Kernel(program, "glr_render_lens_points");

        lensDotsKernel.setArg(0,coordinatesBuffer);
        lensDotsKernel.setArg(1,pointsBuffer);
        lensDotsKernel.setArg(2,m_imgWidth);
        lensDotsKernel.setArg(3,m_imgHeight);
        lensDotsKernel.setArg(4,m_distort); // distort coefficient -> how the lens is deformed (pixels)

        queue.finish();

        // we take each point, skew it based on the lens distort
        // and we draw it
        queue.enqueueNDRangeKernel(
            lensDotsKernel, 
            cl::NullRange, 
            cl::NDRange(m_nPoints, 1, 1), 
            cl::NullRange
        );
        queue.finish();

        // put the poins back in the data buffer
        queue.enqueueReadBuffer(pointsBuffer, CL_TRUE, 0, sizeof(unsigned char) * m_imgWidth * m_imgHeight * 4, data);
        queue.finish();

        //STEP: MERGE PUPIL-RELATED IMAGES 
        cl::Image2D mergeBufferOut(context, 
                    CL_MEM_READ_WRITE, 
                    cl::ImageFormat(CL_RGBA, CL_UNSIGNED_INT8),
                    m_slidWidth,
                    m_slidHeight,
                    0,
                    NULL);

        cl::Image2D pointsBufferOut(context, 
                    CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
                    cl::ImageFormat(CL_RGBA, CL_UNSIGNED_INT8),
                    m_imgWidth,
                    m_imgHeight,
                    0,
                    data);
        
        // first slid, second pupil, third particles
        mergeKernel = cl::Kernel(program, "glr_merge_images");
        mergeKernel.setArg(0, slidBufferOut);
        mergeKernel.setArg(1, pupilBuffer);
        mergeKernel.setArg(2, pointsBufferOut);
        mergeKernel.setArg(3, mergeBufferOut);

        queue.enqueueNDRangeKernel(
            mergeKernel, 
            cl::NullRange, 
            cl::NDRange(m_imgWidth, m_imgHeight, 1), 
            cl::NullRange
        );
        queue.finish();

        queue.enqueueReadImage(mergeBufferOut, CL_TRUE, origin, region, 0, 0 , data,  NULL, NULL);
        queue.finish();

        // STEP: multiply with complex exponential (fresnel term)
        // takes only the first channel from the buffer, which contains the monochrome texture

        cl::Buffer complexApertureBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight * 2);
        cl::Buffer complexExponentialBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight * 2);

        queue.enqueueWriteBuffer(complexExponentialBuffer, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight * 2, m_complexExponential);

        compExpMultKernel.setArg(0, mergeBufferOut);
        compExpMultKernel.setArg(1, complexExponentialBuffer);
        compExpMultKernel.setArg(2, complexApertureBuffer);
        compExpMultKernel.setArg(3, m_imgWidth);

        queue.enqueueNDRangeKernel(
            compExpMultKernel, 
            cl::NullRange, 
            cl::NDRange(m_imgWidth, m_imgHeight, 1), 
            cl::NullRange
        );

        queue.finish();

        // debug 
        // queue.enqueueReadBuffer(complexApertureBuffer, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight * 2, m_complexAperture);
        // queue.finish();

        // cl::Buffer complexApertureBufferMagnitude(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight);

        // for (int i = 0; i < m_imgHeight*m_imgWidth; i++)
        // {
        //     float x = m_complexAperture[2*i];
        //     float y = m_complexAperture[2*i + 1];
        //     magnitude[i] = std::sqrt(x*x + y*y);
        // }
        

        // queue.enqueueWriteBuffer(complexApertureBufferMagnitude, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight, magnitude);
        // queue.finish();
        

        //STEP: APPLY THE FFT TO GET THE PSF

        cl::Buffer psfBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight * 2);

        size_t clLengths[2] = {m_imgWidth, m_imgHeight};
        clfftCreateDefaultPlan(&planHandle, context(), CLFFT_2D, clLengths);
        clfftSetPlanPrecision(planHandle, CLFFT_SINGLE);
        clfftSetLayout(planHandle, CLFFT_COMPLEX_INTERLEAVED, CLFFT_COMPLEX_INTERLEAVED);
        clfftSetResultLocation(planHandle, CLFFT_OUTOFPLACE);

        clfftBakePlan(planHandle, 1, &queue(), NULL, NULL);
        clfftEnqueueTransform(planHandle, CLFFT_FORWARD, 1, &queue(), 0, NULL, NULL, &complexApertureBuffer(), &psfBuffer(), NULL);
        clFinish(queue());

        queue.enqueueReadBuffer(psfBuffer, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight * 2, raw);
        queue.finish();
        
        //STEP: SPECTRAL BLUR
        cl::Buffer monochromePSF(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight);

        // From psfBuffer to monochromePSF

        cl::Image2D fresnelPSF(context, 
                    CL_MEM_READ_WRITE, 
                    cl::ImageFormat(CL_RGBA, CL_FLOAT),
                    m_imgWidth,
                    m_imgHeight,
                    0,
                    NULL);

        computeMagnitudeKernel.setArg(0, psfBuffer);
        computeMagnitudeKernel.setArg(1, fresnelPSF);
        computeMagnitudeKernel.setArg(2, monochromePSF); // debug 
        computeMagnitudeKernel.setArg(3, m_imgWidth);
        computeMagnitudeKernel.setArg(4, m_imgHeight);
        computeMagnitudeKernel.setArg(5, m_lambda);
        computeMagnitudeKernel.setArg(6, m_distance);

        queue.enqueueNDRangeKernel(
            computeMagnitudeKernel, 
            cl::NullRange, 
            cl::NDRange(m_imgWidth, m_imgHeight, 1), 
            cl::NullRange
        );

        queue.finish();

        queue.enqueueReadBuffer(monochromePSF, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight, magnitude);
        queue.finish();

        // TODO: Implement LOG norm 
        for (int i = 0; i < m_imgHeight*m_imgWidth; i++)
        {
            if(magnitude[i] > normFactor)
                normFactor = magnitude[i];
        }


        queue.enqueueReadImage(fresnelPSF, CL_TRUE, origin, region, 0, 0 , raw,  NULL, NULL);
        queue.finish();

        
        // the channels resulting from the spectral blur  
        cl::Buffer redChannelPSF(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight);
        cl::Buffer greenChannelPSF(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight);
        cl::Buffer blueChannelPSF(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight);

        // TODO: adapt it to the spectrum mapping vector
        cl::Buffer spectrumMapping(context, CL_MEM_READ_WRITE, sizeof(float) * SPECTRUM_RESOLUTION * 3);
        queue.enqueueWriteBuffer(spectrumMapping, CL_TRUE, 0, sizeof(float) * SPECTRUM_RESOLUTION * 3, spectrum);

        cl::Image2D fresnelPSF2(context, 
                    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 
                    cl::ImageFormat(CL_RGBA, CL_FLOAT),
                    m_imgWidth,
                    m_imgHeight,
                    0,
                    raw);

        spectralBlurKernel.setArg(0, fresnelPSF2);
        spectralBlurKernel.setArg(1, redChannelPSF);
        spectralBlurKernel.setArg(2, greenChannelPSF);
        spectralBlurKernel.setArg(3, blueChannelPSF);
        spectralBlurKernel.setArg(4, spectrumMapping); // lambda to RGB mapping -> TBD
        spectralBlurKernel.setArg(5, m_imgWidth);
        spectralBlurKernel.setArg(6, m_imgHeight);
        spectralBlurKernel.setArg(7, m_lambda*1000*1000);
        spectralBlurKernel.setArg(8, m_distance);
        spectralBlurKernel.setArg(9, normFactor);

        queue.enqueueNDRangeKernel(
            spectralBlurKernel, 
            cl::NullRange, 
            cl::NDRange(m_imgWidth, m_imgHeight, 1), 
            cl::NullRange
        );

        queue.finish();

        r = new float[m_imgWidth*m_imgHeight];
        g = new float[m_imgWidth*m_imgHeight];
        b = new float[m_imgWidth*m_imgHeight];

        queue.enqueueReadBuffer(redChannelPSF, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight, r);
        queue.finish();

        queue.enqueueReadBuffer(greenChannelPSF, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight, g);
        queue.finish();

        queue.enqueueReadBuffer(blueChannelPSF, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight, b);
        queue.finish();


        // STEP: COMPUTE FFT OF THE SPECTRAL PSF

        // The FFT buffers of the spectral PSF
        cl::Buffer redChannelPSFFFT(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight*2);
        cl::Buffer greenChannelPSFFFT(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight*2);
        cl::Buffer blueChannelPSFFFT(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight*2);

        clfftSetLayout(planHandle, CLFFT_REAL, CLFFT_COMPLEX_INTERLEAVED);
        clfftBakePlan(planHandle, 1, &queue(), NULL, NULL);

        queue.finish();

        // FFT red
        clfftEnqueueTransform(planHandle, CLFFT_FORWARD, 1, &queue(), 0, NULL, NULL, &redChannelPSF(), &redChannelPSFFFT(), NULL);
        clFinish(queue());

        // FFT green
        clfftEnqueueTransform(planHandle, CLFFT_FORWARD, 1, &queue(), 0, NULL, NULL, &greenChannelPSF(), &greenChannelPSFFFT(), NULL);
        clFinish(queue());

        // FFT blue
        clfftEnqueueTransform(planHandle, CLFFT_FORWARD, 1, &queue(), 0, NULL, NULL, &blueChannelPSF(), &blueChannelPSFFFT(), NULL);
        clFinish(queue());

        

        // STEP: multiply  with original m_ImgRedFFT / m_ImgGreenFFT / m_ImgBlueFFT

        
        // The FFT buffers of the original image
        cl::Buffer redChannelFFT(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight*2);
        cl::Buffer greenChannelFFT(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight*2);
        cl::Buffer blueChannelFFT(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight*2);

        queue.enqueueWriteBuffer(redChannelFFT, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight*2, m_ImgRedFFT);
        queue.enqueueWriteBuffer(greenChannelFFT, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight*2,m_ImgGreenFFT);
        queue.enqueueWriteBuffer(blueChannelFFT, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight*2, m_ImgBlueFFT);

        // the FFT buffers of the results of the convolution
        cl::Buffer redChannelMult(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight*2);
        cl::Buffer greenChannelMult(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight*2);
        cl::Buffer blueChannelMult(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight*2);

        queue.finish();

        // TODO: check the maths for the convolution 

        // red channels conv
        convOfFFTsKernel.setArg(0, redChannelPSFFFT);
        convOfFFTsKernel.setArg(1, redChannelFFT);
        convOfFFTsKernel.setArg(2, redChannelMult);
        convOfFFTsKernel.setArg(3, m_imgWidth);
        
        queue.enqueueNDRangeKernel(
            convOfFFTsKernel, 
            cl::NullRange, 
            cl::NDRange(m_imgWidth, m_imgHeight, 1), 
            cl::NullRange
        );
        queue.finish();

        // green channels conv
        convOfFFTsKernel.setArg(0, greenChannelPSFFFT);
        convOfFFTsKernel.setArg(1, greenChannelFFT);
        convOfFFTsKernel.setArg(2, greenChannelMult);
        convOfFFTsKernel.setArg(3, m_imgWidth);
        
        queue.enqueueNDRangeKernel(
            convOfFFTsKernel, 
            cl::NullRange, 
            cl::NDRange(m_imgWidth, m_imgHeight, 1), 
            cl::NullRange
        );
        queue.finish();

        // blue channels conv
        convOfFFTsKernel.setArg(0, blueChannelPSFFFT);
        convOfFFTsKernel.setArg(1, blueChannelFFT);
        convOfFFTsKernel.setArg(2, blueChannelMult);
        convOfFFTsKernel.setArg(3, m_imgWidth);
        
        queue.enqueueNDRangeKernel(
            convOfFFTsKernel, 
            cl::NullRange, 
            cl::NDRange(m_imgWidth, m_imgHeight, 1), 
            cl::NullRange
        );
        queue.finish();


        // STEP: Computing the iFFT of the multiplication (as in convolution result)

        clfftSetLayout(planHandle, CLFFT_COMPLEX_INTERLEAVED, CLFFT_REAL);
        clfftBakePlan(planHandle, 1, &queue(), NULL, NULL);

        cl::Buffer redChanneliFFT(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight*2);
        cl::Buffer greenChanneliFFT(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight*2);
        cl::Buffer blueChanneliFFT(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight*2);
        queue.finish();

        // TODO: replace redCannelFFT with the result of fft multiplication

        // red channel iFFT
        // clfftEnqueueTransform(planHandle, CLFFT_BACKWARD, 1, &queue(), 0, NULL, NULL, &redChannelFFT(), &redChanneliFFT(), NULL);
        clfftEnqueueTransform(planHandle, CLFFT_BACKWARD, 1, &queue(), 0, NULL, NULL, &redChannelMult(), &redChanneliFFT(), NULL);
        clFinish(queue());
        // queue.enqueueReadBuffer(redChanneliFFT, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight * 2, raw);
        // queue.finish();

        // green channel iFFT
        // clfftEnqueueTransform(planHandle, CLFFT_BACKWARD, 1, &queue(), 0, NULL, NULL, &greenChannelFFT(), &greenChanneliFFT(), NULL);
        clfftEnqueueTransform(planHandle, CLFFT_BACKWARD, 1, &queue(), 0, NULL, NULL, &greenChannelMult(), &greenChanneliFFT(), NULL);
        clFinish(queue());
        // queue.enqueueReadBuffer(greenChanneliFFT, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight, m_ImgGreeniFFT);
        // queue.finish();

        // blue channel iFFT
        // clfftEnqueueTransform(planHandle, CLFFT_BACKWARD, 1, &queue(), 0, NULL, NULL, &blueChannelFFT(), &blueChanneliFFT(), NULL);
        clfftEnqueueTransform(planHandle, CLFFT_BACKWARD, 1, &queue(), 0, NULL, NULL, &blueChannelMult(), &blueChanneliFFT(), NULL);
        clFinish(queue());
        // queue.enqueueReadBuffer(blueChanneliFFT, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight*2, raw);
        // queue.finish();

        // tone mapping
        cl::Image2D toneMappedBuffer(context, 
                    CL_MEM_READ_WRITE, 
                    cl::ImageFormat(CL_RGBA, CL_UNSIGNED_INT8),
                    m_imgWidth,
                    m_imgHeight,
                    0,
                    NULL);

        toneMapperKernel.setArg(0, redChanneliFFT);
        toneMapperKernel.setArg(1, greenChanneliFFT);
        toneMapperKernel.setArg(2, blueChanneliFFT);
        toneMapperKernel.setArg(3, toneMappedBuffer);

        // switch between auto-exposure and custom-exposure for tone mapping
        if(m_autoExposure)
            toneMapperKernel.setArg(4, m_autoExposureValue);
        else
            toneMapperKernel.setArg(4, m_exposure);

        toneMapperKernel.setArg(5, m_gamma);
        toneMapperKernel.setArg(6, m_Lwhite);
        toneMapperKernel.setArg(7, m_imgWidth);

        queue.enqueueNDRangeKernel(
            toneMapperKernel, 
            cl::NullRange, 
            cl::NDRange(m_imgWidth, m_imgHeight, 1), 
            cl::NullRange
        );
        queue.finish();

        queue.enqueueReadImage(toneMappedBuffer, CL_TRUE, origin, region, 0, 0 , data,  NULL, NULL);
        queue.finish();

        // DEBUG SECTION
        // NORM


        // // UNCOMMENT FOR COMPLEX APERTURE
        // for (int i = 0; i < m_imgHeight*m_imgWidth; i++)
        // {
        //     float x = m_complexAperture[2*i];
        //     float y = m_complexAperture[2*i + 1];
        //     magnitude[i] = std::sqrt(x*x + y*y);
        //     // data[i*4] = (unsigned char)(255 * std::sqrt(x*x + y*y));
        //     // data[i*4 + 1] = 0; //(unsigned char)(255 * raw[i]);
        //     // data[i*4 + 2] = 0; //(unsigned char)(255 * raw[i]);
        //     // data[i*4 + 3] = 255;
        // }


        // // UNCOMMENT FOR MONOCHROMATIC PSF
        // for (int i = 0; i < m_imgHeight*m_imgWidth; i++)
        // {
        //     data[i*4]     = (unsigned char)(255 * magnitude[i]/normFactor);
        //     data[i*4 + 1] = data[i*4]; 
        //     data[i*4 + 2] = data[i*4];
        //     data[i*4 + 3] = 255;
        // }


        // UNCOMMENT FOR SPECTRAL PSF
        // for (int i = 0; i < m_imgHeight*m_imgWidth; i++)
        // {
        //     data[i*4]     = (unsigned char)(255 * r[i]);
        //     data[i*4 + 1] = (unsigned char)(255 * g[i]);
        //     data[i*4 + 2] = (unsigned char)(255 * b[i]);
        //     data[i*4 + 3] = 255;
        // }

        QImage img(data, m_imgWidth, m_imgHeight, QImage::Format_RGBA8888);
        painter->drawImage(0, 0, img);        
    } catch(cl::Error err) {
         std::cerr << "ERROR: " << err.what() << "(" << getOCLErrorString(err.err()) << ")" << std::endl;
    }
    delete[] data;
    delete[] raw;
    delete[] magnitude;
    delete[] r;
    delete[] g;
    delete[] b;
    
}

// Read exr file data
void TemporalGlareRenderer::readExrFile(const QString& fileName)
{
    if (image != nullptr)
    {
        std::cout << "Flushed previous image \n";
        delete image;
        delete[] m_ImgRedFFT;
        delete[] m_ImgBlueFFT;
        delete[] m_ImgGreenFFT;
    }

    image = new Image(fileName.toUtf8().constData());

    m_imgWidth = image->getWidth();
    m_imgHeight = image->getHeight();

    m_imageChanged = true;

    m_autoExposureValue = image->getAutoKeyValue() / image->getLogAverageLuminance();

    std::cout << "Image Loaded\n";
    std::cout << "width: "<<image->getWidth()<<", height: "<<image->getHeight()<<"\n";

    initTextures();
    std::cout << "Glare textures loaded\n";
    std::cout << "width: "<<m_slidWidth<<", height: "<<m_slidHeight<<"\n";
}

void TemporalGlareRenderer::updateViewSize(int newWidth, int newHeight)
{

}


// Initialize OpenCL resources
void TemporalGlareRenderer::initOpenCL()
{
	try {
		//get all platforms (drivers)
		std::vector<cl::Platform> all_platforms;
		cl::Platform::get(&all_platforms);
		if (all_platforms.size() == 0) {
			std::cout << " No platforms found. Check OpenCL installation!\n";
			exit(1);
		}
		platform = all_platforms[0];
		std::cout << "Using platform: " << platform.getInfo<CL_PLATFORM_NAME>() << "\n";

		//get default device of the default platform
		std::vector<cl::Device> all_devices;
		platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
		if (all_devices.size() == 0) {
			std::cout << " No devices found. Check OpenCL installation!\n";
			exit(1);
		}
		device = all_devices[0];
		std::cout << "Using device: " << device.getInfo<CL_DEVICE_NAME>() << "\n";

		context = cl::Context({ device });

		cl::Program::Sources sources;

        // Render Kernel ?
		std::ifstream kernelFile("render.cl");
		if (kernelFile.fail()) {
			std::cout << "ERROR: can't read the render kernel file\n";
			exit(1);
		}
		std::string src(std::istreambuf_iterator<char>(kernelFile), (std::istreambuf_iterator<char>()));
		sources.push_back(std::make_pair(src.c_str(), src.length()));

        // Tone Map kernel
        std::ifstream kernelFile2("reinhard_extended.cl");
		if (kernelFile2.fail()) {
			std::cout << "ERROR: can't read the tonemap kernel file\n";
			exit(1);
		}
		std::string src2(std::istreambuf_iterator<char>(kernelFile2), (std::istreambuf_iterator<char>()));
		sources.push_back(std::make_pair(src2.c_str(), src2.length()));

        // Fresnel rendering kernel
        std::ifstream kernelFile3("fresnel.cl");
		if (kernelFile3.fail()) {
			std::cout << "ERROR: can't read the fresnel kernel file\n";
			exit(1);
		}
		std::string src3(std::istreambuf_iterator<char>(kernelFile3), (std::istreambuf_iterator<char>()));
		sources.push_back(std::make_pair(src3.c_str(), src3.length()));


        program = cl::Program(context, sources);
	    try {
            program.build({ device });
        } catch(cl::Error err) {
			std::cout << " Error building: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << "\n";
			exit(1);
		}

        queue = cl::CommandQueue(context, device);

        std::cout << "Existing context: " << context() << std::endl;
		
		// kernel = cl::Kernel(program, "lfrender");
        toneMapperKernel = cl::Kernel(program, "tm_reinhard_extended");
        gratingsKernel   = cl::Kernel(program, "glr_render_gratings");
        lensDotsKernel   = cl::Kernel(program, "glr_render_lens_points");
        pupilKernel      = cl::Kernel(program, "glr_render_pupil");
        compExpKernel    = cl::Kernel(program, "generate_complex_exp");
        compExpMultKernel= cl::Kernel(program, "multiply_with_complex_exp");
        spectralBlurKernel=cl::Kernel(program, "spectral_blur");
        convOfFFTsKernel = cl::Kernel(program, "conv_of_ffts");
        computeMagnitudeKernel = cl::Kernel(program, "compute_magnitude_kernel");

        
        clfftInitSetupData(&fftSetup);
        clfftSetup(&fftSetup);

	}
	catch(cl::Error err) {
		// std::cerr << "ERROR: " << err.what() << "(" << getOCLErrorString(err.err()) << ")" << std::endl;
		exit(1);
	}
    

}

void TemporalGlareRenderer::initTextures()
{
    int n; 
    m_slidTexture = stbi_load("../textures/grating3.bmp",
                                &m_slidWidth,
                                &m_slidHeight, 
                                &n, 
                                STBI_rgb_alpha);

    unsigned char* temp;

    // resize
    if(m_imgHeight != m_slidHeight || m_imgWidth != m_slidWidth)
    {
        std::cout << "resized \n";
        stbir_resize_uint8(m_slidTexture, m_slidWidth, m_slidHeight, 0,
                          temp, m_imgWidth, m_imgHeight, 0, 
                          STBI_rgb_alpha);
                          
        m_slidHeight = m_imgHeight;
        m_slidWidth  = m_imgWidth;

        delete [] m_slidTexture;
        m_slidTexture = temp;
    }

    m_pointCoordinates = new float[m_nPoints * 4];
    float* p = m_pointCoordinates;
    for(int i = 0; i < m_nPoints; ++i)
    {
        // rand x coordinate  (-1 , 1)
        int sign1 = ( rand()%2 ? 1 : -1);
        int sign2 = ( rand()%2 ? 1 : -1);

        p[0] = rand() % 1000 / 1000.0f;
        p[2] = rand() % 1000 / 1000.0f;
        
        float radius = std::sqrt(p[0] * p[0] + p[1] * p[1]);
        float dr = deformationCoeff(2*radius/m_imgHeight);

        p[1] = p[0] * dr / radius;
        p[3] = p[2] * dr / radius;    

        p[0] *= sign1;
        p[1] *= sign1;
        p[2] *= sign2;
        p[3] *= sign2;

        p+=4;
    }

    m_ImgRedFFT = new float[m_imgWidth * m_imgHeight*2];
    m_ImgGreenFFT = new float[m_imgWidth * m_imgHeight*2];
    m_ImgBlueFFT = new float[m_imgWidth * m_imgHeight*2];

    // compute the FFTs 

    cl::Buffer redChannel(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight);
    cl::Buffer greenChannel(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight);
    cl::Buffer blueChannel(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight);

    cl::Buffer redChannelFFT(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight*2);
    cl::Buffer greenChannelFFT(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight*2);
    cl::Buffer blueChannelFFT(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight*2);

    queue.enqueueWriteBuffer(redChannel, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight, image->get_rChannel());
    queue.enqueueWriteBuffer(greenChannel, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight, image->get_gChannel());
    queue.enqueueWriteBuffer(blueChannel, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight, image->get_bChannel());
    queue.finish();

    clfftDestroyPlan( &planHandle );

    size_t clLengths[2] = {m_imgWidth, m_imgHeight};
    clfftCreateDefaultPlan(&planHandle, context(), CLFFT_2D, clLengths);
    clfftSetPlanPrecision(planHandle, CLFFT_SINGLE);
    clfftSetLayout(planHandle, CLFFT_REAL, CLFFT_COMPLEX_INTERLEAVED);
    clfftSetResultLocation(planHandle, CLFFT_OUTOFPLACE);

    clfftBakePlan(planHandle, 1, &queue(), NULL, NULL);

    // red channel FFT
    clfftEnqueueTransform(planHandle, CLFFT_FORWARD, 1, &queue(), 0, NULL, NULL, &redChannel(), &redChannelFFT(), NULL);
    clFinish(queue());

    queue.enqueueReadBuffer(redChannelFFT, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight * 2, m_ImgRedFFT);
    queue.finish();


    // green channel FFT
    clfftEnqueueTransform(planHandle, CLFFT_FORWARD, 1, &queue(), 0, NULL, NULL, &greenChannel(), &greenChannelFFT(), NULL);
    clFinish(queue());

    queue.enqueueReadBuffer(greenChannelFFT, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight * 2, m_ImgGreenFFT);
    queue.finish();


    // blue channel FFT
    clfftEnqueueTransform(planHandle, CLFFT_FORWARD, 1, &queue(), 0, NULL, NULL, &blueChannel(), &blueChannelFFT(), NULL);
    clFinish(queue());

    queue.enqueueReadBuffer(blueChannelFFT, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight * 2, m_ImgBlueFFT);
    queue.finish();

    // discard the rest
    clfftDestroyPlan( &planHandle );

    std::cout<<"Textures updated!\n";

}

float TemporalGlareRenderer::noise()
{
    float noisePercentage = rand() % 10;
    noisePercentage = noisePercentage / 100;

    return noisePercentage;
}

// default Radius is 9 mm, and we play around it
void lensOutlinePoly(float& ya, const float &R)
{
    float R2 = R*R;
    float R3 = R2*R;
    float R4 = R3*R;
    float R5 = R4*R;

    ya = -0.00048433393427*R5 + 0.00528772036011*R4	-0.01383693844808*R3 -0.07352941176471*R2 + 2.18;
    if(ya < 0)
    {
        ya = 0;
    }
}

// computes deformation coefficient based on distance 
// from the center 
float TemporalGlareRenderer::deformationCoeff(float d)
{
    return (exp(d) - 1.0) / (exp(1) - 1.0);
}

int TemporalGlareRenderer::getWidth()
{
    return m_imgWidth;
}

int TemporalGlareRenderer::getHeight()
{
    return m_imgHeight;
}

TemporalGlareRenderer::~TemporalGlareRenderer()
{
    delete[] m_apertureTexture;
    delete[] m_complexExponential;
    delete[] m_slidTexture;
    delete[] m_pointCoordinates;

    delete[] m_ImgRedFFT;
    delete[] m_ImgGreenFFT;
    delete[] m_ImgBlueFFT;

    clfftDestroyPlan( &planHandle );
    clfftTeardown();
}