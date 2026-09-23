#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Camera.h"
#include "graphics/GeometryValidation.h"
#include "objects/CompositeValidation.h"
#include "objects/WorkerHierarchyValidation.h"
#include "scene/PyramidLayout.h"
#include "scene/StaticGizaScene.h"

namespace
{
constexpr int initialWidth = 1280;
constexpr int initialHeight = 720;

struct AppState
{
    Camera camera{{28.0f, 18.0f, 38.0f}, {0.0f, 1.0f, 0.0f}, -128.0f, -18.0f};
    float lastMouseX = initialWidth * 0.5f;
    float lastMouseY = initialHeight * 0.5f;
    float deltaTime = 0.0f;
    bool firstMouse = true;
    bool cullingEnabled = true;
    bool wireframeEnabled = false;
    StaticGizaScene* scene = nullptr;
};

void setCameraPreset(AppState& state, int preset)
{
    switch (preset)
    {
    case 2:
        state.camera.SetPose({0.0f, 8.5f, 16.0f}, -90.0f, -13.0f);
        break;
    case 3:
        state.camera.SetPose({-8.0f, 7.0f, 18.0f}, -124.0f, -18.0f);
        break;
    case 4:
        state.camera.SetPose({8.0f, 6.0f, 20.0f}, -115.0f, -11.0f);
        break;
    case 5:
        state.camera.SetPose({15.5f, 5.0f, 14.5f}, -132.0f, -13.0f);
        break;
    case 1:
    default:
        state.camera.SetPose({28.0f, 18.0f, 38.0f}, -128.0f, -18.0f);
        break;
    }
    state.firstMouse = true;
    std::cout << "Camera preset " << preset << " selected.\n";
}

void glfwErrorCallback(int code, const char* description)
{
    std::cerr << "GLFW error " << code << ": " << description << '\n';
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

    state->camera.ProcessMouseMovement(x - state->lastMouseX, state->lastMouseY - y);
    state->lastMouseX = x;
    state->lastMouseY = y;
}

void keyCallback(GLFWwindow* window, int key, int, int action, int)
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
        state->scene->toggleArticulationPreview();
        std::cout << "Articulation preview: "
                  << (state->scene->articulationPreviewEnabled() ? "RUNNING" : "PAUSED") << '\n';
    }
    else if (key == GLFW_KEY_P && state->scene != nullptr)
    {
        state->scene->cycleDemoPose();
        std::cout << "Demo worker pose: " << state->scene->demoPoseName() << '\n';
    }
    else if (key == GLFW_KEY_R && state->scene != nullptr)
    {
        state->scene->resetArticulationPreview();
        std::cout << "Demo worker reset to Standing.\n";
    }
    else if (key >= GLFW_KEY_1 && key <= GLFW_KEY_5)
        setCameraPreset(*state, key - GLFW_KEY_0);
}

void processInput(GLFWwindow* window, AppState& state)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        state.camera.ProcessKeyboard(CameraMovement::FORWARD, state.deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        state.camera.ProcessKeyboard(CameraMovement::BACKWARD, state.deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        state.camera.ProcessKeyboard(CameraMovement::LEFT, state.deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        state.camera.ProcessKeyboard(CameraMovement::RIGHT, state.deltaTime);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        state.camera.ProcessKeyboard(CameraMovement::DOWN, state.deltaTime);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        state.camera.ProcessKeyboard(CameraMovement::UP, state.deltaTime);
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
    bool smokeTest = false;
    bool startWireframe = false;
    bool startWithCulling = true;
    int cameraPreset = 1;
    std::string capturePath;
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
        else if (option == "--smoke-test")
            smokeTest = true;
        else if (option == "--wireframe")
            startWireframe = true;
        else if (option == "--no-cull")
            startWithCulling = false;
        else if (option == "--preset" && argument + 1 < argc)
        {
            const std::string value = argv[++argument];
            if (value.size() != 1 || value[0] < '1' || value[0] > '5')
            {
                std::cerr << "Camera preset must be 1, 2, 3, 4, or 5.\n";
                return 2;
            }
            cameraPreset = value[0] - '0';
        }
        else if (option == "--capture" && argument + 1 < argc)
            capturePath = argv[++argument];
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
    if (!validatePrimitiveFoundation(std::cout) || !validatePyramidLayout(std::cout) ||
        !validateCompositeObjects(std::cout) || !validateWorkerHierarchy(std::cout))
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
                                          "Pyramid at Giza - Phase 4 Hierarchical Workers",
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
    setCameraPreset(state, cameraPreset);
    glfwSetWindowUserPointer(window, &state);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    if (!smokeTest)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetCursorPosCallback(window, mouseCallback);
        std::cout << "Controls: W/A/S/D move, Q/E move vertically, mouse looks, "
                     "1-5 views, C culling, F wireframe, Space pauses articulation, "
                     "P cycles poses, R resets, ESC exits.\n";
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
        StaticGizaScene scene;
        state.scene = &scene;
        float previousTime = static_cast<float>(glfwGetTime());
        int renderedFrames = 0;

        while (glfwWindowShouldClose(window) == GLFW_FALSE)
        {
            const float currentTime = static_cast<float>(glfwGetTime());
            state.deltaTime = currentTime - previousTime;
            previousTime = currentTime;
            if (!smokeTest)
                processInput(window, state);
            scene.update(state.deltaTime);

            int framebufferWidth = 0;
            int framebufferHeight = 0;
            glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
            if (framebufferWidth == 0 || framebufferHeight == 0)
            {
                glfwPollEvents();
                continue;
            }

            glViewport(0, 0, framebufferWidth, framebufferHeight);
            glClearColor(0.47f, 0.68f, 0.86f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const glm::mat4 projection = glm::perspective(
                glm::radians(45.0f),
                static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
                0.1f, 100.0f);
            scene.render(state.camera.GetViewMatrix(), projection, state.camera.Position);

            ++renderedFrames;
            if (smokeTest && renderedFrames == 3 && !capturePath.empty())
                captureFramebuffer(capturePath, framebufferWidth, framebufferHeight);

            runtimeSucceeded = checkOpenGLErrors() && runtimeSucceeded;
            glfwSwapBuffers(window);
            glfwPollEvents();

            if (smokeTest && renderedFrames >= 3)
                break;
        }
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
