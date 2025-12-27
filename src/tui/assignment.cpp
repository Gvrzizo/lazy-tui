#include"../clicall/lazycli.hpp"
#include<ftxui/dom/elements.hpp>
#include<ftxui/component/component.hpp>
#include<ftxui/component/screen_interactive.hpp>
#include<string>
#include<vector>
#include<sstream>
#include<regex>
#include<iostream>
#include <filesystem>
namespace fs = std::filesystem;

using namespace ftxui;

namespace { // 使用匿名命名空间保护内部实现

struct Assignment {
    std::string title;
    std::string id;
    std::string course_name;
    std::string course_id;
    std::string deadline;
    std::string remaining_time;
    std::string link;
};

// 辅助函数：解析作业卡片
std::vector<Assignment> parseAssignmentTodo(const std::string& raw_output) {
    std::vector<Assignment> assignments;
    std::stringstream ss(raw_output);
    std::string line;
    Assignment current;
    bool in_card = false;

    // 正则表达式用于精确提取
    std::regex title_id_regex(R"(^\s*(.*?)\s*\[ID:\s*(\d+)\])");
    std::regex course_regex(R"(^\s*(.*?)\s+(\d+)\s*$)");
    // std::regex deadline_regex(R"(^\s*截止时间:\s*(.*?)\s*\((.*?)\))");
    std::regex deadline_regex(R"(截止时间:\s*(.*))");

    while (std::getline(ss, line)) {
        // 过滤掉边框字符和空白行
        if (line.find("╭") != std::string::npos) {
            current = Assignment();
            in_card = true;
            continue;
        }
        if (line.find("╰") != std::string::npos) {
            if (!current.id.empty()) assignments.push_back(current);
            in_card = false;
            continue;
        }

        // 去除两侧的 │ 和空格
        size_t first_pipe = line.find("│");
        size_t last_pipe = line.rfind("│");
        if (first_pipe == std::string::npos || last_pipe == std::string::npos) continue;

        std::string content = line.substr(first_pipe + 1, last_pipe - first_pipe - 1);
        content = std::regex_replace(content, std::regex(R"(^\s+|\s+$)"), ""); // trim
        if (content.empty()) continue;

        std::smatch match;
        // 匹配标题和 ID
        if (std::regex_search(content, match, title_id_regex)) {
            current.title = match[1];
            current.id = match[2];
        }
        // 匹配截止时间
        else if (std::regex_search(content, match, deadline_regex)) {
            current.deadline = match[1];
            current.remaining_time = match[2];
        }
        // 匹配链接 (简单判断 http)
        else if (content.find("https://") != std::string::npos) {
            current.link = content;
        }
        // 匹配课程名和课程 ID (通常在标题行之后)
        else if (std::regex_search(content, match, course_regex)) {
            current.course_name = match[1];
            current.course_id = match[2];
        }
    }
    return assignments;
}

std::string findResourceIdByName(const std::string& raw_output, const std::string& target_filename) {
    std::stringstream ss(raw_output);
    std::string line;

    // 正则表达式说明：
    // ^│\s*(\d+)\s* : 匹配行首的 │，捕获随后的数字 ID
    // │\s*(.*?)\s*│ : 匹配第二个单元格（文件名），使用非贪婪匹配捕获内容
    // 后面的内容我们暂时不需要，用来闭合结构
    std::regex row_regex(R"(│\s*(\d+)\s*│\s*(.*?)\s*│)");

    while (std::getline(ss, line)) {
        std::smatch match;
        if (std::regex_search(line, match, row_regex)) {
            std::string id = match[1];
            std::string name = match[2];

            // 去除文件名末尾可能存在的由表格对齐产生的多余空格
            name.erase(name.find_last_not_of(" ") + 1);

            // 匹配校验：支持全名匹配
            if (name == target_filename) {
                return id;
            }
        }
    }
    return ""; // 未找到
}

} // namespace

Component assignment(ScreenInteractive &screen, int &cur) {
    // 1. 获取并解析数据
    static std::string cont = lazy::run("lazy assignment todo");
    static auto parsedAssignments = parseAssignmentTodo(cont);

    static bool modalShow = 0;
    static bool fsShow = 0;
    static std::string view_content = "";

    // static fs::path current_path = fs::current_path();
    static fs::path current_path = fs::path(std::getenv("HOME"));
    static std::vector<std::string> file_list;
    static int file_sel = 0;
    auto refresh_files = [&]() {
        file_list.clear();
        file_list.push_back(".. [返回上级]");
        std::vector<fs::directory_entry> dirs, files;
        for (const auto& entry : fs::directory_iterator(current_path)) {
            if (entry.is_directory()) dirs.push_back(entry);
            else files.push_back(entry);
        }
        auto by_name = [](const fs::directory_entry& a, const fs::directory_entry& b) {
            return a.path().filename().string() < b.path().filename().string();
        };
        std::sort(dirs.begin(), dirs.end(), by_name); std::sort(files.begin(), files.end(), by_name);
        for (const auto& d : dirs) file_list.push_back(d.path().filename().string());
        for (const auto& f : files) file_list.push_back(f.path().filename().string());
    };

    refresh_files();

    // 2. 菜单配置
    MenuOption option, fsopt;
    option.entries_option.transform = [&](const EntryState& state) {
        const auto& a = parsedAssignments[state.index];

        auto row = hbox({
            vbox({
                hbox({
                    text(a.title) | bold | (state.focused ? color(Color::Black) : color(Color::White)),
                    text(" #" + a.id) | color(Color::GrayDark) | dim,
                }),
                hbox({
                    text(a.course_name) | (state.focused ? color(Color::Purple3) : color(Color::Cyan)) | size(WIDTH, EQUAL, 25),
                    separator(),
                    text(" ⏳ " + a.remaining_time) | color(Color::Yellow),
                }),
            }) | flex,
        });

        if (state.focused) {
            row = row | bgcolor(Color::BlueLight) | color(Color::Black);
        }
        return row;
    };
    fsopt.entries_option.transform = [&](const EntryState& state) {
        const std::string selected = file_list[state.index];
        fs::path p = current_path / selected;

        auto row = hbox({
            text((!state.index || fs::is_directory(p) ? "   " : "   ") + selected) | bold | (fs::is_directory(p)
            ? (state.focused ? color(Color::Purple3) : color(Color::Cyan))
            : (state.focused ? color(Color::Black) : color(Color::White))),
        });

        if (state.focused) {
            row = row | bgcolor(Color::BlueLight) | color(Color::Black);
        }

        return row;
    };

    static int sel = 0;
    static std::vector<std::string> placeholders(parsedAssignments.size(), "");
    static auto menu = Menu(&placeholders, &sel, option);

    static auto file_menu = Menu(&file_list, &file_sel, fsopt);
    auto submit_window = Renderer(file_menu, [&] {
        return vbox({
            text(" 选择提交文件 ") | hcenter | bold,
            text(current_path.string()) | dim | hcenter,
            separator(),
            file_menu->Render() | vscroll_indicator | frame | size(HEIGHT, EQUAL, 12),
            separator(),
            text(" Enter: 选择/进入 | l: 进入 | h: 上一级目录 | q, s: 关闭窗口 ") | hcenter | dim,
        }) | border | bgcolor(Color::Black) | size(WIDTH, EQUAL, 60);
    });

    // 3. 渲染最终界面
    static auto renderer = Renderer(menu, [&] {
        return vbox({
            hbox({
                text(" 待办作业 ") | bold | bgcolor(Color::White) | color(Color::Black),
                text(" 共 " + std::to_string(parsedAssignments.size()) + " 项") | color(Color::GrayLight),
            }),
            separator(),
            menu->Render() | frame | vscroll_indicator | flex,
            separator(),
            // 底部预览选中的作业详情
            (parsedAssignments.empty() ? text("暂无作业") :
                vbox({
                    text("截止日期: " + parsedAssignments[sel].deadline) | color(Color::Red),
                    text("跳转链接: " + parsedAssignments[sel].link) | color(Color::BlueLight),
                })),
            separator(),
            text(" v: 查看作业详情 | s: 提交作业 | q, h: 回到主菜单 ") | hcenter | dim,
        }) | border;
    });

    static auto resRend = Renderer([&] {
        return vbox({
            text(" 作业详情 ") | hcenter | bold,
            separator(),
            paragraph(view_content) | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 20),
            separator(),
            text(" 按 [q] [h] [v] 或是按钮关闭 ") | hcenter | dim,
        }) | borderDouble | bgcolor(Color::Black) | size(WIDTH, LESS_THAN, 80);
    });

    static auto resComp = Container::Vertical({
        Button("关闭窗口", [&] {modalShow = 0;}, ButtonOption::Animated()),
        resRend,
    });

    renderer |= Modal(resComp, &modalShow);
    renderer |= Modal(submit_window, &fsShow);

    return CatchEvent(renderer, [&](Event e) {
        if (modalShow) {
            if (e == Event::Character('h') || e == Event::Character('q') || e == Event::Character('v')) {
                modalShow = 0;
                return true;
            }
            return false;
        }
        if (fsShow) {
            if (e == Event::Character('s') || e == Event::Character('q')) {
                fsShow = 0;
                return true;
            }
            if (e == Event::Return || e == Event::Character('l')) {
                if (!file_sel) {
                    current_path = current_path.parent_path();
                    refresh_files();
                    return true;
                }
                else {
                    std::string selected = file_list[file_sel];
                    fs::path p = current_path / selected;
                    if (fs::is_directory(p)) {
                        current_path = p;
                        refresh_files();
                    }
                    else if (e == Event::Return) {
                        lazy::run("lazy resource upload " + p.string());
                        std::string resList = lazy::run("lazy resource list");
                        std::string id = findResourceIdByName(resList, selected);
                        lazy::run("lazy assignment submit " + parsedAssignments[sel].id + " -f '" + id + "'");
                        fsShow = 0;
                    }
                    return true;
                }
                if (e == Event::Character('h')) {
                    current_path = current_path.parent_path();
                    refresh_files();
                    return true;
                }
                return false;
            }
        }
        if (e == Event::Character('h') || e == Event::Character('q')) {
            cur = 0;
            return true;
        }
        if (e == Event::Character('v')) {
            if (!modalShow) {
                view_content = lazy::run("lazy assignment view " + parsedAssignments[sel].id);
                modalShow = 1;
            }
            return true;
        }
        if (e == Event::Character("s")) {
            if (!fsShow) {
                refresh_files();
                fsShow = 1;
            }
            return true;
        }
        return false;
    });
}

// Component viewMod() {
//     return 0;
// }
//
// Component assignment(ScreenInteractive &screen, int &cur) {
//     static int assid = 0;
//     static bool modalShow = 0;
//     static auto resComp = assignmentMenu(screen, cur);
//     static auto modComp = viewMod();
//     resComp |= Modal(modComp, &modalShow);
//     return resComp;
// }
