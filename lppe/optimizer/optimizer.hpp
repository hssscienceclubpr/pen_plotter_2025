#pragma once

#include <vector>
#include <map>
#include <string>

using point = std::pair<float, float>; // x, y  (mm)

struct draw_path {
    // それぞれの色ごとに、パスの列を保持する
    // それぞれのパスは、ペンをおろしている間の点の列
    std::map<int, std::vector<std::vector<point>>> paths;
    std::map<int, std::string> color_names; // 色番号 → 色名
};
struct unoptimized_path {
    std::map<int, std::vector<std::vector<point>>> polylines;
    std::map<int, std::vector<std::vector<point>>> contours;
    std::map<int, std::string> color_names; // 色番号 → 色名
};

class Optimizer {
public:
    // paths: 色番号 → パスの列
    // 各パスは、ペンをおろしている間の点の列
    // color_names: 色番号 → 色名
    void optimize_greedy(const unoptimized_path& input, draw_path& output) const;
    void optimize_beam_search(const unoptimized_path& input, draw_path& output) const;
};
