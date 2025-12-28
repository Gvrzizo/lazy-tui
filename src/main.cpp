#include<ftxui/dom/elements.hpp>
#include<ftxui/screen/screen.hpp>
#include<ftxui/component/screen_interactive.hpp>
#include<ftxui/component/component.hpp>

#include"tui/mainMenu.hpp"
#include"tui/course.hpp"
#include"tui/assignment.hpp"

int main() {
    using namespace ftxui;

    auto screen = ScreenInteractive::FullscreenAlternateScreen();
    static int cur = 0;

    auto curTab = Container::Tab({
        mainMenu(screen, cur),
        course(screen, cur),
        assignment(screen, cur),
    }, &cur);

    auto mainComponent = CatchEvent(curTab, [&](Event e) {
        return false;
    });

    screen.Loop(mainComponent);

    return 0;
}
