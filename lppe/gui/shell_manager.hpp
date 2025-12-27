#pragma once

#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <vector>

#include "gui.hpp"
#include "gui_helpers.hpp"

#include "img/vector_data.hpp"

class ShellManager {
    public:
        ShellManager() = default;
        ShellManager(const ShellManager&) = default;
        ShellManager& operator=(const ShellManager&) = default;
        ShellManager(ShellManager&&) = default;
        ShellManager& operator=(ShellManager&&) = default;
        ~ShellManager() = default;

        bool drawGui();

        std::map<std::string, HatchLineSetting> hatchLineSettings;
    private:
        void runAllCommands();
        std::vector<std::string> commands;
};
