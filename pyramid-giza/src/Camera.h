#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Possible discrete movement directions the camera can be told to move in,
// decoupled from actual key codes so main.cpp can map any key it wants
// to these actions.
enum class CameraMovement
{
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

class Camera
{
public:
    // Camera state
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    // Euler angles (in degrees)
    float Yaw;
    float Pitch;

    // Tuning parameters
    float MovementSpeed;
    float MouseSensitivity;

    Camera(glm::vec3 position = glm::vec3(0.0f, 2.0f, 6.0f),
           glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
           float yaw = -90.0f,
           float pitch = 0.0f);

    // Returns the current View matrix, built from Position/Front/Up.
    glm::mat4 GetViewMatrix() const;

    // Called every frame from main.cpp when a movement key is held.
    // deltaTime keeps movement speed consistent regardless of frame rate.
    void ProcessKeyboard(CameraMovement direction, float deltaTime);

    // Called every frame from main.cpp with raw mouse movement deltas.
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

private:
    // Recalculates Front, Right, and Up from the current Yaw/Pitch.
    // Must be called any time Yaw or Pitch changes.
    void updateCameraVectors();
};