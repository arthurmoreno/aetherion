# src/Gui/CMakeLists.txt

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../cmake/")

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
set(NANOBIND_DIR "${CMAKE_SOURCE_DIR}/libs/nanobind")
set(nanobind_DIR "${CMAKE_SOURCE_DIR}/libs/nanobind/cmake")
find_package(nanobind CONFIG REQUIRED)

# Define ImGui source files
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/libs/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_demo.cpp  # Optional: Remove in production
    ${IMGUI_DIR}/implot.cpp
    ${IMGUI_DIR}/implot_demo.cpp  # Optional: Remove in production
    ${IMGUI_DIR}/implot_items.cpp
    ${IMGUI_DIR}/ImGuizmo.cpp
)

set(IMGUI_BACKEND_SOURCES
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)

# Add ImGui as a static library
add_library(imgui_lib STATIC ${IMGUI_SOURCES} ${IMGUI_BACKEND_SOURCES})

# Include directories for ImGui
target_include_directories(imgui_lib PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)


# Collect all .cpp source files in Gui
file(GLOB GUI_SOURCES
    "${CMAKE_SOURCE_DIR}/src/Gui/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Gui/Windows/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Gui/TerminalPrograms/*.cpp"
)

# Collect all .hpp header files in Gui
file(GLOB GUI_HEADERS
    "${CMAKE_SOURCE_DIR}/src/Gui/*.hpp"
    "${CMAKE_SOURCE_DIR}/src/Gui/Windows/*.hpp"
    "${CMAKE_SOURCE_DIR}/src/Gui/TerminalPrograms/*.hpp"
)

# Create a static library for Gui
add_library(Gui STATIC ${GUI_SOURCES} ${GUI_HEADERS})

# Ensure the library is built with PIC
set_target_properties(Gui PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Specify include directories for Gui
target_include_directories(Gui PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${FLATBUFFERS_GENERATED_CPP_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${Python3_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/libs
    # Add other necessary include directories if needed
)

# Link dependencies for Gui
target_link_libraries(Gui PUBLIC
    components
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    Python3::Python
    imgui_lib
    # Add other necessary libraries if needed
)

# Ensure Gui depends on FlatBuffers generation
add_dependencies(Gui GenerateFlatBuffers)