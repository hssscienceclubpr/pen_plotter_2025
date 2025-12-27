#include "vector_data.hpp"

#include <iostream>
#include <list>
#include <set>
#include <atomic>

#include <opencv4/opencv2/imgproc.hpp>
#include <opencv4/opencv2/ximgproc.hpp>

// gemini
static
std::vector<std::vector<cv::Point2f>> generateHatchLines(const cv::Mat& filled, int lineSpacing = 2, int angleDegree = 45) {
    CV_Assert(filled.type() == CV_8UC1);

    std::vector<std::vector<cv::Point2f>> hatchLines;

    // 領域のバウンディングボックス取得
    cv::Rect bbox = cv::boundingRect(filled);
    if (bbox.empty()) return hatchLines; // 領域が空なら終了

    // ラジアン変換と回転行列の計算
    double angleRad = angleDegree * CV_PI / 180.0;
    double cosA = std::cos(angleRad);
    double sinA = std::sin(angleRad);
    
    // 回転の中心をbboxの中心とする
    cv::Point2f center(bbox.x + bbox.width / 2.0f, bbox.y + bbox.height / 2.0f);

    // bboxを完全に包含する、回転前の仮想的な領域の対角線長を計算
    // これにより、ハッチング線がbbox全体を確実にカバーする
    int extendedLen = static_cast<int>(std::ceil(std::sqrt(bbox.width * bbox.width + bbox.height * bbox.height)));

    // マスク作成（ROI）
    // roiMaskはfilled全体でなくbbox領域のみを参照するように修正
    cv::Mat roiMask = filled(bbox);
    
    // ハッチング線は、角度 0° (水平線) の線群を center を中心に angleDegree 回転したものと考える
    // 回転前の線群の開始位置（垂直方向）を計算
    // bboxの左上(bbox.tl())からextendedLen/2の距離を始点、終点とし、線が中心を通るようにオフセットを調整
    
    // y方向の開始オフセットを計算
    int y_start = -extendedLen / 2;
    int y_end = extendedLen / 2;

    // lineSpacing 間隔で回転前の水平線を生成
    for (int offset_y = y_start; offset_y < y_end; offset_y += lineSpacing) {
        
        // 回転前の線分（中心を原点(0,0)とした座標系）
        cv::Point2f p1_local(-extendedLen / 2.0f, (float)offset_y);
        cv::Point2f p2_local(extendedLen / 2.0f, (float)offset_y);
        
        // 点の間隔は1pxとして大きめにとる
        int steps = extendedLen; 
        std::vector<cv::Point2f> linePoints;

        for (int i = 0; i <= steps; ++i) {
            // 線上の点 (0,0) を原点とするローカル座標系
            float px_local = p1_local.x + i * (p2_local.x - p1_local.x) / steps;
            float py_local = p1_local.y + i * (p2_local.y - p1_local.y) / steps;
            
            // angleDegree で回転
            // X' = X cos(A) - Y sin(A)
            // Y' = X sin(A) + Y cos(A)
            float rx = px_local * cosA - py_local * sinA;
            float ry = px_local * sinA + py_local * cosA;
            
            // 画像座標系に戻す（回転中心を考慮）
            // center.x + rx, center.y + ry
            int px_img = static_cast<int>(std::round(center.x + rx));
            int py_img = static_cast<int>(std::round(center.y + ry));

            // bbox座標系に変換
            int px = px_img - bbox.x;
            int py = py_img - bbox.y;

            // bbox 内の点だけを考慮
            if (px >= 0 && px < bbox.width && py >= 0 && py < bbox.height) {
                // マスクが塗られているかチェック (roiMaskはCV_8UC1)
                if (roiMask.at<uchar>(py, px) > 0) {
                    // 塗られている部分の座標を追加 (原画像座標系)
                    linePoints.emplace_back((float)px_img, (float)py_img);
                } else {
                    // 塗られていない領域であれば、線分を区切る
                    if (linePoints.size() >= 2) {
                        // 線分の端点のみを記録
                        hatchLines.push_back({linePoints.front(), linePoints.back()});
                    }
                    linePoints.clear();
                }
            } else {
                // bbox外の点であれば、線分を区切る
                 if (linePoints.size() >= 2) {
                    hatchLines.push_back({linePoints.front(), linePoints.back()});
                 }
                linePoints.clear();
            }
        }

        // ループ終了後の最後の線分追加
        if (linePoints.size() >= 2) {
            hatchLines.push_back({linePoints.front(), linePoints.back()});
        }
    }

    return hatchLines;
}

cv::Mat removeSmallComponents(const cv::Mat& binaryImage, int minSize) {
    cv::Mat labels, stats, centroids;
    int numComponents = cv::connectedComponentsWithStats(binaryImage, labels, stats, centroids, 8);

    std::vector<uchar> removeMask(numComponents, 0);
    for (int i = 1; i < numComponents; ++i) {
        if (stats.at<int>(i, cv::CC_STAT_AREA) < minSize) {
            removeMask[i] = 1;
        }
    }

    cv::Mat filtered = binaryImage.clone();
    for (int y = 0; y < labels.rows; ++y) {
        const int* lblPtr = labels.ptr<int>(y);
        uchar* dstPtr = filtered.ptr<uchar>(y);
        for (int x = 0; x < labels.cols; ++x) {
            if (removeMask[lblPtr[x]]) {
                dstPtr[x] = 0;
            }
        }
    }

    return filtered;
}

static
std::vector<std::vector<cv::Point2f>> simplifyPolylines(
    const std::vector<std::vector<cv::Point2f>>& contours,
    double epsilon = 0.7,
    bool closed = false
) {
    std::vector<std::vector<cv::Point2f>> simplified;

    for (const auto& contour : contours) {
        if (contour.size() < 2){
            continue;
        }else if(contour.size() == 2) {
            simplified.push_back(contour);
            continue;
        }

        std::vector<cv::Point2f> approx;
        if(closed){
            cv::approxPolyDP(contour, approx, epsilon, true);
        }else {
            cv::Point2f first = contour.front();
            cv::Point2f last = contour.back();
            if(first == last) {
                std::vector<cv::Point2f> closedContour(contour.begin(), contour.end() - 1);
                cv::approxPolyDP(closedContour, approx, epsilon, true);
                approx.push_back(approx.front());
            } else {
                cv::approxPolyDP(contour, approx, epsilon, false);
            }
        }

        simplified.push_back(approx);
    }

    return simplified;
}

static
std::vector<cv::Point2f> subdividePolyline(const std::vector<cv::Point2f>& polyline, int N, bool closed = false) {
    std::vector<cv::Point2f> subdivided;
    if (polyline.size() < 2 || N < 1) return polyline;

    size_t count = polyline.size();
    for (size_t i = 0; i < count - 1; ++i) {
        const cv::Point2f& p1 = polyline[i];
        const cv::Point2f& p2 = polyline[i + 1];
        for (int j = 0; j < N; ++j) {
            float t = static_cast<float>(j) / N;
            cv::Point2f pt = p1 + t * (p2 - p1);
            subdivided.push_back(pt);
        }
    }

    if (closed) {
        const cv::Point2f& p1 = polyline.back();
        const cv::Point2f& p2 = polyline.front();
        for (int j = 0; j < N; ++j) {
            float t = static_cast<float>(j) / N;
            cv::Point2f pt = p1 + t * (p2 - p1);
            subdivided.push_back(pt);
        }
    }

    if(!closed && !polyline.empty()){
        subdivided.push_back(polyline.back());
    }

    return subdivided;
}

static
std::vector<std::vector<cv::Point2f>> subdividePolylines(
    const std::vector<std::vector<cv::Point2f>>& polylines,
    int N,
    bool closed = false)
{
    std::vector<std::vector<cv::Point2f>> result;
    result.reserve(polylines.size());

    for (const auto& polyline : polylines) {
        result.push_back(subdividePolyline(polyline, N, closed));
    }

    return result;
}

static
std::vector<std::vector<cv::Point2f>> polylinesIntToFloat2f(const std::vector<std::vector<cv::Point>>& polylines) {
    std::vector<std::vector<cv::Point2f>> polylines2f;
    polylines2f.reserve(polylines.size());
    for (const auto& polyline : polylines) {
        std::vector<cv::Point2f> polyline2f;
        polyline2f.reserve(polyline.size());
        for (const auto& point : polyline) {
            polyline2f.emplace_back(static_cast<float>(point.x), static_cast<float>(point.y));
        }
        polylines2f.push_back(polyline2f);
    }

    return polylines2f;
}

static
std::vector<std::vector<cv::Point>> extractPolylines(const cv::Mat& lines) {
    CV_Assert(lines.type() == CV_8UC1);

    cv::Mat visited = cv::Mat::zeros(lines.size(), CV_8U);
    std::vector<std::vector<cv::Point>> polylines;

    const uchar* lines_ptr = lines.ptr<uchar>(0);
    uchar* visited_ptr = visited.ptr<uchar>(0);

    const int width = lines.cols;

    auto at_lines = [&](int x, int y) -> uchar {
        return lines_ptr[y * width + x];
    };
    auto at_visited = [&](int x, int y) -> uchar& {
        return visited_ptr[y * width + x];
    };

/*
    8近傍
         3 2 1
         4   0
         5 6 7
*/
    const int dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    const int dy[8] = {0, -1, -1, -1, 0, 1, 1, 1};

    const int dir_priorities[][8] = {
        {0, 2, 4, 6, 1, 3, 5, 7}, // 隣り合った方向を優先
        {0, 2, 6, 4, 1, 7, 3, 5}, // 0方向を優先
        {0, 2, 4, 6, 1, 3, 7, 5}, // 1方向を優先
        {2, 0, 4, 6, 1, 3, 7, 5}, // 2方向を優先
        {2, 4, 0, 6, 3, 1, 5, 7}, // 3方向を優先
        {4, 2, 6, 0, 3, 5, 1, 7}, // 4方向を優先
        {4, 6, 0, 2, 5, 3, 7, 1}, // 5方向を優先
        {6, 4, 0, 2, 5, 7, 3, 1}, // 6方向を優先
        {0, 6, 2, 4, 7, 1, 5, 3}, // 7方向を優先
        {0, 6, 2, 4, 7, 1, 3, 5}, // 0(7)方向を優先
        {0, 2, 6, 4, 1, 7, 3, 5}, // 0(1)方向を優先
        {2, 0, 4, 6, 1, 3, 7, 5}, // 2(1)方向を優先
        {2, 4, 0, 6, 3, 1, 5, 7}, // 2(3)方向を優先
        {4, 2, 6, 0, 3, 5, 1, 7}, // 4(3)方向を優先
        {4, 6, 2, 0, 5, 3, 7, 1}, // 4(5)方向を優先
        {6, 4, 0, 2, 5, 7, 3, 1}, // 6(5)方向を優先
        {6, 0, 4, 2, 7, 5, 1, 3}, // 6(7)方向を優先
    };

    auto isValid = [&](int x, int y) {
        return x >= 0 && x < lines.cols && y >= 0 && y < lines.rows;
    };

    std::vector<cv::Point> points;
    cv::findNonZero(lines, points);

    int l = 0; // dir_prioritiesの行番号

    int last_2_dir[2] = {-1, -1};

    for (auto& p : points) {
        if(at_visited(p.x, p.y) != 0){
            continue;
        }

        std::array<std::vector<cv::Point>, 2> polyline;

        at_visited(p.x, p.y) = 1;

        for (int k = 0; k < 2; ++k) {
            cv::Point q = p;
            polyline[k].push_back(p);

            for(;;) {
                if(last_2_dir[0] == -1){
                    l = 0;
                }else if(last_2_dir[1] == -1){
                    l = last_2_dir[0] + 1;
                }else if(last_2_dir[0] == last_2_dir[1]){
                    l = last_2_dir[0] + 1;
                }else{
                    int d = last_2_dir[1] - last_2_dir[0];
                    if(d < 0) d += 8;
                    if(d == 1 || d == 7){
                        int odd, even;
                        if(last_2_dir[0] % 2 == 0){
                            even = last_2_dir[0];
                            odd = last_2_dir[1];
                        }else{
                            even = last_2_dir[1];
                            odd = last_2_dir[0];
                        }
                        l = 9 + even;
                        if(odd == even + 1){
                            ++l;
                        }
                    }else{
                        l = last_2_dir[0] + 1;
                    }
                }

                bool foundNext = false;
                for (int i = 0; i < 8; ++i) {
                    int dir = dir_priorities[l][i];
                    int nx = q.x + dx[dir];
                    int ny = q.y + dy[dir];
                    if (isValid(nx, ny) && at_lines(nx, ny) == 255 && at_visited(nx, ny) == 0) {
                        q = cv::Point(nx, ny);
                        polyline[k].push_back(q);
                        at_visited(nx, ny) = 1;
                        foundNext = true;

                        last_2_dir[1] = last_2_dir[0];
                        last_2_dir[0] = dir;
                        break;
                    }
                }

                if (!foundNext) {
                    break; // 次の点が見つからなければ終了
                }
            }
        }

        if(polyline[1].size() > 1) {
            std::reverse(polyline[0].begin(), polyline[0].end());
            polyline[0].insert(polyline[0].end(), polyline[1].begin() + 1, polyline[1].end());
        }

        for(int k = 0; k < 2; ++k) {
            cv::Point q, r;
            if(k == 0){
                q = polyline[0].front();
            }else{
                q = polyline[0].back();
                if(q == polyline[0].front()) {
                    continue; // 始点と終点が同じ場合はスキップ
                }
            }

            int i;
            bool found = false;
            for(i = 0; i < 8; ++i) {
                int dir = dir_priorities[0][i];
                int nx = q.x + dx[dir];
                int ny = q.y + dy[dir];
                if (isValid(nx, ny) && at_lines(nx, ny) == 255 && at_visited(nx, ny) == 1) {
                    r = cv::Point(nx, ny);
                    auto it = std::find(polyline[0].begin(), polyline[0].end(), r);
                    if(it != polyline[0].end()) {
                        if(k == 0) {
                            if(std::distance(polyline[0].begin(), it) < 4) {
                                continue;
                            }
                        }else{
                            if(std::distance(it, polyline[0].end()) < 4) {
                                continue;
                            }
                        }
                    }
                    found = true;
                    break;
                }
            }

            if(found) {
                if(k == 0) {
                    polyline[0].insert(polyline[0].begin(), r);
                }else{
                    polyline[0].push_back(r);
                }
            }
        }

        if(polyline[0].size() >= 2) {
            polylines.push_back(polyline[0]);
        }

    }

    return polylines;
}

static
void extractContoursFromFilled(const cv::Mat& filled, std::vector<std::vector<cv::Point>>& polylines, std::vector<std::vector<cv::Point>>& contours) {
    CV_Assert(filled.type() == CV_8UC1);

    polylines.clear();
    contours.clear();

    std::vector<std::vector<cv::Point>> raw_contours;
    std::vector<cv::Vec4i> hierarchy; // unused
    cv::findContours(filled, raw_contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);

    std::vector<std::vector<cv::Point>> lines; // polyline or contour
    std::vector<cv::Point> line;
    std::vector<std::vector<cv::Point>> temp_lines_ring;
    bool prevIsEdge = false;
    cv::Point lastEdgePoint;
    for (auto raw_contour : raw_contours) {
        line.clear();
        lines.clear();
        prevIsEdge = false;
        for(size_t i = 0; i < raw_contour.size(); ++i) {
            cv::Point pt = raw_contour[i];
            if(pt.x <= 0 || pt.x >= filled.cols - 1 || pt.y <= 0 || pt.y >= filled.rows - 1) {
                if(line.empty()){ // if prevIsEdge is true, it means line is empty
                    lastEdgePoint = pt;
                }else{
                    line.push_back(pt);
                    lines.push_back(line);
                    line.clear();
                    lastEdgePoint = pt;
                }
                prevIsEdge = true;
            }else{
                if(prevIsEdge){
                    line.push_back(lastEdgePoint);
                    line.push_back(pt);
                }else{
                    line.push_back(pt);
                }
                prevIsEdge = false;
            }
        }
        if(line.size() >= 2){
            if(lines.empty()){ // contour
                lines.push_back(line);
            }else{
                line.push_back(lines.front().front()); // close
                lines.push_back(line);
            }
        }else if(prevIsEdge){
            lines.push_back({lastEdgePoint, line.front()});
        }

        if(lines.size() == 1 && lines[0].size() >= 3){
            contours.push_back(lines[0]);
        }else if(lines.size() > 1){
            temp_lines_ring.clear();
            bool first_skip = false;
            for(size_t i = 0; i < lines.size(); ++i) {
                if(temp_lines_ring.empty()) {
                    temp_lines_ring.push_back(lines[i]);
                }else if(lines[i].front() == temp_lines_ring.back().back()) {
                    //temp_lines_ring.back().insert(temp_lines_ring.back().end(), lines[i].begin() + 1, lines[i].end());
                    // it's faster to use reserve + copy than insert
                    temp_lines_ring.back().reserve(temp_lines_ring.back().size() + lines[i].size() - 1);
                    std::copy(lines[i].begin() + 1, lines[i].end(), std::back_inserter(temp_lines_ring.back()));
                }else{
                    temp_lines_ring.push_back(lines[i]);
                }

                // if temp_lines_ring.size() <= 1, it means there is only a contour.
                if(i == lines.size() - 1 && temp_lines_ring.size() > 1 && temp_lines_ring.front().front() == temp_lines_ring.back().back()) {
                    //temp_lines_ring.back().insert(temp_lines_ring.back().end(), temp_lines_ring.front().begin() + 1, temp_lines_ring.front().end());
                    temp_lines_ring.back().reserve(temp_lines_ring.back().size() + temp_lines_ring.front().size() - 1);
                    std::copy(temp_lines_ring.front().begin() + 1, temp_lines_ring.front().end(), std::back_inserter(temp_lines_ring.back()));
                    //temp_lines_ring.erase(temp_lines_ring.begin());
                    // (maybe,) it's faster to use first skip flag than erase
                    first_skip = true;
                }
            }

            if(temp_lines_ring.size() == 1 && temp_lines_ring[0].size() >= 3){
                contours.push_back(temp_lines_ring[0]);
            }else if(temp_lines_ring.size() > 1){
                // polylines.insert(polylines.end(), temp_lines_ring.begin(), temp_lines_ring.end());
                polylines.reserve(polylines.size() + temp_lines_ring.size());
                if(first_skip) {
                    std::copy(temp_lines_ring.begin() + 1, temp_lines_ring.end(), std::back_inserter(polylines));
                }else{
                    std::copy(temp_lines_ring.begin(), temp_lines_ring.end(), std::back_inserter(polylines));
                }
            }
        }
    }
}

std::vector<cv::Point2f> removePolylineJitter(const std::vector<cv::Point2f>& polyline, bool closed, double epsilon) {
    if(closed) {
        if(polyline.size() < 5) return polyline;
        const int size = static_cast<int>(polyline.size());
        std::vector<bool> clockwise(size, false); // それぞれの角が時計回りか反時計回りか
        for (size_t i = 0; i < size; ++i) {
            cv::Point2f v1 = polyline[i] - polyline[(i - 1 + size) % size];
            cv::Point2f v2 = polyline[(i + 1) % size] - polyline[i];
            double cross = v1.x * v2.y - v1.y * v2.x;
            clockwise[i] = (cross < 0);
        }

        int removed_count = 0;

        std::vector<bool> toRemove(size, false);
        for (size_t i = 0; i < size; ++i) {
            if(clockwise[(i - 1 + size) % size] != clockwise[i] && clockwise[i] != clockwise[(i + 1) % size]) {
                double dist0 = cv::norm(polyline[i] - polyline[(i - 1 + size) % size]);
                double dist1 = cv::norm(polyline[(i + 1) % size] - polyline[i]);
                if(dist0 < epsilon && dist1 < epsilon) {
                    toRemove[i] = true;
                    ++removed_count;
                }
            }
        }

        std::vector<cv::Point2f> result;
        result.reserve(polyline.size() - removed_count);
        for (size_t i = 0; i < polyline.size(); ++i) {
            if (!toRemove[i]) {
                result.push_back(polyline[i]);
            }
        }

        return result;
    }else{
        if (polyline.size() < 4) return polyline;

        std::vector<bool> clockwise(polyline.size() - 2, false); // それぞれの角が時計回りか反時計回りか
        for (size_t i = 1; i < polyline.size() - 1; ++i) {
            cv::Point2f v1 = polyline[i] - polyline[i - 1];
            cv::Point2f v2 = polyline[i + 1] - polyline[i];
            double cross = v1.x * v2.y - v1.y * v2.x;
            clockwise[i - 1] = (cross < 0);
        }

        int removed_count = 0;

        std::vector<bool> toRemove(polyline.size(), false);
        // 2 <= i <= size-3
        for (size_t i = 2; i < polyline.size() - 2; ++i) {
            if(clockwise[i-2] != clockwise[i-1] && clockwise[i-1] != clockwise[i]) {
                double dist0 = cv::norm(polyline[i] - polyline[i - 1]);
                double dist1 = cv::norm(polyline[i + 1] - polyline[i]);
                if(dist0 < epsilon && dist1 < epsilon) {
                    toRemove[i] = true;
                    ++removed_count;
                }
            }
        }
        // i = 1
        if(clockwise[0] != clockwise[1]) {
            double dist0 = cv::norm(polyline[1] - polyline[0]);
            double dist1 = cv::norm(polyline[2] - polyline[1]);
            if(dist0 < epsilon && dist1 < epsilon) {
                toRemove[1] = true;
                ++removed_count;
            }
        }
        // i = size-2
        const int size = static_cast<int>(polyline.size());
        if(clockwise[size-3] != clockwise[size-4]) {
            double dist0 = cv::norm(polyline[size-2] - polyline[size-3]);
            double dist1 = cv::norm(polyline[size-1] - polyline[size-2]);
            if(dist0 < epsilon && dist1 < epsilon) {
                toRemove[size-2] = true;
                ++removed_count;
            }
        }

        std::vector<cv::Point2f> result;
        result.reserve(polyline.size() - removed_count);
        for (size_t i = 0; i < polyline.size(); ++i) {
            if (!toRemove[i]) {
                result.push_back(polyline[i]);
            }
        }

        return result;
    }
}

std::vector<std::vector<cv::Point2f>>
removePolylinesJitter(const std::vector<std::vector<cv::Point2f>>& polylines, bool closed, double epsilon = 2.5) {
    if (polylines.empty()) return {};

    std::vector<std::vector<cv::Point2f>> result;
    result.reserve(polylines.size());
    for (const auto& polyline : polylines) {
        result.push_back(removePolylineJitter(polyline, closed, epsilon));
    }
    return result;
}

void canny(const cv::Mat& src, cv::Mat& edges, int lowThreshold, int highThreshold) {
    CV_Assert(src.type() == CV_8UC3);

    cv::Canny(src, edges, lowThreshold, highThreshold);
}

/**
 * LUT（512通り）を生成する。
 * step = 0: 第1サブステップ (NWG-A)
 * step = 1: 第2サブステップ (NWG-B)
 */
static std::vector<uchar> createNWGLUT(int step)
{
    std::vector<uchar> lut(512, 0);

    for (int code = 0; code < 512; ++code)
    {
        uchar p[9];
        for (int k = 0; k < 9; ++k) {
            p[k] = (code >> k) & 1;
        }

        if (p[4] == 0) continue;

        uchar n[8] = {p[1], p[2], p[5], p[8], p[7], p[6], p[3], p[0]};

        int A = 0;
        for (int k = 0; k < 7; ++k) {
            if (n[k] == 0 && n[k + 1] == 1) {
                ++A;
            }
        }
        if (n[7] == 0 && n[0] == 1) {
            ++A;
        }

        int B = 0;
        for (int k = 0; k < 8; ++k) {
            B += n[k];
        }

        if (A == 1 && (B >= 2 && B <= 6)) {
            bool cond1, cond2;
            if (step == 0) {
                cond1 = (n[0] * n[2] * n[4]) == 0;
                cond2 = (n[2] * n[4] * n[6]) == 0;
            }else{
                cond1 = (n[0] * n[2] * n[6]) == 0;
                cond2 = (n[0] * n[4] * n[6]) == 0;
            }
            if (cond1 && cond2){
                lut[code] = 1;
            }
        }
    }

    return lut;
}

/**
 * LUTを用いた1ステップの削除処理（並列対応）
 */
static bool thinningStepParallel(const cv::Mat &src, cv::Mat &dst, const std::vector<uchar> &lut)
{
    CV_Assert(src.type() == CV_8UC1);
    dst.setTo(0);

    std::atomic<bool> modified(false);

    // OpenCV並列実行: 行ごとに処理を分割
    cv::parallel_for_(cv::Range(1, src.rows - 1), [&](const cv::Range &range) {
        for (int y = range.start; y < range.end; ++y)
        {
            const uchar *prev = src.ptr<uchar>(y - 1);
            const uchar *curr = src.ptr<uchar>(y);
            const uchar *next = src.ptr<uchar>(y + 1);
            uchar *out = dst.ptr<uchar>(y);

            for (int x = 1; x < src.cols - 1; ++x) {
                if (curr[x] == 0) continue;

                int code =
                    (prev[x - 1] << 0) | (prev[x] << 1) | (prev[x + 1] << 2) |
                    (curr[x - 1] << 3) | (curr[x] << 4) | (curr[x + 1] << 5) |
                    (next[x - 1] << 6) | (next[x] << 7) | (next[x + 1] << 8);

                if (lut[code]) {
                    out[x] = 1;
                    modified.store(true, std::memory_order_relaxed);
                }
            }
        }
    });

    return modified.load(std::memory_order_relaxed);
}

/**
 * NWG細線化（LUT + 双方向反復 + 並列処理）
 */
void NWGThinningLUTParallel(const cv::Mat &src, cv::Mat &dst)
{
    CV_Assert(src.type() == CV_8UC1);

    cv::Mat img;
    cv::threshold(src, img, 0, 1, cv::THRESH_BINARY | cv::THRESH_OTSU);
    cv::Mat marker = cv::Mat::zeros(img.size(), CV_8UC1);

    static std::vector<uchar> lutA = createNWGLUT(0);
    static std::vector<uchar> lutB = createNWGLUT(1);

    bool modified;
    do {
        modified = false;

        // Step A
        if (thinningStepParallel(img, marker, lutA)) {
            img -= marker;
            modified = true;
        }

        // Step B
        if (thinningStepParallel(img, marker, lutB)) {
            img -= marker;
            modified = true;
        }

    } while (modified);

    img.convertTo(dst, CV_8UC1, 255);
}

void extractEdgeFromGroupMap(const cv::Mat& gmap, cv::Mat& edges) {
    CV_Assert(gmap.type() == CV_8UC1);

    // 膨張・収縮
    cv::Mat dilated, eroded, diff;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2,2));

    cv::dilate(gmap, dilated, kernel);
    cv::erode(gmap, eroded, kernel);

    // 境界候補（差分）
    cv::absdiff(dilated, eroded, diff);

    edges = diff > 0;
}

void classifyPixels(const cv::Mat& binary, cv::Mat& lines, cv::Mat& thinned_lines, cv::Mat& filled, cv::Mat& vis, int r) {
    CV_Assert(binary.type() == CV_8UC1);
    CV_Assert(r >= 4);
    int radius = std::max(1, r);

    lines = cv::Mat::zeros(binary.size(), CV_8UC1);
    filled = cv::Mat::zeros(binary.size(), CV_8UC1);
    vis = cv::Mat(binary.size(), CV_8UC3, cv::Scalar(255, 255, 255));

    int ksize = radius * 2 + 1;
    cv::Mat kernel = cv::Mat::ones(ksize, ksize, CV_32SC1);
    kernel.at<int>(radius, radius) = 0;
    cv::Mat binary1;
    cv::threshold(binary, binary1, 127, 1, cv::THRESH_BINARY); // 白画素(255) → 1 に変換（演算用）
    cv::Mat count;
    cv::filter2D(binary1, count, CV_32S, kernel, cv::Point(-1, -1), 0, cv::BORDER_CONSTANT);
    // 条件: 白画素かつ count <= 閾値
    int thresholdCount = radius * radius * 2;
    cv::Mat mask = (binary == 255) & (count <= thresholdCount);
    lines.setTo(255, mask);

    cv::Mat near_filled;
    int ellipseSize = std::max(3, 2 * radius + 1);
    cv::morphologyEx(binary, near_filled, cv::MORPH_OPEN, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(ellipseSize, ellipseSize)));
    int rectSize = std::max(3, ellipseSize / 2);
    cv::dilate(near_filled, near_filled, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(rectSize, rectSize)));

    cv::bitwise_not(near_filled, near_filled);
    cv::bitwise_and(lines, near_filled, lines);

    removeSmallComponents(lines, 10).copyTo(lines);

    cv::Mat not_lines, not_filled;
    cv::bitwise_not(lines, not_lines);
    cv::bitwise_and(binary, not_lines, filled);

    removeSmallComponents(filled, 10).copyTo(filled);
    cv::bitwise_not(filled, not_filled);
    cv::bitwise_and(binary, not_filled, lines);

    cv::ximgproc::thinning(lines, thinned_lines, cv::ximgproc::THINNING_ZHANGSUEN);
    cv::Mat labels, stats, centroids;
    cv::Mat t_labels, t_stats, t_centroids;
    int n = cv::connectedComponentsWithStats(lines, labels, stats, centroids, 8);
    int m = cv::connectedComponentsWithStats(thinned_lines, t_labels, t_stats, t_centroids, 8);

    for (int i = 1; i < m; ++i) {
        int area = t_stats.at<int>(i, cv::CC_STAT_AREA);
        if(area < 4){
            std::vector<cv::Point> coords;
            cv::findNonZero(t_labels == i, coords);
            auto l = labels.at<int>(coords[0]);
            cv::Mat mask = (labels == l);
            mask.convertTo(mask, CV_8U);
            filled.setTo(255, mask);
            lines.setTo(0, mask);
            thinned_lines.setTo(0, mask);
        }
    }

    vis.setTo(cv::Vec3b(0, 0, 255), lines == 255);
    vis.setTo(cv::Vec3b(0, 255, 0), filled == 255);
}

void visualize(const VectorData& data, cv::Mat& view_map, cv::Mat& view_map_with_points, cv::Mat& view_map_with_hatch, 
    cv::Mat& view_random_colored, int N) {
    view_map = cv::Mat::zeros(data.height * N, data.width * N, CV_8UC3);
    view_map.setTo(cv::Scalar(255,255,255));
    view_map_with_points = view_map.clone();
    view_map_with_hatch = view_map.clone();
    view_random_colored = view_map.clone();

    const cv::Scalar red(0,0,255);
    const cv::Scalar green(0,255,0);
    const cv::Scalar blue(255,0,0);

    for (const auto& [color_id, polylines] : data.polylines) {
        cv::Scalar color = data.color_values.count(color_id) ? data.color_values.at(color_id) : cv::Scalar(255, 255, 255);
        for (const auto& polyline : polylines) {
            if (polyline.size() >= 2) {
                std::vector<cv::Point> scaled;
                scaled.reserve(polyline.size());
                for (const auto& pt : polyline) {
                    scaled.emplace_back(static_cast<int>(pt.x * N), static_cast<int>(pt.y * N));
                }

                cv::Scalar rand_col(rand() % 256, rand() % 256, rand() % 256);

                cv::polylines(view_map, scaled, false, color, 1, cv::LINE_AA);
                cv::polylines(view_map_with_points, scaled, false, red, 1, cv::LINE_AA);
                cv::polylines(view_map_with_hatch, scaled, false, color, 1, cv::LINE_AA);
                cv::polylines(view_random_colored, scaled, false, rand_col, 1, cv::LINE_AA);

                cv::circle(view_map_with_points, scaled.front(), 2, blue, -1, cv::LINE_AA);
                cv::circle(view_map_with_points, scaled.back(), 2, blue, -1, cv::LINE_AA);
                for (auto it = scaled.begin() + 1; it != scaled.end() - 1; ++it) {
                    cv::circle(view_map_with_points, *it, 2, green, -1, cv::LINE_AA);
                }
            }
        }
    }

    for(const auto& [color_id, contours] : data.contours) {
        for(const auto& contour : contours) {
            if (contour.size() >= 2) {
                cv::Scalar color = data.color_values.count(color_id) ? data.color_values.at(color_id) : cv::Scalar(255, 255, 255);
                std::vector<cv::Point> scaled;
                scaled.reserve(contour.size());
                for (const auto& pt : contour) {
                    scaled.emplace_back(static_cast<int>(pt.x * N), static_cast<int>(pt.y * N));
                }

                cv::Scalar rand_col(rand() % 256, rand() % 256, rand() % 256);

                cv::polylines(view_map, scaled, true, color, 1, cv::LINE_AA);
                cv::polylines(view_map_with_points, scaled, true, green, 1, cv::LINE_AA);
                cv::polylines(view_map_with_hatch, scaled, true, color, 1, cv::LINE_AA);
                cv::polylines(view_random_colored, scaled, true, rand_col, 1, cv::LINE_AA);

                for (const auto& pt : scaled) {
                    cv::circle(view_map_with_points, pt, 2, blue, -1, cv::LINE_AA);
                }
            }
        }
    }

    for(const auto& [color_id, hatch_lines] : data.hatch_lines) {
        for(const auto& line : hatch_lines) {
            if (line.size() >= 2) {
                cv::Scalar color = data.color_values.count(color_id) ? data.color_values.at(color_id) : cv::Scalar(255, 255, 255);
                std::vector<cv::Point> scaled;
                scaled.reserve(line.size());
                for (const auto& pt : line) {
                    scaled.emplace_back(static_cast<int>(pt.x * N), static_cast<int>(pt.y * N));
                }

                cv::polylines(view_map_with_points, scaled, false, blue, 1, cv::LINE_AA);
                cv::polylines(view_map_with_hatch, scaled, false, color, 1, cv::LINE_AA);

                cv::circle(view_map_with_points, scaled.front(), 2, red, -1, cv::LINE_AA);
                cv::circle(view_map_with_points, scaled.back(), 2, red, -1, cv::LINE_AA);
                // hatchの中間点はない
            }
        }
    }

    for(const auto& [color_id, mask] : data.filled_masks) {
        if(mask.empty() || mask.type() != CV_8UC1) {
            continue;
        }
        cv::Mat colorMask;
        cv::cvtColor(mask, colorMask, cv::COLOR_GRAY2BGR);
        cv::Scalar color = data.color_values.count(color_id) ? data.color_values.at(color_id) : cv::Scalar(255, 255, 255);
        cv::Mat resized;
        cv::resize(mask, resized, view_map.size(), 0, 0, cv::INTER_NEAREST);
        view_map.setTo(color, resized);
    }
}

// 浮動小数点座標の許容誤差
constexpr float TOLERANCE = 0.0001f; 

/**
 * 2つのPoint2fが許容誤差内で一致するかを判定
 */
static bool isPointEqual(const cv::Point2f& p1, const cv::Point2f& p2) {
    return cv::norm(p1 - p2) < TOLERANCE;
}

// -------------------------------------------------------------
// Helper: Polylinesを分類する関数 (Step 2と最終処理の共通化)
// -------------------------------------------------------------

/**
 * Polylinesのリストを受け取り、閉じたものをcontoursに移し、開いたものをpolylinesに残す。
 * * @param lines 分類対象の線分リスト (in/out)
 * @param result_contours 閉じた線分が追加されるcontoursリスト (out)
 * @return 残った開いたpolylineのリスト
 */
static std::vector<std::vector<cv::Point2f>> classifyLines(
    std::vector<std::vector<cv::Point2f>>&& lines, 
    std::vector<std::vector<cv::Point2f>>& result_contours
) {
    std::vector<std::vector<cv::Point2f>> remaining_polylines;
    for (auto& line : lines) {
        // 閉じたポリラインは、先頭と末尾の座標が許容誤差内で一致するもの
        if (line.size() > 2 && isPointEqual(line.front(), line.back())) {
            // 閉じた頂点の重複を避けるため、末尾の頂点を除外して保存
            line.pop_back(); 
            result_contours.push_back(std::move(line));
        } else {
            remaining_polylines.push_back(std::move(line));
        }
    }
    return remaining_polylines;
}

// -------------------------------------------------------------
// Helper: Polylines間の端点結合（Step 1）
// -------------------------------------------------------------

/**
 * Polylinesのリストを受け取り、端点が重なるものを結合する。
 * 結合された場合はtrueを返し、polylinesを更新する。
 */
static bool mergePolylines(std::vector<std::vector<cv::Point2f>>& polylines) {
    bool merged_in_pass = false;
    std::vector<std::vector<cv::Point2f>> next_polylines;
    std::vector<bool> consumed(polylines.size(), false);

    for (size_t i = 0; i < polylines.size(); ++i) {
        if (consumed[i]) continue;

        std::vector<cv::Point2f> current = polylines[i];
        consumed[i] = true;
        bool locally_merged = true;

        while (locally_merged) {
            locally_merged = false;

            for (size_t j = 0; j < polylines.size(); ++j) {
                if (i == j || consumed[j]) continue;

                const std::vector<cv::Point2f>& next = polylines[j];
                if (current.empty() || next.empty()) continue;

                cv::Point2f current_start = current.front();
                cv::Point2f current_end = current.back();
                cv::Point2f next_start = next.front();
                cv::Point2f next_end = next.back();

                // 結合処理: 結合可能なパターンは4通り
                bool do_merge = false;
                std::vector<cv::Point2f> merged_result;

                if (isPointEqual(current_end, next_start)) {
                    merged_result = current;
                    merged_result.insert(merged_result.end(), next.begin() + 1, next.end());
                    do_merge = true;
                } else if (isPointEqual(current_start, next_end)) {
                    merged_result = next;
                    merged_result.pop_back(); 
                    merged_result.insert(merged_result.end(), current.begin(), current.end());
                    do_merge = true;
                } else if (isPointEqual(current_end, next_end)) {
                    std::vector<cv::Point2f> reversed_next = next;
                    std::reverse(reversed_next.begin(), reversed_next.end());
                    merged_result = current;
                    merged_result.insert(merged_result.end(), reversed_next.begin() + 1, reversed_next.end());
                    do_merge = true;
                } else if (isPointEqual(current_start, next_start)) {
                    std::reverse(current.begin(), current.end());
                    merged_result = current;
                    merged_result.insert(merged_result.end(), next.begin() + 1, next.end());
                    do_merge = true;
                }

                if (do_merge) {
                    current = std::move(merged_result);
                    consumed[j] = true;
                    locally_merged = true;
                    merged_in_pass = true;
                    break;
                }
            }
        }
        next_polylines.push_back(current);
    }
    
    if (merged_in_pass) {
        polylines = std::move(next_polylines);
    }
    return merged_in_pass;
}

// -------------------------------------------------------------
// Main Function: 結合・分類・再結合
// -------------------------------------------------------------

void optimizeVectorData(const VectorData &src, VectorData &dst) {
    // データの初期化とコピー
    dst.polylines.clear();
    dst.contours.clear();
    dst.hatch_lines = src.hatch_lines;
    dst.color_names = src.color_names;
    dst.color_values = src.color_values;
    dst.filled_masks = src.filled_masks;
    dst.width = src.width;
    dst.height = src.height;
    
    for (const auto& pair : src.polylines) {
        int color_id = pair.first;
        std::vector<std::vector<cv::Point2f>> current_polylines = pair.second;
        
        // 既存のcontoursをコピー (Step 3の結合対象として使用)
        std::vector<std::vector<cv::Point2f>> result_contours;
        const auto& existing_contours_it = src.contours.find(color_id);
        if (existing_contours_it != src.contours.end()) {
            result_contours = existing_contours_it->second;
        }

        // ---------------------------------------------
        // Step 1: 端点が重なるpolylineを結合
        // ---------------------------------------------
        while (mergePolylines(current_polylines));

        // ---------------------------------------------
        // Step 2: 閉じたpolylineをcontoursに移す
        // ---------------------------------------------
        current_polylines = classifyLines(std::move(current_polylines), result_contours);

        // ---------------------------------------------
        // Step 3: contourの頂点状にpolylineの端点があるときにそれらを一筆書きするように結合
        // ---------------------------------------------
        bool merged_in_pass = true;
        while (merged_in_pass) {
            merged_in_pass = false;
            std::vector<std::vector<cv::Point2f>> next_polylines;
            std::vector<bool> consumed(current_polylines.size(), false);

            for (size_t i = 0; i < current_polylines.size(); ++i) {
                if (consumed[i]) continue;

                std::vector<cv::Point2f> current = current_polylines[i];
                consumed[i] = true;
                bool locally_merged = false;

                cv::Point2f current_start = current.front();
                cv::Point2f current_end = current.back();

                // 既存のContour (result_contours) との結合を試みる
                for (size_t c = 0; c < result_contours.size(); ++c) {
                    std::vector<cv::Point2f>& contour = result_contours[c];
                    if (contour.empty()) continue;

                    for (size_t v = 0; v < contour.size(); ++v) {
                        cv::Point2f contour_vertex = contour[v];

                        // 1. current_end が contour_vertex と重なっている場合
                        if (isPointEqual(current_end, contour_vertex)) {
                            // polyline (0-1-2-3) と contour (4-5-6-3-7-4)
                            // 結合結果: 0-1-2-3-7-4-5-6-4
                            
                            std::vector<cv::Point2f> combined = current;
                            
                            // 頂点 v の次 (v+1) から、頂点 v が再び現れるまで順に要素を追加
                            // (v+1) % size() は 7, 4, 5, 6, 4
                            for (size_t k = (v + 1) % contour.size(); k != v; k = (k + 1) % contour.size()) {
                                combined.push_back(contour[k]);
                            }
                            combined.push_back(contour_vertex); // 終点として '4' を追加

                            current = std::move(combined);
                            contour.clear(); // 結合に使用したcontourは削除
                            merged_in_pass = true;
                            locally_merged = true;
                            break; 
                        }
                        
                        // 2. current_start が contour_vertex と重なっている場合
                        else if (isPointEqual(current_start, contour_vertex)) {
                            // polyline (3-2-1-0) と contour (4-5-6-3-7-4)
                            // 結合結果: 4-5-6-3-2-1-0 ではなく、contourを逆順にたどって '4' から '7' を経て '3' に接続
                            
                            std::vector<cv::Point2f> combined;
                            
                            // v-1 から v+1 までを逆順にたどり、vを終点として結合
                            // v の一つ前の頂点 (v == 0 なら size() - 2) から、v の次の頂点まで逆順にたどる
                            // 例: v=3 (3) のとき v-1=6, v+1=7
                            // 4, 7 の逆順 (7, 4) から 6, 5 の順で結合し、最後に current (3, 2, 1, 0) を結合
                            
                            // v の一つ前の頂点
                            size_t start_k = (v == 0) ? contour.size() - 1 : v - 1;
                            // v の頂点の次 (閉じる点 $4$ を含む)
                            size_t end_k = (v + 1) % contour.size();

                            // start_k から逆順に end_k までたどる
                            size_t k = start_k;
                            while (true) {
                                combined.push_back(contour[k]);
                                if (k == end_k) break;
                                k = (k == 0) ? contour.size() - 1 : k - 1;
                            }
                            
                            // currentの残りを結合 (current_start は重複するため除外)
                            combined.insert(combined.end(), current.begin() + 1, current.end());
                            
                            current = std::move(combined);
                            contour.clear();
                            merged_in_pass = true;
                            locally_merged = true;
                            break;
                        }
                    }
                    if (locally_merged) break; 
                }

                next_polylines.push_back(current);
            }
            
            if (merged_in_pass) {
                current_polylines = std::move(next_polylines);
            }
        }
        
        // ---------------------------------------------
        // 最終処理: 結合後の線を再分類
        // ---------------------------------------------
        // Step 3の結果のpolylineを分類し、新しく閉じたものをresult_contoursに追加
        current_polylines = classifyLines(std::move(current_polylines), result_contours);
        
        // 最終的な結果をdstに格納
        if (!current_polylines.empty()) {
            dst.polylines[color_id] = std::move(current_polylines);
        }
        
        // 空でないcontourのみを格納
        for (const auto& contour : result_contours) {
            if (!contour.empty()) {
                dst.contours[color_id].push_back(contour);
            }
        }
    }
}

// ヘルパー: 3点からなる三角形の面積を計算
float triangleArea(const cv::Point2f& p1, const cv::Point2f& p2, const cv::Point2f& p3) {
    // Area = 0.5 * |x1(y2 - y3) + x2(y3 - y1) + x3(y1 - y2)|
    float area = std::abs(p1.x * (p2.y - p3.y) + p2.x * (p3.y - p1.y) + p3.x * (p1.y - p2.y)) / 2.0f;
    return area;
}

// リスト内の頂点情報を格納し、隣接点へのポインタを持つ構造体
struct VwPoint;
using VwPointIterator = std::list<VwPoint>::iterator;

struct VwPoint {
    cv::Point2f pt;
    float importance = 0.0f;
    VwPointIterator prev;
    VwPointIterator next;
};

// V-Wアルゴリズムの最小要素を定義 (面積とイテレータを保持)
struct PointImportance {
    float area;
    // リスト内の頂点へのイテレータを保持
    VwPointIterator it; 
    
    // setで使用するための比較演算子
    // 面積が等しい場合は、イテレータのメモリアドレスでソート (一意性を確保)
    bool operator<(const PointImportance& other) const {
        if (area != other.area) {
            return area < other.area;
        }
        // ポインタ比較で一意性を確保
        return &(*it) < &(*other.it); 
    }
};

/**
 * Visvalingam-Whyattアルゴリズムによるポリラインの単純化
 * @param polyline 単純化する入力ポリライン
 * @param minAreaTolerance 削除しない点の最小三角形面積 (重要度)
 * @return 単純化されたポリライン
 */
std::vector<cv::Point2f> simplifyPolylineVW(
    const std::vector<cv::Point2f>& polyline, 
    float minAreaTolerance
) {
    if (polyline.size() <= 2) {
        return polyline;
    }

    // 1. ポリラインを双方向リスト (std::list) に変換
    std::list<VwPoint> working_list;
    for (const auto& p : polyline) {
        // 初期化時にダミーのイテレータで埋める
        working_list.push_back({p, 0.0f, working_list.end(), working_list.end()});
    }

    // 2. 優先度キュー (std::set) を初期化
    std::set<PointImportance> importance_queue;

    // 3. 頂点間のリンクリストを設定し、初期重要度を計算
    for (auto it = working_list.begin(); it != working_list.end(); ++it) {
        bool is_start_or_end = (it == working_list.begin() || std::next(it) == working_list.end());

        if (is_start_or_end) {
            it->importance = 0.0f;
            // 始点と終点の隣接イテレータは後続の if で設定される
            continue; 
        }

        // 隣接点を設定
        it->prev = std::prev(it);
        it->next = std::next(it);

        // 三角形面積を計算し、重要度として設定
        it->importance = triangleArea(it->prev->pt, it->pt, it->next->pt);
        
        // 優先度キューに追加
        importance_queue.insert({it->importance, it});
    }

    // 4. 反復的な削除プロセス
    while (!importance_queue.empty()) {
        // 最小重要度の点 (P_min) を取得
        PointImportance min_p_info = *importance_queue.begin();
        importance_queue.erase(importance_queue.begin()); 

        if (min_p_info.area >= minAreaTolerance) {
            // 最小面積が閾値を超えたら終了
            break;
        }

        // P_min に対応するリストのイテレータを取得
        auto p_min_it = min_p_info.it;

        // P_min の隣接点 P_prev と P_next を取得
        auto p_prev_it = p_min_it->prev;
        auto p_next_it = p_min_it->next;

        // P_min をリストから削除 (ノードを結合)
        working_list.erase(p_min_it);
        
        // 隣接点 P_prev と P_next の古い重要度をキューから削除し、隣接情報を更新
        // (注: 始点/終点はキューに含まれていないため、削除の必要なし)

        // a) P_prev の古い重要度を削除
        if (p_prev_it != working_list.begin() && p_prev_it->next != working_list.end()) {
            importance_queue.erase({p_prev_it->importance, p_prev_it});
        }
        
        // b) P_next の古い重要度を削除
        if (p_next_it != working_list.begin() && p_next_it->next != working_list.end()) {
            importance_queue.erase({p_next_it->importance, p_next_it});
        }

        // c) P_prev と P_next の隣接情報を更新
        p_prev_it->next = p_next_it;
        p_next_it->prev = p_prev_it;
        
        // d) P_prev の新しい重要度を計算・挿入
        if (p_prev_it != working_list.begin() && p_prev_it->next != working_list.end()) {
            p_prev_it->importance = triangleArea(p_prev_it->prev->pt, p_prev_it->pt, p_prev_it->next->pt);
            importance_queue.insert({p_prev_it->importance, p_prev_it});
        }

        // e) P_next の新しい重要度を計算・挿入
        if (p_next_it != working_list.begin() && p_next_it->next != working_list.end()) {
            p_next_it->importance = triangleArea(p_next_it->prev->pt, p_next_it->pt, p_next_it->next->pt);
            importance_queue.insert({p_next_it->importance, p_next_it});
        }
    }

    // 5. 結果を cv::Point2f ベクターに戻す
    std::vector<cv::Point2f> simplified_polyline;
    for (const auto& p : working_list) {
        simplified_polyline.push_back(p.pt);
    }

    return simplified_polyline;
}

std::vector<std::vector<cv::Point2f>> simplifyPolylinesVW(
    const std::vector<std::vector<cv::Point2f>>& polylines, 
    float minAreaTolerance
) {
    std::vector<std::vector<cv::Point2f>> simplified_polylines;
    simplified_polylines.reserve(polylines.size());

    for (const auto& polyline : polylines) {
        simplified_polylines.push_back(simplifyPolylineVW(polyline, minAreaTolerance));
    }

    return simplified_polylines;
}

void simplifyVectorData(const VectorData& src, VectorData& dst) {
    for(const auto& [color_id, name] : src.color_names) {
        dst.color_names[color_id] = name;
        dst.color_values[color_id] = src.color_values.at(color_id);
        dst.filled_masks[color_id] = src.filled_masks.at(color_id);
        dst.width = src.width;
        dst.height = src.height;

        if(src.polylines.contains(color_id)) {
            auto simplified = simplifyPolylines(subdividePolylines(src.polylines.at(color_id), 2, false), 0.86, false);
            //auto simplified = simplifyPolylinesVW(subdividePolylines(src.polylines.at(color_id), 2, false), 5.0);
            dst.polylines[color_id] = simplified;
        }
        if(src.contours.contains(color_id)) {
            auto simplified = simplifyPolylines(subdividePolylines(src.contours.at(color_id), 2, true), 0.86, true);
            //auto simplified = simplifyPolylinesVW(subdividePolylines(src.contours.at(color_id), 2, true), 5.0);
            dst.contours[color_id] = simplified;
        }
        if(src.hatch_lines.contains(color_id)) {
            dst.hatch_lines[color_id] = src.hatch_lines.at(color_id); // ハッチは簡略化しない
        }
    }
}

void clean_thinned(const cv::Mat& thinned, cv::Mat& cleaned) {
    cleaned = thinned.clone();
    // 8近傍の内、4近傍でない4か所が全て0で、4近傍の内ちょうど3つが1のとき、その画素を1にする
    // OpenCV並列実行: 行ごとに処理を分割
    cv::parallel_for_(cv::Range(1, thinned.rows - 1), [&](const cv::Range &range) {
        for (int y = range.start; y < range.end; ++y) {
            const uchar *prev = thinned.ptr<uchar>(y - 1);
            const uchar *curr = thinned.ptr<uchar>(y);
            const uchar *next = thinned.ptr<uchar>(y + 1);
            uchar *out = cleaned.ptr<uchar>(y);

            for (int x = 1; x < thinned.cols - 1; ++x) {
                if(curr[x] == 0) {
                    int count4 = (prev[x] != 0) + (curr[x - 1] != 0) + (curr[x + 1] != 0) + (next[x] != 0);
                    int count8 = (prev[x - 1] != 0) + (prev[x + 1] != 0) + (next[x - 1] != 0) + (next[x + 1] != 0);
                    if(count4 == 3 && count8 == 0) {
                        out[x] = 255;
                    }
                }
            }
        }
    });
}

float polylineLength(const std::vector<cv::Point2f>& polyline, bool closed = false) {
    float length = 0.0f;
    for (size_t i = 1; i < polyline.size(); ++i) {
        length += cv::norm(polyline[i] - polyline[i - 1]);
    }
    if (closed) {
        length += cv::norm(polyline.front() - polyline.back());
    }
    return length;
}

void removeShortPolylines(std::vector<std::vector<cv::Point2f>>& polylines, float minLength, bool closed = false) {
    polylines.erase(
        std::remove_if(polylines.begin(), polylines.end(),
            [minLength, closed](const std::vector<cv::Point2f>& polyline) {
                return polylineLength(polyline, closed) < minLength;
            }),
        polylines.end()
    );
}

void lastConvertToVectorData(
    VectorData& data, cv::Mat& view_map, cv::Mat& view_map_with_points, cv::Mat& view_map_with_hatch,
    cv::Mat& view_random_colored,
    int hatchLineSpacing, int hatchLineAngle, int minSize, float jitterEpsilon, float minPolylineLength,
    const std::map<std::string, HatchLineSetting>& hatchLineSettings
) {
    for(auto& [color_id, mask] : data.filled_masks) {
        if(mask.empty() || mask.type() != CV_8UC1) {
            continue;
        }
        removeSmallComponents(mask, minSize);
        int use_id = color_id;
        int spacing = hatchLineSpacing;
        std::vector<int> angles = {hatchLineAngle};
        if(hatchLineSettings.contains(data.color_names.at(color_id)) || hatchLineSettings.contains("_")) {
            HatchLineSetting settings;
            if(hatchLineSettings.contains(data.color_names.at(color_id))) {
                settings = hatchLineSettings.at(data.color_names.at(color_id));
            } else {
                settings = hatchLineSettings.at("_");
            }
            std::cout << "Mode: " << settings.mode << ", Spacing: " << settings.spacing << ", Substitute Color: " << settings.substitute_color << std::endl;
            if(settings.spacing > 0) spacing = settings.spacing;
            if(settings.mode == "/"){
                angles = {135};
            }else if(settings.mode == "\\"){ // 逆スラッシュ
                angles = {45};
            }else if(settings.mode == "|"){
                angles = {90};
            }else if(settings.mode == "-"){
                std::cout << "Hatch mode '-'" << std::endl;
                angles = {0};
            }else if(settings.mode == "x"){
                angles = {45, 135};
            }else if(settings.mode == "+"){
                angles = {0, 90};
            }else{
                angles = {hatchLineAngle};
            }
            auto substitute_color = settings.substitute_color;
            for(const auto& [id, name] : data.color_names) {
                std::cout << "Checking color id: " << id << ", name: " << name << std::endl;
                if(name == substitute_color) {
                    use_id = id;
                    break;
                }
            }
        }
        for(auto angle : angles) {
            std::cout << "Generating hatch lines for color: " << data.color_names.at(color_id) << " with angle: " << angle << " and spacing: " << spacing << std::endl;
            auto hatchLines = generateHatchLines(mask, spacing, angle);
            data.hatch_lines[use_id].reserve(data.hatch_lines[use_id].size() + hatchLines.size());
            std::copy(hatchLines.begin(), hatchLines.end(), std::back_inserter(data.hatch_lines[use_id]));
        }
    }
    cv::Mat thinned, cleaned;
    for(auto& [color_id, mask] : data.edge_masks) {
        if(mask.empty() || mask.type() != CV_8UC1) {
            continue;
        }

        NWGThinningLUTParallel(mask, thinned);
        clean_thinned(thinned, cleaned);
        auto polylines = polylinesIntToFloat2f(extractPolylines(cleaned));
        removeShortPolylines(polylines, minPolylineLength);
        auto no_jitter = removePolylinesJitter(polylines, false, jitterEpsilon);
        data.polylines[color_id].reserve(data.polylines[color_id].size() + no_jitter.size());
        std::copy(no_jitter.begin(), no_jitter.end(), std::back_inserter(data.polylines[color_id]));
    }
    for(auto& [color_id, mask] : data.outline_masks) {
        if(mask.empty() || mask.type() != CV_8UC1) {
            continue;
        }
        std::vector<std::vector<cv::Point>> raw_polylines, raw_contours;
        extractContoursFromFilled(mask, raw_polylines, raw_contours);
        auto polylines = polylinesIntToFloat2f(raw_polylines);
        auto contours = polylinesIntToFloat2f(raw_contours);

        int use_id = color_id;
        if(hatchLineSettings.contains(data.color_names.at(color_id))) {
            auto settings = hatchLineSettings.at(data.color_names.at(color_id));
            for(const auto& [id, name] : data.color_names) {
                if(name == settings.substitute_color) {
                    use_id = id;
                    break;
                }
            }
        }
        data.polylines[use_id].reserve(data.polylines[use_id].size() + polylines.size());
        std::copy(polylines.begin(), polylines.end(), std::back_inserter(data.polylines[use_id]));
        data.contours[use_id].reserve(data.contours[use_id].size() + contours.size());
        std::copy(contours.begin(), contours.end(), std::back_inserter(data.contours[use_id]));
    }

    for(auto& [color_id, contours] : data.contours) {
        removeShortPolylines(contours, minPolylineLength, true);
    }

    VectorData optimized;
    optimizeVectorData(data, optimized);
    data = optimized;
    VectorData simplified;
    simplifyVectorData(data, simplified);
    data = simplified;

    visualize(data, view_map, view_map_with_points, view_map_with_hatch, view_random_colored, 2);
}
