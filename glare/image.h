/*
    Adapted from Tizian Zeltner's Tone Mapper 
*/

#ifndef IMAGE_H
#define IMAGE_H

#include "vector_types.h"

#include <iostream>
#include <algorithm>

// include FFT related routines

#include <viennacl/fft.hpp>
#include <viennacl/linalg/fft_operations.hpp>

#include <viennacl/vector.hpp>
#include <viennacl/matrix.hpp>

class Image {
public:
    explicit Image(const std::string &filename);
    ~Image();

    // float4 *getData() { return (float4 *)m_pixels; }


    viennacl::matrix<float>& get_rChannelFFT()  { return m_rChannelFFT;}
    viennacl::matrix<float>& get_gChannelFFT()  { return m_gChannelFFT;}
    viennacl::matrix<float>& get_bChannelFFT()  { return m_bChannelFFT;}

    // cv::Mat get_imgData() {return channels;}

    static void fromLayersToRGBA(unsigned char* data,
                        viennacl::matrix<float>& red,
                        viennacl::matrix<float>& green,
                        viennacl::matrix<float>& blue, 
                        int width,
                        int height);

    static void fromLayersToRGBAf(float* data,
                        viennacl::matrix<float>& red,
                        viennacl::matrix<float>& green,
                        viennacl::matrix<float>& blue, 
                        int width,
                        int height);

    inline float3 getAverageIntensity() const { return {m_averageIntensity_r, m_averageIntensity_g, m_averageIntensity_b}; }
    inline float getMinimumLuminance() const { return m_minimumLuminance; }
    inline float getMaximumLuminance() const { return m_maximumLuminance; }
    inline float getAverageLuminance() const { return m_averageLuminance; }
    inline float getLogAverageLuminance() const { return m_logAverageLuminance; }
    inline float getAutoKeyValue() const { return m_autoKeyValue; }

    inline int getWidth() const { return m_width; }
    inline int getHeight() const { return m_height; }

    // TODO: build PNG saving function 

private:
    
    // viennacl::matrix<float> m_rChannel;
    // viennacl::matrix<float> m_gChannel;
    // viennacl::matrix<float> m_bChannel;

    // viennacl::matrix<float> m_rChannelFFT;
    // viennacl::matrix<float> m_gChannelFFT;
    // viennacl::matrix<float> m_bChannelFFT;

    float* m_red;
    float* m_green;
    float* m_blue;


    int m_width;
    int m_height;

    float m_averageIntensity_r;
    float m_averageIntensity_g;
    float m_averageIntensity_b;
    float m_minimumLuminance;
    float m_maximumLuminance;
    float m_averageLuminance;
    float m_logAverageLuminance;
    float m_autoKeyValue;

    
};

#endif // IMAGE_H