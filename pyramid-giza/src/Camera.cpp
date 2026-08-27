#include "Camera.h"

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : Front(glm::vec3(0.0f, 0.0f, -1.0f)),
      MovementSpeed(3.0f),
      MouseSensitivity(0.1f)
{
    Position = position;
    WorldUp = up;
    Yaw = yaw;
    Pitch = pitch;
    updateCameraVectors();
}

glm::mat4 Camera::GetViewMatrix() const
{
    // Look from Position, toward Position+Front, oriented by Up.
    return glm::lookAt(Position, Position + Front, Up);
}

void Camera::ProcessKeyboard(CameraMovement direction, float deltaTime)
{
    // Distance to move this frame = speed * time elapsed since last frame.
    // This is what makes movement speed feel the same whether the game
    // is running at 30fps or 300fps.
    float velocity = MovementSpeed * deltaTime;

    if (direction == CameraMovement::FORWARD)
        Position += Front * velocity;
    if (direction == CameraMovement::BACKWARD)
        Position -= Front * velocity;
    if (direction == CameraMovement::LEFT)
        Position -= Right * velocity;
    if (direction == CameraMovement::RIGHT)
        Position += Right * velocity;
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch)
{
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw += xoffset;
    Pitch += yoffset;

    // Prevent the camera from flipping upside down when looking straight
    // up or down (would cause disorienting behavior without this clamp).
    if (constrainPitch)
    {
        if (Pitch > 89.0f)
            Pitch = 89.0f;
        if (Pitch < -89.0f)
            Pitch = -89.0f;
    }

    updateCameraVectors();
}

void Camera::updateCameraVectors()
{
    // Convert yaw/pitch (degrees) into a direction vector using trigonometry.
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);

    // Right = perpendicular to Front and world-up.
    Right = glm::normalize(glm::cross(Front, WorldUp));
    // Up = perpendicular to Right and Front (keeps camera "upright").
    Up = glm::normalize(glm::cross(Right, Front));
}