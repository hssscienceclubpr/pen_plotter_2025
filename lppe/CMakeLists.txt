# LEGO Pen Printer(Plotter) Editor
cmake_minimum_required(VERSION 3.11)
project(lppe C CXX)

cmake_policy(SET CMP0072 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- システムライブラリ ---
find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui ximgproc)

# --- glad (手動配置) ---
# extern/glad/src/glad.c が存在する前提
# https://glad.dav1d.de/
# 上のやつで、C/C++ OpenGL Version4.6 Core "Generate a loader" でgenerateする
set(GLAD_SRC "/home/lppe/extern/glad/src/glad.c")
set(GLAD_INCLUDE "/home/lppe/extern/glad/include")

if(NOT EXISTS "${GLAD_SRC}")
    message(FATAL_ERROR "glad.c が存在しません: ${GLAD_SRC}")
endif()

add_library(glad_lib STATIC "${GLAD_SRC}")
target_include_directories(glad_lib PUBLIC "${GLAD_INCLUDE}")

# --- ImGui (手動配置) ---
# extern/imgui 以下に git clone して配置済み前提
set(IMGUI_DIR "/home/lppe/extern/imgui")

add_library(imgui_lib
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(imgui_lib PUBLIC
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)
# imgui_lib の依存は PUBLIC にして伝播
target_link_libraries(imgui_lib PUBLIC
    glfw
    OpenGL::GL
    glad_lib
)

# --- クロスプラットフォーム用プログラム ---
add_library(cross_module "${CMAKE_CURRENT_SOURCE_DIR}/cross/cross.cpp")
target_include_directories(cross_module
    PUBLIC
)

# --- 画像処理モジュール ---
file(GLOB IMG_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/img/*.cpp")
add_library(img_module ${IMG_SOURCES})
target_include_directories(img_module
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(img_module PRIVATE ${OpenCV_LIBS})

# --- パスの最適化モジュール ---
file(GLOB OPIMIZE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/optimizer/*.cpp")
add_library(optimizer_module ${OPIMIZE_SOURCES})
target_include_directories(optimizer_module
    PUBLIC
)

# --- GUI 実行ファイル ---
file(GLOB GUI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gui/*.cpp")
add_executable(gui_app ${GUI_SOURCES})
target_include_directories(gui_app PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
# gui_app は imgui_lib と img_module をリンクするだけで OK
target_link_libraries(gui_app
    PRIVATE
    imgui_lib
    img_module
    cross_module
    optimizer_module
    ${OpenCV_INCLUDE_DIRS}
)
