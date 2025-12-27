#include "colormap_manager.hpp"

#include <iostream>
#include <string>
#include <set>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// ImGui
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "gui_helpers.hpp"
#include "img/colormap_generator.hpp"

#define MAX_COLORS (64)

static int current_predefined_colormap_index = 0;
static const std::vector<std::pair<std::string, Colors>> predefined_colormaps = {
    {"red", {
        {"red", "FF0000"},
    }},
    {"6 Colors", {
        {"red", "FF0000"},
        {"green", "00FF00"},
        {"blue", "0000FF"},
        {"cyan", "00FFFF"},
        {"magenta", "FF00FF"},
        {"yellow", "FFFF00"},
    }},
    {"many Colors", {
        {"warm_skin",      "f5d1b0"},
        {"peach",          "f4b699"},
        {"pastel_yellow",  "f8f4c5"},
        {"muted_coral",    "f28585"},
        {"soft_pink",      "f46ba0"},
        {"raspberry",      "c7527a"},
        {"red_purple",     "b03f87"},
        {"lavender",       "a28abb"},
        {"soft_purple",    "b19acb"},
        {"violet",         "a18ab4"},
        {"deep_purple",    "684d92"},
        {"brown",          "8b654b"},
        {"olive",          "a0a062"},
        {"mint",           "a7e6cd"},
        {"teal",           "55b8d8"},
        {"sky_blue",       "a7d9dd"},
        {"bright_blue",    "476cd6"},
        {"navy",           "2f4170"},
        {"dusty_orange",   "e59570"},
        {"goldenrod",      "f2c25b"},
    }},
};

void ColorMapManager::drawGui(GLuint *button_textures) {
    generatable = true;

    std::string current_mode_str =
        (current_colormap_mode == ColorMapMode::COLOR_MAP_MODE_BINARY) ? "Binary" : "MultiColor";
    if(ImGui::BeginCombo("Color Map Mode", current_mode_str.c_str())){
        if (ImGui::Selectable("Binary")) {
            if(current_colormap_mode != ColorMapMode::COLOR_MAP_MODE_BINARY){
                newest_colormap_available = false;
                current_colormap_mode = ColorMapMode::COLOR_MAP_MODE_BINARY;
            }
        }
        if (ImGui::Selectable("MultiColor")) {
            if(current_colormap_mode != ColorMapMode::COLOR_MAP_MODE_MULTI){
                newest_colormap_available = false;
                current_colormap_mode = ColorMapMode::COLOR_MAP_MODE_MULTI;
            }
        }
        ImGui::EndCombo();
    }
    if(current_colormap_mode == ColorMapMode::COLOR_MAP_MODE_MULTI){
        if(ImGui::Checkbox("Detect Achro", &detect_achro)) {
            newest_colormap_available = false;
        }
    }

    std::set<std::string> color_set;
    bool color_codes_valid;
    int remove_color_index = -1;

    switch(current_colormap_mode){
        case ColorMapMode::COLOR_MAP_MODE_BINARY:
            if(ImGui::SliderInt("threshold", &threshold, 0, 255)){
                newest_colormap_available = false;
            }
            ImGui::SameLine();
            drawColorMarker(IM_COL32(threshold, threshold, threshold, 255));

            if(threshold == 0 || threshold == 255){
                generatable = false;
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: Threshold must be between 0 and 255.");
            }

            break;
        case ColorMapMode::COLOR_MAP_MODE_MULTI:
            if(detect_achro){
                if(ImGui::SliderFloat("Achro Detection Sensitivity", &achro_sensitivity, 0.0f, 1.0f, "%.2f")){
                    newest_colormap_available = false;
                }

                if(ImGui::BeginCombo("Achros", (std::to_string(achros) + " Achros").c_str())){
                    for(int i=2; i<=4; ++i){
                        if(ImGui::Selectable((std::to_string(i) + " Achros").c_str(), achros == i)){
                            if(achros != i){
                                newest_colormap_available = false;
                                achros = i;
                                switch(achros){
                                    case 2:
                                        achro_thresholds = {127, 255, 255, 255};
                                        break;
                                    case 3:
                                        achro_thresholds = {85, 170, 255, 255};
                                        break;
                                    case 4:
                                        achro_thresholds = {64, 128, 192, 255};
                                        break;
                                    default:
                                        std::cerr << "Invalid achros value: " << achros << std::endl;
                                        break;
                                }
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
                switch(achros){
                    case 2:
                        if(ImGui::SliderInt("Black/White Threshold", &achro_thresholds[0], 0, 255)){
                            newest_colormap_available = false;
                        }
                        ImGui::SameLine();
                        drawColorMarker(IM_COL32(achro_thresholds[0], achro_thresholds[0], achro_thresholds[0], 255));
                        break;
                    case 3:
                        if(ImGui::SliderInt("Black/Gray Threshold", &achro_thresholds[0], 0, 255)){
                            newest_colormap_available = false;
                        }
                        ImGui::SameLine();
                        drawColorMarker(IM_COL32(achro_thresholds[0], achro_thresholds[0], achro_thresholds[0], 255));
                        if(ImGui::SliderInt("Gray/White Threshold", &achro_thresholds[1], 0, 255)){
                            newest_colormap_available = false;
                        }
                        ImGui::SameLine();
                        drawColorMarker(IM_COL32(achro_thresholds[1], achro_thresholds[1], achro_thresholds[1], 255));
                        break;
                    case 4:
                        if(ImGui::SliderInt("Black/Gray Threshold", &achro_thresholds[0], 0, 255)){
                            newest_colormap_available = false;
                        }
                        ImGui::SameLine();
                        drawColorMarker(IM_COL32(achro_thresholds[0], achro_thresholds[0], achro_thresholds[0], 255));
                        if(ImGui::SliderInt("Gray/Light Gray Threshold", &achro_thresholds[1], 0, 255)){
                            newest_colormap_available = false;
                        }
                        ImGui::SameLine();
                        drawColorMarker(IM_COL32(achro_thresholds[1], achro_thresholds[1], achro_thresholds[1], 255));
                        if(ImGui::SliderInt("Light Gray/White Threshold", &achro_thresholds[2], 0, 255)){
                            newest_colormap_available = false;
                        }
                        ImGui::SameLine();
                        drawColorMarker(IM_COL32(achro_thresholds[2], achro_thresholds[2], achro_thresholds[2], 255));
                        break;
                    default:
                        std::cerr << "Invalid achros value: " << achros << std::endl;
                        break;
                }
                bool thresholds_sorted = true;
                for(int i=0; i<achros-1; ++i){
                    if(achro_thresholds[i] >= achro_thresholds[i+1]){
                        thresholds_sorted = false;
                        break;
                    }
                }
                if(!thresholds_sorted){
                    generatable = false;
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: Achro thresholds must be in ascending order.");
                }else{
                    ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeightWithSpacing()));
                }
            }

            ImGui::Separator();

            if(ImGui::BeginCombo("Predefined Color Maps", predefined_colormaps[current_predefined_colormap_index].first.c_str())){
                for(int i=0; i<predefined_colormaps.size(); ++i){
                    bool selected = (current_predefined_colormap_index == i);
                    if(ImGui::Selectable(predefined_colormaps[i].first.c_str(), false)){
                        newest_colormap_available = false;
                        current_predefined_colormap_index = i;
                        colormap_colors = predefined_colormaps[i].second;
                    }
                }
                ImGui::EndCombo();
            }

            if(ImGui::BeginTable("colormap_colors", 2, ImGuiTableFlags_SizingStretchSame)){
                ImGui::TableSetupColumn("Color Name", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("Hex Color Code");

                if(detect_achro){
                    for(int i=0; i<4; ++i){
                        switch(achros){
                            case 2:
                                if(i == 1 || i == 2) continue;
                                break;
                            case 3:
                                if(i == 2) continue;
                                break;
                            case 4:
                                break;
                            default:
                                std::cerr << "Invalid achros value: " << achros << std::endl;
                                break;
                        }

                        char buf[8];
                        snprintf(buf, sizeof(buf), "%s", achro_colors[i].second.c_str()); //先頭8文字をコピー

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", achro_colors[i].first.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::PushID(i);
                        ImGui::SameLine();
                        ImGui::PushItemWidth(100);
                        ImGui::InputText("##color box achro", buf, sizeof(buf),
                        ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_AutoSelectAll);
                        ImGui::PopItemWidth();

                        std::string new_color_str(buf);
                        if(new_color_str != achro_colors[i].second){
                            newest_colormap_available = false;
                            if(new_color_str.size() == 6){
                                achro_colors[i].second = new_color_str;
                            }else if(new_color_str.size() > 6){
                                new_color_str = new_color_str.substr(new_color_str.size() - 6, 6);
                                achro_colors[i].second = new_color_str;
                            }
                        }

                        ImGui::SameLine();
                        {
                            int r, g, b;
                            bool success = convertHexToBGR(achro_colors[i].second, b, g, r);
                            drawColorMarker(IM_COL32(r, g, b, 255));
                            if(!success){
                                ImGui::SameLine();
                                generatable = false;
                                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid Hex");
                            }
                        }
                        ImGui::PopID();
                    }
                }

                for(int i=0; i<colormap_colors.size(); ++i){
                    char buf[8];
                    snprintf(buf, sizeof(buf), "%s", colormap_colors[i].second.c_str());

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", colormap_colors[i].first.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushID(i);
                    ImGui::SameLine();
                    ImGui::PushItemWidth(100);
                    ImGui::InputText("##color box", buf, sizeof(buf),
                        ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_AutoSelectAll);
                    ImGui::PopItemWidth();
                    
                    std::string new_color_str(buf);
                    if(new_color_str != colormap_colors[i].second){
                        newest_colormap_available = false;
                        if(new_color_str.size() == 6){
                            colormap_colors[i].second = new_color_str;
                        }else if(new_color_str.size() > 6){
                            new_color_str = new_color_str.substr(new_color_str.size() - 6, 6);
                            colormap_colors[i].second = new_color_str;
                        }
                    }

                    ImGui::SameLine();
                    {
                        int r, g, b;
                        bool success = convertHexToBGR(colormap_colors[i].second, b, g, r);
                        drawColorMarker(IM_COL32(r, g, b, 255));
                        if(!success){
                            ImGui::SameLine();
                            generatable = false;
                            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid Hex");
                        }
                    }

                    ImGui::SameLine();
                    if(miniButton("Remove", button_textures[0], 24, true)){
                        newest_colormap_available = false;
                        remove_color_index = i;
                    }

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }

            if(colormap_colors.size() >= MAX_COLORS){
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Maximum number of colors reached.");
            }else{
                if(miniButton("Add Color", button_textures[3], 24, true)){
                    newest_colormap_available = false;
                    int i = 1;
                    while(true){
                        std::string new_color_name = "color" + std::to_string(i);
                        bool name_exists = false;
                        for(const auto& color : colormap_colors){
                            if(color.first == new_color_name){
                                name_exists = true;
                                break;
                            }
                        }
                        if(!name_exists){
                            colormap_colors.emplace_back(new_color_name, "FFFFFF");
                            break;
                        }
                        ++i;
                    }
                }
            }

            if(remove_color_index != -1){
                if(remove_color_index >= 0 && remove_color_index < colormap_colors.size()){
                    newest_colormap_available = false;
                    colormap_colors.erase(colormap_colors.begin() + remove_color_index);
                }
            }

            // カラーコードの被る色が無いかsetで確認
            color_codes_valid = true;
            color_set.clear();
            for(const auto& color : achro_colors){
                if(color_set.find(color.second) != color_set.end()){
                    generatable = false;
                    color_codes_valid = false;
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: Duplicate hex color codes found.");
                    break;
                }
                color_set.insert(color.second);
            }
            if(color_codes_valid){
                color_set.clear();
                for(const auto& color : colormap_colors){
                    if(color_set.find(color.second) != color_set.end()){
                        generatable = false;
                        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: Duplicate hex color codes found.");
                        break;
                    }
                    color_set.insert(color.second);
                }
            }

            break;
        default:
            std::cerr << "Invalid ColorMapMode: " << static_cast<int>(current_colormap_mode) << std::endl;
            break;
    }

    if(!generatable){
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Color map cannot be generated due to errors above.");
    }
}

void ColorMapManager::startGenerateColorMap(const cv::Mat& src) {
    if(!generatable){
        std::cerr << "Color map cannot be generated due to errors." << std::endl;
        return;
    }
    if(isCalculating()){
        std::cerr << "Color map generation is already in progress." << std::endl;
        return;
    }
    if(newest_colormap_available){
        std::cerr << "A newer color map is already available." << std::endl;
        return;
    }
    if(src.empty()){
        std::cerr << "Source image is empty." << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(mtx);
    // スレッドを作成して計算を開始
    generating_src = src.clone();
    generating_colormap = ColorMap();
    calculating = true;
    newest_colormap_available = true;
    
    std::thread([this]() {
        ColorMap new_colormap;
        cv::Mat new_viewmap;

        switch(current_colormap_mode){
            case ColorMapMode::COLOR_MAP_MODE_BINARY:
                generateBinaryColorMap(generating_src, threshold, new_colormap, new_viewmap);
                break;
            case ColorMapMode::COLOR_MAP_MODE_MULTI:
                if(detect_achro){
                    std::vector<float> achro_thresholds_vec;
                    Colors achro_colors_vec;
                    for(size_t i=0; i<4; ++i){
                        switch(achros){
                            case 2:
                                if(i == 1 || i == 2) continue;
                                break;
                            case 3:
                                if(i == 2) continue;
                                break;
                            case 4:
                                break;
                            default:
                                std::cerr << "Invalid achros value: " << achros << std::endl;
                                break;
                        }
                        achro_colors_vec.push_back(achro_colors[i]);
                    }
                    for(size_t i=0; i<achros-1; ++i){
                        achro_thresholds_vec.push_back(achro_thresholds[i] / 255.0f * 100.0f);
                    }
                    generateAchroColorMap(generating_src, achro_sensitivity, achro_thresholds_vec, achro_colors_vec, colormap_colors, new_colormap, new_viewmap);
                }else{
                    generateMultiColorMap(generating_src, colormap_colors, new_colormap, new_viewmap);
                }
                break;
            default:
                std::cerr << "Invalid ColorMapMode: " << static_cast<int>(current_colormap_mode) << std::endl;
                break;
        }

        {
            std::lock_guard<std::mutex> lock(mtx);
            generating_colormap = new_colormap;
            view_map = new_viewmap;
            calculating = false;
        }
    }).detach();
}

bool ColorMapManager::isCalculating() const {
    return calculating;
}

bool ColorMapManager::isNewestColorMapAvailable() const {
    std::lock_guard<std::mutex> lock(mtx);
    return newest_colormap_available;
}

ColorMap ColorMapManager::getColorMap() const {
    std::lock_guard<std::mutex> lock(mtx);
    if(calculating || !newest_colormap_available){
        std::cerr << "Color map is not available." << std::endl;
        return ColorMap();
    }
    return generating_colormap;
}

cv::Mat ColorMapManager::getViewMap() const {
    std::lock_guard<std::mutex> lock(mtx);
    if(calculating || !newest_colormap_available){
        std::cerr << "View map is not available." << std::endl;
        return cv::Mat();
    }
    return view_map;
}
