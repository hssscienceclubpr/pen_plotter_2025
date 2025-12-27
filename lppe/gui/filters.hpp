#pragma once

#include "gui.hpp"

#include <opencv4/opencv2/core/mat.hpp>

#include "img/filter_funcs.hpp"

class GrayscaleFilter : public Filter {
    public:
        GrayscaleFilter();
        std::unique_ptr<Filter> clone() const {
            return std::make_unique<GrayscaleFilter>(*this);
        }
        void apply(const cv::Mat& src, cv::Mat& dst) override;
        bool drawGui() override;
        std::string getFilterName() const override { return "Grayscale"; }
    private:
        GrayscaleMode mode = GrayscaleMode::Lab;
};

class BilateralFilter : public Filter {
    public:
        BilateralFilter();
        std::unique_ptr<Filter> clone() const {
            return std::make_unique<BilateralFilter>(*this);
        }
        void apply(const cv::Mat& src, cv::Mat& dst) override;
        bool drawGui() override;
        std::string getFilterName() const override { return "Bilateral"; }
    private:
        int diameter = 3;
        float sigmaColor = 15.0f;
        float sigmaSpace = 10.0f;
        int loops = 10;
};
