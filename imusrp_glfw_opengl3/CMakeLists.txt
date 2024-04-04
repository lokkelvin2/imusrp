cmake_minimum_required(VERSION 3.19)
project(imusrp_glfw_opengl3)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Boost is optional but we add it in to mirror the Makefile
find_package(Boost 1.72 REQUIRED COMPONENTS filesystem program_options thread)
message(STATUS "Boost_INCLUDE_DIRS = " ${Boost_INCLUDE_DIRS})
message(STATUS "Boost_LIBRARIES = "${Boost_LIBRARIES})

# Find packages
find_package(OpenGL REQUIRED)
find_package(IPP REQUIRED)
message(STATUS "IPP_FOUND = " ${IPP_FOUND})
message(STATUS "IPP_LIBRARIES = " ${IPP_LIBRARIES})

# Let's use the GLFW3 binaries that are included in imgui
set(GLFW_DIR ${CMAKE_SOURCE_DIR}/../imgui/examples/libs/glfw)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/../imgui
    ${CMAKE_SOURCE_DIR}/../imgui/backends
    ${CMAKE_SOURCE_DIR}/../implot
    ${CMAKE_SOURCE_DIR}/../imusrp_ui
    ${CMAKE_SOURCE_DIR}/include
    $ENV{UHD_INSTALL_DIR}/include
    ${Boost_INCLUDE_DIRS}
    ${GLFW_DIR}/include
)

# Source files
set(SOURCES
    src/main.cpp
    ../imgui/imgui.cpp
    ../imgui/imgui_demo.cpp
    ../imgui/imgui_draw.cpp
    ../imgui/imgui_tables.cpp
    ../imgui/imgui_widgets.cpp
    ../imgui/backends/imgui_impl_glfw.cpp
    ../imgui/backends/imgui_impl_opengl3.cpp
    ../implot/implot_demo.cpp
    ../implot/implot_items.cpp
    ../implot/implot.cpp
    ../imusrp_ui/ImUsrpUi.cpp
    ../imusrp_ui/ImUsrpUiRx.cpp
)

# Compiler flags
if (MSVC)
    message(STATUS ${MSVC_VERSION} " " ${MSVC_TOOLSET_VERSION})
    message(STATUS "Setting compile options for MSVC")
    add_compile_definitions(IMGUI_USER_CONFIG=\"imusrp_imgui_config.h\")
    add_compile_options(/W3 /MD /O2)
else()
    # TODO: add support for unix and mac
endif()

# Add the executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
if(WIN32)
    message(STATUS "Linking libraries for WIN32")
    
    target_link_libraries(
        ${PROJECT_NAME} 
        ${OPENGL_LIBRARIES}
        ${GLFW_DIR}/lib-vc2010-64/glfw3.lib
        IPP::ippcore
        IPP::ipps
        IPP::ippvm
        $ENV{UHD_INSTALL_DIR}/lib/uhd.lib
        ${Boost_LIBRARIES}
    )
else()
    # TODO: add support for unix and mac
endif()

