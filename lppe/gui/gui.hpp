#pragma once

#include <string>
#include <chrono>
#include <functional>
#include <vector>
#include <map>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"

#include <opencv4/opencv2/core/mat.hpp>

#include "img/colormap_generator.hpp"
#include "img/vector_data.hpp"

class Filter{
    public:
        Filter();
        virtual std::unique_ptr<Filter> clone() const = 0;
        virtual void apply(const cv::Mat& src, cv::Mat& dst) = 0;
        virtual bool drawGui() = 0; // return true if parameters changed
        virtual std::string getFilterName() const = 0;
        int unique_id;
};
class FilterRegistry {
    public:
        using Creator = std::function<std::unique_ptr<Filter>()>;

        static FilterRegistry& instance() {
            static FilterRegistry registry;
            return registry;
        }

        void registerFilter(const std::string& name, Creator creator);

        std::vector<std::string> getFilterNames() const;
        std::unique_ptr<Filter> createFilter(const int i) const;
        int getFilterCount() const;
    
    private:
        FilterRegistry() = default;
        std::vector<std::string> filter_names;
        std::vector<Creator> creators;
};
// マクロでフィルタ登録を簡略化
#define REGISTER_BEGIN \
    namespace { \
        struct FilterRegistrar { \
            FilterRegistrar() {
#define REGISTER_END \
            } \
        }; \
        static FilterRegistrar global_filter_registrar; \
    }
#define REGISTER_FILTER(name, class_type) \
    FilterRegistry::instance().registerFilter(name, []() { return std::make_unique<class_type>(); });

class VectorConverter {
    public:
        VectorConverter();
        virtual std::unique_ptr<VectorConverter> clone() const = 0;
        virtual void apply(const cv::Mat& original, const ColorMap& colorMap, const cv::Mat& modeMap, const int mode, VectorData& vectorData) = 0;
        virtual bool drawGui() = 0; // return true if parameters changed
        virtual std::string getConverterName() const = 0;
        int unique_id;
};
class VectorConverterRegistry {
    public:
        using Creator = std::function<std::unique_ptr<VectorConverter>()>;

        static VectorConverterRegistry& instance() {
            static VectorConverterRegistry registry;
            return registry;
        }

        void registerConverter(const std::string& name, Creator creator);

        std::vector<std::string> getConverterNames() const;
        std::unique_ptr<VectorConverter> createConverter(const int i) const;
        int getConverterCount() const;
    
    private:
        VectorConverterRegistry() = default;
        std::vector<std::string> converter_names;
        std::vector<Creator> creators;
};
// マクロでコンバータ登録を簡略化
#define REGISTER_CONVERTER_BEGIN \
    namespace { \
        struct ConverterRegistrar { \
            ConverterRegistrar() {
#define REGISTER_CONVERTER_END \
            } \
        }; \
        static ConverterRegistrar global_converter_registrar; \
    }
#define REGISTER_CONVERTER(name, class_type) \
    VectorConverterRegistry::instance().registerConverter(name, []() { return std::make_unique<class_type>(); });

const std::vector<cv::Scalar> converter_gui_colors = {
    cv::Scalar(255, 0, 0),   // Red
    cv::Scalar(0, 255, 0),   // Green
    cv::Scalar(0, 0, 255),   // Blue
    cv::Scalar(255, 255, 0), // Cyan
    cv::Scalar(255, 0, 255), // Magenta
    cv::Scalar(0, 255, 255), // Yellow
    cv::Scalar(255, 165, 0), // Orange
    cv::Scalar(128, 0, 128), // Purple
    cv::Scalar(0, 128, 128), // Teal
    cv::Scalar(128, 128, 0)  // Olive
};

void setup();
void end();
void drawGui(float fps);
void cleanUp();
void processInput(GLFWwindow* window);
void getTargetFrameTime(std::chrono::nanoseconds& frame_time);
