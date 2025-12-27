#pragma once

#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"

#include <opencv4/opencv2/core/mat.hpp>

cv::Mat imread_color(const std::string& path);
void loadTextureFromFile(const char* filename, GLuint* out_texture, ImVec2* out_size);
ImVec2 drawMat(const cv::Mat& mat, int max_width = -1, int max_height = -1,
    cv::Scalar* hovered_color = nullptr, float* mouse_x = nullptr, float* mouse_y = nullptr);
void cleanupTextures();
void cleanupTexturesOverCache();
void openPopup(const std::string& name);
void closePopup(const std::string& name);
void openAllPopups();
void openFileDialog(const std::string& name, const std::string& directory);
bool fileDialog(const std::string& name, std::string& out_path);
bool miniButton(const char* label, GLuint texture_id, int size, bool selectable = true);
void drawColorMarker(ImU32 color);
void BeginRectWithBorder();
bool EndRectWithBorder(bool active);
