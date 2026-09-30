#pragma once

#include <iosfwd>

struct GLFWwindow;

class CursorController
{
public:
    CursorController();

    void onCursorEnter(bool entered, GLFWwindow* window = nullptr);
    void onWindowFocus(bool focused, GLFWwindow* window = nullptr);
    bool handleCursorPos(double xpos, double ypos, float& deltaX, float& deltaY);
    void onMouseButton(int button, int action, GLFWwindow* window = nullptr);
    bool handleKey(int key, int action, GLFWwindow* window = nullptr);

    bool cursorInsideWindow() const { return cursorInsideWindow_; }
    bool windowFocused() const { return windowFocused_; }
    bool mouseControlEnabled() const { return mouseControlEnabled_; }
    bool cursorCaptured() const { return cursorCaptured_; }

    void setCaptured(bool captured, GLFWwindow* window = nullptr);
    void reset();

    static bool validateCursorController(std::ostream& output);

private:
    bool cursorInsideWindow_ = true;
    bool windowFocused_ = true;
    bool mouseControlEnabled_ = true;
    bool cursorCaptured_ = true;
    float lastX_ = 0.0f;
    float lastY_ = 0.0f;
    bool firstMouse_ = true;
};
