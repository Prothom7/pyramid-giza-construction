#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "animation/AnimationValidation.h"
#include "camera/CameraController.h"
#include "camera/CameraValidation.h"
#include "graphics/GeometryValidation.h"
#include "lighting/ShadowMap.h"
#include "lighting/SunController.h"
#include "objects/CompositeValidation.h"
#include "objects/WorkerHierarchyValidation.h"
#include "scene/PyramidLayout.h"
#include "scene/IndustrialLandscape.h"
#include "scene/MonumentalSite.h"
#include "scene/ObjectEnrichment.h"
#include "scene/StaticGizaScene.h"

namespace
{
constexpr int initialWidth = 1280;
constexpr int initialHeight = 720;

struct AppState
{
    CameraController cameraController;
    float lastMouseX = initialWidth * 0.5f;
    float lastMouseY = initialHeight * 0.5f;
    float deltaTime = 0.0f;
    bool firstMouse = true;
    bool cullingEnabled = true;
    bool wireframeEnabled = false;
    StaticGizaScene* scene = nullptr;
};

void setCameraPreset(AppState& state, int preset, bool instant)
{
    const std::size_t index = static_cast<std::size_t>(std::clamp(preset, 1, 9) - 1);
    state.cameraController.selectPreset(index, instant);
    state.firstMouse = true;
    const CameraPose& pose = CameraController::presets()[index];
    std::cout << "Camera preset " << preset << " - " << pose.name
              << (instant ? " (instant)" : " (smooth)") << ".\n";
}

void glfwErrorCallback(int code, const char* description)
{
    std::cerr << "GLFW error " << code << ": " << description << '\n';
}

void printSunState(const StaticGizaScene& scene, const char* prefix)
{
    const SunState& sun = scene.sunState();
    std::cout << prefix << ": " << sun.timeOfDay << ":00, automatic "
              << (scene.automaticSun() ? "ON" : "OFF")
              << ", direction (" << sun.light.direction.x << ", "
              << sun.light.direction.y << ", " << sun.light.direction.z
              << "), intensity " << sun.light.intensity << '\n';
}

bool parseLightingDebugMode(const std::string& value, LightingDebugMode& mode)
{
    if (value == "normal")
        mode = LightingDebugMode::Normal;
    else if (value == "diffuse")
        mode = LightingDebugMode::DiffuseOnly;
    else if (value == "specular")
        mode = LightingDebugMode::SpecularOnly;
    else if (value == "normals")
        mode = LightingDebugMode::Normals;
    else if (value == "unlit")
        mode = LightingDebugMode::UnlitBaseColor;
    else
        return false;
    return true;
}

void framebufferSizeCallback(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouseCallback(GLFWwindow* window, double xPosition, double yPosition)
{
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr)
        return;

    const float x = static_cast<float>(xPosition);
    const float y = static_cast<float>(yPosition);
    if (state->firstMouse)
    {
        state->lastMouseX = x;
        state->lastMouseY = y;
        state->firstMouse = false;
    }

    state->cameraController.handleMouseDelta(x - state->lastMouseX,
                                             state->lastMouseY - y);
    state->lastMouseX = x;
    state->lastMouseY = y;
}

void scrollCallback(GLFWwindow* window, double, double yOffset)
{
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (state != nullptr)
        state->cameraController.handleScroll(static_cast<float>(yOffset));
}

void keyCallback(GLFWwindow* window, int key, int, int action, int mods)
{
    if (action != GLFW_PRESS)
        return;

    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr)
        return;

    if (key == GLFW_KEY_C)
    {
        state->cullingEnabled = !state->cullingEnabled;
        if (state->cullingEnabled)
            glEnable(GL_CULL_FACE);
        else
            glDisable(GL_CULL_FACE);
        std::cout << "Back-face culling: " << (state->cullingEnabled ? "ON" : "OFF") << '\n';
    }
    else if (key == GLFW_KEY_F)
    {
        state->wireframeEnabled = !state->wireframeEnabled;
        glPolygonMode(GL_FRONT_AND_BACK, state->wireframeEnabled ? GL_LINE : GL_FILL);
        std::cout << "Wireframe: " << (state->wireframeEnabled ? "ON" : "OFF") << '\n';
    }
    else if (key == GLFW_KEY_SPACE && state->scene != nullptr)
    {
        state->scene->togglePlayback();
        std::cout << "Animation playback toggled; state: "
                  << state->scene->animationStateName() << '\n';
    }
    else if (key == GLFW_KEY_P && state->scene != nullptr)
    {
        state->scene->cycleDemoPose();
        std::cout << "Demo worker pose: " << state->scene->demoPoseName() << '\n';
    }
    else if (key == GLFW_KEY_R && state->scene != nullptr)
    {
        state->scene->resetAnimation();
        std::cout << "Animation reset to Idle.\n";
    }
    else if (key == GLFW_KEY_N && state->scene != nullptr)
    {
        state->scene->advanceAnimationState();
        std::cout << "Advanced to animation state: " << state->scene->animationStateName() << '\n';
    }
    else if (key == GLFW_KEY_L && state->scene != nullptr)
    {
        state->scene->toggleAnimationLoop();
        std::cout << "Animation loop: " << (state->scene->animationLooping() ? "ON" : "OFF") << '\n';
    }
    else if (key == GLFW_KEY_M && state->scene != nullptr)
    {
        state->scene->toggleCoordinatedAnimation();
        std::cout << "Coordinated construction animation: "
                  << (state->scene->coordinatedAnimationEnabled() ? "ON" : "OFF") << '\n';
    }
    else if ((key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD) && state->scene != nullptr)
    {
        state->scene->adjustAnimationSpeed(0.25f);
        std::cout << "Animation speed: " << state->scene->animationSpeed() << "x\n";
    }
    else if ((key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) && state->scene != nullptr)
    {
        state->scene->adjustAnimationSpeed(-0.25f);
        std::cout << "Animation speed: " << state->scene->animationSpeed() << "x\n";
    }
    else if (key == GLFW_KEY_0)
    {
        state->cameraController.reset();
        state->firstMouse = true;
        std::cout << "Camera reset to monumental overview.\n";
    }
    else if (key == GLFW_KEY_O)
    {
        state->cameraController.togglePyramidOrbit();
        state->firstMouse = true;
        std::cout << "Camera mode: "
                  << CameraController::modeName(state->cameraController.mode()) << '\n';
    }
    else if (key == GLFW_KEY_T && state->scene != nullptr)
    {
        state->cameraController.toggleTransportFollow(state->scene->transportTarget());
        state->firstMouse = true;
        std::cout << "Camera mode: "
                  << CameraController::modeName(state->cameraController.mode()) << '\n';
    }
    else if (key == GLFW_KEY_G)
    {
        state->cameraController.toggleGuidedDemo();
        state->firstMouse = true;
        std::cout << "Camera mode: "
                  << CameraController::modeName(state->cameraController.mode()) << '\n';
    }
    else if (key == GLFW_KEY_K)
    {
        const CameraPose pose = state->cameraController.currentPose();
        std::cout << "Camera position = (" << pose.position.x << ", "
                  << pose.position.y << ", " << pose.position.z << ") yaw = "
                  << pose.yaw << " pitch = " << pose.pitch << " FOV = "
                  << pose.fovDegrees << " mode = "
                  << CameraController::modeName(state->cameraController.mode()) << '\n';
    }
    else if (key == GLFW_KEY_U && state->scene != nullptr)
    {
        state->scene->toggleAutomaticSun();
        printSunState(*state->scene, "Automatic sun toggled");
    }
    else if (key == GLFW_KEY_LEFT_BRACKET && state->scene != nullptr)
    {
        state->scene->adjustSunTime(-SunController::manualStepHours);
        printSunState(*state->scene, "Sun time adjusted");
    }
    else if (key == GLFW_KEY_RIGHT_BRACKET && state->scene != nullptr)
    {
        state->scene->adjustSunTime(SunController::manualStepHours);
        printSunState(*state->scene, "Sun time adjusted");
    }
    else if (key == GLFW_KEY_F1 && state->scene != nullptr)
    {
        state->scene->selectMorningSun();
        printSunState(*state->scene, "Morning preset");
    }
    else if (key == GLFW_KEY_F2 && state->scene != nullptr)
    {
        state->scene->selectNoonSun();
        printSunState(*state->scene, "Noon preset");
    }
    else if (key == GLFW_KEY_F3 && state->scene != nullptr)
    {
        state->scene->selectEveningSun();
        printSunState(*state->scene, "Evening preset");
    }
    else if (key == GLFW_KEY_V && state->scene != nullptr)
    {
        state->scene->cycleLightingDebugMode();
        std::cout << "Lighting debug mode: "
                  << state->scene->lightingDebugModeName() << '\n';
    }
    else if (key == GLFW_KEY_H && state->scene != nullptr)
    {
        state->scene->toggleShadows();
        std::cout << "Directional shadows: "
                  << (state->scene->shadowsEnabled() ? "ON" : "OFF") << '\n';
    }
    else if (key == GLFW_KEY_J && state->scene != nullptr)
    {
        state->scene->cycleShadowDebugMode();
        std::cout << "Shadow debug mode: "
                  << state->scene->shadowDebugModeName() << '\n';
    }
    else if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9)
        setCameraPreset(*state, key - GLFW_KEY_0, (mods & GLFW_MOD_SHIFT) != 0);
}

void processInput(GLFWwindow* window, AppState& state)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);

    CameraSpeedMode speedMode = CameraSpeedMode::Normal;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)
        speedMode = CameraSpeedMode::Slow;
    else if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
             glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
        speedMode = CameraSpeedMode::Fast;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        state.cameraController.move(CameraMovement::FORWARD, state.deltaTime, speedMode);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        state.cameraController.move(CameraMovement::BACKWARD, state.deltaTime, speedMode);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        state.cameraController.move(CameraMovement::LEFT, state.deltaTime, speedMode);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        state.cameraController.move(CameraMovement::RIGHT, state.deltaTime, speedMode);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        state.cameraController.move(CameraMovement::DOWN, state.deltaTime, speedMode);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        state.cameraController.move(CameraMovement::UP, state.deltaTime, speedMode);
}

bool checkOpenGLErrors()
{
    bool clean = true;
    for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError())
    {
        clean = false;
        std::cerr << "OpenGL error: 0x" << std::hex << error << std::dec << '\n';
    }
    return clean;
}

void captureFramebuffer(const std::string& path, int width, int height)
{
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    std::ofstream output(path, std::ios::binary);
    if (!output)
        throw std::runtime_error("Could not create framebuffer capture: " + path);
    output << "P6\n" << width << ' ' << height << "\n255\n";
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 3;
    for (int row = height - 1; row >= 0; --row)
    {
        const char* rowStart = reinterpret_cast<const char*>(pixels.data() + rowBytes * row);
        output.write(rowStart, static_cast<std::streamsize>(rowBytes));
    }
    std::cout << "Framebuffer capture written to " << path << '\n';
}
} // namespace

int main(int argc, char** argv)
{
    bool geometryValidationOnly = false;
    bool sceneValidationOnly = false;
    bool compositeValidationOnly = false;
    bool hierarchyValidationOnly = false;
    bool animationValidationOnly = false;
    bool siteValidationOnly = false;
    bool industrialValidationOnly = false;
    bool cameraValidationOnly = false;
    bool enrichmentValidationOnly = false;
    bool lightingValidationOnly = false;
    bool shadowValidationOnly = false;
    bool smokeTest = false;
    float smokeDurationSeconds = 0.0f;
    bool startWireframe = false;
    bool startWithCulling = true;
    int cameraPreset = 1;
    float initialAnimationTime = 0.0f;
    float initialSunTime = SunController::morningTime;
    bool sunTimeSpecified = false;
    bool startAutomaticSun = true;
    bool startWithShadows = true;
    int shadowResolution = ShadowSettings::defaultResolution;
    LightingDebugMode initialLightingMode = LightingDebugMode::Normal;
    ShadowDebugMode initialShadowDebugMode = ShadowDebugMode::Normal;
    std::string capturePath;
    std::string initialCameraMode = "free";
    for (int argument = 1; argument < argc; ++argument)
    {
        const std::string option = argv[argument];
        if (option == "--validate-geometry")
            geometryValidationOnly = true;
        else if (option == "--validate-scene")
            sceneValidationOnly = true;
        else if (option == "--validate-composites")
            compositeValidationOnly = true;
        else if (option == "--validate-hierarchy")
            hierarchyValidationOnly = true;
        else if (option == "--validate-animation")
            animationValidationOnly = true;
        else if (option == "--validate-site")
            siteValidationOnly = true;
        else if (option == "--validate-industrial")
            industrialValidationOnly = true;
        else if (option == "--validate-camera")
            cameraValidationOnly = true;
        else if (option == "--validate-enrichment")
            enrichmentValidationOnly = true;
        else if (option == "--validate-lighting")
            lightingValidationOnly = true;
        else if (option == "--validate-shadows")
            shadowValidationOnly = true;
        else if (option == "--smoke-test")
            smokeTest = true;
        else if (option == "--smoke-duration" && argument + 1 < argc)
        {
            try
            {
                smokeDurationSeconds = std::stof(argv[++argument]);
            }
            catch (...)
            {
                std::cerr << "Smoke duration must be between 0 and 60 seconds.\n";
                return 2;
            }
            if (smokeDurationSeconds <= 0.0f || smokeDurationSeconds > 60.0f)
            {
                std::cerr << "Smoke duration must be between 0 and 60 seconds.\n";
                return 2;
            }
            smokeTest = true;
        }
        else if (option == "--wireframe")
            startWireframe = true;
        else if (option == "--no-cull")
            startWithCulling = false;
        else if (option == "--preset" && argument + 1 < argc)
        {
            const std::string value = argv[++argument];
            if (value.size() != 1 || value[0] < '1' || value[0] > '9')
            {
                std::cerr << "Camera preset must be between 1 and 9.\n";
                return 2;
            }
            cameraPreset = value[0] - '0';
        }
        else if (option == "--capture" && argument + 1 < argc)
            capturePath = argv[++argument];
        else if (option == "--camera-mode" && argument + 1 < argc)
        {
            initialCameraMode = argv[++argument];
            if (initialCameraMode != "free" && initialCameraMode != "orbit" &&
                initialCameraMode != "follow" && initialCameraMode != "demo")
            {
                std::cerr << "Camera mode must be free, orbit, follow, or demo.\n";
                return 2;
            }
        }
        else if (option == "--animation-time" && argument + 1 < argc)
        {
            try
            {
                initialAnimationTime = std::stof(argv[++argument]);
            }
            catch (...)
            {
                std::cerr << "Animation time must be a non-negative number.\n";
                return 2;
            }
            if (initialAnimationTime < 0.0f)
            {
                std::cerr << "Animation time must be a non-negative number.\n";
                return 2;
            }
        }
        else if (option == "--sun-time" && argument + 1 < argc)
        {
            try
            {
                initialSunTime = std::stof(argv[++argument]);
            }
            catch (...)
            {
                std::cerr << "Sun time must be between 6 and 18 hours.\n";
                return 2;
            }
            if (initialSunTime < SunController::daylightStart ||
                initialSunTime > SunController::daylightEnd)
            {
                std::cerr << "Sun time must be between 6 and 18 hours.\n";
                return 2;
            }
            sunTimeSpecified = true;
            startAutomaticSun = false;
        }
        else if (option == "--auto-sun")
            startAutomaticSun = true;
        else if (option == "--static-sun")
            startAutomaticSun = false;
        else if (option == "--shadows")
            startWithShadows = true;
        else if (option == "--no-shadows")
            startWithShadows = false;
        else if (option == "--shadow-debug-factor")
            initialShadowDebugMode = ShadowDebugMode::Factor;
        else if (option == "--shadow-resolution" && argument + 1 < argc)
        {
            try
            {
                shadowResolution = std::stoi(argv[++argument]);
            }
            catch (...)
            {
                std::cerr << "Shadow resolution must be a power of two from 512 to 8192.\n";
                return 2;
            }
            if (shadowResolution < 512 || shadowResolution > 8192 ||
                (shadowResolution & (shadowResolution - 1)) != 0)
            {
                std::cerr << "Shadow resolution must be a power of two from 512 to 8192.\n";
                return 2;
            }
        }
        else if (option == "--lighting-mode" && argument + 1 < argc)
        {
            if (!parseLightingDebugMode(argv[++argument], initialLightingMode))
            {
                std::cerr << "Lighting mode must be normal, diffuse, specular, normals, or unlit.\n";
                return 2;
            }
        }
        else
        {
            std::cerr << "Unknown or incomplete option: " << option << '\n';
            return 2;
        }
    }

    if (geometryValidationOnly)
        return validatePrimitiveFoundation(std::cout) ? 0 : 1;
    if (sceneValidationOnly)
        return validatePyramidLayout(std::cout) ? 0 : 1;
    if (compositeValidationOnly)
        return validateCompositeObjects(std::cout) ? 0 : 1;
    if (hierarchyValidationOnly)
        return validateWorkerHierarchy(std::cout) ? 0 : 1;
    if (animationValidationOnly)
        return validateConstructionAnimation(std::cout) ? 0 : 1;
    if (siteValidationOnly)
        return validateMonumentalSite(std::cout) ? 0 : 1;
    if (industrialValidationOnly)
        return validateIndustrialLandscape(std::cout) ? 0 : 1;
    if (cameraValidationOnly)
        return validateCameraNavigation(std::cout) ? 0 : 1;
    if (enrichmentValidationOnly)
        return validateObjectEnrichment(std::cout) ? 0 : 1;
    if (lightingValidationOnly)
        return validatePhase7Lighting(std::cout) ? 0 : 1;
    if (shadowValidationOnly)
        return validatePhase8Shadows(std::cout) ? 0 : 1;
    if (!validatePrimitiveFoundation(std::cout) || !validatePyramidLayout(std::cout) ||
        !validateCompositeObjects(std::cout) || !validateWorkerHierarchy(std::cout) ||
        !validateConstructionAnimation(std::cout) || !validateMonumentalSite(std::cout) ||
        !validateIndustrialLandscape(std::cout) || !validateCameraNavigation(std::cout) ||
        !validateObjectEnrichment(std::cout) || !validatePhase7Lighting(std::cout) ||
        !validatePhase8Shadows(std::cout))
        return 1;

    glfwSetErrorCallback(glfwErrorCallback);
    if (glfwInit() == GLFW_FALSE)
    {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    if (smokeTest)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(initialWidth, initialHeight,
                                          "Pyramid at Giza - Phase 8 Directional Shadows",
                                          nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Failed to create an OpenGL 3.3 Core window.\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0)
    {
        std::cerr << "Failed to initialize GLAD.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n'
              << "Renderer: " << glGetString(GL_RENDERER) << '\n';

    AppState state;
    setCameraPreset(state, cameraPreset, true);
    glfwSetWindowUserPointer(window, &state);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetScrollCallback(window, scrollCallback);
    if (!smokeTest)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetCursorPosCallback(window, mouseCallback);
        std::cout << "Controls: W/A/S/D/Q/E move, Shift fast, Ctrl precision, mouse looks, "
                     "wheel zoom/orbit radius, 1-9 smooth views, Shift+1-9 instant, "
                     "0 camera reset, O orbit, T transport follow, G guided demo, K camera info, "
                     "U automatic sun, [/] sun time, F1/F2/F3 morning/noon/evening, "
                     "V lighting debug, H shadows, J shadow-factor debug, "
                     "C culling, F wireframe, Space pause, R animation reset, N next state, "
                     "L loop, M animation mode, +/- speed, P debug pose, ESC exits.\n";
    }
    glfwSwapInterval(smokeTest ? 0 : 1);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    state.cullingEnabled = startWithCulling;
    state.wireframeEnabled = startWireframe;
    if (!startWithCulling)
        glDisable(GL_CULL_FACE);
    if (startWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    std::cout << "Raster mode: " << (startWireframe ? "wireframe" : "filled")
              << ", back-face culling: " << (startWithCulling ? "ON" : "OFF") << '\n';

    bool runtimeSucceeded = true;
    try
    {
        StaticGizaScene scene(shadowResolution);
        state.scene = &scene;
        if (initialAnimationTime > 0.0f)
        {
            scene.update(initialAnimationTime);
            std::cout << "Animation initialized at state: " << scene.animationStateName() << '\n';
        }
        // Apply the requested lighting after animation initialization so the sun clock
        // is never implicitly advanced by --animation-time.
        scene.setSunTime(sunTimeSpecified ? initialSunTime : SunController::morningTime);
        scene.setSunAutomatic(startAutomaticSun);
        scene.setLightingDebugMode(initialLightingMode);
        scene.setShadowsEnabled(startWithShadows);
        scene.setShadowDebugMode(initialShadowDebugMode);
        printSunState(scene, "Initial sun");
        std::cout << "Lighting debug mode: " << scene.lightingDebugModeName() << '\n';
        std::cout << "Directional shadows: " << (scene.shadowsEnabled() ? "ON" : "OFF")
                  << ", debug: " << scene.shadowDebugModeName()
                  << ", resolution: " << scene.shadowSettings().resolution << " x "
                  << scene.shadowSettings().resolution << '\n';
        if (initialCameraMode == "orbit")
            state.cameraController.togglePyramidOrbit();
        else if (initialCameraMode == "follow")
            state.cameraController.toggleTransportFollow(scene.transportTarget());
        else if (initialCameraMode == "demo")
            state.cameraController.toggleGuidedDemo();
        if (initialCameraMode != "free")
        {
            state.cameraController.update(1.0f, scene.transportTarget());
            std::cout << "Camera initialized in mode: "
                      << CameraController::modeName(state.cameraController.mode()) << '\n';
        }
        float previousTime = static_cast<float>(glfwGetTime());
        const float smokeStartTime = previousTime;
        int renderedFrames = 0;

        while (glfwWindowShouldClose(window) == GLFW_FALSE)
        {
            const float currentTime = static_cast<float>(glfwGetTime());
            state.deltaTime = currentTime - previousTime;
            previousTime = currentTime;
            if (!smokeTest)
                processInput(window, state);
            scene.update(state.deltaTime);
            state.cameraController.update(state.deltaTime, scene.transportTarget());

            int framebufferWidth = 0;
            int framebufferHeight = 0;
            glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
            if (framebufferWidth == 0 || framebufferHeight == 0)
            {
                glfwPollEvents();
                continue;
            }

            glViewport(0, 0, framebufferWidth, framebufferHeight);
            const glm::vec3 skyColor = scene.skyColor();
            glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const glm::mat4 projection = glm::perspective(
                glm::radians(state.cameraController.fovDegrees()),
                static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
                CameraController::nearPlane, CameraController::farPlane);
            const Camera& activeCamera = state.cameraController.camera();
            scene.render(activeCamera.GetViewMatrix(), projection, activeCamera.Position,
                         framebufferWidth, framebufferHeight);

            ++renderedFrames;
            if (smokeTest && renderedFrames == 3 && !capturePath.empty())
                captureFramebuffer(capturePath, framebufferWidth, framebufferHeight);

            runtimeSucceeded = checkOpenGLErrors() && runtimeSucceeded;
            glfwSwapBuffers(window);
            glfwPollEvents();

            const bool timedSmokeComplete =
                smokeDurationSeconds > 0.0f &&
                currentTime - smokeStartTime >= smokeDurationSeconds;
            const bool shortSmokeComplete =
                smokeDurationSeconds <= 0.0f && renderedFrames >= 3;
            if (smokeTest && (timedSmokeComplete || shortSmokeComplete))
                break;
        }
        if (smokeDurationSeconds > 0.0f)
            std::cout << "Timed OpenGL smoke duration completed: "
                      << smokeDurationSeconds << " seconds.\n";
        state.scene = nullptr;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Runtime failure: " << error.what() << '\n';
        runtimeSucceeded = false;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    if (smokeTest)
        std::cout << (runtimeSucceeded ? "OpenGL smoke test passed.\n"
                                       : "OpenGL smoke test failed.\n");
    return runtimeSucceeded ? 0 : 1;
}
