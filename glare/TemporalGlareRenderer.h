#ifndef TemporalGlareRenderer_H
#define TemporalGlareRenderer_H

#include <QWidget>
#include <QVector3D>
#include <QMatrix4x4>
#include <QString>

#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>

#include "image.h"
#include "vector_types.h"

#include <time.h>

#include <clFFT/clFFT.h>

class TemporalGlareRenderer
{
public:
    TemporalGlareRenderer();
    ~TemporalGlareRenderer();

public:
    void paint(QPainter *painter, QPaintEvent *event, int elapsed, const QSize &destSize);
    void readExrFile(const QString& fileName);

    float focus = 500.0f;
    float apertureSize = 8.0f;
	int viewWidth, viewHeight; // Resolution of the rendered image in pixels

	float camera_fov = 90.f;

    // Tone mapping parameters 

    float m_gamma;
    float m_Lwhite;
    float m_alpha;

    float m_exposure;
    float m_autoExposureValue;

    bool m_autoExposure;

private:
    void updateViewSize(int newWidth, int newHeight);
    float noise();
    void updatePupilDiameter();
    void updateApertureTexture();
    void initTextures();

    float deformationCoeff(float d);

    // OpenCL stuff 
    void initOpenCL();

    cl::Platform platform;
    cl::Device device;
    cl::Context context;
    cl::Program program;
    cl::CommandQueue queue;

    cl::Kernel kernel;
    cl::Kernel toneMapperKernel;
    cl::Kernel floatToUintRBGAKernel;
    cl::Kernel mergeKernel;
    cl::Kernel gratingsKernel;
    cl::Kernel lensDotsKernel;
    cl::Kernel pupilKernel;
    cl::Kernel compExpKernel;
    cl::Kernel compExpMultKernel;
    cl::Kernel spectralBlurKernel;
    cl::Kernel convOfFFTsKernel;

    // Image data
    int m_imgWidth;
    int m_imgHeight;

    bool m_imageChanged;

    int nrows=0;
    int ncols=0;
    int debug = 0;

    Image *image = nullptr;

    //Aperture related stuff

    //Aperture data
    float m_pupilRadiusPx;
    const float m_maxPupilSize;
    float m_fieldLuminance;
    float m_apperture;
    cl_float2 m_pupilCenter;

    //Aperture texture
    float* m_apertureTexture;

    //slid texture -> loaded once
    unsigned char* m_slidTexture;
    int m_slidWidth;
    int m_slidHeight;
    float m_slidRadiusPx;

    //lens particles
    float* m_pointCoordinates; //x, alphadx, y, alphady
    int m_nPoints;

    //we neglect the vitreous for a moment
    //we also neglect flinching 

    //complex exponential
    float* m_complexExponential;
    float* m_complexAperture;
    float m_lambda;
    float m_distance;

    clfftSetupData fftSetup;
    clfftPlanHandle planHandle;

    float* m_ImgRedFFT;
    float* m_ImgGreenFFT;
    float* m_ImgBlueFFT;


};


#endif // TemporalGlareRenderer_H
