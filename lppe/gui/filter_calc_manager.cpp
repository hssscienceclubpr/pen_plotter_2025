#include "filter_calc_manager.hpp"

#include <iostream>
#include <thread>
#include <mutex>

// 与えられたfilterと画像のコピーを取り、スレッドを作って計算を開始する
void FilterCalcManager::startCalculation(std::vector<std::unique_ptr<Filter>>& filters, const cv::Mat& inputImage) {
    std::lock_guard<std::mutex> lock(mtx);
    if (calculating) {
        std::cerr << "Calculation is already in progress." << std::endl;
        return; // すでに計算中なら何もしない
    }
    // フィルターと画像のコピーを作成
    filters_copy.clear();
    filters_copy.reserve(filters.size());
    for (const auto& filter : filters) {
        filters_copy.push_back(filter->clone());
    }
    inputImage_copy = inputImage.clone();
    processedImages.clear();
    lastCompletedFilterIndex = -1;
    calculating = true;
    newestImageAvailable = true;

    // スレッドを作成して計算を開始
    std::thread([this]() {
        cv::Mat currentImage = inputImage_copy.clone();
        for (size_t i = 0; i < filters_copy.size(); ++i) {
            cv::Mat dst;
            filters_copy[i]->apply(currentImage, dst);
            {
                std::lock_guard<std::mutex> lock(mtx);
                processedImages.push_back(dst);
                lastCompletedFilterIndex = i; // 最後に完了したフィルターのインデックスを更新
            }
            currentImage = std::move(dst); // 次のフィルターの入力として使用
        }
        {
            std::lock_guard<std::mutex> lock(mtx);
            calculating = false;
        }
    }).detach();
}

bool FilterCalcManager::isCalculating() const {
    std::lock_guard<std::mutex> lock(mtx);
    return calculating;
}

bool FilterCalcManager::isNewestImageAvailable() const {
    std::lock_guard<std::mutex> lock(mtx);
    return newestImageAvailable;
}

void FilterCalcManager::changedInput() {
    std::lock_guard<std::mutex> lock(mtx);
    newestImageAvailable = false;
}

int FilterCalcManager::getLastCompletedFilterIndex() const {
    std::lock_guard<std::mutex> lock(mtx);
    return static_cast<int>(lastCompletedFilterIndex);
}

std::vector<cv::Mat> FilterCalcManager::getProcessedImages() const {
    std::lock_guard<std::mutex> lock(mtx);
    return processedImages;
}
