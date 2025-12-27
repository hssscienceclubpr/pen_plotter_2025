#pragma once

#include <map>
#include <vector>

#include <opencv4/opencv2/core/mat.hpp>

/*
Colormap Generator
少ない数の色にsrcをマッピングする
*/

struct ColorMap {
    cv::Mat colorMap;
    std::map<int, cv::Mat> MapOfColor;
    std::map<int, std::string> MapOfColorName;
    std::map<int, cv::Scalar> MapOfColorValueBGR;

    ColorMap clone() const {
        ColorMap copy;
        copy.colorMap = colorMap.clone();
        for (const auto& [key, mat] : MapOfColor) {
            copy.MapOfColor[key] = mat.clone();
        }
        copy.MapOfColorName = MapOfColorName;
        copy.MapOfColorValueBGR = MapOfColorValueBGR;
        return copy;
    }
};

using Colors = std::vector<std::pair<std::string, std::string>>; // first: name, second: hex color code

void generateBinaryColorMap(const cv::Mat& src, int threshold, ColorMap& colorMap, cv::Mat& viewMap);
void generateMultiColorMap(const cv::Mat& src, Colors& colors, ColorMap& colorMap, cv::Mat& viewMap);
void generateAchroColorMap(const cv::Mat& src, float achro_sensitivity, std::vector<float>& achro_thresholds,
    Colors& achro_colors, Colors& colors, ColorMap& colorMap, cv::Mat& viewMap);
bool convertHexToBGR(const std::string& hex, int &b, int &g, int &r);
