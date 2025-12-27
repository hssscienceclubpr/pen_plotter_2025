#include "optimizer.hpp"

#include <iostream>
#include <fstream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// ImGui
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <opencv4/opencv2/core/mat.hpp>
#include <opencv4/opencv2/imgproc/imgproc.hpp>

#include "cross/cross.hpp"

static unoptimized_path convertToUnoptimizedPath(const VectorData& data) {
    // polylines -> polylines
    // contours -> contours
    // hatch_lines -> polylines

    unoptimized_path u_path;
    u_path.polylines.clear();
    u_path.contours.clear();
    u_path.color_names = data.color_names;

    std::map<int, std::vector<std::vector<point>>> polylines_pts;

    for(const auto& [color_id, lines] : data.polylines) {
        for(const auto& line : lines) {
            std::vector<point> pts;
            pts.reserve(line.size());
            for(const auto& pt : line) {
                pts.emplace_back(pt.x, pt.y);
            }
            if(!pts.empty()) {
                polylines_pts[color_id].push_back(pts);
            }
        }
    }
    for(const auto& [color_id, lines] : data.hatch_lines) {
        for(const auto& line : lines) {
            std::vector<point> pts;
            pts.reserve(line.size());
            for(const auto& pt : line) {
                pts.emplace_back(pt.x, pt.y);
            }
            if(!pts.empty()) {
                polylines_pts[color_id].push_back(pts);
            }
        }
    }
    u_path.polylines = polylines_pts;

    for(const auto& [color_id, lines] : data.contours) {
        for(const auto& line : lines) {
            std::vector<point> pts;
            pts.reserve(line.size());
            for(const auto& pt : line) {
                pts.emplace_back(pt.x, pt.y);
            }
            if(!pts.empty()) {
                u_path.contours[color_id].push_back(pts);
            }
        }
    }

    return u_path;
}
static void analyzePath(const VectorData& vector_data, const draw_path& path, cv::Mat& view_img, std::string& analysis, int N) {
    int total_points = 0;
    int total_paths = 0;

    for(const auto& [color_id, paths] : path.paths) {
        total_paths += paths.size();
        for(const auto& pts : paths) {
            total_points += pts.size();

            // 折れ線の点数チェック
            assert(!pts.empty() && "Path contains empty points vector");
        }
    }

    analysis = "Optimized Path Analysis:\n";
    analysis += "Total Paths: " + std::to_string(total_paths) + "\n";
    analysis += "Total Points: " + std::to_string(total_points) + "\n";

    float total_length = 0.0f;
    for(const auto& [color_id, paths] : path.paths) {
        if(paths.empty()) continue;
        for(const auto& pts : paths) {
            assert(pts.size() >= 2 && "Polyline must have at least 2 points for length calculation");

            for(int i = 0; i < pts.size() - 1; ++i) {
                total_length += std::sqrt(std::pow(pts[i+1].first - pts[i].first, 2) +
                                          std::pow(pts[i+1].second - pts[i].second, 2));
            }
        }

        // パス間の移動距離
        for(int i = 0; i < paths.size() - 1; ++i) {
            assert(!paths[i].empty() && "Previous path is empty");
            assert(!paths[i+1].empty() && "Next path is empty");
            total_length += std::sqrt(std::pow(paths[i+1].front().first - paths[i].back().first, 2) +
                                      std::pow(paths[i+1].front().second - paths[i].back().second, 2));
        }
    }

    analysis += "Total Length: " + std::to_string(total_length) + "\n";
    analysis += "(" + std::to_string(static_cast<int>(std::round(total_length / 1000.0f))) + " meters)\n";

    view_img = cv::Mat::zeros(N * vector_data.height, N * vector_data.width, CV_8UC3);
    view_img.setTo(cv::Scalar(255,255,255));

    for(const auto& [color_id, paths] : path.paths) {
        for(const auto& pts : paths) {
            if(pts.size() < 2) continue;

            std::vector<cv::Point> scaled;
            scaled.reserve(pts.size());
            for(const auto& pt : pts) {
                scaled.emplace_back(static_cast<int>(pt.first * N), static_cast<int>(pt.second * N));
            }
            cv::polylines(view_img, scaled, false, cv::Scalar(0, 0, 0), 1, cv::LINE_AA);
        }

        if(paths.size() < 2) continue;

        for(int i = 0; i < paths.size() - 1; ++i) {
            assert(!paths[i].empty() && "Previous path is empty for red line");
            assert(!paths[i+1].empty() && "Next path is empty for red line");
            cv::line(view_img,
                cv::Point(static_cast<int>(paths[i].back().first * N), static_cast<int>(paths[i].back().second * N)),
                cv::Point(static_cast<int>(paths[i+1].front().first * N), static_cast<int>(paths[i+1].front().second * N)),
                cv::Scalar(0, 0, 255), 1, cv::LINE_AA
            );
        }
    }
}

void OptimizerGui::drawGui(const VectorData& data) {
    std::lock_guard<std::mutex> lock(mtx);
    if(calculating) {
        ImGui::Text("Calculating... Please wait.");
        return;
    }
    data_copy = data;

    ImGui::Text("Optimize the drawing paths for shorter travel distance.");
    if(ImGui::Button("Start Optimization")) {
        calculating = true;
        optimized_paths.paths.clear();
        optimized_paths.color_names = data.color_names;
        std::thread([this]() {
            draw_path result;
            Optimizer optimizer;
            auto u_path = convertToUnoptimizedPath(this->data_copy);
            if(this->beam_search) {
                optimizer.optimize_beam_search(u_path, result);
            } else {
                optimizer.optimize_greedy(u_path, result);
            }
            std::cout << "Optimization completed." << std::endl;
            cv::Mat view_img;
            std::string analysis;
            analyzePath(this->data_copy, result, view_img, analysis, 5);
            std::cout << "Analysis:\n" << analysis << std::endl;
            {
                std::lock_guard<std::mutex> lock(this->mtx);
                this->optimized_paths = result;
                this->calculating = false;
                this->data_available = true;
                this->view_img = view_img;
                this->analysis = analysis;
            }
        }).detach();
    }

    if(ImGui::Checkbox("Use Beam Search (slower, better)", &beam_search)){}

    ImGui::Dummy(ImVec2(0,10));
    if(data_available) {
        ImGui::Text("%s", analysis.c_str());
    }

    ImGui::Dummy(ImVec2(0,20));
    if(ImGui::Button("Write to file")){
        write_path_to_file(getExecutableDir() + "/output/optimized_path.txt", optimized_paths);
    }
}

bool OptimizerGui::getOptimizedData(draw_path& out_paths) {
    std::lock_guard<std::mutex> lock(mtx);
    if(data_available){
        out_paths = optimized_paths;
        return true;
    }else{
        return false;
    }
}

cv::Mat OptimizerGui::getViewImage() const {
    std::lock_guard<std::mutex> lock(mtx);
    if(data_copy.width <= 0 || data_copy.height <= 0){
        return cv::Mat();
    }
    return view_img;
}

void OptimizerGui::write_path_to_file(const std::string& filename, const draw_path& path) {
    /*
    ファイル構造：
    N: 色の数 (1-64)
    n: next polyline
    e: end of color
    color name: 64文字以下
    x y: 座標(mm) float

    N
    color 0 name
    color 1 name
    ...
    color (N-1) name
    color 0 data
    color 1 data
    ...
    color (N-1) data

    color data:
    x y
    x y
    ...
    n
    x y
    x y
    ...
    n
    ...
    e
    */

    std::ofstream ofs(filename);
    if(!ofs){
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return;
    }

    if(optimized_paths.paths.empty()){
        std::cerr << "No optimized paths to write." << std::endl;
        return;
    }
    if(optimized_paths.color_names.empty()){
        std::cerr << "No color names available." << std::endl;
        return;
    }
    if(optimized_paths.color_names.size() > 64){
        std::cerr << "Too many colors to write (max 64)." << std::endl;
        return;
    }
    std::vector<int> valid_color_ids;
    for(const auto& [color_id, paths] : optimized_paths.paths) {
        if(paths.size() > 0){
            valid_color_ids.push_back(color_id);
        }
    }
    ofs << valid_color_ids.size() << std::endl;
    for(const auto& color_id : valid_color_ids) {
        ofs << optimized_paths.color_names[color_id] << std::endl;
    }
    for(const auto& color_id : valid_color_ids) {
        const auto& paths = optimized_paths.paths[color_id];
        for(size_t i = 0; i < paths.size(); ++i) {
            for(const auto& pt : paths[i]) {
                ofs << pt.first << " " << pt.second << std::endl;
            }
            if(i + 1 < paths.size()){
                ofs << "n" << std::endl; // next polyline
            }
        }
        ofs << "e" << std::endl; // end of color
    }

    ofs.close();
    std::cout << "Optimized path written to " << filename << std::endl;
}
