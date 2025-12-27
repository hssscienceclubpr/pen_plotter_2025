#include "filters.hpp"

#include <iostream>
#include <string>
#include <memory>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// ImGui
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <opencv4/opencv2/core/mat.hpp>
#include <opencv4/opencv2/imgcodecs.hpp>

#include "img/filter_funcs.hpp"
#include "cross/cross.hpp"

#define MAX_DIAMETER (9)
#define MAX_SIGMA_COLOR (50.0f)
#define MAX_SIGMA_SPACE (50.0f)
#define MAX_LOOPS (20)

static int next_id = 0;

static const char* grayscale_modes[] = {
    "Lab", "Default"
};
GrayscaleFilter::GrayscaleFilter() : mode(GrayscaleMode::Lab) {}
void GrayscaleFilter::apply(const cv::Mat& src, cv::Mat& dst) {
    convertToGrayScale(src, dst, mode);
}
bool GrayscaleFilter::drawGui() {
    bool changed = false;
    const int index = (mode == GrayscaleMode::Lab) ? 0 : 1;
    ImGui::SetNextItemWidth(150);
    if(ImGui::BeginCombo("##grayscale mode combo box", grayscale_modes[index])){
        for(int i = 0; i < IM_ARRAYSIZE(grayscale_modes); ++i){
            bool is_selected = (i == index);
            if (ImGui::Selectable(grayscale_modes[i], is_selected)) {
                if(i != index){
                    changed = true;
                }
                mode = (i == 0) ? GrayscaleMode::Lab : GrayscaleMode::Default;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus(); // デフォルトでフォーカス
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

BilateralFilter::BilateralFilter() {}
void BilateralFilter::apply(const cv::Mat& src, cv::Mat& dst) {
    bilateral(src, dst, diameter, static_cast<double>(sigmaColor), static_cast<double>(sigmaSpace), loops);
}
bool BilateralFilter::drawGui() {
    bool changed = false;

    if (ImGui::BeginTable("filter_params", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Control");

        auto old_diameter = diameter;
        auto old_sigmaColor = sigmaColor;
        auto old_sigmaSpace = sigmaSpace;
        auto old_loops = loops;

        // diameter
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("diameter");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(150);
        ImGui::SliderInt("##slider_diameter", &diameter, 3, 9);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushItemWidth(50);
        ImGui::InputInt("##input_diameter", &diameter, 2, 4);
        ImGui::PopItemWidth();
        if (diameter % 2 == 0) diameter += 1;
        if (diameter < 3) diameter = 3;
        else if (diameter > 9) diameter = 9;

        // sigmaColor
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("sigmaColor");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(150);
        ImGui::SliderFloat("##slider_sigmaColor", &sigmaColor, 0.0f, MAX_SIGMA_COLOR, "%.2f");
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushItemWidth(60);
        ImGui::InputFloat("##input_sigmaColor", &sigmaColor, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();
        if(sigmaColor < 0.0f){
            sigmaColor = 0.0f;
        }else if(sigmaColor > MAX_SIGMA_COLOR){
            sigmaColor = MAX_SIGMA_COLOR;
        }

        // sigmaSpace
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("sigmaSpace");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(150);
        ImGui::SliderFloat("##slider_sigmaSpace", &sigmaSpace, 0.0f, MAX_SIGMA_SPACE, "%.2f");
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushItemWidth(60);
        ImGui::InputFloat("##input_sigmaSpace", &sigmaSpace, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();
        if(sigmaSpace < 0.0f){
            sigmaSpace = 0.0f;
        }else if(sigmaSpace > MAX_SIGMA_SPACE){
            sigmaSpace = MAX_SIGMA_SPACE;
        }

        // loops
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("loops");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(150);
        ImGui::SliderInt("##slider_loops", &loops, 1, 20);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushItemWidth(50);
        ImGui::InputInt("##input_loops", &loops, 1, 5);
        ImGui::PopItemWidth();
        if(loops < 1){
            loops = 1;
        }else if(loops > 20){
            loops = 20;
        }

        if(diameter != old_diameter || sigmaColor != old_sigmaColor
            || sigmaSpace != old_sigmaSpace || loops != old_loops){
            changed = true;
        }
        ImGui::EndTable();
    }

    return changed;
}

REGISTER_BEGIN
    REGISTER_FILTER("Grayscale", GrayscaleFilter);
    REGISTER_FILTER("Bilateral", BilateralFilter);
REGISTER_END
