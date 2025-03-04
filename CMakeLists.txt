cmake_minimum_required(VERSION 3.10)
project(Raindrop)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_custom_target(run
	COMMAND raindrop
    DEPENDS raindrop
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Check if POLICY CMP0072 exist and make use of it if it does
if (POLICY CMP0072)
  cmake_policy (SET CMP0072 NEW)
endif(POLICY CMP0072)

# Find OpenGL and GLFW
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

add_subdirectory("libs")

# Add the source files
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp")

set(MAIN_SOURCE "src/main.cpp")

# Set the executable
add_executable(Raindrop ${SOURCE_FILES} ${MAIN_SOURCE})

# Link libraries
target_link_libraries(Raindrop
	glfw
    glad
    X11
	Xfixes
	OpenGL::GL
)

# Optionally, add custom compile options
target_compile_options(Raindrop PRIVATE -Wall -Wextra)
