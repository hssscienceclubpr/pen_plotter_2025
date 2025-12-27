#include "converter_manager.hpp"

#include <iostream>
#include <thread>
#include <atomic>

#include "gui.hpp"
#include "gui_helpers.hpp"

ConverterManager::ConverterManager() {
    // Register built-in converters
    converters.push_back(std::make_unique<EmptyConverter>());
    converter_ids.push_back(0);
}

void ConverterManager::drawGui(int& selected_index, GLuint *button_textures) {
    VectorConverterRegistry& registry = VectorConverterRegistry::instance();
    std::vector<std::string> converter_names = registry.getConverterNames();

    ImGui::Text("Hatch Line Spacing");
    ImGui::SameLine();
    ImGui::PushItemWidth(150);
    if(ImGui::InputInt("##Hatch Line Spacing", &hatch_line_spacing)){
        newest_data_available = false; // Mark data as outdated if parameters changed
        if(hatch_line_spacing < 1) hatch_line_spacing = 1;
        if(hatch_line_spacing > 100) hatch_line_spacing = 1000;
    }
    ImGui::PopItemWidth();

    ImGui::Text("No Jitter Epsilon");
    ImGui::SameLine();
    ImGui::PushItemWidth(150);
    if(ImGui::InputFloat("##No Jitter Epsilon", &no_jitter_epsilon)){
        newest_data_available = false; // Mark data as outdated if parameters changed
        if(no_jitter_epsilon < 0.0) no_jitter_epsilon = 0.0;
        if(no_jitter_epsilon > 20.0) no_jitter_epsilon = 20.0;
    }
    ImGui::PopItemWidth();

    ImGui::Text("Min Polyline Length");
    ImGui::SameLine();
    ImGui::PushItemWidth(150);
    if(ImGui::InputFloat("##Min Polyline Length", &min_polyline_length)){
        newest_data_available = false; // Mark data as outdated if parameters changed
        if(min_polyline_length < 0.0) min_polyline_length = 0.0;
        if(min_polyline_length > 100.0) min_polyline_length = 100.0;
    }
    ImGui::PopItemWidth();

    ImGui::Text("Add Vector Converter");
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(2, 0));
    ImGui::SameLine();
    bool converter_limit_reached = (converters.size() >= converter_gui_colors.size());
    ImGui::PushItemWidth(200);
    if (ImGui::BeginCombo("Converter", converters[(std::max)(0, gui_selected_index)]->getConverterName().c_str())) {
        for(size_t i = 1; i < converter_names.size(); ++i) { // skip "Empty" converter at index 0
            if (ImGui::Selectable(converter_names[i].c_str(), gui_selected_index == i)) {
                if(!converter_limit_reached) {
                    newest_data_available = false; // Mark data as outdated if converter list changed
                    converters.push_back(registry.createConverter(i));
                    int new_id = 0;
                    while(std::find(converter_ids.begin(), converter_ids.end(), new_id) != converter_ids.end()) {
                        ++new_id;
                    }
                    converter_ids.push_back(new_id);
                    gui_selected_index = converters.size() - 1;
                }
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    if(converter_limit_reached) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Converter limit reached. Cannot add more converters.");
    }

    ImGui::Dummy(ImVec2(0, 10));

    int remove_index = -1;

    for(size_t i = 1; i < converters.size(); ++i) { // skip "Empty" converter at index 0
        const int id = converter_ids[i];

        ImGui::PushID(id);
        BeginRectWithBorder();
        if(miniButton("remove", button_textures[0], 24)){
            remove_index = i;
        }
        ImGui::SameLine();
        ImGui::Text("%s", converters[i]->getConverterName().c_str());
        ImGui::SameLine();
        cv::Scalar color = converter_gui_colors[converter_ids[i] % converter_gui_colors.size()];
        drawColorMarker(IM_COL32(color[2], color[1], color[0], 255));
        if(converters[i]->drawGui()){
            newest_data_available = false; // Mark data as outdated if parameters changed
        }
        bool active = (selected_converter_id == id);
        if(EndRectWithBorder(active)){
            selected_converter_id = id;
            selected_index = id;
        }
        ImGui::PopID();
    }

    if(remove_index >= 0){
        if(remove_index >= 0 && remove_index < converters.size()){
            if(selected_converter_id == converter_ids[remove_index]){
                selected_converter_id = -1;
                selected_index = -1;
            }
            converters.erase(converters.begin() + remove_index);
            converter_ids.erase(converter_ids.begin() + remove_index);
            gui_selected_index = -1;
        }
    }
}

void ConverterManager::startCalculation(const cv::Mat& original, const ColorMap& colorMap, const cv::Mat& mode_map, const ShellManager& shell_manager) {
    std::lock_guard<std::mutex> lock(mtx);

    if(calculating) {
        std::cerr << "ConverterManager::startCalculation: Already calculating." << std::endl;
        return; // Already calculating
    }
    calculating = true;
    newest_data_available = true;

    // Copy inputs
    original_copy = original.clone();
    colorMap_copy = colorMap.clone();
    mode_map_copy = mode_map.clone();
    shell_manager_copy = shell_manager; // Assuming ShellManager is copyable

    std::thread([this]() {
        VectorData new_vector_data;
        new_vector_data.width = original_copy.cols;
        new_vector_data.height = original_copy.rows;
        new_vector_data.color_names = colorMap_copy.MapOfColorName;
        for(const auto& [id, name] : colorMap_copy.MapOfColorName) {
            std::cout << "Color ID: " << id << ", Name: " << name << std::endl;
        }
        for (const auto& [color_id, _] : colorMap_copy.MapOfColor) {
            new_vector_data.filled_masks[color_id] = cv::Mat::zeros(original_copy.size(), CV_8UC1);
            new_vector_data.edge_masks[color_id] = cv::Mat::zeros(original_copy.size(), CV_8UC1);
            new_vector_data.outline_masks[color_id] = cv::Mat::zeros(original_copy.size(), CV_8UC1);
            new_vector_data.color_values[color_id] = cv::Scalar(0,0,0);
            if(colorMap_copy.MapOfColorValueBGR.find(color_id) != colorMap_copy.MapOfColorValueBGR.end()) {
                new_vector_data.color_values[color_id] = colorMap_copy.MapOfColorValueBGR.at(color_id);
            }
        }
        for(size_t i = 1; i < converters.size(); ++i) { // skip "Empty" converter at index 0
            int mode = i; // mode corresponds to converter index
            std::cout << "Applying converter: " << converters[i]->getConverterName() << " (ID: " << converter_ids[i] << ")" << std::endl;
            converters[i]->apply(original_copy, colorMap_copy, mode_map_copy, mode, new_vector_data);
            std::cout << "Finished converter: " << converters[i]->getConverterName() << std::endl;
        }
        cv::Mat view_map_temp, view_map_with_points_temp, view_map_with_hatch_temp, view_random_colored_temp;
        lastConvertToVectorData(new_vector_data, view_map_temp, view_map_with_points_temp, view_map_with_hatch_temp, view_random_colored_temp, hatch_line_spacing, 45, 20, no_jitter_epsilon, min_polyline_length, shell_manager_copy.hatchLineSettings);
        {
            std::lock_guard<std::mutex> lock(mtx);
            vector_data = std::move(new_vector_data);
            view_map = std::move(view_map_temp);
            view_map_with_points = std::move(view_map_with_points_temp);
            view_map_with_hatch = std::move(view_map_with_hatch_temp);
            view_random_colored = std::move(view_random_colored_temp);
            calculating = false;
        }
    }).detach();
}

bool ConverterManager::isCalculating() const {
    std::lock_guard<std::mutex> lock(mtx);
    return calculating;
}

bool ConverterManager::isNewestDataAvailable() const {
    std::lock_guard<std::mutex> lock(mtx);
    return newest_data_available;
}

void ConverterManager::changedInput() {
    std::lock_guard<std::mutex> lock(mtx);
    newest_data_available = false; // Mark data as outdated if input image changed
}

void ConverterManager::getVectorData(VectorData& outData) const {
    std::lock_guard<std::mutex> lock(mtx);
    outData = vector_data;
}

bool ConverterManager::getViewMaps(cv::Mat& viewMap, cv::Mat& viewMapWithPoints, cv::Mat& viewMapWithHatch, cv::Mat& viewRandomColored) const {
    std::lock_guard<std::mutex> lock(mtx);
    if(view_map.empty() || view_map_with_points.empty() || view_map_with_hatch.empty() || view_random_colored.empty()) {
        return false;
    }
    viewMap = view_map;
    viewMapWithPoints = view_map_with_points;
    viewMapWithHatch = view_map_with_hatch;
    viewRandomColored = view_random_colored;
    return true;
}
