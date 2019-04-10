/*
    Adapted from Tizian Zeltner's Tone Mapper 
*/

#include "image.h"

#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>

float convert(void *p, int offset, int type) {
    switch(type) {
    case TINYEXR_PIXELTYPE_UINT: {
        int32_t *pix = (int32_t *)p;
        return (float)pix[offset];
    }
    case TINYEXR_PIXELTYPE_HALF:
    case TINYEXR_PIXELTYPE_FLOAT: {
        float *pix = (float *)p;
        return pix[offset];
    }
    default:
        return 0.f;
    }
}


Image::Image(const std::string &filename) {
    EXRImage img;
    InitEXRImage(&img);

    const char *err = nullptr;
    if (ParseMultiChannelEXRHeaderFromFile(&img, filename.c_str(), &err) != 0) {
        std::cerr << "Error: Could not parse EXR file: " << err << std::endl;
        return;
    }

    for (int i = 0; i < img.num_channels; ++i) {
        if (img.requested_pixel_types[i] == TINYEXR_PIXELTYPE_HALF)
            img.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
    }

    if (LoadMultiChannelEXRFromFile(&img, filename.c_str(), &err) != 0) {
        std::cerr << "Error: Could not open EXR file: " << err << std::endl;
        return;
    }

    m_width = img.width;
    m_height= img.height;

    m_red   = new float[m_width * m_height];
    m_green = new float[m_width * m_height];
    m_blue  = new float[m_width * m_height];

    int idxR = -1, idxG = -1, idxB = -1;
    for (int c = 0; c < img.num_channels; ++c) {
        if (strcmp(img.channel_names[c], "R") == 0) {
            idxR = c;
        }
        if (strcmp(img.channel_names[c], "G") == 0) {
            idxG = c;
        }
        if (strcmp(img.channel_names[c], "B") == 0) {
            idxB = c;
        }
    }

    float rgb;
    for (int i = 0; i < m_height; ++i) {
        for (int j = 0; j < m_width; ++j) {
            int index = m_width * i + j;

            if (img.num_channels == 1) {
                rgb = convert(img.images[0], index, img.pixel_types[0]);
                m_red[index]   = rgb;
                m_green[index] = rgb;
                blue[index]  = rgb;
            }
            else {
                m_red[index]      = convert(img.images[idxR], index, img.pixel_types[idxR]);
                m_green[index]    = convert(img.images[idxG], index, img.pixel_types[idxG]);
                m_blue[index]     = convert(img.images[idxB], index, img.pixel_types[idxB]);
            }
        }
    }

    float delta = 1e-4f;
    m_averageLuminance = 0.f;
    m_logAverageLuminance = 0.f;
    m_minimumLuminance = std::numeric_limits<float>::max();
    m_maximumLuminance = std::numeric_limits<float>::min();
    m_averageIntensity_r = 0.0f;
    m_averageIntensity_g = 0.0f;
    m_averageIntensity_b = 0.0f;

    for (int i = 0; i < m_height; ++i) {
        for (int j = 0; j < m_width; ++j) {
            int index = m_width * i + j;
            m_averageIntensity_r += m_red[index];
            m_averageIntensity_g += m_green[index];
            m_averageIntensity_b += m_blue[index];
            float lum = m_red[index] * 0.212671f + m_green[index] * 0.715160f + m_blue[index] * 0.072169f;
            m_averageLuminance += lum;
            m_logAverageLuminance += std::log(delta + lum);
            if (lum > m_maximumLuminance) m_maximumLuminance = lum;
            if (lum < m_minimumLuminance) m_minimumLuminance = lum;
        }
    }

    m_averageIntensity_r = m_averageIntensity_r / (m_width * m_height);
    m_averageIntensity_g = m_averageIntensity_g / (m_width * m_height);
    m_averageIntensity_b = m_averageIntensity_b / (m_width * m_height);
    m_averageLuminance = m_averageLuminance / (m_width * m_height);
    m_logAverageLuminance = std::exp(m_logAverageLuminance / (m_width * m_height));

    // Formula taken from "Perceptual Effects in Real-time Tone Mapping" by Krawczyk et al.
    m_autoKeyValue = 1.03f - 2.f / (2.f + std::log10(m_logAverageLuminance + 1.f));

    // m_rChannel = viennacl::matrix<float>(m_red, viennacl::MAIN_MEMORY, m_height, m_width);
    // m_gChannel = viennacl::matrix<float>(m_green, viennacl::MAIN_MEMORY, m_height, m_width);
    // m_bChannel = viennacl::matrix<float>(m_blue, viennacl::MAIN_MEMORY, m_height, m_width);

    FreeEXRImage(&img);


    // compute the FFT transforms of the image only once, when loaded
    // viennacl::fft(m_rChannel, m_rChannelFFT, -1.0f);
    // viennacl::fft(m_gChannel, m_gChannelFFT, -1.0f);
    // viennacl::fft(m_bChannel, m_bChannelFFT, -1.0f);
}

inline float clamp(float x, float a, float b)
{
    return x < a ? a : (x > b ? b : x);
}

void Image::fromLayersToRGBA(unsigned char* data,
                        viennacl::matrix<float>& red,
                        viennacl::matrix<float>& green,
                        viennacl::matrix<float>& blue,
                        int width,
                        int height)
{
    if(red.size1() != green.size1() || green.size1() != blue.size1() || blue.size1() != red.size1())
    {
        // assert smth bad
        std::cout << "BAD";
        return;
    }

    if(red.size2() != green.size2() || green.size2() != blue.size2() || blue.size2() != red.size2())
    {
        // assert smth bad
        std::cout << "BAD";
        return;
    }

    int pixels = width * height;
    int maxIndex = width * height * 4;
    int currentIndex = 0;
    unsigned char* p = data;

    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width && currentIndex < maxIndex; ++j) 
        {
            p[0] = (unsigned char)(clamp(red(i,j)*255/pixels, 0, 255));
            p[1] = (unsigned char)(clamp(green(i,j)*255/pixels, 0, 255));
            p[2] = (unsigned char)(clamp(blue(i,j)*255/pixels, 0, 255));
            p[3] = 255;

            currentIndex+=4;
            p+=4;
        }
    }
}

void Image::fromLayersToRGBAf(float* data,
                        viennacl::matrix<float>& red,
                        viennacl::matrix<float>& green,
                        viennacl::matrix<float>& blue,
                        int width,
                        int height)
{
    if(red.size1() != green.size1() || green.size1() != blue.size1() || blue.size1() != red.size1())
    {
        // assert smth bad
        std::cout << "BAD";
        return;
    }

    if(red.size2() != green.size2() || green.size2() != blue.size2() || blue.size2() != red.size2())
    {
        // assert smth bad
        std::cout << "BAD";
        return;
    }

    float pixels = width * height;
    int maxIndex = width * height * 4;
    int currentIndex = 0;
    float* p = data;

    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width && currentIndex < maxIndex; ++j) 
        {
            p[0] = clamp(red(i,j)/pixels, 0.0f, 1.0f);
            p[1] = clamp(green(i,j)/pixels, 0.0f, 1.0f);
            p[2] = clamp(blue(i,j)/pixels, 0.0f, 1.0f);
            p[3] = 1.0f;

            currentIndex+=4;
            p+=4;
        }
    }



}

Image::~Image()
{
    delete[] m_red;
    delete[] m_green;
    delete[] m_blue;
}