#include "filter_funcs.hpp"

#include <iostream>

#include <opencv4/opencv2/core/mat.hpp>
#include <opencv4/opencv2/imgproc.hpp>

void convertToGrayScale(const cv::Mat& src, cv::Mat& dst, GrayscaleMode mode){
    cv::Mat floatMat, labMat, floatGray, gray;

    switch (mode){
    case GrayscaleMode::Lab:
        src.convertTo(floatMat, CV_32FC3, 1.0 / 255.0);
        cv::cvtColor(floatMat, labMat, cv::COLOR_BGR2Lab);
        cv::extractChannel(labMat, floatGray, 0);
        floatGray.convertTo(gray, CV_8UC1, 255.0 / 100.0);
        cv::cvtColor(gray, dst, cv::COLOR_GRAY2BGR);
        break;
    case GrayscaleMode::Default:
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(gray, dst, cv::COLOR_GRAY2BGR);
        break;
    default:
        std::cerr << "convertToGrayScale : invaild mode (" << static_cast<int>(mode) << ")" << std::endl;
        break;
    }
}

void bilateral(const cv::Mat& src, cv::Mat& dst, int diameter, double sigmaColor, double sigmaSpace, int loops) {
    cv::Mat current = src.clone();
    for (int i = 0; i < loops; ++i) {
        cv::bilateralFilter(current, dst, diameter, sigmaColor, sigmaSpace);
        if(i < loops - 1){
            current = dst.clone();
        }
    }
}
