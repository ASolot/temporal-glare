#ifndef TemporalGlareRenderer_H
#define TemporalGlareRenderer_H

#include <QWidget>
#include <QVector3D>
#include <QMatrix4x4>
#include <QString>

#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>

#include "image.h"

// // include FFT related routines
// #ifndef VIENNACL_WITH_OPENCL
//     #define VIENNACL_WITH_OPENCL
// #endif

// #include <viennacl/fft.hpp>
// #include <viennacl/linalg/fft_operations.hpp>

// #include <viennacl/vector.hpp>
// #include <viennacl/matrix.hpp>


class TemporalGlareRenderer
{
public:
    TemporalGlareRenderer();

public:
    void paint(QPainter *painter, QPaintEvent *event, int elapsed, const QSize &destSize);
    void readExrFile(const QString& fileName);

    float focus = 500.0f;
    float apertureSize = 8.0f;
    QVector3D K_pos; // Camera position
    QVector3D K_pos_0; // Initial cmera position
	int viewWidth, viewHeight; // Resolution of the rendered image in pixels

	float camera_fov = 90.f;

private:
    void updateViewSize(int newWidth, int newHeight);

    // OpenCL stuff 
    void initOpenCL();
    void calculateArrayCameraViewTransform();
    void calculateAllTransforms();
    cl::Platform platform;
    cl::Device device;
    cl::Context context;
    cl::Program program;
    cl::CommandQueue queue;
    cl::Kernel kernel;
    cl::Kernel toneMapperKernel;
    cl::Image2D vcamImage;

    // Image data
    cl::Image3D lfData;
    int imgWidth;
    int imgHeight;
    int nrows=0;
    int ncols=0;
    int debug = 0;

    Image *image = nullptr;

};


#endif // TemporalGlareRenderer_H
