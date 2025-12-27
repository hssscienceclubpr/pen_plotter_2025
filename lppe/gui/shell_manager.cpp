#include "shell_manager.hpp"

static bool is_integer(const std::string& s) {
    if (s.empty()) return false;
    const char* begin = s.data();
    const char* end   = s.data() + s.size();
    int value;
    auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc() && result.ptr == end;
}

bool ShellManager::drawGui() {
    static char buffer[1024 * 16];
    bool runned = false;

    ImGui::Text("Please use shell if you want to something advanced.");
    if(ImGui::Button("Run")){
        std::cout << "Running shell command: " << buffer << std::endl;
        runAllCommands();
        runned = true;
    }
    if(ImGui::InputTextMultiline("##shell_input", buffer, sizeof(buffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 32), ImGuiInputTextFlags_AllowTabInput)){
        // bufferを改行で分割してcommandsに追加
        commands.clear();
        std::string buf_str(buffer);
        size_t pos = 0;
        while((pos = buf_str.find('\n')) != std::string::npos) {
            std::string line = buf_str.substr(0, pos);
            if(!line.empty()) {
                commands.push_back(line);
            }
            buf_str.erase(0, pos + 1);
        }
        if(!buf_str.empty()) {
            commands.push_back(buf_str);
        }
    }

    return runned;
}

void ShellManager::runAllCommands() {
    for(const std::string& cmd : commands) {
        // スペースで区切る
        std::vector<std::string> args;
        size_t pos = 0;
        std::string cmd_copy = cmd;
        while((pos = cmd_copy.find(' ')) != std::string::npos) {
            std::string arg = cmd_copy.substr(0, pos);
            if(!arg.empty()) {
                args.push_back(arg);
            }
            cmd_copy.erase(0, pos + 1);
        }
        if(!cmd_copy.empty()) {
            args.push_back(cmd_copy);
        }

        if(args.empty()) {
            continue;
        }
        if(args[0] == "hatch"){
            if(args.size() <= 2){
                continue;
            }
            hatchLineSettings[args[1]] = HatchLineSetting();
            auto key = args[1];
            for(size_t i=2; i<args.size(); ++i){
                std::cout << "Arg: " << args[i] << std::endl;
                std::string value = args[i];
                if(value == "/" || value == "-" || value == "\\" || value == "|" || value == "+" || value == "x"){
                    hatchLineSettings[key].mode = value;
                }else if(is_integer(value)){
                    int spacing = std::stoi(value);
                    if(spacing >= 1 && spacing <= 1000){
                        hatchLineSettings[key].spacing = spacing;
                    }
                }else if(value.size() > 0){
                    hatchLineSettings[key].substitute_color = value;
                }
            }
        }
    }
}
