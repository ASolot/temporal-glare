/*
    Adapted from Tizian Zeltner's Tone Mapper 
*/

#include "image.h"


// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #include <stb_image_write.h>

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

    float *red   = new float[m_width * m_height];
    float *green = new float[m_width * m_height];
    float *blue  = new float[m_width * m_height];

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
                red[index]   = rgb;
                green[index] = rgb;
                blue[index]  = rgb;
            }
            else {
                red[index]      = convert(img.images[idxR], index, img.pixel_types[idxR]);
                green[index]    = convert(img.images[idxG], index, img.pixel_types[idxG]);
                blue[index]     = convert(img.images[idxB], index, img.pixel_types[idxB]);
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
            m_averageIntensity_r += red[index];
            m_averageIntensity_g += green[index];
            m_averageIntensity_b += blue[index];
            float lum = red[index] * 0.212671f + green[index] * 0.715160f + blue[index] * 0.072169f;
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

    m_rChannel = viennacl::matrix<float>(red, viennacl::MAIN_MEMORY, m_height, m_width);
    m_gChannel = viennacl::matrix<float>(green, viennacl::MAIN_MEMORY, m_height, m_width);
    m_bChannel = viennacl::matrix<float>(blue, viennacl::MAIN_MEMORY, m_height, m_width);

    FreeEXRImage(&img);
    delete[] red;
    delete[] green;
    delete[] blue;

    // compute the FFT transforms of the image only once, when loaded
    viennacl::fft(m_rChannel, m_rChannelFFT);
    viennacl::fft(m_gChannel, m_gChannelFFT);
    viennacl::fft(m_bChannel, m_bChannelFFT);
}