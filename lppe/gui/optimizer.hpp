#pragma once

#include "img/vector_data.hpp"
#include "optimizer/optimizer.hpp"

#include <thread>
#include <mutex>

class OptimizerGui {
public:
    void drawGui(const VectorData& data);
    bool getOptimizedData(draw_path& out_paths);
    cv::Mat getViewImage() const;
    bool isCalculating() const { return calculating; }
private:
    void write_path_to_file(const std::string& filename, const draw_path& path);
    bool calculating = false;
    VectorData data_copy;
    draw_path optimized_paths;
    bool data_available = false;
    cv::Mat view_img;
    std::string analysis;

    bool beam_search = false;

    mutable std::mutex mtx;
};
