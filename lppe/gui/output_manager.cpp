#include "output_manager.hpp"

#include <iostream>

#include <opencv4/opencv2/imgproc.hpp>

static int positive_modulo(int i, int n) {
    if(n <= 0){
        std::cerr << "positive_modulo: n must be positive, but got " << n << std::endl;
        return 0;
    }
    return (i % n + n) % n;
}

/*
new x = ax + by + c
new y = dx + ey + f
Apply linear transform to vector data
don't change cv::Mat data
*/
static void applyLinearTransformVectorData(const VectorData& in, VectorData& out, double a, double b, double c, double d, double e, double f) {
    // ignore width and height
    out.width = 0;
    out.height = 0;

    out.polylines.clear();
    for(const auto& [color, lines] : in.polylines) {
        for(const auto& line : lines) {
            std::vector<cv::Point2f> new_line;
            for(const auto& pt : line) {
                float new_x = static_cast<float>(a * pt.x + b * pt.y + c);
                float new_y = static_cast<float>(d * pt.x + e * pt.y + f);
                new_line.emplace_back(new_x, new_y);
            }
            out.polylines[color].push_back(new_line);
        }
    }

    out.contours.clear();
    for(const auto& [color, lines] : in.contours) {
        for(const auto& line : lines) {
            std::vector<cv::Point2f> new_line;
            for(const auto& pt : line) {
                float new_x = static_cast<float>(a * pt.x + b * pt.y + c);
                float new_y = static_cast<float>(d * pt.x + e * pt.y + f);
                new_line.emplace_back(new_x, new_y);
            }
            out.contours[color].push_back(new_line);
        }
    }

    out.hatch_lines.clear();
    for(const auto& [color, lines] : in.hatch_lines) {
        for(const auto& line : lines) {
            std::vector<cv::Point2f> new_line;
            for(const auto& pt : line) {
                float new_x = static_cast<float>(a * pt.x + b * pt.y + c);
                float new_y = static_cast<float>(d * pt.x + e * pt.y + f);
                new_line.emplace_back(new_x, new_y);
            }
            out.hatch_lines[color].push_back(new_line);
        }
    }

    out.color_names = in.color_names;
    out.color_values = in.color_values;
}

static void mergeVectorData(const VectorData& src0, const VectorData& src1, VectorData& dst) {
    dst.width = 0;
    dst.height = 0;

    dst.polylines = src0.polylines;
    for(const auto& [color, lines] : src1.polylines) {
        for(const auto& line : lines) {
            dst.polylines[color].push_back(line);
        }
    }

    dst.contours = src0.contours;
    for(const auto& [color, lines] : src1.contours) {
        for(const auto& line : lines) {
            dst.contours[color].push_back(line);
        }
    }

    dst.hatch_lines = src0.hatch_lines;
    for(const auto& [color, lines] : src1.hatch_lines) {
        for(const auto& line : lines) {
            dst.hatch_lines[color].push_back(line);
        }
    }

    dst.color_names = src0.color_names;
    for(const auto& [color, name] : src1.color_names) {
        if(dst.color_names.count(color) == 0) {
            dst.color_names[color] = name;
        }
    }

    dst.color_values = src0.color_values;
    for(const auto& [color, value] : src1.color_values) {
        if(dst.color_values.count(color) == 0) {
            dst.color_values[color] = value;
        }
    }
}

static void getRectVectorData(const VectorData& data, double& min_x, double& min_y, double& max_x, double& max_y) {
    std::vector<double> xs, ys;
    for(const auto& [color, lines] : data.polylines) {
        for(const auto& line : lines) {
            for(const auto& pt : line) {
                xs.push_back(pt.x);
                ys.push_back(pt.y);
            }
        }
    }
    for(const auto& [color, lines] : data.contours) {
        for(const auto& line : lines) {
            for(const auto& pt : line) {
                xs.push_back(pt.x);
                ys.push_back(pt.y);
            }
        }
    }
    for(const auto& [color, lines] : data.hatch_lines) {
        for(const auto& line : lines) { 
            for(const auto& pt : line) {
                xs.push_back(pt.x);
                ys.push_back(pt.y);
            }
        }
    }

    if(xs.empty() || ys.empty()){
        min_x = min_y = max_x = max_y = 0;
        return;
    }

    auto [min_x_it, max_x_it] = std::minmax_element(xs.begin(), xs.end());
    auto [min_y_it, max_y_it] = std::minmax_element(ys.begin(), ys.end());
    min_x = *min_x_it;
    max_x = *max_x_it;
    min_y = *min_y_it;
    max_y = *max_y_it;
}

int sumMapSizes(const std::map<int, std::vector<std::vector<cv::Point2f>>>& m) {
    int sum = 0;
    for (const auto& [key, vec] : m) {
        sum += static_cast<int>(vec.size());
    }
    return sum;
}

void OutputManager::drawGui(const VectorData& vector_data, GLuint *button_textures) {
    ImGui::BeginDisabled(isCalculatingViewImage());
    if(ImGui::Button("Reload View")){
        startCalculation(vector_data);
    }
    ImGui::EndDisabled();
    if(isCalculatingViewImage()){
        ImGui::SameLine();
        ImGui::Text("Calculating...");
    }

    if(ImGui::Button("Reset to Default")){
        paper_size = "A4";
        paper_width = 210;
        paper_height = 297;
        paper_margin = 10;
        direction = 0;
        allow_center_drift = true;
        size_percent = 100;
    }

    ImGui::Dummy(ImVec2(0, 5));

    if(ImGui::Checkbox("Double", &double_mode)){}
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10, 0));
    ImGui::SameLine();
    if(ImGui::Checkbox("Add Border", &add_border)){}

    ImGui::Dummy(ImVec2(0, 5));

    ImGui::PushItemWidth(150);

    if(ImGui::BeginCombo("Paper Size", "A4")){
        if(ImGui::Selectable("A4")){
            paper_size = "A4";
            paper_width = 210;
            paper_height = 297;
        }
        if(ImGui::Selectable("custom")){
            paper_size = "custom";
        }
        ImGui::EndCombo();
    }
    if(paper_size == "custom"){
        ImGui::InputInt("Width (mm)", &paper_width);
        if(paper_width <= 0) paper_width = 1;
        if(paper_width > 1000) paper_width = 1000;
        ImGui::InputInt("Height (mm)", &paper_height);
        if(paper_height <= 0) paper_height = 1;
        if(paper_height > 1000) paper_height = 1000;
    }

    ImGui::Dummy(ImVec2(0, 5));

    ImGui::InputInt("Margin (mm)", &paper_margin);
    if(paper_margin < 0) paper_margin = 0;
    int max_margin = (paper_width < paper_height ? paper_width : paper_height) / 2;
    max_margin = max_margin > 100 ? 100 : max_margin;
    if(paper_margin > max_margin) paper_margin = max_margin;

    ImGui::InputInt("Direction", &direction);
    direction = positive_modulo(direction, 360);

    ImGui::PopItemWidth();

    ImGui::Checkbox("Allow Center Drift", &allow_center_drift);

    ImGui::PushItemWidth(150);
    ImGui::InputInt("Size (%)", &size_percent);
    if(size_percent < 1) size_percent = 1;
    if(size_percent > 100) size_percent = 100;
    ImGui::PopItemWidth();

    ImGui::Separator();
    int polyline_count = sumMapSizes(vector_data.polylines);
    int contour_count = sumMapSizes(vector_data.contours);
    int hatch_count = sumMapSizes(vector_data.hatch_lines);
    ImGui::Text("total: %d", polyline_count + contour_count + hatch_count);
    ImGui::Text("polylines: %d", polyline_count);
    ImGui::Text("contours: %d", contour_count);
    ImGui::Text("hatch lines: %d", hatch_count);
}

void OutputManager::startCalculation(const VectorData& vector_data) {
    std::lock_guard<std::mutex> lock(mtx);

    if(isCalculating) {
        std::cerr << "OutputManager::startCalculation: Already calculating." << std::endl;
        return; // Already calculating
    }
    isCalculating = true;

    // Copy inputs
    vector_data_copy = vector_data;

    std::thread([this]() {
        cv::Mat new_view_img;
        VectorData new_output_data;

        std::cout << "OutputManager: Rendering view image..." << std::endl;
        renderViewImage(vector_data_copy, new_output_data, new_view_img);
        std::cout << "OutputManager: View image rendered." << std::endl;

        {
            std::lock_guard<std::mutex> lock(mtx);
            view_img = new_view_img;
            output_data = new_output_data;
            isCalculating = false;
        }
    }).detach();
}

void OutputManager::renderViewImage(const VectorData& vector_data, VectorData& output_data, cv::Mat& img) {
    if(vector_data.width <= 0 || vector_data.height <= 0){
        std::cerr << "OutputManager::renderViewImage: Invalid vector data size." << std::endl;
        img = cv::Mat();
        return;
    }

    if(paper_width <= 2 * paper_margin || paper_height <= 2 * paper_margin){
        std::cerr << "OutputManager::renderViewImage: Invalid paper size or margin." << std::endl;
        img = cv::Mat();
        return;
    }

    int drawable_width = paper_width - 2 * paper_margin;
    int drawable_height = paper_height - 2 * paper_margin;

    // move center to origin
    VectorData moved_data;
    applyLinearTransformVectorData(vector_data, moved_data, 1, 0, -vector_data.width * 0.5, 0, 1, -vector_data.height * 0.5);

    // rotate vector data
    VectorData rotated_data;
    double rad = direction * CV_PI / 180.0;
    double a = std::cos(rad);
    double b = -std::sin(rad);
    double d = std::sin(rad);
    double e = std::cos(rad);
    applyLinearTransformVectorData(moved_data, rotated_data, a, b, 0, d, e, 0);

    // get bounding box
    double min_x, min_y, max_x, max_y;
    getRectVectorData(rotated_data, min_x, min_y, max_x, max_y);

    VectorData paper_fit_data;
    // scale to fit paper
    if(allow_center_drift){
        double scale = (std::min)(
            static_cast<double>(drawable_width) / (max_x - min_x),
            static_cast<double>(drawable_height) / (max_y - min_y)
        );
        double center_x = (min_x + max_x) * 0.5;
        double center_y = (min_y + max_y) * 0.5;
        applyLinearTransformVectorData(rotated_data, paper_fit_data, scale, 0, -center_x * scale, 0, scale, -center_y * scale);
    }else{
        double scale = (std::min)(
            static_cast<double>(drawable_width * 0.5) / (std::max)(max_x, -min_x),
            static_cast<double>(drawable_height * 0.5) / (std::max)(max_y, -min_y)
        );
        applyLinearTransformVectorData(rotated_data, paper_fit_data, scale, 0, 0, 0, scale, 0);
    }

    double final_scale = static_cast<double>(size_percent) / 100.0;
    if(double_mode){
        // rotate 90 degrees and sqrt(1/2) scale
        VectorData rotated_90_data;
        applyLinearTransformVectorData(paper_fit_data, rotated_90_data, 0, -std::sqrt(0.5) * final_scale, 0, std::sqrt(0.5) * final_scale, 0, 0);
        VectorData upper_half_data, lower_half_data;
        applyLinearTransformVectorData(rotated_90_data, upper_half_data, 1, 0, 0, 0, 1, -drawable_height * 0.25);
        applyLinearTransformVectorData(rotated_90_data, lower_half_data, 1, 0, 0, 0, 1, drawable_height * 0.25);
        // merge
        mergeVectorData(upper_half_data, lower_half_data, paper_fit_data);

        final_scale = 1.0;
    }

    VectorData final_data;
    applyLinearTransformVectorData(paper_fit_data, final_data, final_scale, 0, paper_width * 0.5, 0, final_scale, paper_height * 0.5);
    final_data.width = paper_width;
    final_data.height = paper_height;

    cv::Mat view_map, view_map_with_points, view_map_with_hatch, view_random_colored;
    const int N = 5; // 0.2mm per pixel
    visualize(final_data, view_map, view_map_with_points, view_map_with_hatch, view_random_colored, N);

    img = view_map_with_hatch;

    // draw border
    if(size_percent < 100){
        cv::rectangle(img,
            cv::Point((paper_width * 0.5 - (paper_width * 0.5 - paper_margin) * final_scale) * N,
                      (paper_height * 0.5 - (paper_height * 0.5 - paper_margin) * final_scale) * N),
            cv::Point((paper_width * 0.5 + (paper_width * 0.5 - paper_margin) * final_scale) * N - 1,
                      (paper_height * 0.5 + (paper_height * 0.5 - paper_margin) * final_scale) * N - 1),
            cv::Scalar(255,0,0), 1, cv::LINE_AA
        );
    }
    cv::rectangle(img,
        cv::Point(paper_margin * N, paper_margin * N),
        cv::Point((paper_width - paper_margin) * N - 1, (paper_height - paper_margin) * N - 1),
        cv::Scalar(0,0,255), 1, cv::LINE_AA
    );
    if(double_mode){
        cv::line(img,
            cv::Point(paper_margin * N, paper_height * 0.5 * N),
            cv::Point((paper_width - paper_margin) * N - 1, paper_height * 0.5 * N),
            cv::Scalar(0,0,255), 1, cv::LINE_AA
        );
    }

    if(add_border){
        int border_color = -1;
        for(const auto& [color, name] : vector_data.color_names) {
            border_color = color;
            if(name == "black") {
                break;
            }
        }
        if(!final_data.contours.count(border_color)){
            final_data.contours[border_color] = {};
        }
        if(!final_data.polylines.count(border_color)){
            final_data.polylines[border_color] = {};
        }
        final_data.contours.at(border_color).push_back({
            cv::Point2f(paper_margin, paper_margin),
            cv::Point2f(paper_width - paper_margin, paper_margin),
            cv::Point2f(paper_width - paper_margin, paper_height - paper_margin),
            cv::Point2f(paper_margin, paper_height - paper_margin)
        });
        if(double_mode){
            final_data.polylines.at(border_color).push_back({
                cv::Point2f(paper_margin, paper_height * 0.5),
                cv::Point2f(paper_width - paper_margin, paper_height * 0.5)
            });
        }
    }

    output_data = final_data;
}

cv::Mat OutputManager::getViewImage() const {
    std::lock_guard<std::mutex> lock(mtx);
    if(isCalculating || view_img.empty()){
        return cv::Mat();
    }
    return view_img;
}

bool OutputManager::getOutputData(VectorData& out_data) const {
    std::lock_guard<std::mutex> lock(mtx);
    if(isCalculating || output_data.width <= 0 || output_data.height <= 0){
        return false;
    }
    out_data = output_data;
    return true;
}
