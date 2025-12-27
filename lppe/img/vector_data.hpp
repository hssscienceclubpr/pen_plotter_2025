#pragma once

#include <map>
#include <vector>

#include <opencv4/opencv2/core.hpp>

struct HatchLineSetting {
    int spacing = -1; // px
    std::string mode = "/"; // [/-\|+x]
    std::string substitute_color = ""; // 代わりに使う色
};

struct VectorData {
    std::map<int, std::vector<std::vector<cv::Point2f>>> polylines;
    std::map<int, std::vector<std::vector<cv::Point2f>>> contours;
    std::map<int, std::vector<std::vector<cv::Point2f>>> hatch_lines;
    std::map<int, cv::Mat> filled_masks; // 計算によりhatch_linesに変換される
    std::map<int, cv::Mat> edge_masks; // 計算によりpolylinesに変換される
    std::map<int, cv::Mat> outline_masks; // 枠線を付けたい領域が塗られたcv::Matであり、計算によりcontoursに変換される
    std::map<int, std::string> color_names; // 色番号 → 色名
    std::map<int, cv::Scalar> color_values; // 色番号 → BGR
    int width = 0, height = 0;
};

cv::Mat removeSmallComponents(const cv::Mat& binaryImage, int minSize = 3);
void canny(const cv::Mat& src, cv::Mat& edges, int lowThreshold = 100, int highThreshold = 200);
void extractEdgeFromGroupMap(const cv::Mat& gmap, cv::Mat& edges);
void classifyPixels(const cv::Mat& binary, cv::Mat& lines, cv::Mat& thinned_lines, cv::Mat& filled, cv::Mat& vis, int r=7);
void lastConvertToVectorData(VectorData& data, cv::Mat& view_map, cv::Mat& view_map_with_points, cv::Mat& view_map_with_hatch, cv::Mat& view_random_colored, int hatchLineSpacing, int hatchLineAngle, int minSize, float jitterEpsilon, float minPolylineLength, const std::map<std::string, HatchLineSetting>& hatchLineSettings);

void visualize(const VectorData& data, cv::Mat& view_map, cv::Mat& view_map_with_points, cv::Mat& view_map_with_hatch, cv::Mat& view_random_colored, int N);
