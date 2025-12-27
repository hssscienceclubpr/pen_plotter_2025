#include "vector_converters.hpp"

#include <iostream>

#include <opencv4/opencv2/core/mat.hpp>
#include <opencv4/opencv2/imgproc/imgproc.hpp>

#include "img/vector_data.hpp"

EmptyConverter::EmptyConverter() {}
void EmptyConverter::apply(const cv::Mat& original, const ColorMap& colorMap, const cv::Mat& modeMap, const int mode, VectorData& outData) {}
bool EmptyConverter::drawGui() {
    std::cerr << "EmptyConverter::drawGui called." << std::endl;
    ImGui::Text("If you can see this, something is wrong.");
    return false;
}

EdgeConverter::EdgeConverter() {}
void EdgeConverter::apply(const cv::Mat& original, const ColorMap& colorMap, const cv::Mat& modeMap, const int mode, VectorData& outData) {
    if(original.empty() || original.channels() != 3) {
        std::cerr << "EdgeConverter::apply: original image is empty or not a 3-channel BGR image." << std::endl;
        return;
    }
    if(colorMap.colorMap.empty() || colorMap.colorMap.type() != CV_8UC1) {
        std::cerr << "EdgeConverter::apply: colorMap is empty or not CV_8UC1." << std::endl;
        return;
    }
    if(modeMap.empty() || modeMap.type() != CV_8UC1) {
        std::cerr << "EdgeConverter::apply: modeMap is empty or not CV_8UC1." << std::endl;
        return;
    }

    for(auto pair : colorMap.MapOfColorName) {
        int color_id = pair.first;

        if(colorMap.MapOfColor.find(color_id) == colorMap.MapOfColor.end()) {
            continue;
        }
        if(pair.second == "white"){
            continue; // Skip background color
        }
        cv::Mat mask = colorMap.MapOfColor.at(color_id);
        if(mask.empty() || mask.type() != CV_8UC1) {
            continue;
        }

        removeSmallComponents(mask, min_size).copyTo(mask);
        if(opening_radius > 0){
            cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                        cv::Size(2 * opening_radius + 1, 2 * opening_radius + 1),
                                                        cv::Point(opening_radius, opening_radius));
            cv::morphologyEx(mask, mask, cv::MORPH_OPEN, element);
        }

        outData.edge_masks[color_id] = outData.edge_masks[color_id] | (mask & (modeMap == mode));
    }
}
bool EdgeConverter::drawGui() {
    bool changed = false;
    ImGui::PushItemWidth(200);
    if(ImGui::SliderInt("Min Size", &min_size, 1, 20)){
        changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::PushItemWidth(200);
    if(ImGui::SliderInt("Opening Radius", &opening_radius, 0, 20)){
        changed = true;
    }
    ImGui::PopItemWidth();
    return changed;
}

FillConverter::FillConverter() {}
void FillConverter::apply(const cv::Mat& original, const ColorMap& colorMap, const cv::Mat& modeMap, const int mode, VectorData& outData) {
    if(original.empty() || original.channels() != 3) {
        std::cerr << "FillConverter::apply: original image is empty or not a 3-channel BGR image." << std::endl;
        return;
    }
    if(colorMap.colorMap.empty() || colorMap.colorMap.type() != CV_8UC1) {
        std::cerr << "FillConverter::apply: colorMap is empty or not CV_8UC1." << std::endl;
        return;
    }
    if(modeMap.empty() || modeMap.type() != CV_8UC1) {
        std::cerr << "FillConverter::apply: modeMap is empty or not CV_8UC1." << std::endl;
        return;
    }

    cv::Mat back_outline_mask;

    for(auto pair : colorMap.MapOfColorName) {
        int color_id = pair.first;

        if(colorMap.MapOfColor.find(color_id) == colorMap.MapOfColor.end()) {
            continue;
        }

        cv::Mat mask = colorMap.MapOfColor.at(color_id);
        if(mask.empty() || mask.type() != CV_8UC1) {
            continue;
        }

        removeSmallComponents(mask, min_size).copyTo(mask);
        if(opening_radius > 0){
            cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                        cv::Size(2 * opening_radius + 1, 2 * opening_radius + 1),
                                                        cv::Point(opening_radius, opening_radius));
            cv::morphologyEx(mask, mask, cv::MORPH_OPEN, element);
        }
        
        cv::Mat col_mask = outData.filled_masks[color_id] | (mask & (modeMap == mode));
        if(closing_radius > 0){
            cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                        cv::Size(2 * closing_radius + 1, 2 * closing_radius + 1),
                                                        cv::Point(closing_radius, closing_radius));
            cv::morphologyEx(col_mask, col_mask, cv::MORPH_CLOSE, element);
        }
        cv::Mat uneroded = col_mask.clone();
        if(erosion_radius > 0){
            cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                        cv::Size(2 * erosion_radius + 1, 2 * erosion_radius + 1),
                                                        cv::Point(erosion_radius, erosion_radius));
            cv::dilate(col_mask, col_mask, element);
        }else if(erosion_radius < 0){
            int radius = -erosion_radius;
            cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                        cv::Size(2 * radius + 1, 2 * radius + 1),
                                                        cv::Point(radius, radius));
            cv::erode(col_mask, col_mask, element);
        }

        if(pair.second == "white"){
            if(back_outline.size() > 0){
                back_outline_mask = (~(colorMap.MapOfColor.at(color_id))) & (modeMap == mode);
            }
            continue;
        }

        outData.filled_masks[color_id] = col_mask;
        if(outline_mode) {
            outData.outline_masks[color_id] = outData.outline_masks[color_id] | (uneroded & (modeMap == mode));
        }
    }

    if(back_outline.size() > 0 || !(back_outline_mask.empty())){
        for(auto& pair : colorMap.MapOfColorName) {
            if(pair.second == back_outline){
                std::cout << "Applying back outline to color: " << pair.second << std::endl;
                outData.outline_masks[pair.first] = outData.outline_masks[pair.first] | back_outline_mask;
            }
        }
    }
    if(canny_mode.size() > 0){
        cv::Mat canny_mask;
        canny(original, canny_mask, low_threshold, high_threshold);
        for(auto& pair : colorMap.MapOfColorName) {
            if(pair.second == canny_mode){
                outData.edge_masks[pair.first] = outData.edge_masks[pair.first] | (canny_mask & (modeMap == mode));
            }
        }
    }
    if(color_edges.size() > 0){
        cv::Mat edge_mask;
        extractEdgeFromGroupMap(colorMap.colorMap, edge_mask);
        for(auto& pair : colorMap.MapOfColorName) {
            if(pair.second == color_edges){
                outData.edge_masks[pair.first] = outData.edge_masks[pair.first] | (edge_mask & (modeMap == mode));
            }
        }
    }
}
bool FillConverter::drawGui() {
    char buf[32];

    bool result = false;
    if(ImGui::Checkbox("Outline Mode", &outline_mode)){
        result = true;
    }
    ImGui::PushItemWidth(200);
    std::strcpy(buf, canny_mode.c_str());
    if(ImGui::InputText("Canny", buf, sizeof(buf))){
        canny_mode = buf;
        result = true;
    }
    ImGui::PopItemWidth();
    ImGui::PushItemWidth(150);
    ImGui::BeginDisabled(canny_mode.empty());
    if(ImGui::InputInt("Low Threshold", &low_threshold)){
        low_threshold = std::clamp(low_threshold, 0, 1000);
        result = true;
    }
    if(ImGui::InputInt("High Threshold", &high_threshold)){
        high_threshold = std::clamp(high_threshold, 0, 1000);
        result = true;
    }
    ImGui::EndDisabled();
    ImGui::PopItemWidth();
    ImGui::PushItemWidth(200);
    std::strcpy(buf, color_edges.c_str());
    if(ImGui::InputText("Color Edges", buf, sizeof(buf))){
        color_edges = buf;
        result = true;
    }
    std::strcpy(buf, back_outline.c_str());
    if(ImGui::InputText("Back Outline", buf, sizeof(buf))){
        back_outline = buf;
        result = true;
    }
    ImGui::PopItemWidth();

    ImGui::PushItemWidth(200);
    if(ImGui::SliderInt("Closing Radius", &closing_radius, 0, 20)){
        result = true;
    }
    ImGui::PopItemWidth();
    ImGui::PushItemWidth(200);
    if(ImGui::SliderInt("Erosion Radius", &erosion_radius, -20, 20)){
        result = true;
    }
    ImGui::PopItemWidth();

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::PushItemWidth(200);
    if(ImGui::SliderInt("Min Size", &min_size, 1, 20)){
        result = true;
    }
    ImGui::PopItemWidth();
    ImGui::PushItemWidth(200);
    if(ImGui::SliderInt("Opening Radius", &opening_radius, 0, 20)){
        result = true;
    }
    ImGui::PopItemWidth();

    return result;
}

LineAndFillConverter::LineAndFillConverter() {}
void LineAndFillConverter::apply(const cv::Mat& original, const ColorMap& colorMap, const cv::Mat& modeMap, const int mode, VectorData& outData) {
    if(original.empty() || original.channels() != 3) {
        std::cerr << "LineAndFillConverter::apply: original image is empty or not a 3-channel BGR image." << std::endl;
        return;
    }
    if(colorMap.colorMap.empty() || colorMap.colorMap.type() != CV_8UC1) {
        std::cerr << "LineAndFillConverter::apply: colorMap is empty or not CV_8UC1." << std::endl;
        return;
    }
    if(modeMap.empty() || modeMap.type() != CV_8UC1) {
        std::cerr << "LineAndFillConverter::apply: modeMap is empty or not CV_8UC1." << std::endl;
        return;
    }

    cv::Mat rbit, lines, thinned_lines, filled, vis;
    for(auto pair : colorMap.MapOfColorName) {
        int color_id = pair.first;

        if(colorMap.MapOfColor.find(color_id) == colorMap.MapOfColor.end()) {
            continue;
        }
        if(pair.second == "white"){
            continue; // Skip background color
        }
        cv::Mat mask = colorMap.MapOfColor.at(color_id);
        if(mask.empty() || mask.type() != CV_8UC1) {
            continue;
        }

        removeSmallComponents(mask, min_size).copyTo(rbit);
        if(opening_radius > 0){
            cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                        cv::Size(2 * opening_radius + 1, 2 * opening_radius + 1),
                                                        cv::Point(opening_radius, opening_radius));
            cv::morphologyEx(rbit, rbit, cv::MORPH_OPEN, element);
        }
        classifyPixels(rbit & (modeMap == mode), lines, thinned_lines, filled, vis, radius);

        outData.edge_masks[color_id] = outData.edge_masks[color_id] | lines;
        outData.filled_masks[color_id] = outData.filled_masks[color_id] | filled;
        if(outline_mode) {
            outData.outline_masks[color_id] = outData.outline_masks[color_id] | filled;
        }
    }
}
bool LineAndFillConverter::drawGui() {
    bool changed = false;
    if(ImGui::Checkbox("Outline Mode", &outline_mode)){
        changed = true;
    }
    ImGui::PushItemWidth(200);
    if(ImGui::SliderInt("Radius", &radius, 4, 20)){
        changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::PushItemWidth(200);
    if(ImGui::SliderInt("Min Size", &min_size, 1, 20)){
        changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::PushItemWidth(200);
    if(ImGui::SliderInt("Opening Radius", &opening_radius, 0, 20)){
        changed = true;
    }
    return changed;
}

OutlineConverter::OutlineConverter() {}
void OutlineConverter::apply(const cv::Mat& original, const ColorMap& colorMap, const cv::Mat& modeMap, const int mode, VectorData& outData) {
    if(original.empty() || original.channels() != 3) {
        std::cerr << "OutlineConverter::apply: original image is empty or not a 3-channel BGR image." << std::endl;
        return;
    }
    if(colorMap.colorMap.empty() || colorMap.colorMap.type() != CV_8UC1) {
        std::cerr << "OutlineConverter::apply: colorMap is empty or not CV_8UC1." << std::endl;
        return;
    }
    if(modeMap.empty() || modeMap.type() != CV_8UC1) {
        std::cerr << "OutlineConverter::apply: modeMap is empty or not CV_8UC1." << std::endl;
        return;
    }

    for(auto pair : colorMap.MapOfColorName) {
        int color_id = pair.first;

        if(colorMap.MapOfColor.find(color_id) == colorMap.MapOfColor.end()) {
            continue;
        }
        if(pair.second == "white"){
            continue; // Skip background color
        }
        cv::Mat mask = colorMap.MapOfColor.at(color_id);
        if(mask.empty() || mask.type() != CV_8UC1) {
            continue;
        }

        removeSmallComponents(mask, min_size).copyTo(mask);
        if(opening_radius > 0){
            cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                        cv::Size(2 * opening_radius + 1, 2 * opening_radius + 1),
                                                        cv::Point(opening_radius, opening_radius));
            cv::morphologyEx(mask, mask, cv::MORPH_OPEN, element);
        }

        outData.outline_masks[color_id] = outData.outline_masks[color_id] | (mask & (modeMap == mode));
    }
}
bool OutlineConverter::drawGui() {
    bool changed = false;
    ImGui::PushItemWidth(200);
    if(ImGui::SliderInt("Min Size", &min_size, 1, 20)){
        changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::PushItemWidth(200);
    if(ImGui::SliderInt("Opening Radius", &opening_radius, 0, 20)){
        changed = true;
    }
    ImGui::PopItemWidth();
    return changed;
}

REGISTER_CONVERTER_BEGIN
    REGISTER_CONVERTER("Empty", EmptyConverter)
    REGISTER_CONVERTER("Edge", EdgeConverter)
    REGISTER_CONVERTER("Fill", FillConverter)
    REGISTER_CONVERTER("Line and Fill", LineAndFillConverter)
    REGISTER_CONVERTER("Outline", OutlineConverter)
REGISTER_CONVERTER_END
