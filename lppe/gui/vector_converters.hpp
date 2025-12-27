#pragma once

#include "gui.hpp"
#include "img/colormap_generator.hpp"

class EmptyConverter : public VectorConverter {
    public:
        EmptyConverter();
        std::unique_ptr<VectorConverter> clone() const {
            return std::make_unique<EmptyConverter>(*this);
        }
        void apply(const cv::Mat& original, const ColorMap& colorMap, const cv::Mat& modeMap, const int mode, VectorData& outData) override;
        bool drawGui() override;
        std::string getConverterName() const { return "Empty"; }
};

class EdgeConverter : public VectorConverter {
    public:
        EdgeConverter();
        std::unique_ptr<VectorConverter> clone() const {
            return std::make_unique<EdgeConverter>(*this);
        }
        void apply(const cv::Mat& original, const ColorMap& colorMap, const cv::Mat& modeMap, const int mode, VectorData& outData) override;
        bool drawGui() override;
        std::string getConverterName() const { return "Edge"; }
    private:
        int min_size = 1;
        int opening_radius = 0;
};

class FillConverter : public VectorConverter {
    public:
        FillConverter();
        FillConverter(const FillConverter&) = default;
        std::unique_ptr<VectorConverter> clone() const {
            return std::make_unique<FillConverter>(*this);
        }
        void apply(const cv::Mat& original, const ColorMap& colorMap, const cv::Mat& modeMap, const int mode, VectorData& outData) override;
        bool drawGui() override;
        std::string getConverterName() const { return "Fill"; }
    private:
        bool outline_mode = false;
        std::string canny_mode = "";
        int low_threshold = 100;
        int high_threshold = 200;
        std::string color_edges = "";
        std::string back_outline = "";
        int closing_radius = 0;
        int erosion_radius = 0;
        int min_size = 1;
        int opening_radius = 0;
};

class LineAndFillConverter : public VectorConverter {
    public:
        LineAndFillConverter();
        std::unique_ptr<VectorConverter> clone() const {
            return std::make_unique<LineAndFillConverter>(*this);
        }
        void apply(const cv::Mat& original, const ColorMap& colorMap, const cv::Mat& modeMap, const int mode, VectorData& outData) override;
        bool drawGui() override;
        std::string getConverterName() const { return "Line and Fill"; }
    private:
        bool outline_mode = false;
        int radius = 7;
        int min_size = 1;
        int opening_radius = 0;
};

class OutlineConverter : public VectorConverter {
    public:
        OutlineConverter();
        std::unique_ptr<VectorConverter> clone() const {
            return std::make_unique<OutlineConverter>(*this);
        }
        void apply(const cv::Mat& original, const ColorMap& colorMap, const cv::Mat& modeMap, const int mode, VectorData& outData) override;
        bool drawGui() override;
        std::string getConverterName() const { return "Outline"; }
    private:
        int min_size = 1;
        int opening_radius = 0;
};
