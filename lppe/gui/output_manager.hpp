#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <thread>

#include <opencv4/opencv2/core/mat.hpp>

#include "vector_converters.hpp"

class OutputManager {
    public:
        OutputManager() = default;
        ~OutputManager() = default;

        void drawGui(const VectorData& vector_data, GLuint *button_textures);
        cv::Mat getViewImage() const;
        bool isCalculatingViewImage() const {
            std::lock_guard<std::mutex> lock(mtx);
            return isCalculating;
        }
        bool getOutputData(VectorData& out_data) const;
    private:
        void startCalculation(const VectorData& vector_data);
        void renderViewImage(const VectorData& vector_data, VectorData& output_data, cv::Mat& img);

        bool double_mode = false;
        bool add_border = false;
        std::string paper_size = "A4";
        int paper_width = 210; // mm
        int paper_height = 297; // mm
        int paper_margin = 20; // mm
        int direction = 0; // 0-359
        bool allow_center_drift = true;
        int size_percent = 100; // 1-100

        cv::Mat view_img;
        VectorData vector_data_copy;
        VectorData output_data;
        bool isCalculating = false;

        mutable std::mutex mtx;
};
