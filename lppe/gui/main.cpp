#include <iostream>
#include <thread>
#include <chrono>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// ImGui
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "cross/cross.hpp"
#include "gui.hpp"

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

int main() {
    // GLFW 初期化
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "LEGO Pen Printer(Plotter) Editor", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // V-Sync

    // glad 初期化
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize glad\n";
        return -1;
    }

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // --- ImGui 初期化 ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Docking 有効、Viewports 無効
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    std::string font_path = getExecutableDir() + "/assets/fonts/";

    ImFont* font_en = io.Fonts->AddFontFromFileTTF(
        (font_path + "Roboto-Regular.ttf").c_str(), 25.0f,
        nullptr, io.Fonts->GetGlyphRangesDefault()
    );
    ImFont* font_jp = io.Fonts->AddFontFromFileTTF(
        (font_path + "NotoSansJP-Regular.ttf").c_str(), 25.0f,
        nullptr, io.Fonts->GetGlyphRangesJapanese()
    );
    io.FontDefault = font_jp;

    ImGui::StyleColorsDark();

    // バックエンド初期化
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    setup();

    auto start_of_frame = std::chrono::high_resolution_clock::now();
    float fps = -1.0f;

    // --- メインループ ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (!window) {
            std::cerr << "GLFW window is NULL!" << std::endl;
            return -1;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        processInput(window);

        // Docking スペース（画面全体に配置）
        ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        ImGui::DockSpaceOverViewport(
            0,                // dockspace_id
            main_viewport,    // viewport
            ImGuiDockNodeFlags_None,
            nullptr
        );

        // 無題ウィンドウを画面いっぱいに表示
        ImGui::SetNextWindowPos(ImVec2(0,0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("untitled window", nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_MenuBar
        );

        drawGui(fps);

        ImGui::End(); // untitled window

        // 描画
        ImGui::Render();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);

        cleanUp();

        // フレームレート制御
        auto end_of_frame = std::chrono::high_resolution_clock::now();
        auto frame_duration = end_of_frame - start_of_frame;
        start_of_frame = end_of_frame;

        std::chrono::nanoseconds target_frame_time;
        getTargetFrameTime(target_frame_time);

        // 6. 目標フレーム時間との差分を計算
        auto sleep_duration = target_frame_time - frame_duration;

        // 7. 差分時間がプラスであればスリープ
        if (sleep_duration > std::chrono::nanoseconds(0)) {
            std::this_thread::sleep_for(sleep_duration);
        }else{
            sleep_duration = std::chrono::nanoseconds(0);
        }

        fps = 1.0f / std::chrono::duration<float>(frame_duration + sleep_duration).count();
    }

    end();

    // --- 終了処理 ---
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "Application terminated gracefully." << std::endl;

    return 0;
}
