#include<ftxui/component/component.hpp>           // for Menu
#include<ftxui/component/component_options.hpp>   // for MenuOption
#include<ftxui/component/screen_interactive.hpp>
#include<vector>

using namespace ftxui;

Component mainMenu(ScreenInteractive &screen, int &cur) {

    static std::vector<std::string> entries = {
        "Course",
        "Assignment",
        "Resource(WIP)",
        "Quit"
    };
    static int sel = 0;

    MenuOption opt;
    opt.on_enter = screen.ExitLoopClosure();

    auto menu = Menu(&entries, &sel, opt);

    return menu
        | CatchEvent([&](Event e) {
            if (e == Event::Return || e == Event::Character("l")) {
                if (sel == 0) cur = 1;
                if (sel == 1) cur = 2;
                if (sel == 2) cur = 0;
                if (sel == 3) screen.Exit();
                return true;
            }
            else if (e == Event::Character("q")) {
                screen.Exit();
                return true;
            }
            return false;
        }
    );
}
