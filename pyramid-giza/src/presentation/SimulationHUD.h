#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct SimulationHUDState
{
    float constructionProgress = 0.75f;
    std::string constructionStage = "UpperCourses";
    int activeBlockNumber = 101;
    std::string logisticsState = "HAULING";
    std::string routeDescription = "Quarry -> Ramp";
    int activeWorkers = 8;
    bool sandSimEnabled = true;
    bool waterSimEnabled = true;
    bool effectsEnabled = true;
    float fps = 60.0f;

    // Quarry card
    int quarryRockId = 101;
    std::string quarryState = "Cutting";
    int quarryWorkers = 4;
    float quarryProgress = 45.0f;

    // Pulley card
    std::string pulleyState = "Lift";
    float pulleyHeight = 4.2f;

    // Construction card
    int activeLevel = 18;
    int levelBlockCount = 42;
    std::string frontierPhase = "Align";

    // Panel states
    bool hudVisible = true;
    bool showHelp = true;
    bool showContextCards = true;
    bool debugOverlay = false;
};

class SimulationHUD
{
public:
    SimulationHUD();
    ~SimulationHUD();

    void initGL();
    void render(int screenWidth, int screenHeight, const SimulationHUDState& state);
    void toggleVisible() { visible_ = !visible_; }
    void toggleHelp() { showHelp_ = !showHelp_; }
    void toggleDebug() { debugOverlay_ = !debugOverlay_; }

    bool visible() const { return visible_; }
    bool showHelp() const { return showHelp_; }
    bool debugOverlay() const { return debugOverlay_; }

    static bool validateSimulationHUD(std::ostream& output);

private:
    struct HUDVertex
    {
        glm::vec2 position;
        glm::vec2 texCoord;
        glm::vec4 color;
    };

    void initFontTexture();
    void initBuffers();
    void drawQuad(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color,
                  const glm::vec2& uvMin = {0.0f, 0.0f}, const glm::vec2& uvMax = {0.0f, 0.0f});
    void drawText(const std::string& text, float x, float y, float size,
                  const glm::vec4& color = {1.0f, 1.0f, 1.0f, 1.0f});
    void drawProgressBar(float x, float y, float width, float height, float progress,
                         const glm::vec4& barColor, const glm::vec4& bgColor);
    void flush(int screenWidth, int screenHeight);

    bool initialized_ = false;
    bool visible_ = true;
    bool showHelp_ = true;
    bool debugOverlay_ = false;

    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
    unsigned int fontTexture_ = 0;
    unsigned int shaderProgram_ = 0;

    std::vector<HUDVertex> vertices_;
    std::vector<std::uint32_t> indices_;
};
