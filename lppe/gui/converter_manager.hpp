#pragma once

#include <string>
#include <vector>
#include <mutex>

#include <opencv4/opencv2/core/mat.hpp>

#include "vector_converters.hpp"
#include "shell_manager.hpp"

class ConverterManager {
public:
    ConverterManager();

    void drawGui(int& selected_index, GLuint *button_textures);
    bool isCalculating() const;
    bool isNewestDataAvailable() const;
    void changedInput();
    void startCalculation(const cv::Mat& original, const ColorMap& colorMap, const cv::Mat& mode_map, const ShellManager& shell_manager);
    void getVectorData(VectorData& outData) const;
    bool getViewMaps(cv::Mat& viewMap, cv::Mat& viewMapWithPoints, cv::Mat& viewMapWithHatch, cv::Mat& viewRandomColored) const;

private:
    std::vector<std::unique_ptr<VectorConverter>> converters;
    std::vector<int> converter_ids;
    cv::Mat original_copy;
    ColorMap colorMap_copy;
    cv::Mat mode_map_copy;
    ShellManager shell_manager_copy;
    int selected_converter_id = -1;
    int gui_selected_index = 0;
    int hatch_line_spacing = 10;
    float no_jitter_epsilon = 4.0; // px
    float min_polyline_length = 0.0; // px
    bool calculating = false;
    bool newest_data_available = false;
    VectorData vector_data;
    cv::Mat view_map, view_map_with_points, view_map_with_hatch, view_random_colored;
    mutable std::mutex mtx;
};
