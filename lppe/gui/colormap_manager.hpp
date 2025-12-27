#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <thread>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <opencv4/opencv2/core/mat.hpp>

#include "shell_manager.hpp"

enum class ColorMapMode {
    COLOR_MAP_MODE_BINARY,
    COLOR_MAP_MODE_MULTI,
};

class ColorMapManager {
public:
    void drawGui(GLuint *button_textures);
    bool isGeneratable() const { return generatable; }
    void startGenerateColorMap(const cv::Mat& src);
    bool isCalculating() const;
    bool isNewestColorMapAvailable() const;
    ColorMap getColorMap() const;
    cv::Mat getViewMap() const;
private:
    Colors colormap_colors = {
        {"Red", "FF0000"},
        {"Green", "00FF00"},
        {"Blue", "0000FF"},
        {"Cyan", "00FFFF"},
        {"Magenta", "FF00FF"},
        {"Yellow", "FFFF00"},
    };
    Colors achro_colors = {
        {"black", "000000"},
        {"gray", "555555"},
        {"light gray", "AAAAAA"},
        {"white", "FFFFFF"},
    };
    ColorMapMode current_colormap_mode = ColorMapMode::COLOR_MAP_MODE_BINARY;
    int threshold = 160; // for binary mode
    bool detect_achro = true;
    int achros = 3; // 2(black/white),3(black/gray/white),4(black/gray/light gray/white)
    std::array<int, 4> achro_thresholds = {85, 170, 255, 255};
    float achro_sensitivity = 0.15f; // 0.0 ~ 1.0

    bool generatable = true;

    cv::Mat generating_src;
    ColorMap generating_colormap;
    cv::Mat view_map;
    bool calculating = false;
    bool newest_colormap_available = false;
    mutable std::mutex mtx;
};
