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
#include "graphics/ShowcaseScene.h"

namespace
{
constexpr int initialWidth = 1280;
constexpr int initialHeight = 720;

struct AppState
{
    Camera camera{{0.0f, 0.25f, 10.5f}, {0.0f, 1.0f, 0.0f}, -90.0f, 0.0f};
    float lastMouseX = initialWidth * 0.5f;
    float lastMouseY = initialHeight * 0.5f;
    float deltaTime = 0.0f;
    bool firstMouse = true;
    bool cullingEnabled = true;
    bool wireframeEnabled = false;
};

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
    bool validationOnly = false;
    bool smokeTest = false;
    std::string capturePath;
    for (int argument = 1; argument < argc; ++argument)
    {
        const std::string option = argv[argument];
        if (option == "--validate-geometry")
            validationOnly = true;
        else if (option == "--smoke-test")
            smokeTest = true;
        else if (option == "--capture" && argument + 1 < argc)
            capturePath = argv[++argument];
        else
        {
            std::cerr << "Unknown or incomplete option: " << option << '\n';
            return 2;
        }
    }

    if (!validatePrimitiveFoundation(std::cout))
        return 1;
    if (validationOnly)
        return 0;

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
                                          "Pyramid at Giza - Phase 1 Primitive Foundation",
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
    glfwSetWindowUserPointer(window, &state);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    if (!smokeTest)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetCursorPosCallback(window, mouseCallback);
        std::cout << "Controls: W/A/S/D move, mouse looks, C toggles culling, "
                     "F toggles wireframe, ESC exits.\n";
    }
    glfwSwapInterval(smokeTest ? 0 : 1);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    bool runtimeSucceeded = true;
    try
    {
        ShowcaseScene scene;
        float previousTime = static_cast<float>(glfwGetTime());
        int renderedFrames = 0;

        while (glfwWindowShouldClose(window) == GLFW_FALSE)
        {
            const float currentTime = static_cast<float>(glfwGetTime());
            state.deltaTime = currentTime - previousTime;
            previousTime = currentTime;
            if (!smokeTest)
                processInput(window, state);

            int framebufferWidth = 0;
            int framebufferHeight = 0;
            glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
            if (framebufferWidth == 0 || framebufferHeight == 0)
            {
                glfwPollEvents();
                continue;
            }

            glViewport(0, 0, framebufferWidth, framebufferHeight);
            glClearColor(0.055f, 0.075f, 0.11f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const glm::mat4 projection = glm::perspective(
                glm::radians(45.0f),
                static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
                0.1f, 100.0f);
            scene.render(state.camera.GetViewMatrix(), projection, state.camera.Position,
                         smokeTest ? 0.0f : currentTime);

            ++renderedFrames;
            if (smokeTest && renderedFrames == 3 && !capturePath.empty())
                captureFramebuffer(capturePath, framebufferWidth, framebufferHeight);

            runtimeSucceeded = checkOpenGLErrors() && runtimeSucceeded;
            glfwSwapBuffers(window);
            glfwPollEvents();

            if (smokeTest && renderedFrames >= 3)
                break;
        }
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
