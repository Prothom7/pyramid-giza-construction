#include "camera/CursorController.h"

#include <cmath>
#include <iostream>

#include <GLFW/glfw3.h>

CursorController::CursorController()
{
    reset();
}

void CursorController::reset()
{
    cursorInsideWindow_ = true;
    windowFocused_ = true;
    mouseControlEnabled_ = true;
    cursorCaptured_ = true;
    lastX_ = 0.0f;
    lastY_ = 0.0f;
    firstMouse_ = true;
}

void CursorController::setCaptured(bool captured, GLFWwindow* window)
{
    cursorCaptured_ = captured;
    if (window != nullptr)
    {
        glfwSetInputMode(window, GLFW_CURSOR,
                         captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
    mouseControlEnabled_ = captured && cursorInsideWindow_ && windowFocused_;
    firstMouse_ = true;
}

void CursorController::onCursorEnter(bool entered, GLFWwindow* window)
{
    cursorInsideWindow_ = entered;
    if (!entered)
    {
        mouseControlEnabled_ = false;
        firstMouse_ = true;
        if (window != nullptr && cursorCaptured_)
        {
            // Release cursor capture so OS mouse can freely move
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            cursorCaptured_ = false;
        }
    }
    else
    {
        firstMouse_ = true;
        if (windowFocused_ && cursorCaptured_)
            mouseControlEnabled_ = true;
    }
}

void CursorController::onWindowFocus(bool focused, GLFWwindow* window)
{
    windowFocused_ = focused;
    if (!focused)
    {
        mouseControlEnabled_ = false;
        firstMouse_ = true;
        if (window != nullptr && cursorCaptured_)
        {
            // Restore normal OS cursor when window loses focus
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            cursorCaptured_ = false;
        }
    }
    else
    {
        firstMouse_ = true;
        if (cursorInsideWindow_ && cursorCaptured_)
            mouseControlEnabled_ = true;
    }
}

bool CursorController::handleCursorPos(double xpos, double ypos, float& deltaX, float& deltaY)
{
    deltaX = 0.0f;
    deltaY = 0.0f;

    const float x = static_cast<float>(xpos);
    const float y = static_cast<float>(ypos);

    // Camera manipulation strictly requires window focus and cursor inside
    if (!mouseControlEnabled_ || !cursorInsideWindow_ || !windowFocused_)
    {
        lastX_ = x;
        lastY_ = y;
        firstMouse_ = true;
        return false;
    }

    if (firstMouse_)
    {
        lastX_ = x;
        lastY_ = y;
        firstMouse_ = false;
        return false;
    }

    deltaX = x - lastX_;
    deltaY = lastY_ - y;
    lastX_ = x;
    lastY_ = y;

    return true;
}

void CursorController::onMouseButton(int button, int action, GLFWwindow* window)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        if (cursorInsideWindow_ && windowFocused_ && !cursorCaptured_)
        {
            // Re-engage mouse look capture on click inside window
            setCaptured(true, window);
        }
    }
}

bool CursorController::handleKey(int key, int action, GLFWwindow* window)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        if (cursorCaptured_)
        {
            // First ESC releases cursor capture to OS mode
            setCaptured(false, window);
            return true; // consumed
        }
        // If already released, allow ESC to close application
        return false;
    }
    return false;
}

bool CursorController::validateCursorController(std::ostream& output)
{
    CursorController ctrl;

    // 1. Initial state: inside & focused -> enabled
    bool t1 = ctrl.mouseControlEnabled() && ctrl.cursorInsideWindow() && ctrl.windowFocused();
    float dx = 0.0f, dy = 0.0f;
    ctrl.handleCursorPos(100.0, 100.0, dx, dy);
    ctrl.handleCursorPos(110.0, 105.0, dx, dy);
    t1 = t1 && (dx == 10.0f) && (dy == -5.0f);

    // 2. Cursor leaves window -> camera input disabled and ignored
    ctrl.onCursorEnter(false);
    bool t2 = !ctrl.cursorInsideWindow() && !ctrl.mouseControlEnabled();
    const bool movedOutside = ctrl.handleCursorPos(200.0, 200.0, dx, dy);
    t2 = t2 && !movedOutside && (dx == 0.0f) && (dy == 0.0f);

    // 3. Cursor returns inside -> restores
    ctrl.onCursorEnter(true);
    ctrl.setCaptured(true);
    bool t3 = ctrl.cursorInsideWindow() && ctrl.mouseControlEnabled();

    // 4. Focus lost -> camera input disabled
    ctrl.onWindowFocus(false);
    bool t4 = !ctrl.windowFocused() && !ctrl.mouseControlEnabled();
    const bool movedUnfocused = ctrl.handleCursorPos(300.0, 300.0, dx, dy);
    t4 = t4 && !movedUnfocused;

    // 5. Focus restored -> correct behavior resumes
    ctrl.onWindowFocus(true);
    ctrl.setCaptured(true);
    bool t5 = ctrl.windowFocused() && ctrl.mouseControlEnabled();
    ctrl.handleCursorPos(400.0, 400.0, dx, dy);
    ctrl.handleCursorPos(405.0, 395.0, dx, dy);
    t5 = t5 && (dx == 5.0f) && (dy == 5.0f);

    // 6. ESC key releases capture
    const bool consumed = ctrl.handleKey(GLFW_KEY_ESCAPE, GLFW_PRESS);
    bool t6 = consumed && !ctrl.cursorCaptured() && !ctrl.mouseControlEnabled();

    const bool valid = t1 && t2 && t3 && t4 && t5 && t6;

    output << "Phase 13 Cursor & Input Behavior Validation\n"
           << "  cursor inside + focused enables camera control: "
           << (t1 ? "PASS" : "FAIL") << '\n'
           << "  cursor outside window disables camera look (ignored): "
           << (t2 ? "PASS" : "FAIL") << '\n'
           << "  window focus lost releases cursor & disables input: "
           << (t4 ? "PASS" : "FAIL") << '\n'
           << "  window focus restored resumes camera look: "
           << (t5 ? "PASS" : "FAIL") << '\n'
           << "  ESC releases cursor capture cleanly to OS: "
           << (t6 ? "PASS" : "FAIL") << '\n'
           << (valid ? "Cursor controller validation passed.\n"
                     : "Cursor controller validation failed.\n");

    return valid;
}
