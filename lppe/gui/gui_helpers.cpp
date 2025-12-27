#include "gui.hpp"

#include <iostream>
#include <filesystem>
#include <vector>
#include <set>
#include <string>
#include <stack>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// ImGui
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <opencv4/opencv2/core/mat.hpp>
#include <opencv4/opencv2/imgcodecs.hpp>
#include <opencv4/opencv2/imgproc.hpp>

#include "cross/cross.hpp"

cv::Mat imread_color(const std::string& path) {
    cv::Mat img;
    cv::Scalar fillColor(255, 255, 255); // 白背景

    img = cv::imread(path);
    if(img.channels() == 1) {
        cv::cvtColor(img, img, cv::COLOR_GRAY2BGR);
        return img;
    } else if(img.channels() == 3) {
        return img;
    } else if(img.channels() == 4) {
        // BGRA → BGR
        cv::Mat bgr, alpha;
        cv::cvtColor(img, bgr, cv::COLOR_BGRA2BGR);
        cv::extractChannel(img, alpha, 3);

        // alpha → float(0.0〜1.0)
        cv::Mat alpha_f;
        alpha.convertTo(alpha_f, CV_32FC1, 1.0 / 255.0);

        // alphaを3チャンネルに拡張
        cv::Mat alpha3;
        cv::cvtColor(alpha_f, alpha3, cv::COLOR_GRAY2BGR);

        // 背景色
        cv::Mat bg(img.size(), CV_8UC3, fillColor);

        // float変換
        cv::Mat bgr_f, bg_f;
        bgr.convertTo(bgr_f, CV_32FC3);
        bg.convertTo(bg_f, CV_32FC3);

        // 合成
        cv::Mat dst_f = alpha3.mul(bgr_f) + (cv::Scalar(1.0f, 1.0f, 1.0f) - alpha3).mul(bg_f);

        // 8bitに戻す
        cv::Mat dst;
        dst_f.convertTo(dst, CV_8UC3);
        return dst;
    }else{
        std::cerr << "imread_color : unsupported number of channels: " << img.channels() << std::endl;
        return img;
    }
}

void loadTextureFromFile(const char* filename, GLuint* out_texture, ImVec2* out_size) {
    GLuint g_image_texture = 0;
    cv::Mat mat;
    int g_image_width = 0;
    int g_image_height = 0;

    mat = cv::imread(filename);

    if (mat.empty()) {
        std::cerr << "Failed to load image: " << filename << std::endl;
        return;
    }

    if (g_image_texture != 0) {
        glDeleteTextures(1, &g_image_texture);
    }

    glGenTextures(1, &g_image_texture);
    glBindTexture(GL_TEXTURE_2D, g_image_texture);

    // テクスチャパラメータの設定
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    g_image_width = mat.cols;
    g_image_height = mat.rows;

    // Matのフォーマットを変換
    GLenum format = GL_BGRA;
    int channels = mat.channels();
    if (channels == 1) {
        format = GL_RED;
    } else if (channels == 3) {
        format = GL_BGR;
    } else if (channels == 4) {
        format = GL_BGRA;
    } else {
        std::cerr << "Unsupported number of channels: " << channels << std::endl;
        return;
    }

    // Matのデータをテクスチャにアップロード
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mat.cols, mat.rows, 0, format, GL_UNSIGNED_BYTE, mat.data);
    if(g_image_texture == 0){
        std::cerr << "Failed to create texture from image: " << filename << std::endl;
        return;
    }

    *out_texture = g_image_texture;
    if(out_size != nullptr){
        *out_size = ImVec2((float)g_image_width, (float)g_image_height);
    }
}

#define MAX_CACHED_TEXTURES (16)
static std::vector<std::pair<cv::Mat, GLuint>> textures_cache = {};
ImVec2 drawMat(const cv::Mat& mat, int max_width, int max_height, cv::Scalar* hovered_color, float* mouse_x, float* mouse_y) {
    GLuint g_image_texture = 0;
    int g_image_width = 0;
    int g_image_height = 0;

    ImVec2 img_size;
    float scale = 1.0f;

    if (mat.empty()) {
        return ImVec2(0, 0);
    }

    bool found_in_cache = false;
    for(const auto& [cached_mat, tex] : textures_cache) {
        if (cached_mat.data == mat.data) { // メモリのアドレスのみで比較
            found_in_cache = true;

            g_image_texture = tex;
            g_image_width = cached_mat.cols;
            g_image_height = cached_mat.rows;
            if (max_width > 0 && max_height > 0) {
                scale = (std::min)((float)max_width / g_image_width, (float)max_height / g_image_height);
            }
            img_size = ImVec2((float)g_image_width * scale, (float)g_image_height * scale);
            ImGui::Image((void*)(intptr_t)g_image_texture, img_size);

            break;
        }
    }

    if(!found_in_cache) {
        glDeleteTextures(1, &g_image_texture);
        glGenTextures(1, &g_image_texture);
        glBindTexture(GL_TEXTURE_2D, g_image_texture);

        // テクスチャパラメータの設定
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        int alignment = (mat.cols * mat.channels() * sizeof(uchar));
        if(alignment % 4 == 0) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        } else if(alignment % 2 == 0) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
        } else {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        }

        g_image_width = mat.cols;
        g_image_height = mat.rows;

        // Matのフォーマットを変換
        GLenum format = GL_BGRA;
        int channels = mat.channels();
        if (channels == 1) {
            format = GL_RED;
        } else if (channels == 3) {
            format = GL_BGR;
        } else if (channels == 4) {
            format = GL_BGRA;
        } else {
            std::cerr << "Unsupported number of channels: " << channels << std::endl;
            return ImVec2(0, 0);
        }

        if (max_width > 0 && max_height > 0) {
            scale = (std::min)((float)max_width / g_image_width, (float)max_height / g_image_height);
        }

        // Matのデータをテクスチャにアップロード
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mat.cols, mat.rows, 0, format, GL_UNSIGNED_BYTE, mat.data);

        img_size = ImVec2((float)g_image_width * scale, (float)g_image_height * scale);

        // ImGuiにテクスチャを描画
        ImGui::Image((void*)(intptr_t)g_image_texture, img_size);

        textures_cache.emplace_back(mat, g_image_texture);
    }

    if(hovered_color != nullptr || mouse_x != nullptr || mouse_y != nullptr) {
        if(ImGui::IsItemHovered()) {
            ImVec2 img_pos = ImGui::GetItemRectMin();
            ImVec2 img_size = ImGui::GetItemRectSize();
            ImVec2 mouse_pos = ImGui::GetIO().MousePos;
            if(mouse_pos.x >= img_pos.x && mouse_pos.x <= img_pos.x + img_size.x &&
            mouse_pos.y >= img_pos.y && mouse_pos.y <= img_pos.y + img_size.y) {
                int img_x = (int)((mouse_pos.x - img_pos.x) / scale);
                if(mouse_x != nullptr) {
                    *mouse_x = img_x;
                }
                int img_y = (int)((mouse_pos.y - img_pos.y) / scale);
                if(mouse_y != nullptr) {
                    *mouse_y = img_y;
                }
                if(hovered_color != nullptr) {
                    if(img_x >= 0 && img_x < g_image_width && img_y >= 0 && img_y < g_image_height) {
                        cv::Vec3b color = mat.at<cv::Vec3b>(img_y, img_x);
                        if(mat.channels() == 1) {
                            *hovered_color = cv::Scalar(color[0], color[0], color[0]);
                        } else if(mat.channels() == 3) {
                            *hovered_color = cv::Scalar(color[0], color[1], color[2]);
                        } else if(mat.channels() == 4) {
                            cv::Vec4b color4 = mat.at<cv::Vec4b>(img_y, img_x);
                            *hovered_color = cv::Scalar(color4[0], color4[1], color4[2], color4[3]);
                        } else {
                            std::cerr << "Unsupported number of channels: " << mat.channels() << std::endl;
                            *hovered_color = cv::Scalar(-1, -1, -1);
                        }
                    } else {
                        *hovered_color = cv::Scalar(-1, -1, -1);
                    }
                }
            } else {
                if(hovered_color != nullptr){
                    *hovered_color = cv::Scalar(-1, -1, -1);
                }
            }
        } else {
            if(hovered_color != nullptr){
                *hovered_color = cv::Scalar(-1, -1, -1);
            }
        }
    }

    return img_size;
}
void cleanupTextures() {
    for (const auto& [mat, tex] : textures_cache) {
        glDeleteTextures(1, &tex);
    }
    textures_cache.clear();
}
void cleanupTexturesOverCache() {
    if(textures_cache.size() > MAX_CACHED_TEXTURES) {
        cleanupTextures();
        std::cout << "Texture cache cleared." << std::endl;
    }
}

static std::set<std::string> popup_states;
void openPopup(const std::string& name) {
    popup_states.insert(name);
}
void closePopup(const std::string& name) {
    ImGui::CloseCurrentPopup();
    popup_states.erase(name);
}
void openAllPopups() {
    for (const auto& name : popup_states) {
        ImGui::OpenPopup(name.c_str());
    }
}

std::string current_file_dialog_directory;
std::vector<std::string> current_file_dialog_files;
static int selected_file_index = -1;
static char filename_buffer[64] = "";
void openFileDialog(const std::string& name, const std::string& directory) {
    openPopup(name.c_str());
    current_file_dialog_directory = directory;
    current_file_dialog_files.clear();
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            current_file_dialog_files.push_back(entry.path().filename().string());
        }
    }
    selected_file_index = -1;
    filename_buffer[0] = '\0';
}
bool fileDialog(const std::string& name, std::string& out_path) {
    bool close_requested = false;

    const float dialog_width = 400.0f;

    ImGui::Text("Select a file from the list below:");

    ImGui::BeginChild("FileList", ImVec2(dialog_width, 300), true);
    if(selected_file_index >= 0 && !(current_file_dialog_files[selected_file_index].starts_with(filename_buffer))) {
        ImGui::Selectable(current_file_dialog_files[selected_file_index].c_str(), true);
    }
    for (int i = 0; i < current_file_dialog_files.size(); ++i) {
        const std::string& filename = current_file_dialog_files[i];
        const bool is_selected = (i == selected_file_index);
        if(filename.starts_with(filename_buffer) == false) {
            continue;
        }
        if (ImGui::Selectable(filename.c_str(), is_selected)) {
            out_path = current_file_dialog_directory + "/" + filename;
            selected_file_index = i;
        }
    }
    ImGui::EndChild();

    ImGui::Text("Search: ");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(dialog_width - ImGui::GetCursorPosX() - ImGui::GetStyle().ItemSpacing.x);
    ImGui::InputText("##filename", filename_buffer, sizeof(filename_buffer));

    ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight()));

    if (ImGui::Button("Cancel") || current_file_dialog_files.empty()) {
        out_path.clear();
        close_requested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Select") && selected_file_index >= 0) {
        out_path = current_file_dialog_directory + "/" + current_file_dialog_files[selected_file_index];
        close_requested = true;
    }

    if(close_requested) {
        closePopup(name);
        return true;
    }

    return false;
}

bool miniButton(const char* label, GLuint texture_id, int size, bool selectable) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
    if(selectable) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f,0.7f,0.7f,0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f,1.0f,1.0f,1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f,1.0f,1.0f,1.0f));
    }else{
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f,0.3f,0.3f,0.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f,0.3f,0.3f,0.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f,0.3f,0.3f,0.3f));
    }
    bool clicked = ImGui::ImageButton(
        label,
        (ImTextureID)(intptr_t)texture_id,
        ImVec2(size, size),
        ImVec2(0, 0), ImVec2(1, 1),
        ImGui::GetStyleColorVec4(ImGuiCol_Button),
        ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
    );
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    clicked = clicked && selectable;
    return clicked;
}

void drawColorMarker(ImU32 color) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    p.y += 4.0f;
    ImVec2 size = ImVec2(20, 20);

    draw_list->AddRectFilled(
        p,
        ImVec2(p.x + size.x, p.y + size.y),
        color,
        0.0f
    );
    draw_list->AddRect(
        p,
        ImVec2(p.x + size.x, p.y + size.y),
        IM_COL32(0xFF, 0x66, 0, 255),
        0.0f,
        0,
        2.0f
    );

    ImGui::Dummy(size);
}

static std::stack<ImVec2> cursor_pos_stack;
static const float border_space = 10.0f;
static const float radius = 5.0f;
static const float border_thickness = 2.0f;
static const float border_thickness_active = 5.0f;
static const float item_spacing = 5.0f;
static const float item_spacing_head = 5.0f;
static const float separetor_padding = 3.0f;
static const float rect_width = 400.0f;
static const ImU32 active_border_color = IM_COL32(0, 100, 200, 255);
void BeginRectWithBorder() {
    cursor_pos_stack.push(ImGui::GetCursorScreenPos());
    ImGui::Dummy(ImVec2(0, item_spacing_head));
    ImGui::Indent(border_space);
}
bool EndRectWithBorder(bool active) {
    ImGui::Unindent(border_space);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 start = cursor_pos_stack.top();
    cursor_pos_stack.pop();
    ImVec2 end;
    end.x = start.x + rect_width + border_space * 2;
    end.y = ImGui::GetCursorScreenPos().y + border_space;

    draw_list->AddRect(
        start,
        end,
        active ? active_border_color : ImGui::GetColorU32(ImGuiCol_Border),
        radius,
        0,
        active ? border_thickness_active : border_thickness
    );

    ImGui::SetCursorScreenPos(start);
    ImVec2 rect_size(end.x - start.x, end.y - start.y);
    bool result = ImGui::InvisibleButton("filter_invisible_button", rect_size);

    ImGui::Dummy(ImVec2(0, item_spacing + border_space));

    return result;
}
