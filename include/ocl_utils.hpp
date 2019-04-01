#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <exception>
#include <tuple>
#include <algorithm>
#include <random>
#include <math.h>
#include <stdint.h>

#if __APPLE__
  #include <OpenCL/opencl.h>
#else
  #include <CL/opencl.h>
#endif

#include <CL/cl.hpp>
#include <iostream>

#include "cxxopts.hpp"

typedef unsigned int uint;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define DEFAULT_TEST_SIZE 1000

std::tuple<cl::Program, cl::Context, cl::Device> initializeOCL(const char* kernelSrcFile, const unsigned int platformID, const unsigned int deviceID) {
    //-----------------------------------------------------
    // STEP I: Discover and initialize the platforms
    //-----------------------------------------------------
    std::vector<cl::Platform> all_platforms;
    cl::Platform::get(&all_platforms);
    if (all_platforms.size() == 0){
        throw "ERROR: No platforms found. Check OpenCL installation!\n";
    }
    if(platformID >= all_platforms.size() || platformID < 0) {
        throw "ERROR: Invalid PlatformID!\n";
    }
    cl::Platform myPlatform = all_platforms[platformID];
    std::cout << "Using platform: " << myPlatform.getInfo<CL_PLATFORM_NAME>() << std::endl;

    //-----------------------------------------------------
    // STEP II: Discover and initialize the devices
    //-----------------------------------------------------
    std::vector<cl::Device> all_devices;
    myPlatform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
    if (all_devices.size() == 0){
        throw "ERROR: No devices found. Check OpenCL installation!\n";
    }
    if(deviceID >= all_devices.size() || deviceID < 0) {
        throw "ERROR: Invalid DeviceID!\n";
    }
    cl::Device myDevice = all_devices[0];
    std::cout << "Using device: " << myDevice.getInfo<CL_DEVICE_NAME>() << "\n";

    //-----------------------------------------------------
    // STEP III: Create a context
    //-----------------------------------------------------
    cl::Context context({ myDevice });

    //-----------------------------------------------------
    // STEP IV: Read the Kernel source file
    //-----------------------------------------------------
    cl::Program::Sources sources;

    std::ifstream kernelFile(kernelSrcFile);
    if(kernelFile.fail()){
        throw "ERROR: can't read the kernel file. Make sure you start in the directory where the kernel file is located.";
    }
    std::string src(std::istreambuf_iterator<char>(kernelFile), (std::istreambuf_iterator<char>()));
    sources.push_back(std::make_pair(src.c_str(), src.length()));

    //-----------------------------------------------------
    // STEP V: Create and compile the program
    //-----------------------------------------------------
    cl::Program program(context, sources);
    try {
        program.build({ myDevice }, "-cl-std=CL1.2");
    }
    catch (cl::Error err) {
        std::cout << " Error building: " <<
        program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(myDevice) << "\n";
        throw "ERROR: Compiling the OpenCL Kernel Code";
    }

    return std::make_tuple(program, context, myDevice);
}

/*
  Utility Functions to generate random vector (of integers between 1 to 100) of given size
*/

std::vector<int> generate_random_vector(int size, int min, int max) {
    // First create an instance of an engine.
    std::random_device rnd_device;
    // Specify the engine and distribution.
    std::mt19937 mersenne_engine {rnd_device()};  // Generates random integers
    std::uniform_int_distribution<int> dist {min, max};

    auto gen = [&dist, &mersenne_engine](){
                    return dist(mersenne_engine);
                };

    std::vector<int> vec(size);
    std::generate(begin(vec), end(vec), gen);

    return vec;
}

std::vector<uint8_t> intToByte (std::vector<int> A) {
    std::vector<uint8_t> converted(A.size());
    for(size_t i = 0; i<A.size(); i++)
        converted[i] = (uint8_t) A[i];
    return converted;
}

/*
    Utility functions for image handling
*/

struct STB_Image
{
    unsigned char* data;
    int width;
    int height;
    int num_channels; // # 8-bit components per pixel
};

STB_Image load_image(const char* image_file_path) {
    STB_Image image;
    image.data = stbi_load(image_file_path, &image.width, &image.height, &image.num_channels, 4);
    if(image.data == NULL) {
        throw "ERROR: Cannot load image file!\n";
    }
    return image;
}

void write_image(STB_Image image, const char* file_path) {
    if(!stbi_write_png(file_path, image.width, image.height, 4, image.data, image.width*4)) {
        throw "ERROR: Cannot write the image\n";
    }
}


const char *getOCLErrorString(cl_int error)
{
switch(error){
    // run-time and JIT compiler errors
    case 0: return "CL_SUCCESS";
    case -1: return "CL_DEVICE_NOT_FOUND";
    case -2: return "CL_DEVICE_NOT_AVAILABLE";
    case -3: return "CL_COMPILER_NOT_AVAILABLE";
    case -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
    case -5: return "CL_OUT_OF_RESOURCES";
    case -6: return "CL_OUT_OF_HOST_MEMORY";
    case -7: return "CL_PROFILING_INFO_NOT_AVAILABLE";
    case -8: return "CL_MEM_COPY_OVERLAP";
    case -9: return "CL_IMAGE_FORMAT_MISMATCH";
    case -10: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
    case -11: return "CL_BUILD_PROGRAM_FAILURE";
    case -12: return "CL_MAP_FAILURE";
    case -13: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
    case -14: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
    case -15: return "CL_COMPILE_PROGRAM_FAILURE";
    case -16: return "CL_LINKER_NOT_AVAILABLE";
    case -17: return "CL_LINK_PROGRAM_FAILURE";
    case -18: return "CL_DEVICE_PARTITION_FAILED";
    case -19: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";

    // compile-time errors
    case -30: return "CL_INVALID_VALUE";
    case -31: return "CL_INVALID_DEVICE_TYPE";
    case -32: return "CL_INVALID_PLATFORM";
    case -33: return "CL_INVALID_DEVICE";
    case -34: return "CL_INVALID_CONTEXT";
    case -35: return "CL_INVALID_QUEUE_PROPERTIES";
    case -36: return "CL_INVALID_COMMAND_QUEUE";
    case -37: return "CL_INVALID_HOST_PTR";
    case -38: return "CL_INVALID_MEM_OBJECT";
    case -39: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
    case -40: return "CL_INVALID_IMAGE_SIZE";
    case -41: return "CL_INVALID_SAMPLER";
    case -42: return "CL_INVALID_BINARY";
    case -43: return "CL_INVALID_BUILD_OPTIONS";
    case -44: return "CL_INVALID_PROGRAM";
    case -45: return "CL_INVALID_PROGRAM_EXECUTABLE";
    case -46: return "CL_INVALID_KERNEL_NAME";
    case -47: return "CL_INVALID_KERNEL_DEFINITION";
    case -48: return "CL_INVALID_KERNEL";
    case -49: return "CL_INVALID_ARG_INDEX";
    case -50: return "CL_INVALID_ARG_VALUE";
    case -51: return "CL_INVALID_ARG_SIZE";
    case -52: return "CL_INVALID_KERNEL_ARGS";
    case -53: return "CL_INVALID_WORK_DIMENSION";
    case -54: return "CL_INVALID_WORK_GROUP_SIZE";
    case -55: return "CL_INVALID_WORK_ITEM_SIZE";
    case -56: return "CL_INVALID_GLOBAL_OFFSET";
    case -57: return "CL_INVALID_EVENT_WAIT_LIST";
    case -58: return "CL_INVALID_EVENT";
    case -59: return "CL_INVALID_OPERATION";
    case -60: return "CL_INVALID_GL_OBJECT";
    case -61: return "CL_INVALID_BUFFER_SIZE";
    case -62: return "CL_INVALID_MIP_LEVEL";
    case -63: return "CL_INVALID_GLOBAL_WORK_SIZE";
    case -64: return "CL_INVALID_PROPERTY";
    case -65: return "CL_INVALID_IMAGE_DESCRIPTOR";
    case -66: return "CL_INVALID_COMPILER_OPTIONS";
    case -67: return "CL_INVALID_LINKER_OPTIONS";
    case -68: return "CL_INVALID_DEVICE_PARTITION_COUNT";

    // extension errors
    case -1000: return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
    case -1001: return "CL_PLATFORM_NOT_FOUND_KHR";
    case -1002: return "CL_INVALID_D3D10_DEVICE_KHR";
    case -1003: return "CL_INVALID_D3D10_RESOURCE_KHR";
    case -1004: return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
    case -1005: return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
    default: return "Unknown OpenCL error";
    }
}

void failOnOCLError(cl_int status)
{
    if( status != CL_SUCCESS ) {
        std::cerr << "OpenCL error: " << getOCLErrorString(status) << std::endl;
        exit(1);
    }
}

void listPlatformsandDevices(){
    try
    {
        //get all platforms (drivers)
        std::vector<cl::Platform> all_platforms;
        cl::Platform::get(&all_platforms);
        if (all_platforms.size() == 0){
            std::cout << "No platforms found. Check OpenCL installation!\n";
            exit(1);
        }

        unsigned int platform_id = 0;
        for( auto platform : all_platforms )
        {
            std::cout << "Platform (id=" << platform_id << "): " << platform.getInfo<CL_PLATFORM_NAME>() << " version " << platform.getInfo<CL_PLATFORM_VERSION>() << std::endl;
            std::cout << "  Vendor: " << platform.getInfo<CL_PLATFORM_VENDOR>() << std::endl;
            std::cout << "  Extensions: " << platform.getInfo<CL_PLATFORM_EXTENSIONS>() << std::endl;

            std::vector<cl::Device> all_devices;
            platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
            if (all_devices.size() == 0) {
                std::cout << " No devices found for this platform. Check OpenCL installation!\n";
            }
            unsigned int device_id = 0;
            for( cl::Device &device : all_devices )
            {
                std::cout << "  Device (id=" << device_id << "): " << device.getInfo<CL_DEVICE_NAME>() << "\n";
                cl_device_type dev_type = device.getInfo<CL_DEVICE_TYPE>();
                std::cout << "    Type: ";
                switch( dev_type ) {
                case CL_DEVICE_TYPE_CPU:
                std::cout << "CPU";
                break;
                case CL_DEVICE_TYPE_GPU:
                std::cout << "GPU";
                break;
                case CL_DEVICE_TYPE_ACCELERATOR:
                std::cout << "Accelerator";
                break;
                case CL_DEVICE_TYPE_DEFAULT:
                std::cout << "Default";
                break;
                }
                std::cout << std::endl;
                std::cout << "    Version: " << device.getInfo<CL_DEVICE_VERSION>() << std::endl;
                std::cout << "    Global memory: " << device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>() << " bytes" << std::endl;
                std::cout << "    Local memory: " << device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() << " bytes" << std::endl;
                std::cout << "    Max compute units: " << device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << std::endl;
                std::cout << "    Max work group size: " << device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << std::endl;
                device_id++;
            }
            platform_id++;
        }
    }
    catch (cl::Error err)
    {
        std::cerr << "ERROR: " << err.what() << "(" << getOCLErrorString(err.err()) << ")" << std::endl;
        exit(1);
    }
}

std::string getFileName(const std::string& path) {
    char separator = '/';
#ifdef _WIN32
    separator = '\\';
#endif

    size_t i = path.rfind(separator, path.length());
    return (path.substr(i+1, path.length() - i));
}


std::tuple<const uint, const uint, const uint> parseOptions(int argc, char* argv[], bool input_image_required = false) {
    try {
        if (argc == 1 && input_image_required)
        {
            throw std::domain_error("Image location not specified");
        }

        cxxopts::Options options(argv[0], "Tasks in OpenCL");
        options.add_options()
            ("l, list", "List all available platforms and devices")
            ("p, platform", "Platform selected", cxxopts::value<uint>())
            ("d, device", "Device selected", cxxopts::value<uint>());
        if (!input_image_required)
        {
            // Test size option is only required for first 3 tasks
            options.add_options()
                ("t, testSize", "Length of the input", cxxopts::value<uint>());
        }
        cxxopts::ParseResult result = options.parse(argc, argv);

        const bool list_all = result["list"].as<bool>();
        if (list_all)
        {
            listPlatformsandDevices();
            exit(0);
        }

        uint platform_id, device_id, test_size;
        try
        {
            platform_id = result["platform"].as<uint>();
        }
        catch (std::exception &err)
        {
            std::cout << "Platform ID not provided, using the default value" << std::endl;
            platform_id = 0;
        }
        try
        {
            device_id = result["device"].as<uint>();
        }
        catch (std::exception &err)
        {
            std::cout << "Device ID not provided, using the default value" << std::endl;
            device_id = 0;
        }
        try
        {
            test_size = result["testSize"].as<uint>();
        }
        catch(const std::exception &err)
        {
            if (!input_image_required)
            {
                std::cout << "Test size not provided, using the default value" << std::endl;
            }
            test_size = DEFAULT_TEST_SIZE;
        }
        
        return std::make_tuple(platform_id, device_id, test_size);
    }
    catch (const std::exception &e)
	{
        std::cerr << "ERROR: parsing options: " << e.what() << std::endl;
        std::cout << "\nUsage: " << getFileName(argv[0]) << " [options]";
        if (input_image_required)
        {
            std::cout <<  " <path_to_image_file>";
        }
        std::cout << std::endl << std::endl;
        std::cout << "Options" << std::endl;
        std::cout << "\t-l\t--list\t\t\tList all platforms and devices" << std::endl;
        std::cout << "\t-p\t--platform[=ID]\t\tPlatform to use" << std::endl;
        std::cout << "\t-d\t--device[=ID]\t\tDevice to use" << std::endl;
        if (!input_image_required)
        {
            std::cout << "\t-t\t--testSize[=size]\tSize of input for the program" << std::endl;
        }
        exit(1);
	}
}
