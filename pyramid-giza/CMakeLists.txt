cmake_minimum_required(VERSION 3.10)
project(PyramidGiza)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- GLFW ----
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)

# ---- GLAD ----
add_library(glad STATIC external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

# ---- GLM ----
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE external/glm)

# ---- Our executable ----
add_executable(PyramidGiza src/main.cpp)

target_include_directories(PyramidGiza PRIVATE
    external/glfw/include
    external/glad/include
    external/glm
)

target_link_libraries(PyramidGiza PRIVATE
    glfw
    glad
    glm
)

if(WIN32)
    target_link_libraries(PyramidGiza PRIVATE opengl32)
endif()