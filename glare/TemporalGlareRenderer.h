#ifndef TemporalGlareRenderer_H
#define TemporalGlareRenderer_H

#include <QWidget>
#include <QVector3D>
#include <QMatrix4x4>
#include <QString>

#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>

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
    cl::Image2D vcamImage;

    // Light Field data
    cl::Image3D lfData;
    int imgWidth;
    int imgHeight;
    int nrows=0;
    int ncols=0;
    int debug = 0;

    // Tranformation Matrices
    std::vector<QMatrix4x4> Vi;
    std::vector<QVector4D> w_cam; // World coordindates of each camera
	std::vector<float> transMatsVec; // Transformation matrices for all array cameras
	std::vector<float> camPosArr;
	float apertureTrans[16];  // Aperture transformations
    cl::Buffer pixelTransMats;   // Matrix_virtual_camera_pixel_to_camera_[i]_pixel
    cl::Buffer apertureTransMats; // Matric_virtual_camera_pixel_to_aperture_plane_coordinate
    cl::Buffer camPos;  // World coordindates of each camera
};


#endif // TemporalGlareRenderer_H
