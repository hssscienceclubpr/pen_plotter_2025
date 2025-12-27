#pragma once

#include <opencv4/opencv2/core/mat.hpp>

enum class GrayscaleMode {
    Lab,
    Default,
};
void convertToGrayScale(const cv::Mat& src, cv::Mat& dst, GrayscaleMode mode = GrayscaleMode::Lab);
void bilateral(const cv::Mat& src, cv::Mat& dst, int diameter, double sigmaColor, double sigmaSpace, int loops);
