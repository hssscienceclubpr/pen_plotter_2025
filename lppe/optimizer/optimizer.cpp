#include "optimizer.hpp"

#include <iostream>
#include <limits>
#include <cmath>
#include <algorithm>

// テストのために、そのままコピーするだけ
static void no_optimize(const unoptimized_path& input, draw_path& output) {
    output.paths.clear();
    output.color_names = input.color_names;
    for(const auto& [color_id, polylines] : input.polylines) {
        for(const auto& pts : polylines) {
            if(!pts.empty()) {
                output.paths[color_id].push_back(pts);
            }
        }
    }
    for(const auto& [color_id, contours] : input.contours) {
        for(const auto& pts : contours) {
            if(!pts.empty()) {
                std::vector<point> closed_pts = pts;
                if(pts.front() != pts.back() && pts.size() >= 2) {
                    // 閉じていない輪郭は閉じる
                    closed_pts.push_back(pts.front());
                }
                output.paths[color_id].push_back(closed_pts);
            }
        }
    }
}

// ユーティリティ関数: 2点間の距離の二乗を計算（平方根計算を避けるため）
static float distance_sq(const point& a, const point& b) {
    float dx = a.first - b.first;
    float dy = a.second - b.second;
    return dx * dx + dy * dy;
}

// パス要素を格納し、反転が必要かどうかを識別するためのヘルパ構造体
struct path_element {
    std::vector<point> pts;
    bool is_open; // true: polyline (反転可能), false: contour
};

/**
 * @brief 貪欲法を用いて、色ごとに描画パスの順序を最適化する。
 * 開いた線(polyline)については、反転の可能性も考慮し、ペン上げ移動距離を最小化する。
 * @param input 最適化前の描画要素
 * @param output 最適化後の描画パス
 */
static void greedy_optimize(const unoptimized_path& input, draw_path& output) {
    output.paths.clear();
    output.color_names = input.color_names;

    // 初期ペン位置として原点(0, 0)を設定
    const point initial_pos = {0.0f, 0.0f};

    // 色ごとに最適化を実行
    for (const auto& [color_id, color_name] : input.color_names) {
        // 1. その色の全ての要素を一つのリストに集める
        std::vector<path_element> all_elements_for_color;

        // 1-1. polylines (開いた線: is_open = true) の追加
        if (input.polylines.count(color_id)) {
            for (const auto& pts : input.polylines.at(color_id)) {
                if (pts.size() >= 2) {
                    all_elements_for_color.push_back({pts, true});
                }
            }
        }

        // 1-2. contours (閉じた線: is_open = false) の追加（必要なら閉じる）
        if (input.contours.count(color_id)) {
            for (const auto& pts : input.contours.at(color_id)) {
                if (pts.size() >= 2) {
                    std::vector<point> closed_pts = pts;
                    // 閉じていない輪郭を閉じる
                    if (pts.front() != pts.back() && pts.size() >= 2) {
                        closed_pts.push_back(pts.front());
                    }
                    // contourは本質的に閉じたパスとして扱い、反転は不要（または意味がない）とする
                    all_elements_for_color.push_back({closed_pts, false}); 
                }
            }
        }

        // 2. 貪欲法によるパスの最適化
        if (all_elements_for_color.empty()) {
            continue;
        }

        std::vector<std::vector<point>> optimized_paths;
        point current_pos = initial_pos;
        
        // 最適化のために使用する一時的なパスリスト
        std::vector<bool> path_used(all_elements_for_color.size(), false);
        int paths_remaining = all_elements_for_color.size();

        while (paths_remaining > 0) {
            float min_dist_sq = -1.0f;
            int best_path_index = -1;
            bool reverse_best_path = false; // 最適なパスを反転させるかどうか

            // 現在のペン位置から最も近い開始点を持つパスを探す
            for (int i = 0; i < all_elements_for_color.size(); ++i) {
                if (path_used[i]) {
                    continue;
                }

                const auto& element = all_elements_for_color[i];
                const auto& path = element.pts;

                // 2-1. パスの「順方向」の始点までの距離を評価
                const point& start_fwd = path.front();
                float dist_fwd_sq = distance_sq(current_pos, start_fwd);

                if (best_path_index == -1 || dist_fwd_sq < min_dist_sq) {
                    min_dist_sq = dist_fwd_sq;
                    best_path_index = i;
                    reverse_best_path = false;
                }

                // 2-2. 開いた線(polyline)の場合のみ、「逆方向」の始点（元の終点）までの距離を評価
                if (element.is_open && path.size() >= 2) {
                    const point& start_rev = path.back(); // 逆方向の始点
                    float dist_rev_sq = distance_sq(current_pos, start_rev);

                    if (dist_rev_sq < min_dist_sq) {
                        min_dist_sq = dist_rev_sq;
                        best_path_index = i;
                        reverse_best_path = true; // 反転を選択
                    }
                }
            }

            // 3. 最適なパスを選択・反転し、リストに追加
            if (best_path_index != -1) {
                std::vector<point> next_path = all_elements_for_color[best_path_index].pts;
                
                if (reverse_best_path) {
                    // 反転が必要な場合、パスの要素を反転させる
                    std::reverse(next_path.begin(), next_path.end());
                }
                
                optimized_paths.push_back(next_path);
                
                // ペン位置を次のパスの終点に更新
                current_pos = next_path.back();
                
                // 使用済みとしてマーク
                path_used[best_path_index] = true;
                paths_remaining--;
            } else {
                break;
            }
        }

        // 4. 結果をoutputに格納
        output.paths[color_id] = std::move(optimized_paths);
    }
}

static void beam_search_optimize_fast(const unoptimized_path& input,
                                      draw_path& output,
                                      int beam_width,
                                      int top_k) {
    output.paths.clear();
    output.color_names = input.color_names;

    auto distance = [](const point& a, const point& b) -> float {
        float dx = a.first - b.first;
        float dy = a.second - b.second;
        return std::sqrt(dx*dx + dy*dy);
    };

    for (const auto& [color_id, color_name] : input.color_names) {
        std::vector<std::vector<point>> candidates;

        // 開いた線
        if (auto it = input.polylines.find(color_id); it != input.polylines.end()) {
            for (const auto& pts : it->second)
                if (pts.size() >= 2) candidates.push_back(pts);
        }
        // 閉じた輪郭
        if (auto it = input.contours.find(color_id); it != input.contours.end()) {
            for (const auto& pts : it->second) {
                std::vector<point> closed_pts = pts;
                if (pts.front() != pts.back()) closed_pts.push_back(pts.front());
                candidates.push_back(closed_pts);
            }
        }

        if (candidates.empty()) {
            output.paths[color_id] = {};
            continue;
        }

        // ビーム状態
        struct BeamNode {
            std::vector<std::vector<point>> seq; // 選択パス
            std::vector<bool> used;              // 使用済みフラグ
            point current_end;                   // ペン位置
            float total_length;
        };

        BeamNode initial;
        initial.seq.push_back(candidates.front());
        initial.used.resize(candidates.size(), false);
        initial.used[0] = true;
        initial.current_end = candidates.front().back();
        initial.total_length = 0.0f;

        std::vector<BeamNode> beam = {initial};

        for (size_t step = 1; step < candidates.size(); ++step) {
            std::vector<BeamNode> next_beam;

            for (const auto& node : beam) {
                // 距離を計算して top_k のみ展開
                struct CandidateInfo { size_t idx; bool reverse_flag; float dist; };
                std::vector<CandidateInfo> dists;

                for (size_t i = 0; i < candidates.size(); ++i) {
                    if (node.used[i]) continue;
                    const auto& pts = candidates[i];
                    if (pts.empty()) continue;

                    // 開いた線の反転を考慮
                    float d_front = distance(node.current_end, pts.front());
                    float d_back  = distance(node.current_end, pts.back());
                    if (d_front <= d_back)
                        dists.push_back({i, false, d_front});
                    else
                        dists.push_back({i, true, d_back});
                }

                // top_k に制限
                std::sort(dists.begin(), dists.end(),
                          [](const CandidateInfo& a, const CandidateInfo& b){ return a.dist < b.dist; });
                int expand_limit = std::min((int)dists.size(), top_k);

                for (int j = 0; j < expand_limit; ++j) {
                    auto& c = dists[j];
                    std::vector<point> chosen = candidates[c.idx];
                    if (c.reverse_flag) std::reverse(chosen.begin(), chosen.end());

                    BeamNode new_node = node;
                    new_node.seq.push_back(chosen);
                    new_node.used[c.idx] = true;
                    new_node.current_end = chosen.back();
                    new_node.total_length += c.dist;
                    next_beam.push_back(std::move(new_node));
                }
            }

            // ビーム幅制限
            std::sort(next_beam.begin(), next_beam.end(),
                      [](const BeamNode& a, const BeamNode& b){ return a.total_length < b.total_length; });
            if ((int)next_beam.size() > beam_width) next_beam.resize(beam_width);
            beam = std::move(next_beam);
        }

        // 最短経路を安全に格納
        if (!beam.empty()) {
            std::vector<std::vector<point>> ordered;
            for (auto& pts : beam.front().seq)
                if (pts.size() >= 2) ordered.push_back(std::move(pts));
            output.paths[color_id] = std::move(ordered);
        } else {
            output.paths[color_id] = {};
        }
    }
}

static void greedy_optimize_free_start(const unoptimized_path& input,
                                       draw_path& output) {
    output.paths.clear();
    output.color_names = input.color_names;

    auto distance = [](const point& a, const point& b) -> float {
        float dx = a.first - b.first;
        float dy = a.second - b.second;
        return std::sqrt(dx*dx + dy*dy);
    };

    for (const auto& [color_id, color_name] : input.color_names) {
        std::vector<std::vector<point>> candidates;

        // 開いた線
        if (auto it = input.polylines.find(color_id); it != input.polylines.end()) {
            for (const auto& pts : it->second)
                if (pts.size() >= 2) candidates.push_back(pts);
        }
        // 閉じた輪郭
        if (auto it = input.contours.find(color_id); it != input.contours.end()) {
            for (const auto& pts : it->second) {
                std::vector<point> closed_pts = pts;
                if (pts.front() != pts.back()) closed_pts.push_back(pts.front());
                candidates.push_back(closed_pts);
            }
        }

        if (candidates.empty()) {
            output.paths[color_id] = {};
            continue;
        }

        std::vector<std::vector<point>> best_seq;
        float best_total_length = std::numeric_limits<float>::max();

        // 最初の線を全候補から選択
        for (size_t start_idx = 0; start_idx < candidates.size(); ++start_idx) {
            std::vector<bool> used(candidates.size(), false);
            std::vector<std::vector<point>> seq;
            seq.push_back(candidates[start_idx]);
            used[start_idx] = true;
            point current_end = candidates[start_idx].back();
            float total_length = 0.0f;

            // 残りを貪欲法で選択
            for (size_t step = 1; step < candidates.size(); ++step) {
                float min_dist = std::numeric_limits<float>::max();
                size_t next_idx = candidates.size();
                bool reverse_flag = false;

                for (size_t i = 0; i < candidates.size(); ++i) {
                    if (used[i]) continue;
                    const auto& pts = candidates[i];
                    if (pts.empty()) continue;

                    float d_front = distance(current_end, pts.front());
                    float d_back  = distance(current_end, pts.back());
                    if (d_front < min_dist) { min_dist = d_front; next_idx = i; reverse_flag = false; }
                    if (d_back  < min_dist) { min_dist = d_back; next_idx = i; reverse_flag = true; }
                }

                if (next_idx == candidates.size()) break; // 残りなし

                std::vector<point> next_pts = candidates[next_idx];
                if (reverse_flag) std::reverse(next_pts.begin(), next_pts.end());

                seq.push_back(next_pts);
                used[next_idx] = true;
                current_end = next_pts.back();
                total_length += min_dist;
            }

            if (total_length < best_total_length) {
                best_total_length = total_length;
                best_seq = std::move(seq);
            }
        }

        output.paths[color_id] = std::move(best_seq);
    }
}

static void greedy_optimize_nlookahead(const unoptimized_path& input,
                                       draw_path& output,
                                       size_t n) {
    output.paths.clear();
    output.color_names = input.color_names;

    auto distance = [](const point& a, const point& b) -> float {
        float dx = a.first - b.first;
        float dy = a.second - b.second;
        return std::sqrt(dx*dx + dy*dy);
    };

    for (const auto& [color_id, color_name] : input.color_names) {
        std::vector<std::vector<point>> candidates;

        // 開いた線
        if (auto it = input.polylines.find(color_id); it != input.polylines.end()) {
            for (const auto& pts : it->second)
                if (pts.size() >= 2) candidates.push_back(pts);
        }
        // 閉じた輪郭
        if (auto it = input.contours.find(color_id); it != input.contours.end()) {
            for (const auto& pts : it->second) {
                std::vector<point> closed_pts = pts;
                if (pts.front() != pts.back()) closed_pts.push_back(pts.front());
                candidates.push_back(closed_pts);
            }
        }

        if (candidates.empty()) {
            output.paths[color_id] = {};
            continue;
        }

        std::vector<std::vector<point>> best_seq;
        float best_total_length = std::numeric_limits<float>::max();

        // 各候補を最初の線として試す
        for (size_t start_idx = 0; start_idx < candidates.size(); ++start_idx) {
            std::vector<bool> used(candidates.size(), false);
            std::vector<std::vector<point>> seq;
            seq.push_back(candidates[start_idx]);
            used[start_idx] = true;
            point current_end = candidates[start_idx].back();
            float total_length = 0.0f;

            // 最初の n ステップまで貪欲探索
            for (size_t step = 1; step < std::min(n, candidates.size()); ++step) {
                float min_dist = std::numeric_limits<float>::max();
                size_t next_idx = candidates.size();
                bool reverse_flag = false;

                for (size_t i = 0; i < candidates.size(); ++i) {
                    if (used[i]) continue;
                    const auto& pts = candidates[i];
                    if (pts.empty()) continue;

                    float d_front = distance(current_end, pts.front());
                    float d_back  = distance(current_end, pts.back());
                    if (d_front < min_dist) { min_dist = d_front; next_idx = i; reverse_flag = false; }
                    if (d_back  < min_dist) { min_dist = d_back; next_idx = i; reverse_flag = true; }
                }

                if (next_idx == candidates.size()) break; // 残りなし

                std::vector<point> next_pts = candidates[next_idx];
                if (reverse_flag) std::reverse(next_pts.begin(), next_pts.end());

                seq.push_back(next_pts);
                used[next_idx] = true;
                current_end = next_pts.back();
                total_length += min_dist;
            }

            // n ステップまでの距離で最良の開始線を選択
            if (total_length < best_total_length) {
                best_total_length = total_length;
                best_seq.clear();
                best_seq.push_back(candidates[start_idx]);
            }
        }

        // 残りは通常の貪欲法で追加
        std::vector<bool> used(candidates.size(), false);
        used[std::distance(candidates.begin(), std::find(candidates.begin(), candidates.end(), best_seq.front()))] = true;
        point current_end = best_seq.front().back();

        while (true) {
            float min_dist = std::numeric_limits<float>::max();
            size_t next_idx = candidates.size();
            bool reverse_flag = false;

            for (size_t i = 0; i < candidates.size(); ++i) {
                if (used[i]) continue;
                const auto& pts = candidates[i];
                if (pts.empty()) continue;

                float d_front = distance(current_end, pts.front());
                float d_back  = distance(current_end, pts.back());
                if (d_front < min_dist) { min_dist = d_front; next_idx = i; reverse_flag = false; }
                if (d_back  < min_dist) { min_dist = d_back; next_idx = i; reverse_flag = true; }
            }

            if (next_idx == candidates.size()) break;

            std::vector<point> next_pts = candidates[next_idx];
            if (reverse_flag) std::reverse(next_pts.begin(), next_pts.end());

            best_seq.push_back(next_pts);
            used[next_idx] = true;
            current_end = next_pts.back();
        }

        output.paths[color_id] = std::move(best_seq);
    }
}

void Optimizer::optimize_greedy(const unoptimized_path& input, draw_path& output) const{
    //greedy_optimize(input, output);
    //no_optimize(input, output);
    //beam_search_optimize_fast(input, output, 12, 8);
    //greedy_optimize_free_start(input, output);
    greedy_optimize_nlookahead(input, output, 3);
}

void Optimizer::optimize_beam_search(const unoptimized_path& input, draw_path& output) const{
    beam_search_optimize_fast(input, output, 12, 8);
}
