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
	viewWidth(0), viewHeight(0), ncols(0), nrows(0)
{
    initOpenCL();
}

void TemporalGlareRenderer::paint(QPainter *painter, QPaintEvent *event, int elapsed, const QSize &destSize)
{
    if( nrows == 0 ) // No LF data available
        return;

    unsigned char* data = NULL;
    try {
        updateViewSize( destSize.width(), destSize.height() );

        calculateAllTransforms();
		kernel.setArg(2,pixelTransMats);
        kernel.setArg(3,apertureTransMats);
        kernel.setArg(5,apertureSize);
		queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(viewWidth, viewHeight, 1), cl::NullRange);

        // Read Image back and display
        data = new unsigned char[viewWidth*viewHeight*4];
        cl::size_t<3> origin;
        origin[0] = 0; origin[1] = 0, origin[2] = 0;
        cl::size_t<3> region;
        region[0] = viewWidth; region[1] = viewHeight; region[2] = 1;
        queue.enqueueReadImage(vcamImage, CL_TRUE, origin, region, 0, 0 , data,  NULL, NULL);
        queue.finish();

        QImage img(data, viewWidth, viewHeight, QImage::Format_RGBA8888);
        painter->drawImage(0, 0, img);        
    } catch(cl::Error err) {
         std::cerr << "ERROR: " << err.what() << "(" << getOCLErrorString(err.err()) << ")" << std::endl;
    }
    delete[] data;
}

// Calculate View tranformation matrix for each camera and fill camera position buffer
void TemporalGlareRenderer::calculateArrayCameraViewTransform()
{
    camPosArr.resize(3*nrows*ncols);
    // View Transformation matrices for array cameras
    for (int row = 0; row < nrows; row++) {
        for (int col = 0; col < ncols; col++) {
            QMatrix4x4 vij; // View matrix for each array camera
            QVector4D &w_cam_i = w_cam[col+row*ncols];
			vij = lookAtRH(QVector3D(w_cam_i.x(),w_cam_i.y(),0), QVector3D(w_cam_i.x(),w_cam_i.y(),1), QVector3D(0,1,0));
            Vi.push_back(vij);
            camPosArr[(col+row*ncols)*3+0] = (float)w_cam_i.x();
            camPosArr[(col+row*ncols)*3+1] = (float)w_cam_i.y();
            camPosArr[(col+row*ncols)*3+2] = 0.0f;
        }
    }
    queue.enqueueWriteBuffer(camPos,CL_TRUE,0,sizeof(float)*3*nrows*ncols,camPosArr.data());
}

void TemporalGlareRenderer::calculateAllTransforms()
{
    // Intrinsic camera matrix of data camera
	float proj_d = tan(camera_fov / 2.f * M_PI / 180.f);
	QMatrix4x4 Kci (imgWidth/proj_d,  0.0f,           0.0f,  imgWidth/2.0f,
                    0.0f,           -imgWidth/proj_d,  0.0f,  imgHeight/2.0f,
                    0.0f,           0.0f,           1.0f,  0.0f,
                    0.0f,           0.0f,           0.0f,  1.0f);

	// Intrinsic camera matrix of the virtual camera
	QMatrix4x4 KK(viewWidth/proj_d, 0.0f, 0.0f, viewWidth / 2.0f,
		0.0f, -viewWidth/proj_d, 0.0f, viewHeight / 2.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);


    QVector3D N_F(0.f,0.f,1.f); // Normal of the focus plane
    QVector3D w_F(0.f,0.f,focus); // Point of the focus plane

    // Projection matrix for cameras in the array
	QMatrix4x4 Pc(1.0f,     0.0f,      0.0f,  0.0f,
                    0.0f, 1.0f,      0.0f,  0.0f,
                    0.0f,     0.0f,  1.0f,  0.0f,
                    0.0f,      0.0f,      1.0f,  0.0f);

    QMatrix4x4 Vk;  // View matrix for the virtual camera K	
	Vk = lookAtRH(K_pos, K_pos + QVector3D(0, 0, 1), QVector3D(0, 1, 0));


	QMatrix4x4 PcK = Pc;
    // Specifying focal plane in the virtual camera K coordinates
	PcK.setRow(2, QVector4D(N_F.x(), N_F.y(), N_F.z(), -QVector3D::dotProduct(N_F, w_F)));

	QMatrix4x4 transWK = KK * PcK * Vk;  // From World to Virtual camera K coordinates

	QMatrix4x4 transKW = transWK.inverted();

    transMatsVec.resize(16*nrows*ncols);
    int ndx = 0;
    for (int i = 0; i < nrows; i++) {
        for (int j = 0; j < ncols; j++) {

            int cam_index = i*ncols + j;
            // Transform focal plane equation to the camera array coordinates
            QMatrix4x4 KI = Vi[cam_index]*Vk.inverted(); // Transformation from the virtual camera to camera array 
            QMatrix4x4 KIN = QMatrix4x4(KI.normalMatrix());
            QVector3D N_Ft = KIN*N_F; // Transform K -> Ci camera coordinates
            QVector3D w_Ft = KI*w_F; // Transform K -> Ci camera coordinates

            QMatrix4x4 PcI = Pc;	
			PcI.setRow(2, QVector4D(N_Ft.x(), N_Ft.y(), N_Ft.z(), -QVector3D::dotProduct(N_Ft, Vi[i*ncols + j]*w_Ft)));
			QMatrix4x4 transWC = Kci * PcI * Vi[cam_index]; // From the world coordinates to the camera-array pixel coordinates
			QMatrix4x4 transKC = transWC*transKW; 

			QMatrix4x4 transWCt = transKC.transposed();
            for(int k=0;k<16;k++){
				// Copy the matrix elements in the row-major order
				transMatsVec[16*ndx+k] = *(transWCt.data()+k);
            }
            ndx++;
        }
    }

    // Send data to GPU
    queue.enqueueWriteBuffer(pixelTransMats,CL_TRUE,0,sizeof(float)*16*nrows*ncols,transMatsVec.data());

	// Aperture transform: from pK to w_A

	QVector3D N_A(0.f, 0.f, 1.f); // Normal of the aperture/camera plane
	QVector3D w_A(0.f, 0.f, 0.f); // Point of the aperture/camera plane
	
	QMatrix4x4 PcA(1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f);

	QMatrix4x4 transAK = Kci * PcA * Vk; // Transformation from word to virtual camera K pixel coords
	// Because the aperture plane is in the word and not camera coordinate system, it is the easiest
	// to set the 3rd row of the matrix to the desired plane equation
	transAK.setRow(2, QVector4D(N_A.x(), N_A.y(), N_A.z(), -QVector3D::dotProduct(N_A, w_A)) );

	QMatrix4x4 transKAt = transAK.inverted().transposed();

	for (int k = 0; k<16; k++) {
		// Copy the matrix elements in the row-major order
		apertureTrans[k] = *(transKAt.data() + k);
	}

	queue.enqueueWriteBuffer(apertureTransMats, CL_TRUE, 0, sizeof(float) * 16, apertureTrans);

}

// Read exr file data
void TemporalGlareRenderer::readExrFile(const QString& fileName)
{
    int rMax = 0;
    int cMax = 0;

    // QStringList files = QDir(lf_dir).entryList();

    // // Find the size of the camera array
    // for (int i = 0; i < files.size(); i++) {
    //     QFileInfo f(files[i]);
    //     if (f.suffix() == "png") {
    //         int row, col;
    //         double camx, camy;
    //         char ext[64];
    //         int ret = sscanf(files[i].toStdString().c_str(),
    //                          "out_%d_%d_%lf_%lf%s", &row, &col, &camy, &camx, ext);
    //         if (ret == 5) {
    //             rMax = std::max(rMax, row + 1);
    //             cMax = std::max(cMax, col + 1);
    //         } else {
    //             qCritical("ERROR: Invalid File name!");
    //             std::abort();
    //         }
    //     }
    // }
    // nrows = rMax;
    // ncols = cMax;
	// if (nrows < 1 || ncols < 1) {
	// 	qCritical("ERROR: No light field images found");
	// 	std::abort();
	// }
    //             qDebug() << "loaded";

    //         } else {
    //             qCritical("ERROR: Invalid File name!");
    //             std::abort();
    //         }
    //     }
    // }
    // // Put the virtual camera in the center 
    // K_pos /= (double)w_cam.size();
    // K_pos.setZ(-40.f);
    // K_pos_0 = K_pos;
    // qDebug() << "VCamera" << "at (" << K_pos.x() << ", " << K_pos.y() <<")";


    // try {

    //     lfData = cl::Image3D(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
	// 				  // CL_RGBA - because Nvidia does not support RGB, we need to use RGBA
	// 	              // CL_UNORM_INT8 - pixels should be read with read_imagef. The values will be normalized 0-1
    //                   cl::ImageFormat(CL_RGBA, CL_UNORM_INT8), 
    //                   imgWidth,
    //                   imgHeight,
    //                   rMax * cMax,
    //                   0,
    //                   0,
    //                   imageData.data());

    //     camPos = cl::Buffer(context,CL_MEM_READ_WRITE,sizeof(float)*3*nrows*ncols);
	// 	apertureTransMats = cl::Buffer(context, CL_MEM_READ_WRITE, sizeof(float) * 16);
	// 	calculateArrayCameraViewTransform();

    //     pixelTransMats = cl::Buffer(context,CL_MEM_READ_WRITE,sizeof(float)*16*nrows*ncols);
    //     calculateAllTransforms();

    //     kernel.setArg(0,lfData);
    //     kernel.setArg(2,pixelTransMats);
    //     kernel.setArg(3,apertureTransMats);
    //     kernel.setArg(4,camPos);
    //     kernel.setArg(5,apertureSize);
    //     kernel.setArg(6,nrows);
    //     kernel.setArg(7,ncols);
    // }
    // catch(cl::Error err) {
    //     std::cerr << "ERROR: " << err.what() << "(" << getOCLErrorString(err.err()) << ")" << std::endl;
    //     exit(1);
    // }

}

void TemporalGlareRenderer::updateViewSize(int newWidth, int newHeight)
{
    if( newWidth == viewWidth && newHeight == viewHeight )
        return;
    
    viewWidth = newWidth;
    viewHeight = newHeight;

    // Output image
    vcamImage = cl::Image2D(context,
                CL_MEM_WRITE_ONLY,
                cl::ImageFormat(CL_RGBA, CL_UNORM_INT8),
                viewWidth,
                viewHeight,
                0,
                NULL);
                
    kernel.setArg(1,vcamImage);
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

		std::ifstream kernelFile("render.cl");
		if (kernelFile.fail()) {
			std::cout << "ERROR: can' read the kernel file\n";
			exit(1);
		}
		std::string src(std::istreambuf_iterator<char>(kernelFile), (std::istreambuf_iterator<char>()));
		sources.push_back(std::make_pair(src.c_str(), src.length()));

        program = cl::Program(context, sources);
	    try {
            program.build({ device });
        } catch(cl::Error err) {
			std::cout << " Error building: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << "\n";
			exit(1);
		}
		queue = cl::CommandQueue(context, device);
		kernel = cl::Kernel(program, "lfrender");
	}
	catch(cl::Error err) {
		std::cerr << "ERROR: " << err.what() << "(" << getOCLErrorString(err.err()) << ")" << std::endl;
		exit(1);
	}

}

