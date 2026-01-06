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

class AlwaysFocusable : public ftxui::ComponentBase {
public:
    AlwaysFocusable(ftxui::Component inner) {
        Add(inner);
    }

    bool Focusable() const override { return true; }
};

struct Course {
    std::string id;
    std::string name;
    std::string teacher;
    std::string time;
    std::string department;
    std::string year;
};

struct Courseware {
    std::string id;
    std::string name;
    std::string time;
    std::string size;
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
    std::string block = "│";
#ifdef _WIN32
    block = "|";
#endif

    while (std::getline(ss, line)) {
        // 1. 只处理包含数据分隔符 '│' (或者Windows上'|')的行，忽略边框 '━', '┳' 等
        if (line.find(block) == std::string::npos) continue;

        // 2. 按 '│' 分割行
        std::vector<std::string> cols;
        std::stringstream lineStream(line);
        std::string cell;
        const std::string delimiter = block;
        size_t start = 0, pos;
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

std::vector<Courseware> parseCoursewares(const std::string& raw_output) {
    std::vector<Courseware> coursewares;
    std::stringstream ss(raw_output);
    std::string line;
    std::string block = "│";
#ifdef _WIN32
    block = "|";
#endif

    while (std::getline(ss, line)) {
        // 1. 只处理包含数据分隔符 '│' (或者Windows上'|') 的行，忽略边框 '━', '┳' 等
        if (line.find(block) == std::string::npos) continue;

        // 2. 按 '│' 分割行
        std::vector<std::string> cols;
        std::stringstream lineStream(line);
        std::string cell;
        const std::string delimiter = block;
        size_t start = 0, pos;
        while ((pos = line.find(delimiter, start)) != std::string::npos) {
            std::string cell = line.substr(start, pos - start);
            cols.push_back(trim(cell));
            start = pos + delimiter.size();
        }

        // 由于首尾都有 │，分割后 cols[0] 通常为空，cols[1] 才是 ID
        if (cols.size() < 5) continue;

        std::string id = cols[1];

        // 3. 判断是新课程还是上一行的续行
        if (!id.empty() && std::isdigit(id[0])) {
            // 是新课程 ID
            Courseware c;
            c.id = id;
            c.name = cols[2];
            c.time = cols[3];
            c.size = cols[4];
            coursewares.push_back(c);
        } else if (!coursewares.empty()) {
            // ID 为空，说明是多行内容的后续部分，追加到上一个课程
            Courseware& last = coursewares.back();
            if (!cols[2].empty()) last.name += cols[2];
            if (!cols[3].empty()) last.time += cols[3];
            if (!cols[4].empty()) last.size += cols[4];
        }
    }
    return coursewares;
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

} // namespace

Component course(ftxui::ScreenInteractive &screen, int &cur) {
    static std::vector<Course> parsedCourses;
    static std::vector<Courseware> parsedCoursewares;
    static bool loading = 1;
    static std::vector<std::string> placeholders;
    static std::vector<std::string> placeholdersCourseware;
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
    MenuOption option, optionCourseware;
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
            separator(),
            text(c.year) | size(WIDTH, EQUAL, 10) | color(Color::GrayDark),
        });

        if (state.focused) {
            row = row | inverted | bold;
        }

        return row;
    };

    optionCourseware.entries_option.transform = [&](const EntryState& state) {
        if (loading) return text("Loading...");

        const auto& c = parsedCoursewares[state.index];

        auto row = hbox({
            text(c.id) | size(WIDTH, EQUAL, 7) | color(Color::Cyan),
            separator(),
            text(c.name) | flex,
            separator(),
            text(c.time) | size(WIDTH, EQUAL, 20),
            separator(),
            text(c.size) | size(WIDTH, EQUAL, 8),
        });

        if (state.focused) {
            row = row | inverted | bold;
        }

        return row;
    };

    static std::vector<std::string> popupEntries = {
        "Syllabus(WIP)",
        "Coursewares",
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
        }) | border;
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
    static int selCourseware = 0;
    static auto menuOri = Menu(&placeholders, &sel, option);
    static auto menu = Make<AlwaysFocusable>(menuOri);
    static auto coursewareMenuOri = Menu(&placeholdersCourseware, &selCourseware, optionCourseware);
    static auto coursewareMenu = Make<AlwaysFocusable>(coursewareMenuOri);

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
            paragraph(" [Enter], [l]: 查看课程详情 |  [q], [h]: 回到主菜单 | [j], [k], [↑], [↓] 选择条目 ") | hcenter | dim,
            separator(),
            menu->Render() | frame | vscroll_indicator | flex,
        }) | border;
    });

    static bool coursewareShow = 0;
    static auto rendererCoursewares = Container::Vertical({
        Button("关闭窗口", [&] {coursewareShow = 0;}, ButtonOption::Animated()),
        Renderer(coursewareMenu, [&] {
            return vbox({
                text("课程资源列表") | hcenter | bold,
                separator(),
                paragraph(" [Enter], [l]: 下载资源 |  [q], [h]: 回到主菜单 | [j], [k], [↑], [↓] 选择条目 ") | hcenter | dim,
                separator(),
                coursewareMenu->Render() | frame | vscroll_indicator | flex,
            }) | border;
        })}
    );

    static bool subNoti = 0;
    static std::string inRes;

    static auto showRes = Renderer([&] {
        return vbox({
            text(inRes) | center | flex
        }) | border;
    });

    renderer |= Modal(popupMenuRend, &popupMenuShow);
    renderer |= Modal(popup, &popupShow);
    renderer |= Modal(rendererCoursewares, &coursewareShow);
    renderer |= Modal(showRes, &subNoti);

    return renderer | CatchEvent([&](Event e) {
        if (coursewareShow) {
            if (e == Event::Character('q') || e == Event::Character('h')) {
                coursewareShow = 0;
                return true;
            }
            if (e == Event::Return || e == Event::Character('l')) {
                std::thread([&] {
                    inRes = "下载中......";
                    subNoti = 1;
                    screen.RequestAnimationFrame();
                    lazy::run("lazy resource download " + parsedCoursewares[selCourseware].id);
                    inRes = "下载完毕！";
                    screen.RequestAnimationFrame();
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    subNoti = 0;
                    screen.RequestAnimationFrame();
                }).detach();
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
                if (popupSel == 1) {
                    std::thread([&] {
                        std::string id = parsedCourses[sel].id;
                        inRes = "读取中......";
                        subNoti = 1;
                        screen.RequestAnimationFrame();
                        auto oridata = lazy::run("lazy course view coursewares " + id + " -A");
                        auto data = parseCoursewares(oridata);
                        selCourseware = 0;
                        parsedCoursewares = std::move(data);
                        placeholdersCourseware.assign(parsedCoursewares.size(), "");
                        coursewareShow = 1;
                        subNoti = 0;
                        screen.RequestAnimationFrame();
                    }).detach();
                    return true;
                }
                return true;
            }
            return false;
        }
        if (popupShow) {
            if (e == Event::Character('q') || e == Event::Character('h')) {
                popupShow = 0;
                return true;
            }
            return false;
        }
        if (e == Event::Character('q') || e == Event::Character('h')) {
            cur = 0;
            return true;
        }
        if (e == Event::Return || e == Event::Character('l')) {
            popupMenuShow = 1;
            return true;
        }
        return false;
    });
}
