cmake_minimum_required(VERSION 3.16)
project(PyramidGiza VERSION 0.1.0 LANGUAGES C CXX)

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

# ---- Course project executable ----
add_executable(PyramidGiza
    src/main.cpp
    src/Shader.cpp
    src/Camera.cpp
    src/graphics/Mesh.cpp
    src/graphics/PrimitiveGenerator.cpp
    src/graphics/GeometryValidation.cpp
    src/scene/PyramidLayout.cpp
    src/scene/MonumentalSite.cpp
    src/scene/IndustrialLandscape.cpp
    src/scene/SceneTypes.cpp
    src/scene/StaticGizaScene.cpp
    src/objects/Worker.cpp
    src/objects/Sledge.cpp
    src/objects/ConstructionProps.cpp
    src/objects/Scaffold.cpp
    src/objects/CompositeValidation.cpp
    src/objects/WorkerHierarchyValidation.cpp
    src/animation/ConstructionAnimation.cpp
    src/animation/AnimationValidation.cpp
)

target_include_directories(PyramidGiza PRIVATE
    external/glfw/include
    external/glad/include
    external/glm
    src
)

target_link_libraries(PyramidGiza PRIVATE
    glfw
    glad
    glm
)

if(WIN32)
    target_link_libraries(PyramidGiza PRIVATE opengl32)
endif()

if(MSVC)
    target_compile_options(PyramidGiza PRIVATE /W4 /permissive-)
else()
    target_compile_options(PyramidGiza PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(PyramidGiza PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_command(TARGET PyramidGiza POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:PyramidGiza>/shaders
)

enable_testing()
add_test(NAME Phase1GeometryValidation COMMAND PyramidGiza --validate-geometry)
add_test(NAME Phase2SceneValidation COMMAND PyramidGiza --validate-scene)
add_test(NAME Phase3CompositeValidation COMMAND PyramidGiza --validate-composites)
add_test(NAME Phase4WorkerHierarchyValidation COMMAND PyramidGiza --validate-hierarchy)
add_test(NAME Phase5ConstructionAnimationValidation COMMAND PyramidGiza --validate-animation)
add_test(NAME Phase5_5MonumentalSiteValidation COMMAND PyramidGiza --validate-site)
add_test(NAME Phase5_6IndustrialLandscapeValidation COMMAND PyramidGiza --validate-industrial)
