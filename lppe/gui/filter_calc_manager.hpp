#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <thread>

#include <opencv4/opencv2/core/mat.hpp>

#include "gui.hpp"

class FilterCalcManager {
public:
    FilterCalcManager() = default;
    FilterCalcManager(const FilterCalcManager&) = delete;
    FilterCalcManager& operator=(const FilterCalcManager&) = delete;

    void startCalculation(std::vector<std::unique_ptr<Filter>>& filters, const cv::Mat& inputImage);
    bool isCalculating() const;
    int getLastCompletedFilterIndex() const;
    std::vector<cv::Mat> getProcessedImages() const;
    bool isNewestImageAvailable() const;
    void changedInput();
private:
    std::vector<std::unique_ptr<Filter>> filters_copy;
    cv::Mat inputImage_copy;
    std::vector<cv::Mat> processedImages;
    bool calculating = false;
    bool newestImageAvailable = false;
    size_t lastCompletedFilterIndex = -1;
    mutable std::mutex mtx;
};
