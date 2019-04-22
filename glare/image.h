/*
    Adapted from Tizian Zeltner's Tone Mapper 
*/

#ifndef IMAGE_H
#define IMAGE_H

#include "vector_types.h"

#include <iostream>
#include <algorithm>

class Image {
public:
    explicit Image(const std::string &filename);
    ~Image();

    // float* get_rChannelFFT()  { return m_redFFT;}
    // float* get_gChannelFFT()  { return m_greenFFT;}
    // float* get_bChannelFFT()  { return m_blueFFT;}

    float* get_rChannel()  { return m_red;}
    float* get_gChannel()  { return m_green;}
    float* get_bChannel()  { return m_blue;}

    // cv::Mat get_imgData() {return channels;}

    static void fromLayersToRGBA(unsigned char* data,
                        float* red,
                        float* green,
                        float* blue, 
                        int width,
                        int height);

    static void fromLayersToRGBAf(float* data,
                        float* red,
                        float* green,
                        float* blue, 
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

    float* m_red;
    float* m_green;
    float* m_blue;

    float* m_redPadded;
    float* m_greenPadded;
    float* m_bluePadded;

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