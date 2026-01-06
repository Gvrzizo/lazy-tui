#include"../clicall/lazycli.hpp"
#include<ftxui/dom/elements.hpp>
#include<ftxui/component/component.hpp>
#include<ftxui/component/screen_interactive.hpp>
#include<string>
#include<vector>
#include<sstream>
#include<regex>
#include<iostream>
#include<fstream>
#include<filesystem>
#include<thread>
#include<chrono>

namespace fs = std::filesystem;

using namespace ftxui;

namespace { // 使用匿名命名空间保护内部实现

class AlwaysFocusable : public ftxui::ComponentBase {
public:
    AlwaysFocusable(ftxui::Component inner) {
        Add(inner);
    }

    bool Focusable() const override { return true; }
};

struct Assignment {
    std::string title;
    std::string id;
    std::string course_name;
    std::string course_id;
    std::string deadline;
    std::string remaining_time;
    std::string link;
    std::string fileid;
};

std::string getfileid(std::string id, std::string cont) {
    std::stringstream ss(cont);
    std::string line;
    std::regex file_regex(R"(文件ID:\s*(\d+))");
    while (std::getline(ss, line)) {
        std::smatch match;
        if (std::regex_search(line, match, file_regex)) {
            return match[1];
        }
    }
    return "";
}

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
    std::regex deadline_regex(R"(截止时间:\s*(.*?)\s*\((.*)\))");

    while (std::getline(ss, line)) {
        if (line.find("[作业]") != std::string::npos && !in_card) {
            current = Assignment();
            in_card = true;
            continue;
        }
        if (line.find("─") != std::string::npos || (line.find("+") != std::string::npos && line.find("-") != std::string::npos)) {
            if (!current.id.empty()) assignments.push_back(current);
            in_card = false;
            continue;
        }

        std::string block = "│";
#ifdef _WIN32
        block = "|";
#endif
        size_t first_pipe = line.find(block);
        size_t last_pipe = line.rfind(block);
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

std::string getConfigPath() {
    const char* home = std::getenv("HOME");
    if (home && *home) return std::string(home) + "/.config/.lazy_tui_config";
    return ".lazy_tui_config";
}

void savePath(const std::string& path) {
    std::ofstream ofs(getConfigPath());
    if (ofs.is_open()) {
        ofs << path;
        ofs.close();
    }
}

std::string loadPath() {
    std::ifstream ifs(getConfigPath());
    std::string path;
    if (ifs.is_open() && std::getline(ifs, path)) {
        if (!path.empty() && fs::exists(path)) {
            return path;
        }
    }

    const char* home = std::getenv("HOME");
    if (home && *home && fs::exists(home)) {
        return std::string(home);
    }

    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile && *userprofile && fs::exists(userprofile)) {
        return std::string(userprofile);
    }
    return ".";
}

static std::string resList, id, selected;
static fs::path p;

} // namespace

Component assignment(ScreenInteractive &screen, int &cur) {
    static std::string cont;
    static std::vector<Assignment> parsedAssignments;
    static bool loading = 1;

    static bool modalShow = 0;
    static bool fsShow = 0;
    static std::string viewContent = "";

    static std::vector<std::string> placeholders;

    static std::once_flag flag;
    std::call_once(flag, [&]() {
        std::thread([&]() {
            // 在后台线程执行耗时操作
            std::string cont = lazy::run("lazy assignment todo -A");
            auto data = parseAssignmentTodo(cont);

            // 切换回主线程前更新数据
            parsedAssignments = std::move(data);
            placeholders.assign(parsedAssignments.size(), "");
            loading = false;

            screen.PostEvent(Event::Custom);
        }).detach();
    });

    static fs::path currentPath = loadPath();
    static std::vector<std::string> fileList;
    static int fileSel = 0;
    auto refreshFiles = [&]() {
        fileList.clear();
        fileList.push_back(".. [返回上级]");
        std::vector<fs::directory_entry> dirs, files;
        for (const auto& entry : fs::directory_iterator(currentPath)) {
            if (entry.is_directory()) dirs.push_back(entry);
            else files.push_back(entry);
        }
        auto by_name = [](const fs::directory_entry& a, const fs::directory_entry& b) {
            return a.path().filename().string() < b.path().filename().string();
        };
        std::sort(dirs.begin(), dirs.end(), by_name); std::sort(files.begin(), files.end(), by_name);
        for (const auto& d : dirs) fileList.push_back(d.path().filename().string());
        for (const auto& f : files) fileList.push_back(f.path().filename().string());
    };

    refreshFiles();

    // 菜单配置
    MenuOption option, fsopt;
    option.entries_option.transform = [&](const EntryState& state) {
        if (loading) return text("Loading...");

        const auto& a = parsedAssignments[state.index];

        auto row = hbox({
            vbox({
                hbox({
                    text(a.title) | bold | (state.focused ? color(Color::Black) : color(Color::White)),
                    state.focused ? (text(" #" + a.id) | color(Color::GrayDark)) : (text(" #" + a.id) | color(Color::GrayDark) | dim),
                }),
                hbox({
                    text(a.course_name) | (state.focused ? color(Color::Purple3) : color(Color::Cyan)) | size(WIDTH, EQUAL, 25),
                    separator(),
                    text(" ⏳ " + a.remaining_time) | (state.focused ? color(Color::Purple3) : color(Color::Aquamarine1)),
                }),
            }) | flex,
        });

        if (state.focused) {
            row = row | bgcolor(Color::BlueLight) | color(Color::Black);
        }
        return row;
    };
    fsopt.entries_option.transform = [&](const EntryState& state) {
        const std::string selected = fileList[state.index];
        fs::path p = currentPath / selected;

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
    static auto menuOri = Menu(&placeholders, &sel, option);
    static auto menu = Make<AlwaysFocusable>(menuOri);

    static auto fileMenu = Menu(&fileList, &fileSel, fsopt);
    static auto submitWindow = Renderer(fileMenu, [&] {
        return vbox({
            text(" 选择提交文件 ") | hcenter | bold,
            text(currentPath.string()) | dim | hcenter,
            separator(),
            fileMenu->Render() | vscroll_indicator | frame | flex, //size(HEIGHT, EQUAL, 12),
            separator(),
            paragraph(" [Enter]: 选择/进入 | [l]: 进入 | [h]: 上一级目录 | [q], [s]: 退出 ") | hcenter | dim,
        }) | border | bgcolor(Color::Black) | flex; //size(WIDTH, EQUAL, 60);
    });
    static bool subNoti = 0;
    static std::string inRes = "提交中......";

    static auto showRes = Renderer([&] {
        return vbox({
            text(inRes) | center | flex
        }) | border;
    });

    // 渲染最终界面
    static auto renderer = Renderer(menu, [&] {
        if (loading) {
            return vbox({
                text(" [v]: 查看作业详情 | [s]: 提交作业 | [q], [h]: 回到主菜单 ") | hcenter | dim,
                separator(),
                text("正在获取数据，请稍候...") | center | flex,
            }) | border | flex;
        }

        return vbox({
            hbox({
                text(" 待办作业 ") | bold | bgcolor(Color::White) | color(Color::Black),
                text(" 共 " + std::to_string(parsedAssignments.size()) + " 项") | color(Color::GrayLight),
            }),
            separator(),
            paragraph(" [v]: 查看作业详情 | [s]: 提交作业 | [q], [h]: 回到主菜单 | [j], [k], [↑], [↓] 选择条目 | [r] 刷新 ") | hcenter | dim,
            separator(),
            menu->Render() | frame | vscroll_indicator | flex,
            separator(),
            // 底部预览选中的作业详情
            (parsedAssignments.empty() ? text("暂无作业") :
                vbox({
                    text("截止日期: " + parsedAssignments[sel].deadline) | color(Color::Red),
                    text("跳转链接: " + parsedAssignments[sel].link) | color(Color::BlueLight),
                })),
        }) | border;
    });

    static auto resRend = Renderer([&] {
        return vbox({
            text(" 作业详情 ") | hcenter | bold,
            separator(),
            paragraph(viewContent) | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 20),
            separator(),
            text(" [d] 下载附件 | [q], [h], [v] 或是按钮关闭 ") | hcenter | dim,
        }) | borderDouble | bgcolor(Color::Black) | flex;// size(WIDTH, LESS_THAN, 80);
    });

    static auto resComp = Container::Vertical({
        Button("关闭窗口", [&] {modalShow = 0;}, ButtonOption::Animated()),
        resRend,
    });

    renderer |= Modal(resComp, &modalShow);
    renderer |= Modal(submitWindow, &fsShow);
    renderer |= Modal(showRes, &subNoti);

    return CatchEvent(renderer, [&](Event e) {
        if (modalShow) {
            if (e == Event::Character('h') || e == Event::Character('q') || e == Event::Character('v')) {
                modalShow = 0;
                return true;
            }
            if (e == Event::Character('d')) {
                if (!parsedAssignments[sel].fileid.empty()) {
                    std::thread([&] {
                        inRes = "附件下载中......";
                        subNoti = 1;
                        screen.RequestAnimationFrame();
                        lazy::run("lazy resource download " + parsedAssignments[sel].fileid);
                        inRes = "附件下载完毕！";
                        screen.RequestAnimationFrame();
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        subNoti = 0;
                        screen.RequestAnimationFrame();
                    }).detach();
                }
                return true;
            }
            return false;
        }
        if (fsShow) {
            if (e == Event::Character('s') || e == Event::Character('q')) {
                savePath(currentPath.string());
                fsShow = 0;
                return true;
            }
            if (e == Event::Return || e == Event::Character('l')) {
                if (!fileSel) {
                    currentPath = currentPath.parent_path();
                    refreshFiles();
                    return true;
                }
                else {
                    selected = fileList[fileSel];
                    p = currentPath / selected;
                    if (fs::is_directory(p)) {
                        currentPath = p;
                        refreshFiles();
                    }
                    else if (e == Event::Return) {
                        std::thread([&] {
                            inRes = "提交中......";
                            subNoti = 1;
                            screen.RequestAnimationFrame();
                            savePath(currentPath.string());
                            lazy::run("lazy resource upload '" + p.string() + "'");
                            resList = lazy::run("lazy resource list");
                            id = findResourceIdByName(resList, selected);
                            lazy::run("lazy assignment submit " + parsedAssignments[sel].id + " -f '" + id + "'");
                            std::thread([&] {
                                std::string cont = lazy::run("lazy assignment todo -A");
                                auto data = parseAssignmentTodo(cont);
                                parsedAssignments = std::move(data);
                                placeholders.assign(parsedAssignments.size(), "");
                                inRes = "提交完毕！";
                                screen.RequestAnimationFrame();
                                std::this_thread::sleep_for(std::chrono::seconds(1));
                                fsShow = 0;
                                subNoti = 0;
                                screen.RequestAnimationFrame();
                            }).detach();
                        }).detach();
                    }
                    return true;
                }
            }
            if (e == Event::Character('h')) {
                currentPath = currentPath.parent_path();
                refreshFiles();
                return true;
            }
            return false;
        }
        if (e == Event::Character('h') || e == Event::Character('q')) {
            cur = 0;
            return true;
        }
        if (e == Event::Character('v') && !loading) {
            if (!modalShow) {
                viewContent = "Loading...";
                modalShow = 1;
                std::string target_id = parsedAssignments[sel].id;
                std::thread([&screen, target_id]() {
                    std::string result = lazy::run("lazy assignment view " + target_id);
                    parsedAssignments[sel].fileid = getfileid(target_id, result);
                    viewContent = std::move(result);
                    screen.PostEvent(Event::Custom);
                }).detach();
            }
            return true;
        }
        if (e == Event::Character("s") && !loading) {
            if (!fsShow) {
                refreshFiles();
                fsShow = 1;
            }
            return true;
        }
        if (e == Event::Character("r") && !loading && !modalShow && !fsShow) {
            std::thread([&] {
                inRes = "刷新中......";
                subNoti = 1;
                screen.RequestAnimationFrame();
                std::string cont = lazy::run("lazy assignment todo -A");
                auto data = parseAssignmentTodo(cont);
                parsedAssignments = std::move(data);
                placeholders.assign(parsedAssignments.size(), "");
                subNoti = 0;
                screen.RequestAnimationFrame();
            }).detach();
            return true;
        }
        return false;
    });
}
