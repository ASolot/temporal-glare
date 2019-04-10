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




#ifndef VIENNACL_WITH_OPENCL
  #define VIENNACL_WITH_OPENCL
#endif


#include <viennacl/ocl/backend.hpp>
#include <viennacl/vector.hpp>
#include <viennacl/matrix.hpp>
#include <viennacl/linalg/matrix_operations.hpp>
#include <viennacl/linalg/norm_2.hpp>
#include <viennacl/linalg/prod.hpp>

#include <viennacl/fft.hpp>
#include <viennacl/linalg/fft_operations.hpp>

#include "ocl_utils.hpp"



/* 
  A right-handed version of the QMatrix4x4.lookAt method
*/
QMatrix4x4 lookAtRH(QVector3D eye, QVector3D center, QVector3D up)
{
	QMatrix4x4 la;
	la.lookAt(eye, center, up);
	QMatrix4x4 inv;
	inv(0, 0) = -1;
	inv(2, 2) = -1;
	return inv*la;
}

TemporalGlareRenderer::TemporalGlareRenderer() :
	m_imgWidth(0), m_imgHeight(0), m_maxPupilSize(9.0f), ncols(0), nrows(0), 
    m_pupilRadiusPx(0), m_fieldLuminance(100), m_nPoints(750), m_lambda(575.0f/1000.0f/1000.0f), m_distance(20)
{
    m_apertureTexture = nullptr;
    m_slidTexture = nullptr;

    m_imageChanged = false;
    initOpenCL();
    
    srand (time(NULL));
}

void TemporalGlareRenderer::updatePupilDiameter()
{
    // TODO: limit variation based on time 
    float p = 4.9 - 3*tanh(0.4 * (log(m_fieldLuminance) + 1));
    m_apperture = p + noise() * m_maxPupilSize / p * sqrt( 1 - p / m_maxPupilSize );
    m_pupilRadiusPx = m_imgHeight / 2.0f * m_apperture / (m_maxPupilSize + 2.0f);
    m_slidRadiusPx  = m_imgHeight / 2.0f * 1.0f / m_maxPupilSize;
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
        compExpKernel.setArg(1, m_lambda);
        compExpKernel.setArg(2, m_distance);
        compExpKernel.setArg(3, m_imgWidth);

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
    try {
        // updateViewSize( destSize.width(), destSize.height() );

        raw  = new float[m_imgWidth*m_imgHeight*2];
        data = new unsigned char[m_imgWidth*m_imgHeight*4];
        memset(data, 255, m_imgWidth * m_imgHeight * 4);

        cl::size_t<3> origin;
        origin[0] = 0; origin[1] = 0, origin[2] = 0;
        cl::size_t<3> region;
        region[0] = m_imgWidth; region[1] = m_imgHeight; region[2] = 1;


        std::cout<<"Frame Updated\n";
        updateApertureTexture();

        // make the neccessary updates 
        updatePupilDiameter(); 
        
        // // TODO: adapt based on slider
        // float t = 1.0f;
        // float tmp = (t - 0.5f) * 20.f;
        // float exposure = std::pow(2.f, tmp);

        // float gamma     = 5.0f;
        // float Lwhite    = 5.0f;

        // toneMapperKernel.setArg(0, inputImage);
        // toneMapperKernel.setArg(1, toneMapped);
        // toneMapperKernel.setArg(2, exposure);
        // toneMapperKernel.setArg(3, gamma);
        // toneMapperKernel.setArg(4, Lwhite);

        // queue.enqueueNDRangeKernel(
        //     toneMapperKernel, 
        //     cl::NullRange, 
        //     cl::NDRange(image->getWidth(), image->getHeight(), 1), 
        //     cl::NullRange
        // );
        // queue.finish();


        //pupil generation
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

        // gratings 
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
        gratingsKernel.setArg(2, m_slidRadiusPx);
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
        
        // lens points 
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
        lensDotsKernel.setArg(4,0.0f);

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

        // merge images 
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

        // queue.enqueueReadImage(mergeBufferOut, CL_TRUE, origin, region, 0, 0 , data,  NULL, NULL);
        // queue.finish();

        // multiply with complex exponential (fresnel term)
        // takes only the first channel

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


        // // fft to get the image


        cl::Buffer psfBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight*2);

        size_t clLengths[2] = {m_imgWidth, m_imgHeight};
        clfftCreateDefaultPlan(&planHandle, context(), CLFFT_2D, clLengths);
        clfftSetPlanPrecision(planHandle, CLFFT_SINGLE);
        clfftSetLayout(planHandle, CLFFT_COMPLEX_INTERLEAVED, CLFFT_COMPLEX_INTERLEAVED);
        clfftSetResultLocation(planHandle, CLFFT_OUTOFPLACE);

        clfftBakePlan(planHandle, 1, &queue(), NULL, NULL);
        clfftEnqueueTransform(planHandle, CLFFT_FORWARD, 1, &queue(), 0, NULL, NULL, &complexApertureBuffer(), &psfBuffer(), NULL);
        clFinish(queue());

        // queue.enqueueReadBuffer(psfBuffer, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight * 2, raw);
        // queue.finish();

        cl::Buffer spectralBlurBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * m_imgWidth * m_imgHeight);


        spectralBlurKernel.setArg(0, psfBuffer);
        spectralBlurKernel.setArg(1, spectralBlurBuffer);
        spectralBlurKernel.setArg(2, m_imgWidth);
        spectralBlurKernel.setArg(3, m_lambda);
        spectralBlurKernel.setArg(4, m_distance);

        queue.enqueueNDRangeKernel(
            spectralBlurKernel, 
            cl::NullRange, 
            cl::NDRange(m_imgWidth, m_imgHeight, 1), 
            cl::NullRange
        );

        queue.finish();

        queue.enqueueReadBuffer(spectralBlurBuffer, CL_TRUE, 0, sizeof(float) * m_imgWidth * m_imgHeight, raw);
        queue.finish();


        for (int i = 0; i < m_imgHeight*m_imgWidth; i++)
        {
            data[i*4] = (unsigned char)(255 * raw[i]);
            data[i*4 + 1] = (unsigned char)(255 * raw[i]);
            data[i*4 + 2] = (unsigned char)(255 * raw[i]);
            data[i*4 + 3] = 255;
        }
        
        
        // spectral blur 

        // fft 

        // convolve  with original

        // ifft 

        // tone mapping

        QImage img(data, m_imgWidth, m_imgHeight, QImage::Format_RGBA8888);
        painter->drawImage(0, 0, img);        
    } catch(cl::Error err) {
         std::cerr << "ERROR: " << err.what() << "(" << getOCLErrorString(err.err()) << ")" << std::endl;
    }
    delete[] data;
}

// Read exr file data
void TemporalGlareRenderer::readExrFile(const QString& fileName)
{
    if (image != nullptr)
    {
        std::cout << "Flushed previous image \n";
        delete image;
    }

    image = new Image(fileName.toUtf8().constData());

    m_imgWidth = image->getWidth();
    m_imgHeight = image->getHeight();

    m_imageChanged = true;

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

        // make viennacl get along with our business
        // http://viennacl.sourceforge.net/doc/manual-custom-contexts.html
        viennacl::ocl::setup_context(0, context(), device(), queue());
        viennacl::ocl::switch_context(0);

        std::cout << "Existing context: " << context() << std::endl;
        std::cout << "ViennaCL uses context: " << viennacl::ocl::current_context().handle().get() << std::endl;
		
		// kernel = cl::Kernel(program, "lfrender");
        toneMapperKernel = cl::Kernel(program, "tm_reinhard_extended");
        gratingsKernel   = cl::Kernel(program, "glr_render_gratings");
        lensDotsKernel   = cl::Kernel(program, "glr_render_lens_points");
        pupilKernel      = cl::Kernel(program, "glr_render_pupil");
        compExpKernel    = cl::Kernel(program, "generate_complex_exp");
        compExpMultKernel= cl::Kernel(program, "multiply_with_complex_exp");
        spectralBlurKernel=cl::Kernel(program, "spectral_blur");

        
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
    m_slidTexture = stbi_load("../textures/grating2.bmp",
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
        int sign = ( rand()%2 ? 1 : -1);
        p[0] = rand() % 1000 / 1000.0f;
        p[1] = deformationCoeff(p[0]);
        p[0] *= sign;
        p[1] *= sign;
        
        // rand y coordinate (-1 , 1)
        sign = ( rand()%2 ? 1 : -1);
        p[2] = rand() % 1000 / 1000.0f;
        p[3] = deformationCoeff(p[2]);
        p[2] *= sign;
        p[3] *= sign;

        p+=4;
    }
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

TemporalGlareRenderer::~TemporalGlareRenderer()
{
    delete[] m_apertureTexture;
    delete[] m_complexExponential;
    delete[] m_slidTexture;
    delete[] m_pointCoordinates;

    clfftDestroyPlan( &planHandle );
    clfftTeardown();
}