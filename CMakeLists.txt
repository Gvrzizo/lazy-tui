cmake_minimum_required(VERSION 3.16)
project(lazytui)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG v6.1.9
)
FetchContent_MakeAvailable(ftxui)

if(WIN32)
    add_definitions(-Dpopen=_popen)
    add_definitions(-Dpclose=_pclose)
endif()

add_executable(lazytui
    src/main.cpp
    src/clicall/lazycli.cpp
    src/tui/mainMenu.cpp 
    src/tui/assignment.cpp
    src/tui/resource.cpp
    src/tui/course.cpp
)

target_include_directories(lazytui PRIVATE src)

target_link_libraries(lazytui PRIVATE
    ftxui::component
    ftxui::dom
    ftxui::screen
)

install(TARGETS lazytui RUNTIME DESTINATION bin)
install(FILES README.md DESTINATION share/doc/lazytui)
