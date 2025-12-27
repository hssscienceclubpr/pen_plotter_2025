#include "gui.hpp"

#include <iostream>
#include <thread>
#include <mutex>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// ImGui
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <opencv4/opencv2/core/mat.hpp>
#include <opencv4/opencv2/imgcodecs.hpp>
#include <opencv4/opencv2/imgproc.hpp>

#include "gui_helpers.hpp"
#include "filters.hpp"
#include "filter_calc_manager.hpp"
#include "colormap_manager.hpp"
#include "converter_manager.hpp"
#include "vector_converters.hpp"
#include "output_manager.hpp"
#include "shell_manager.hpp"
#include "optimizer.hpp"
#include "cross/cross.hpp"

#define MAX_FILTERS (100)

#define BUTTON_TEXTURES (7)

static cv::Mat editing_img;
static cv::Mat mode_map; // For convert to vector
static FilterCalcManager filter_calc_manager;
static GLuint button_textures[BUTTON_TEXTURES];

static ConverterManager converter_manager;
static ShellManager shell_manager;

static int current_image_index = 0;

static cv::Scalar hovered_color(-1, -1, -1);

void setup() {
    editing_img = imread_color(getExecutableDir() + "/assets/images/default.jpeg");
    mode_map = cv::Mat::zeros(editing_img.size(), CV_8UC1);
    if (editing_img.empty()) {
        std::cerr << "Failed to load image." << std::endl;
    }
    for(int i = 0; i < BUTTON_TEXTURES; ++i){
        button_textures[i] = 0;
    }
    loadTextureFromFile((getExecutableDir() + "/assets/images/clear_24dp_FFFFFF.png").c_str(), &button_textures[0], nullptr);
    loadTextureFromFile((getExecutableDir() + "/assets/images/arrow_upward_24dp_FFFFFF.png").c_str(), &button_textures[1], nullptr);
    loadTextureFromFile((getExecutableDir() + "/assets/images/arrow_downward_24dp_FFFFFF.png").c_str(), &button_textures[2], nullptr);
    loadTextureFromFile((getExecutableDir() + "/assets/images/add_2_24dp_FFFFFF_FILL0_wght400_GRAD0_opsz24.png").c_str(), &button_textures[3], nullptr);
    loadTextureFromFile((getExecutableDir() + "/assets/images/draw_24dp_FFFFFF_FILL0_wght400_GRAD0_opsz24.png").c_str(), &button_textures[4], nullptr);
    loadTextureFromFile((getExecutableDir() + "/assets/images/rectangle_24dp_FFFFFF_FILL0_wght400_GRAD0_opsz24.png").c_str(), &button_textures[5], nullptr);
    loadTextureFromFile((getExecutableDir() + "/assets/images/colors_24dp_FFFFFF_FILL0_wght400_GRAD0_opsz24.png").c_str(), &button_textures[6], nullptr);
}
void end() {
    cleanupTextures();
    for (int i = 0; i < BUTTON_TEXTURES; ++i) {
        if (button_textures[i] != 0) {
            glDeleteTextures(1, &button_textures[i]);
            button_textures[i] = 0;
        }
    }
}
void getTargetFrameTime(std::chrono::nanoseconds& frame_time) {
    frame_time = std::chrono::nanoseconds(1000000000 / 60); // 約60FPS
}
void processInput(GLFWwindow* window) {
    if(ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift && glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS){
        if(hovered_color[0] >= 0){
            if(editing_img.channels() == 1){
                std::string hex = cv::format("%02X", (int)hovered_color[0]);
                hex = hex + hex + hex;
                glfwSetClipboardString(window, hex.c_str());
                std::cout << "Copied to clipboard: Gray " << (int)hovered_color[0] << std::endl;
            } else if(editing_img.channels() == 3 || editing_img.channels() == 4){
                std::string hex = cv::format("%02X%02X%02X", (int)hovered_color[2], (int)hovered_color[1], (int)hovered_color[0]);
                glfwSetClipboardString(window, hex.c_str());
                std::cout << "Copied to clipboard: #" << hex << std::endl;
            }
        }
    }
}
void cleanUp() {
    cleanupTexturesOverCache();
}

static std::vector<std::unique_ptr<Filter>> filters;
static std::vector<int> filter_indexes;
static int last_pushed_filter_index = 0;
void drawFiltersGui() {
    const float separetor_padding = 3.0f;

    auto& registry = FilterRegistry::instance();
    auto filter_count = registry.getFilterCount();
    auto filter_names = registry.getFilterNames();

    size_t action_index = -1;
    int action_type = -1; // 0: remove, 1: up, 2: down

    ImGui::Text("Add Filter");
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(2, 0));
    ImGui::SameLine();
    if(ImGui::BeginCombo("##Add Filter", filter_names[last_pushed_filter_index].c_str())){
        int filter_count = registry.getFilterCount();
        for(int i = 0; i < filter_count; ++i){
            if (ImGui::Selectable(registry.getFilterNames()[i].c_str(), false)) {
                filters.push_back(registry.createFilter(i));
                last_pushed_filter_index = i;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Dummy(ImVec2(0, separetor_padding));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, separetor_padding));

    for(size_t i = 0; i < filters.size(); ++i) {
        ImGui::PushID(filters[i]->unique_id);

        BeginRectWithBorder();

        ImGui::Text("%s", (filters[i]->getFilterName()).c_str());
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(10, 0));
        ImGui::SameLine();
        if(button_textures[0] == 0){
            std::cerr << "Button texture not loaded." << std::endl;
        }
        if(miniButton("remove", button_textures[0], 24)){
            action_index = i;
            action_type = 0;
        }
        ImGui::SameLine();
        if(miniButton("up", button_textures[1], 24, i > 0)){
            action_index = i;
            action_type = 1;
        }
        ImGui::SameLine();
        if(miniButton("down", button_textures[2], 24, i < filters.size() - 1)){
            action_index = i;
            action_type = 2;
        }
        if(filters[i]->drawGui()){
            filter_calc_manager.changedInput();
        }

        bool selected = (current_image_index == static_cast<int>(i));

        bool clicked = EndRectWithBorder(selected);

        if(action_type < 0 && clicked){
            current_image_index = static_cast<int>(i);
        }

        ImGui::PopID();
    }

    switch(action_type) {
        case 0: // remove
            if (action_index >= 0 && action_index < filters.size()) {
                filters.erase(filters.begin() + action_index);
            }
            filter_calc_manager.changedInput();
            break;
        case 1: // up
            if (action_index > 0 && action_index < filters.size()) {
                std::swap(filters[action_index], filters[action_index - 1]);
            }
            filter_calc_manager.changedInput();
            break;
        case 2: // down
            if (action_index >= 0 && action_index < filters.size() - 1) {
                std::swap(filters[action_index], filters[action_index + 1]);
            }
            filter_calc_manager.changedInput();
            break;
        default:
            break;
    }
}

void drawFilterTabGui() {
    if(ImGui::Button("Show Original")){
        current_image_index = -1;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(filter_calc_manager.isCalculating() || filter_calc_manager.isNewestImageAvailable());
    if(ImGui::Button("Apply Filters")){
        filter_calc_manager.startCalculation(filters, editing_img);
        current_image_index = -1;
    }
    ImGui::EndDisabled();
    drawFiltersGui();
}

static ColorMapManager color_map_manager;
void drawColorMapTabGui() {
    bool generate = false;
    ImGui::BeginDisabled(!filter_calc_manager.isNewestImageAvailable() || !color_map_manager.isGeneratable() || color_map_manager.isCalculating() || color_map_manager.isNewestColorMapAvailable());
    if(ImGui::Button("Generate Color Map")){
        generate = true;
    }
    ImGui::EndDisabled();
    if(color_map_manager.isCalculating()){
        ImGui::SameLine();
        ImGui::Text("Calculating...");
    }

    ImGui::Dummy(ImVec2(0, 20));

    color_map_manager.drawGui(button_textures);
    if(generate && color_map_manager.isGeneratable() && !color_map_manager.isCalculating() && filter_calc_manager.isNewestImageAvailable()){
        cv::Mat src;
        if(filter_calc_manager.getProcessedImages().empty()){
            src = editing_img;
        } else {
            src = filter_calc_manager.getProcessedImages().back();
        }
        color_map_manager.startGenerateColorMap(src);

        converter_manager.changedInput();
    }
}

int selected_index = 0;
static cv::Mat mixed_img;
static cv::Mat mode_map_colored;
static float old_mouse_x = -1, old_mouse_y = -1;
static float rect_start_x = -1, rect_start_y = -1;
static float pen_size = 5.0f;
int drawEditor(const cv::Mat& img, int convert_to_vector_mode, int draw_mode, cv::Mat& mode_map, int max_width = -1, int max_height = -1) {
    if(img.empty()){
        std::cerr << "Image is empty." << std::endl;
        return 0;
    }
    if(img.channels() != 3 && img.channels() != 4){
        std::cerr << "Image must have 3 or 4 channels." << std::endl;
        return 0;
    }
    if(mode_map.size() != img.size() || mode_map.type() != CV_8UC1){
        std::cerr << "Mode map size or type is invalid. Resetting mode map." << std::endl;
        mode_map = cv::Mat::zeros(img.size(), CV_8UC1);
    }

    if(mode_map_colored.size() != img.size() || mode_map_colored.type() != CV_8UC3){
        mode_map_colored = cv::Mat::zeros(img.size(), CV_8UC3);
    }
    for(size_t i = 0; i < converter_gui_colors.size(); ++i) {
        mode_map_colored.setTo(converter_gui_colors[i], mode_map == (int)i);
    }

    // αブレンド
    cv::Mat mixed_img;
    if(!color_map_manager.isCalculating() && color_map_manager.isNewestColorMapAvailable()){
        cv::addWeighted(mode_map_colored, 0.5, color_map_manager.getViewMap(), 0.5, 0.0, mixed_img);
    } else {
        cv::addWeighted(mode_map_colored, 0.5, img, 0.5, 0.0, mixed_img);
    }
    float mouse_x = -1;
    float mouse_y = -1;

    cv::Mat draw_img;
    if(convert_to_vector_mode == 0){
        draw_img = img.clone();
    }else if(convert_to_vector_mode == 1){
        if(!color_map_manager.isNewestColorMapAvailable() || color_map_manager.isCalculating()){
            draw_img = img.clone();
        }else{
            draw_img = color_map_manager.getViewMap().clone();
        }
    }else if(convert_to_vector_mode == 2){
        draw_img = mixed_img.clone();
    }else if(convert_to_vector_mode == 3){
        draw_img = mode_map_colored.clone();
    }else{
        std::cerr << "Invalid convert_to_vector_mode: " << convert_to_vector_mode << std::endl;
        return 0;
    }

    if(old_mouse_x >= 0 && old_mouse_y >= 0){
        if(draw_mode == 0){
            cv::circle(draw_img, cv::Point((int)old_mouse_x, (int)old_mouse_y), pen_size, cv::Scalar(0, 255, 255), 2, cv::LINE_8);
        }else if(draw_mode == 1){
            if(rect_start_x >= 0 && rect_start_y >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left)){
                cv::rectangle(draw_img, 
                    cv::Point((int)rect_start_x, (int)rect_start_y),
                    cv::Point((int)old_mouse_x, (int)old_mouse_y), cv::Scalar(0, 255, 255), 2, cv::LINE_8);
            }
        }
    }

    cleanupTextures();

    ImVec2 img_size = drawMat(draw_img, max_width, max_height, &hovered_color, &mouse_x, &mouse_y);

    if(mouse_x >= 0 && mouse_y >= 0){
        if(draw_mode == 0){
            if(ImGui::GetIO().MouseWheel != 0.0f){
                pen_size += ImGui::GetIO().MouseWheel;
                if(pen_size < 1.0f) pen_size = 1.0f;
                if(pen_size > 100.0f) pen_size = 100.0f;
            }

            if(ImGui::IsMouseDown(ImGuiMouseButton_Left)){
                if(mouse_x >= 0 && mouse_x < mode_map.cols && mouse_y >= 0 && mouse_y < mode_map.rows){
                    if(mouse_x != old_mouse_x || mouse_y != old_mouse_y){
                        converter_manager.changedInput();
                        cv::line(mode_map, 
                            cv::Point((int)old_mouse_x, (int)old_mouse_y),
                            cv::Point((int)mouse_x, (int)mouse_y), cv::Scalar(selected_index), pen_size, cv::LINE_8);
                    }
                }
            }
        }else if(draw_mode == 1){
            if(ImGui::IsMouseDown(ImGuiMouseButton_Left)){
                if(rect_start_x < 0 || rect_start_y < 0){
                    rect_start_x = mouse_x;
                    rect_start_y = mouse_y;
                }
            } else {
                if(rect_start_x >= 0 && rect_start_y >= 0 && old_mouse_x >= 0 && old_mouse_y >= 0){
                    if(rect_start_x != old_mouse_x && rect_start_y != old_mouse_y){
                        converter_manager.changedInput();
                        cv::rectangle(mode_map, 
                            cv::Point((int)rect_start_x, (int)rect_start_y),
                            cv::Point((int)old_mouse_x, (int)old_mouse_y), cv::Scalar(selected_index), -1, cv::LINE_8);
                    }
                    rect_start_x = -1;
                    rect_start_y = -1;
                }
            }
        }
    }

    old_mouse_x = mouse_x;
    old_mouse_y = mouse_y;

    return img_size.x;
}

void drawConvertersGui() {
    ImGui::BeginDisabled(converter_manager.isCalculating() || converter_manager.isNewestDataAvailable() || color_map_manager.isCalculating() || !color_map_manager.isNewestColorMapAvailable());
    if(ImGui::Button("Convert to vector")){
        if(mode_map.empty() || mode_map.size() != editing_img.size() || mode_map.type() != CV_8UC1){
            std::cerr << "Mode map is invalid." << std::endl;
            return;
        }
        converter_manager.startCalculation(editing_img, color_map_manager.getColorMap(), mode_map, shell_manager);
    }
    ImGui::EndDisabled();
    if(converter_manager.isCalculating()){
        ImGui::SameLine();
        ImGui::Text("Calculating...");
    }
    converter_manager.drawGui(selected_index, button_textures);
}

static bool write_display_img = false;
// 0: original, 1: colormap, 2: mixed, 3: area, 4: view, 5: view with points, 6: view with hatch, 7: view random colored
static int convert_to_vector_mode = 0;
static int draw_mode = 1; // 0: draw, 1: rectangle
void drawConvertToVectorTabGui() {
    ImGui::BeginDisabled(convert_to_vector_mode == 0);
    if(ImGui::Button("Original")){
        convert_to_vector_mode = 0;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(convert_to_vector_mode == 1);
    if(ImGui::Button("Color Map")){
        convert_to_vector_mode = 1;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(convert_to_vector_mode == 2);
    if(ImGui::Button("Mixed")){
        convert_to_vector_mode = 2;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(convert_to_vector_mode == 3);
    if(ImGui::Button("Area")){
        convert_to_vector_mode = 3;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10, 0));
    ImGui::SameLine();
    ImGui::BeginDisabled(convert_to_vector_mode == 4);
    if(ImGui::Button("View")){
        convert_to_vector_mode = 4;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(convert_to_vector_mode == 5);
    if(ImGui::Button("View with Points")){
        convert_to_vector_mode = 5;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(convert_to_vector_mode == 6);
    if(ImGui::Button("View with Hatch")){
        convert_to_vector_mode = 6;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(convert_to_vector_mode == 7);
    if(ImGui::Button("View random colored")){
        convert_to_vector_mode = 7;
    }
    ImGui::EndDisabled();

    ImGui::Dummy(ImVec2(0, 10));

    if(ImGui::Button("Write display img")){
        write_display_img = true;
    }else{
        write_display_img = false;
    }

    ImGui::Dummy(ImVec2(0, 10));

    if(miniButton("draw", button_textures[4], 24, draw_mode != 0)){
        draw_mode = 0;
    }
    ImGui::SameLine();
    if(miniButton("rectangle", button_textures[5], 24, draw_mode != 1)){
        draw_mode = 1;
    }
    ImGui::SameLine();
    if(draw_mode == 0){
        ImGui::Text("Draw");
    } else if(draw_mode == 1){
        ImGui::Text("Rectangle");
    }
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10, 0));
    ImGui::SameLine();
    if(miniButton("all fill", button_textures[6], 24)){
        if(mode_map.empty() || mode_map.size() != editing_img.size() || mode_map.type() != CV_8UC1){
            std::cerr << "Mode map is invalid." << std::endl;
            return;
        }
        converter_manager.changedInput();
        mode_map.setTo(cv::Scalar(selected_index));
    }

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));

    drawConvertersGui();
}

static OutputManager output_manager;
void drawOutputGui() {
    VectorData vector_data;
    converter_manager.getVectorData(vector_data);
    output_manager.drawGui(vector_data, button_textures);
}

void drawShellGui() {
    if(shell_manager.drawGui()) {
        converter_manager.changedInput();
    }
}

static OptimizerGui optimizer_gui;
void drawOptimizeGui() {
    VectorData vector_data;
    output_manager.getOutputData(vector_data);
    optimizer_gui.drawGui(vector_data);
}

static int tab_mode = 0; // 0: filter, 1: colormap, 2: convert to vector, 3: output, 4: shell, 5: optimize
void drawGui(float fps) {
    if(ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Image")) {
                openFileDialog("Open Image", getExecutableDir() + "/saves/images");
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImVec2 content_region_avail = ImGui::GetContentRegionAvail();
    float window_width = content_region_avail.x;
    float window_height = content_region_avail.y;

    ImGui::Columns(2, "columuns", false);

    cv::Mat display_img;
    const int max_width = window_width * 0.5;
    const int max_height = window_height;
    if(tab_mode == 0){
        if(current_image_index == -1) {
            display_img = editing_img;
        } else if(current_image_index <= filter_calc_manager.getLastCompletedFilterIndex()) {
            display_img = filter_calc_manager.getProcessedImages()[current_image_index];
        } else {
            display_img = editing_img;
        }
        ImVec2 img_size = drawMat(display_img, max_width, max_height, &hovered_color);
        ImGui::SetColumnWidth(0, img_size.x + ImGui::GetStyle().ItemSpacing.x * 2);
    }else if(tab_mode == 1){
        if(color_map_manager.isNewestColorMapAvailable() && !color_map_manager.isCalculating()){
            display_img = color_map_manager.getViewMap();
        } else {
            if(filter_calc_manager.getProcessedImages().empty()){
                display_img = editing_img;
            } else {
                display_img = filter_calc_manager.getProcessedImages().back();
            }
        }
        ImVec2 img_size = drawMat(display_img, max_width, max_height, &hovered_color);
        ImGui::SetColumnWidth(0, img_size.x + ImGui::GetStyle().ItemSpacing.x * 2);
    }else if(tab_mode == 2){
        if(convert_to_vector_mode > 3){
            if(convert_to_vector_mode == 4){
                if(converter_manager.isNewestDataAvailable() && !converter_manager.isCalculating()){
                    cv::Mat view_map;
                    cv::Mat view_map_with_points;
                    cv::Mat view_map_with_hatch;
                    cv::Mat view_random_colored;
                    if(converter_manager.getViewMaps(view_map, view_map_with_points, view_map_with_hatch, view_random_colored)){
                        display_img = view_map;
                    } else {
                        display_img = editing_img;
                    }
                } else {
                    display_img = editing_img;
                }
            } else if(convert_to_vector_mode == 5){
                if(converter_manager.isNewestDataAvailable() && !converter_manager.isCalculating()){
                    cv::Mat view_map;
                    cv::Mat view_map_with_points;
                    cv::Mat view_map_with_hatch;
                    cv::Mat view_random_colored;
                    if(converter_manager.getViewMaps(view_map, view_map_with_points, view_map_with_hatch, view_random_colored)){
                        display_img = view_map_with_points;
                    } else {
                        display_img = editing_img;
                    }
                } else {
                    display_img = editing_img;
                }
            } else if(convert_to_vector_mode == 6){
                if(converter_manager.isNewestDataAvailable() && !converter_manager.isCalculating()){
                    cv::Mat view_map;
                    cv::Mat view_map_with_points;
                    cv::Mat view_map_with_hatch;
                    cv::Mat view_random_colored;
                    if(converter_manager.getViewMaps(view_map, view_map_with_points, view_map_with_hatch, view_random_colored)){
                        display_img = view_map_with_hatch;
                    } else {
                        display_img = editing_img;
                    }
                } else {
                    display_img = editing_img;
                }
            } else if(convert_to_vector_mode == 7){
                if(converter_manager.isNewestDataAvailable() && !converter_manager.isCalculating()){
                    cv::Mat view_map;
                    cv::Mat view_map_with_points;
                    cv::Mat view_map_with_hatch;
                    cv::Mat view_random_colored;
                    if(converter_manager.getViewMaps(view_map, view_map_with_points, view_map_with_hatch, view_random_colored)){
                        display_img = view_random_colored;
                    } else {
                        display_img = editing_img;
                    }
                } else {
                    display_img = editing_img;
                }
            } else {
                std::cerr << "Invalid convert_to_vector_mode: " << convert_to_vector_mode << std::endl;
                display_img = editing_img;
            }
            ImVec2 img_size = drawMat(display_img, max_width, max_height, &hovered_color);
            ImGui::SetColumnWidth(0, img_size.x + ImGui::GetStyle().ItemSpacing.x * 2);
        }else{
            int width = drawEditor(editing_img, convert_to_vector_mode, draw_mode, mode_map, max_width, max_height);
            ImGui::SetColumnWidth(0, width + ImGui::GetStyle().ItemSpacing.x * 2);
        }
        if(write_display_img && !display_img.empty()){
            std::string write_path = getExecutableDir() + "/display_img.png";
            if(imwrite(write_path, display_img)){
                std::cout << "Wrote display image to: " << write_path << std::endl;
            } else {
                std::cerr << "Failed to write display image to: " << write_path << std::endl;
            }
        }
    }else if(tab_mode == 3){
        display_img = output_manager.getViewImage();
        if(display_img.empty()){
            display_img = editing_img;
        }
        ImVec2 img_size = drawMat(display_img, max_width, max_height, &hovered_color);
        ImGui::SetColumnWidth(0, img_size.x + ImGui::GetStyle().ItemSpacing.x * 2);
    }else if(tab_mode == 4){
        display_img = editing_img;
        ImVec2 img_size = drawMat(display_img, max_width, max_height, &hovered_color);
        ImGui::SetColumnWidth(0, img_size.x + ImGui::GetStyle().ItemSpacing.x * 2);
    }else if(tab_mode == 5){
        display_img = optimizer_gui.getViewImage();
        if(display_img.empty()) display_img = editing_img;
        ImVec2 img_size = drawMat(display_img, max_width, max_height, &hovered_color);
        ImGui::SetColumnWidth(0, img_size.x + ImGui::GetStyle().ItemSpacing.x * 2);
    }
    ImGui::NextColumn();
    ImGui::Text("Image Size: %d x %d", editing_img.cols, editing_img.rows);

    if (fps > 0) {
        ImGui::Text("FPS: %.1f", fps);
    } else {
        ImGui::Text("FPS: N/A");
    }

    if(hovered_color[0] >= 0){
        if(display_img.channels() == 1){
            ImGui::Text("Hovered Color: Gray %d", (int)hovered_color[0]);
            ImGui::SameLine();
            drawColorMarker(IM_COL32(hovered_color[0], hovered_color[0], hovered_color[0], 255));
        } else if(display_img.channels() == 3 || display_img.channels() == 4){
            ImGui::Text("Hovered Color: #%s", 
                (cv::format("%02X%02X%02X", (int)hovered_color[2], (int)hovered_color[1], (int)hovered_color[0])).c_str()
            );
            ImGui::SameLine();
            drawColorMarker(IM_COL32(hovered_color[2], hovered_color[1], hovered_color[0], 255));
        }
    } else {
        ImGui::Text("Hovered Color: N/A");
    }
    ImGui::Text("Hex copy to clipboard: Control + Shift + C");

    ImGui::Dummy(ImVec2(0, ImGui::GetStyle().ItemSpacing.y));

    if(ImGui::BeginTabBar("Mode Tab Bar")){
        if(ImGui::BeginTabItem("Filter")){
            tab_mode = 0;
            ImGui::Dummy(ImVec2(0, 5));
            drawFilterTabGui();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Color Map")){
            tab_mode = 1;
            ImGui::Dummy(ImVec2(0, 5));
            drawColorMapTabGui();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Convert To Vector")){
            tab_mode = 2;
            ImGui::Dummy(ImVec2(0, 5));
            drawConvertToVectorTabGui();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Output")){
            tab_mode = 3;
            ImGui::Dummy(ImVec2(0, 5));
            drawOutputGui();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Shell")){
            tab_mode = 4;
            ImGui::Dummy(ImVec2(0, 5));
            drawShellGui();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Optimize")){
            tab_mode = 5;
            ImGui::Dummy(ImVec2(0, 5));
            drawOptimizeGui();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    openAllPopups();

    if(ImGui::BeginPopupModal("Open Image", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string out_path;
        if(fileDialog("Open Image", out_path)){
            if(!out_path.empty()) {
                editing_img = imread_color(out_path);
                mode_map = cv::Mat::zeros(editing_img.size(), CV_8UC1);
                current_image_index = -1;
                filter_calc_manager.changedInput();
                std::cout << "Selected file: " << out_path << std::endl;
            }
        }
        ImGui::EndPopup();
    }
}
