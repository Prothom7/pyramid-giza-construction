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
    src/camera/CameraController.cpp
    src/camera/CameraValidation.cpp
    src/lighting/SunController.cpp
    src/lighting/ShadowMap.cpp
    src/graphics/Mesh.cpp
    src/graphics/PrimitiveGenerator.cpp
    src/graphics/GeometryValidation.cpp
    src/graphics/Texture.cpp
    src/graphics/Frustum.cpp
    src/graphics/SkyBackground.cpp
    src/graphics/CloudLayer.cpp
    src/graphics/InstanceBatch.cpp
    src/effects/ParticleSystem.cpp
    src/presentation/ShowcaseController.cpp
    src/scene/PyramidLayout.cpp
    src/scene/PyramidInterior.cpp
    src/scene/MonumentalSite.cpp
    src/scene/IndustrialLandscape.cpp
    src/scene/ObjectEnrichment.cpp
    src/scene/SceneIntegrity.cpp
    src/scene/RampValidation.cpp
    src/scene/SupportSystem.cpp
    src/scene/SceneTypes.cpp
    src/scene/StaticGizaScene.cpp
    src/objects/Worker.cpp
    src/objects/Sledge.cpp
    src/objects/ConstructionProps.cpp
    src/objects/Scaffold.cpp
    src/objects/CompositeValidation.cpp
    src/objects/WorkerHierarchyValidation.cpp
    src/animation/ConstructionAnimation.cpp
    src/animation/QuarryPulleyAnimation.cpp
    src/animation/AnimationValidation.cpp
    src/animation/ConstructionTimeline.cpp
    src/camera/CursorController.cpp
    src/presentation/SimulationHUD.cpp
    src/scene/QuarrySystem.cpp
    src/scene/ConstructionLogistics.cpp
    src/scene/ConstructionSimulation.cpp
    src/scene/SandSimulation.cpp
    src/scene/WaterSimulation.cpp
    src/scene/SphinxMonument.cpp
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
add_test(NAME Phase6CameraNavigationValidation COMMAND PyramidGiza --validate-camera)
add_test(NAME Phase6_5ObjectEnrichmentValidation COMMAND PyramidGiza --validate-enrichment)
add_test(NAME Phase7LightingValidation COMMAND PyramidGiza --validate-lighting)
add_test(NAME Phase8ShadowMappingValidation COMMAND PyramidGiza --validate-shadows)
add_test(NAME Phase9ConstructionTimelineValidation COMMAND PyramidGiza --validate-construction)
add_test(NAME Phase9TextureUvValidation COMMAND PyramidGiza --validate-textures)
add_test(NAME Phase9SceneIntegrityValidation COMMAND PyramidGiza --validate-layout)
add_test(NAME Phase10InstancingValidation COMMAND PyramidGiza --validate-instancing)
add_test(NAME Phase10FrustumValidation COMMAND PyramidGiza --validate-frustum)
add_test(NAME Phase10PerformanceStructureValidation COMMAND PyramidGiza --validate-renderer-structure)
add_test(NAME Phase11ParticleValidation COMMAND PyramidGiza --validate-particles)
add_test(NAME Phase11EffectEventValidation COMMAND PyramidGiza --validate-effect-events)
add_test(NAME Phase11EnvironmentMotionValidation COMMAND PyramidGiza --validate-environment-motion)
add_test(NAME Phase12ShowcaseTimelineValidation COMMAND PyramidGiza --validate-showcase-timeline)
add_test(NAME Phase12ShowcaseCameraValidation COMMAND PyramidGiza --validate-showcase-camera)
add_test(NAME Phase12ShowcaseStateValidation COMMAND PyramidGiza --validate-showcase-state)
add_test(NAME Phase12_5TimelapseRepairValidation COMMAND PyramidGiza --validate-timelapse-repair)
add_test(NAME Phase12_5RampClearanceValidation COMMAND PyramidGiza --validate-ramp-clearance)
add_test(NAME Phase12_6SupportSurfaceValidation COMMAND PyramidGiza --validate-supports)
add_test(NAME Phase12_6GroundingValidation COMMAND PyramidGiza --validate-grounding)
add_test(NAME Phase12_6StageDependencyValidation COMMAND PyramidGiza --validate-stage-dependencies)
add_test(NAME Phase12_7QuarryPulleyAnimationValidation COMMAND PyramidGiza --validate-quarry-pulley)
add_test(NAME Phase12_7QuarryPulleySupportValidation COMMAND PyramidGiza --validate-quarry-pulley-support)
add_test(NAME Phase12_8InteriorGeometryValidation COMMAND PyramidGiza --validate-interior-geometry)
add_test(NAME Phase12_8InteriorConnectivityValidation COMMAND PyramidGiza --validate-interior-connectivity)
add_test(NAME Phase12_8InteriorNavigationValidation COMMAND PyramidGiza --validate-interior-navigation)
add_test(NAME Phase12_8_1EntranceFacadeValidation COMMAND PyramidGiza --validate-entrance)
add_test(NAME Phase13QuarryValidation COMMAND PyramidGiza --validate-quarry)
add_test(NAME Phase13LogisticsValidation COMMAND PyramidGiza --validate-logistics)
add_test(NAME Phase13UpperAccessStageOneValidation COMMAND PyramidGiza --validate-upper-access)
add_test(NAME Phase13UpperAccessStageTwoValidation COMMAND PyramidGiza --validate-upper-access-stage-two)
add_test(NAME Phase13PhysicalPlacementValidation COMMAND PyramidGiza --validate-placement)
add_test(NAME Phase13SandValidation COMMAND PyramidGiza --validate-sand)
add_test(NAME Phase13WaterValidation COMMAND PyramidGiza --validate-water)
add_test(NAME Phase13BoatWaterValidation COMMAND PyramidGiza --validate-boat-water)
add_test(NAME Phase13BoatNavigationValidation COMMAND PyramidGiza --validate-boat-navigation)
add_test(NAME Phase13BoatWakeValidation COMMAND PyramidGiza --validate-boat-wake)
add_test(NAME Phase13SphinxValidation COMMAND PyramidGiza --validate-sphinx)
add_test(NAME Phase13InputValidation COMMAND PyramidGiza --validate-input)
add_test(NAME Phase13SimulationHUDValidation COMMAND PyramidGiza --validate-simulation-hud)
add_test(NAME Phase13SkyBackgroundValidation COMMAND PyramidGiza --validate-sky)
add_test(NAME Phase13CloudLayerValidation COMMAND PyramidGiza --validate-clouds)
