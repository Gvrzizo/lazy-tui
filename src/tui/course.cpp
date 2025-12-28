#include"../clicall/lazycli.hpp"
#include<ftxui/dom/elements.hpp>
#include<ftxui/component/component.hpp>
#include<ftxui/component/screen_interactive.hpp>
#include<string>
#include<iostream>
#include<vector>
#include<thread>

using namespace ftxui;

namespace {

struct Course {
    std::string id;
    std::string name;
    std::string teacher;
    std::string time;
    std::string department;
    std::string year;
};

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

std::vector<Course> parseCourseTable(const std::string& raw_output) {
    std::vector<Course> courses;
    std::stringstream ss(raw_output);
    std::string line;

    while (std::getline(ss, line)) {
        // 1. 只处理包含数据分隔符 '│' 的行，忽略边框 '━', '┳' 等
        // 注意这个符号不是ASCII中的 '|'
        if (line.find("│") == std::string::npos) continue;

        // 2. 按 '│' 分割行
        std::vector<std::string> cols;
        std::stringstream lineStream(line);
        std::string cell;
        const std::string delimiter = "│";
        size_t start = 0; size_t pos;
        while ((pos = line.find(delimiter, start)) != std::string::npos) {
            std::string cell = line.substr(start, pos - start);
            cols.push_back(trim(cell));
            start = pos + delimiter.size();
        }

        // 由于首尾都有 │，分割后 cols[0] 通常为空，cols[1] 才是 ID
        if (cols.size() < 7) continue;

        std::string id = cols[1];

        // 3. 判断是新课程还是上一行的续行
        if (!id.empty() && std::isdigit(id[0])) {
            // 是新课程 ID
            Course c;
            c.id = id;
            c.name = cols[2];
            c.teacher = cols[3];
            c.time = cols[4];
            c.department = cols[5];
            c.year = cols[6];
            courses.push_back(c);
        } else if (!courses.empty()) {
            // ID 为空，说明是多行内容的后续部分，追加到上一个课程
            Course& last = courses.back();
            if (!cols[2].empty()) last.name += cols[2];
            if (!cols[3].empty()) last.teacher += cols[3];
            if (!cols[4].empty()) last.time += cols[4];
            if (!cols[5].empty()) last.department += cols[5];
            // year 通常不会跨行，根据需要决定是否追加
        }
    }
    return courses;
}

std::string formatTeacher(const std::string& full_list, int max_count = 3) {
    std::vector<std::string> names;
    std::stringstream ss(full_list);
    std::string name;

    // 按逗号分割出名字
    while (std::getline(ss, name, ',')) {
        std::string trimmed = trim(name);
        if (!trimmed.empty()) {
            names.push_back(trimmed);
        }
    }

    if (names.size() <= max_count) {
        return full_list;
    }

    std::string result = names[0];
    for (int i = 1; i < max_count; ++i) {
        result += ", " + names[i];
    }
    return result + " 等";
}

}

Component course(ftxui::ScreenInteractive &screen, int &cur) {
    // static std::string cont = lazy::run("lazy course list -A");
    static std::vector<Course> parsedCourses;
    static bool loading = 1;
    static std::vector<std::string> placeholders;
    static std::once_flag flag;
    std::call_once(flag, [&]() {
        std::thread([&]() {
            // 在后台线程执行耗时操作
            std::string cont = lazy::run("lazy course list -A");
            auto data = parseCourseTable(cont);

            // 切换回主线程前更新数据
            parsedCourses = std::move(data);
            placeholders.assign(parsedCourses.size(), "");
            loading = false;

            // Let the screen reload or sth idk
            screen.PostEvent(Event::Custom);
        }).detach();
    });
    // for (auto i : parsedCourses) {
    //     std::cout << i.id << i.name << i.teacher << i.time << i.department << i.year << std::endl;
    // }
    //
    // static std::vector<std::string> menu_entries;
    // if (menu_entries.empty()) {
    //     for (const auto& c : parsedCourses) {
    //         menu_entries.push_back(c.id + " | " + c.name + " (" + formatTeacher(c.teacher) + ")" + " | " + c.year);
    //     }
    // }
    MenuOption option;
    option.entries_option.transform = [&](const EntryState& state) {
        if (loading) return text("Loading...");

        const auto& c = parsedCourses[state.index];

        std::string teacher_short = formatTeacher(c.teacher, 3);

        auto row = hbox({
            text(c.id) | size(WIDTH, EQUAL, 6) | color(Color::Cyan),
            separator(),
            text(c.name) | flex,
            separator(),
            text(teacher_short) | size(WIDTH, EQUAL, 25) | color(Color::GrayDark),
        });

        if (state.focused) {
            row = row | inverted | bold;
        }

        return row;
    };

    static std::vector<std::string> popupEntries = {
        "Syllabus(WIP)",
        "Coursewares(WIP)",
        "Members",
        "Rollcalls",
        "Quit",
    };

    static bool popupMenuShow = 0;
    static int popupSel = 0;
    MenuOption popupOpt;
    static auto popupMenu = Menu(&popupEntries, &popupSel, popupOpt);
    static auto popupMenuRend = Renderer(popupMenu, [&] {
        return vbox({
            popupMenu->Render() | center | flex
        }) | border | flex;
    });

    static std::string popCont;
    static auto popupRend = Renderer([&] {
        return vbox({
            paragraph(popCont) | center | flex,
            separator(),
            text(" 按 [q], [h] 或是按钮关闭 ") | hcenter | dim,
        }) | border | flex;
    });
    static bool popupShow = 0;
    static auto popup = Container::Vertical({
        Button("关闭窗口", [&] {popupShow = 0;}, ButtonOption::Animated()),
        popupRend,
    });

    static int sel = 0;
    // static auto menu = Menu(&menu_entries, &sel, option);
    static auto menu = Menu(&placeholders, &sel, option);

    static auto renderer = Renderer(menu, [&] {
        if (loading) {
            return vbox({
                text("课程列表") | hcenter | bold,
                separator(),
                text("正在获取数据，请稍候...") | center | flex
            }) | border;
        }

        return vbox({
            text("课程列表") | hcenter | bold,
            separator(),
            paragraph(" [Enter]: 查看课程详情 |  [q], [h]: 回到主菜单 | [j], [k], [↑], [↓] 选择条目 ") | hcenter | dim,
            separator(),
            menu->Render() | frame | vscroll_indicator | flex,
        }) | border;
    });

    renderer |= Modal(popupMenuRend, &popupMenuShow);
    renderer |= Modal(popup, &popupShow);

    return renderer | CatchEvent([&](Event e) {
        if (popupShow) {
            if (e == Event::Character('q') || e == Event::Character('h')) {
                popupShow = 0;
                return true;
            }
            return false;
        }
        if (popupMenuShow) {
            if (e == Event::Character('q') || e == Event::Character('h')) {
                popupMenuShow = 0;
                return true;
            }
            if (e == Event::Character('l') || e == Event::Return) {
                if (popupSel == 4) {
                    popupMenuShow = 0;
                    return true;
                }
                if (popupSel == 3) {
                    std::thread([&] {
                        std::string id = parsedCourses[sel].id;
                        popCont = "读取中......";
                        popupShow = 1;
                        screen.RequestAnimationFrame();
                        auto data = lazy::run("lazy course view rollcalls " + id + " -A");
                        popCont = std::move(data);
                        screen.RequestAnimationFrame();
                    }).detach();
                    return true;
                }
                if (popupSel == 2) {
                    std::thread([&] {
                        std::string id = parsedCourses[sel].id;
                        popCont = "读取中......";
                        popupShow = 1;
                        screen.RequestAnimationFrame();
                        auto data = lazy::run("lazy course view members " + id);
                        popCont = std::move(data);
                        screen.RequestAnimationFrame();
                    }).detach();
                    return true;
                }
                return true;
            }
            return false;
        }
        if (e == Event::Character('q') || e == Event::Character('h')) {
            cur = 0;
            return true;
        }
        if (e == Event::Return) {
            popupMenuShow = 1;
            return true;
        }
        return false;
    });
}
