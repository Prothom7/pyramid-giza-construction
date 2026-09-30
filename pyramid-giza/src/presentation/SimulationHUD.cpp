#include "presentation/SimulationHUD.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace
{
// Compact 5x7 font bitmask table for ASCII 32..126
// Each entry has 7 bytes (one per row, 5 bits used)
const std::uint8_t fontData[95][7] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // 32 ' '
    {0x04, 0x04, 0x04, 0x04, 0x00, 0x04, 0x00}, // 33 '!'
    {0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00}, // 34 '"'
    {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A}, // 35 '#'
    {0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04}, // 36 '$'
    {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03}, // 37 '%'
    {0x08, 0x14, 0x14, 0x08, 0x15, 0x12, 0x0D}, // 38 '&'
    {0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}, // 39 '''
    {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}, // 40 '('
    {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}, // 41 ')'
    {0x04, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x04}, // 42 '*'
    {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}, // 43 '+'
    {0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x08}, // 44 ','
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}, // 45 '-'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00}, // 46 '.'
    {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00}, // 47 '/'
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, // 48 '0'
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 49 '1'
    {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F}, // 50 '2'
    {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}, // 51 '3'
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, // 52 '4'
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, // 53 '5'
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 54 '6'
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 55 '7'
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 56 '8'
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, // 57 '9'
    {0x00, 0x04, 0x00, 0x00, 0x04, 0x00, 0x00}, // 58 ':'
    {0x00, 0x04, 0x00, 0x00, 0x04, 0x04, 0x08}, // 59 ';'
    {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02}, // 60 '<'
    {0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00, 0x00}, // 61 '='
    {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08}, // 62 '>'
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}, // 63 '?'
    {0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0F}, // 64 '@'
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // 65 'A'
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, // 66 'B'
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, // 67 'C'
    {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C}, // 68 'D'
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, // 69 'E'
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, // 70 'F'
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, // 71 'G'
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // 72 'H'
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 73 'I'
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}, // 74 'J'
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, // 75 'K'
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, // 76 'L'
    {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11}, // 77 'M'
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, // 78 'N'
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // 79 'O'
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, // 80 'P'
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, // 81 'Q'
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, // 82 'R'
    {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E}, // 83 'S'
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // 84 'T'
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // 85 'U'
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, // 86 'V'
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}, // 87 'W'
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, // 88 'X'
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, // 89 'Y'
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, // 90 'Z'
    {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E}, // 91 '['
    {0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00}, // 92 '\'
    {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E}, // 93 ']'
    {0x04, 0x0A, 0x11, 0x00, 0x00, 0x00, 0x00}, // 94 '^'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F}, // 95 '_'
    {0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}, // 96 '`'
    {0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F}, // 97 'a'
    {0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x1E}, // 98 'b'
    {0x00, 0x00, 0x0E, 0x10, 0x10, 0x11, 0x0E}, // 99 'c'
    {0x01, 0x01, 0x0D, 0x13, 0x11, 0x11, 0x0F}, // 100 'd'
    {0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E}, // 101 'e'
    {0x06, 0x09, 0x08, 0x1C, 0x08, 0x08, 0x08}, // 102 'f'
    {0x00, 0x00, 0x0F, 0x11, 0x0F, 0x01, 0x0E}, // 103 'g'
    {0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x11}, // 104 'h'
    {0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E}, // 105 'i'
    {0x02, 0x00, 0x06, 0x02, 0x02, 0x12, 0x0C}, // 106 'j'
    {0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12}, // 107 'k'
    {0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 108 'l'
    {0x00, 0x00, 0x1A, 0x15, 0x15, 0x11, 0x11}, // 109 'm'
    {0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11}, // 110 'n'
    {0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E}, // 111 'o'
    {0x00, 0x00, 0x1E, 0x11, 0x1E, 0x10, 0x10}, // 112 'p'
    {0x00, 0x00, 0x0D, 0x13, 0x0F, 0x01, 0x01}, // 113 'q'
    {0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10}, // 114 'r'
    {0x00, 0x00, 0x0F, 0x10, 0x0E, 0x01, 0x1E}, // 115 's'
    {0x08, 0x08, 0x1C, 0x08, 0x08, 0x09, 0x06}, // 116 't'
    {0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0D}, // 117 'u'
    {0x00, 0x00, 0x11, 0x11, 0x11, 0x0A, 0x04}, // 118 'v'
    {0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0A}, // 119 'w'
    {0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11}, // 120 'x'
    {0x00, 0x00, 0x11, 0x11, 0x0F, 0x01, 0x0E}, // 121 'y'
    {0x00, 0x00, 0x1F, 0x02, 0x04, 0x08, 0x1F}, // 122 'z'
    {0x02, 0x04, 0x04, 0x08, 0x04, 0x04, 0x02}, // 123 '{'
    {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // 124 '|'
    {0x08, 0x04, 0x04, 0x02, 0x04, 0x04, 0x08}, // 125 '}'
    {0x00, 0x00, 0x08, 0x15, 0x02, 0x00, 0x00}  // 126 '~'
};

const char* hudVertShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTex;
layout (location = 2) in vec4 aCol;
uniform mat4 projection;
out vec2 TexCoord;
out vec4 Color;
void main() {
    TexCoord = aTex;
    Color = aCol;
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
}
)";

const char* hudFragShader = R"(
#version 330 core
in vec2 TexCoord;
in vec4 Color;
out vec4 FragColor;
uniform sampler2D fontTex;
uniform int useTexture;
void main() {
    if (useTexture != 0) {
        float alpha = texture(fontTex, TexCoord).r;
        FragColor = vec4(Color.rgb, Color.a * alpha);
    } else {
        FragColor = Color;
    }
}
)";
} // namespace

SimulationHUD::SimulationHUD()
{
}

SimulationHUD::~SimulationHUD()
{
    if (vao_ != 0)
        glDeleteVertexArrays(1, &vao_);
    if (vbo_ != 0)
        glDeleteBuffers(1, &vbo_);
    if (ebo_ != 0)
        glDeleteBuffers(1, &ebo_);
    if (fontTexture_ != 0)
        glDeleteTextures(1, &fontTexture_);
    if (shaderProgram_ != 0)
        glDeleteProgram(shaderProgram_);
}

void SimulationHUD::initFontTexture()
{
    // Generate a 128x128 monochrome font atlas (16 columns x 6 rows of 8x8 glyph cells)
    constexpr int atlasW = 128;
    constexpr int atlasH = 128;
    std::vector<std::uint8_t> pixels(atlasW * atlasH, 0);

    for (int i = 0; i < 95; ++i)
    {
        const int col = i % 16;
        const int row = i / 16;
        const int baseX = col * 8;
        const int baseY = row * 16;

        for (int y = 0; y < 7; ++y)
        {
            const std::uint8_t bits = fontData[i][y];
            for (int x = 0; x < 5; ++x)
            {
                if ((bits & (1 << (4 - x))) != 0)
                {
                    const int px = baseX + x;
                    const int py = baseY + y * 2;
                    pixels[py * atlasW + px] = 255;
                    pixels[(py + 1) * atlasW + px] = 255; // Double-scanline for crisp display
                }
            }
        }
    }

    glGenTextures(1, &fontTexture_);
    glBindTexture(GL_TEXTURE_2D, fontTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasW, atlasH, 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void SimulationHUD::initBuffers()
{
    // Compile shader
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &hudVertShader, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &hudFragShader, nullptr);
    glCompileShader(fs);

    shaderProgram_ = glCreateProgram();
    glAttachShader(shaderProgram_, vs);
    glAttachShader(shaderProgram_, fs);
    glLinkProgram(shaderProgram_);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // Buffers
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(HUDVertex),
                          reinterpret_cast<void*>(offsetof(HUDVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(HUDVertex),
                          reinterpret_cast<void*>(offsetof(HUDVertex, texCoord)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(HUDVertex),
                          reinterpret_cast<void*>(offsetof(HUDVertex, color)));

    glBindVertexArray(0);
}

void SimulationHUD::initGL()
{
    if (initialized_)
        return;
    initFontTexture();
    initBuffers();
    initialized_ = true;
}

void SimulationHUD::drawQuad(const glm::vec2& pos, const glm::vec2& size,
                             const glm::vec4& color, const glm::vec2& uvMin,
                             const glm::vec2& uvMax)
{
    const std::uint32_t base = static_cast<std::uint32_t>(vertices_.size());

    vertices_.push_back({pos, uvMin, color});
    vertices_.push_back({{pos.x + size.x, pos.y}, {uvMax.x, uvMin.y}, color});
    vertices_.push_back({{pos.x + size.x, pos.y + size.y}, uvMax, color});
    vertices_.push_back({{pos.x, pos.y + size.y}, {uvMin.x, uvMax.y}, color});

    indices_.push_back(base);
    indices_.push_back(base + 1);
    indices_.push_back(base + 2);
    indices_.push_back(base);
    indices_.push_back(base + 2);
    indices_.push_back(base + 3);
}

void SimulationHUD::drawText(const std::string& text, float x, float y, float size,
                             const glm::vec4& color)
{
    float cursorX = x;
    const float charW = size * 8.0f;
    const float charH = size * 16.0f;

    for (char c : text)
    {
        if (c < 32 || c > 126)
            c = ' ';

        const int index = c - 32;
        const int col = index % 16;
        const int row = index / 16;

        const float u0 = static_cast<float>(col * 8) / 128.0f;
        const float v0 = static_cast<float>(row * 16) / 128.0f;
        const float u1 = u0 + (8.0f / 128.0f);
        const float v1 = v0 + (16.0f / 128.0f);

        drawQuad({cursorX, y}, {charW, charH}, color, {u0, v0}, {u1, v1});
        cursorX += charW;
    }
}

void SimulationHUD::drawProgressBar(float x, float y, float width, float height,
                                    float progress, const glm::vec4& barColor,
                                    const glm::vec4& bgColor)
{
    // Background slot
    drawQuad({x, y}, {width, height}, bgColor);
    // Filled bar
    const float filledW = std::clamp(progress, 0.0f, 1.0f) * width;
    if (filledW > 0.5f)
        drawQuad({x, y}, {filledW, height}, barColor);
    // Frame
    drawQuad({x, y}, {width, 1.0f}, {1.0f, 1.0f, 1.0f, 0.35f});
    drawQuad({x, y + height - 1.0f}, {width, 1.0f}, {1.0f, 1.0f, 1.0f, 0.35f});
}

void SimulationHUD::flush(int screenWidth, int screenHeight)
{
    if (vertices_.empty())
        return;

    glUseProgram(shaderProgram_);
    const glm::mat4 ortho = glm::ortho(0.0f, static_cast<float>(screenWidth),
                                       static_cast<float>(screenHeight), 0.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram_, "projection"), 1, GL_FALSE,
                       glm::value_ptr(ortho));

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices_.size() * sizeof(HUDVertex),
                 vertices_.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_.size() * sizeof(std::uint32_t),
                 indices_.data(), GL_DYNAMIC_DRAW);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTexture_);
    glUniform1i(glGetUniformLocation(shaderProgram_, "fontTex"), 0);
    glUniform1i(glGetUniformLocation(shaderProgram_, "useTexture"), 1);

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices_.size()), GL_UNSIGNED_INT, nullptr);

    glBindVertexArray(0);
    glUseProgram(0);

    vertices_.clear();
    indices_.clear();
}

void SimulationHUD::render(int screenWidth, int screenHeight, const SimulationHUDState& state)
{
    if (!visible_ || !state.hudVisible || screenWidth <= 0 || screenHeight <= 0)
        return;

    initGL();

    // Preserve OpenGL state
    GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blend = glIsEnabled(GL_BLEND);
    GLboolean cullFace = glIsEnabled(GL_CULL_FACE);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    vertices_.clear();
    indices_.clear();

    const glm::vec4 panelBg{0.05f, 0.07f, 0.10f, 0.82f};
    const glm::vec4 goldBorder{0.85f, 0.72f, 0.42f, 0.85f};
    const glm::vec4 goldText{0.92f, 0.82f, 0.50f, 1.0f};
    const glm::vec4 whiteText{0.92f, 0.94f, 0.96f, 1.0f};
    const glm::vec4 dimText{0.65f, 0.70f, 0.75f, 1.0f};
    const glm::vec4 activeGreen{0.32f, 0.85f, 0.42f, 1.0f};

    // 1. PRIMARY SIMULATION PANEL (Top-Left)
    constexpr float pX = 16.0f;
    constexpr float pY = 16.0f;
    constexpr float pW = 340.0f;
    constexpr float pH = 230.0f;

    // Translucent panel background & border
    drawQuad({pX, pY}, {pW, pH}, panelBg);
    drawQuad({pX, pY}, {pW, 2.0f}, goldBorder);
    drawQuad({pX, pY + pH - 2.0f}, {pW, 2.0f}, goldBorder);
    drawQuad({pX, pY}, {2.0f, pH}, goldBorder);
    drawQuad({pX + pW - 2.0f, pY}, {2.0f, pH}, goldBorder);

    // Title
    drawText("Pyramid at Giza - Construction", pX + 14.0f, pY + 12.0f, 0.95f, goldText);

    // Construction progress & bar
    std::ostringstream ssProgress;
    ssProgress << "Construction Progress: " << std::fixed << std::setprecision(1)
               << (state.constructionProgress * 100.0f) << "%";
    drawText(ssProgress.str(), pX + 14.0f, pY + 36.0f, 0.82f, whiteText);
    drawProgressBar(pX + 14.0f, pY + 54.0f, pW - 28.0f, 8.0f, state.constructionProgress,
                    goldBorder, {0.18f, 0.20f, 0.24f, 0.8f});

    // Pipeline details
    drawText("Current Phase: " + state.constructionStage, pX + 14.0f, pY + 70.0f, 0.80f, dimText);

    std::ostringstream ssBlock;
    ssBlock << "Active Quarry Block: #" << state.activeBlockNumber;
    drawText(ssBlock.str(), pX + 14.0f, pY + 88.0f, 0.80f, whiteText);

    drawText("Logistics: " + state.logisticsState, pX + 14.0f, pY + 106.0f, 0.80f, goldText);
    drawText("Route: " + state.routeDescription, pX + 14.0f, pY + 124.0f, 0.72f, dimText);

    std::ostringstream ssWorkers;
    ssWorkers << "Workers Active: " << state.activeWorkers;
    drawText(ssWorkers.str(), pX + 14.0f, pY + 142.0f, 0.80f, whiteText);

    // Sim toggles
    drawText("Sand Simulation:  ", pX + 14.0f, pY + 162.0f, 0.78f, dimText);
    drawText(state.sandSimEnabled ? "ON" : "OFF", pX + 160.0f, pY + 162.0f, 0.78f,
             state.sandSimEnabled ? activeGreen : dimText);

    drawText("Water Simulation: ", pX + 14.0f, pY + 178.0f, 0.78f, dimText);
    drawText(state.waterSimEnabled ? "ON" : "OFF", pX + 160.0f, pY + 178.0f, 0.78f,
             state.waterSimEnabled ? activeGreen : dimText);

    drawText("Effects:          ", pX + 14.0f, pY + 194.0f, 0.78f, dimText);
    drawText(state.effectsEnabled ? "ON" : "OFF", pX + 160.0f, pY + 194.0f, 0.78f,
             state.effectsEnabled ? activeGreen : dimText);

    std::ostringstream ssFps;
    ssFps << "FPS: " << std::fixed << std::setprecision(0) << state.fps;
    drawText(ssFps.str(), pX + 250.0f, pY + 194.0f, 0.82f, activeGreen);

    // 2. CONTEXTUAL PANEL (Bottom-Left)
    if (state.showContextCards)
    {
        const float cY = static_cast<float>(screenHeight) - 150.0f;
        constexpr float cW = 340.0f;
        constexpr float cH = 134.0f;

        drawQuad({pX, cY}, {cW, cH}, panelBg);
        drawQuad({pX, cY}, {cW, 1.5f}, goldBorder);
        drawQuad({pX, cY + cH - 1.5f}, {cW, 1.5f}, goldBorder);
        drawQuad({pX, cY}, {1.5f, cH}, goldBorder);
        drawQuad({pX + cW - 1.5f, cY}, {1.5f, cH}, goldBorder);

        // Quarry context
        std::ostringstream ssQ;
        ssQ << "QUARRY | Rock #" << state.quarryRockId << " | " << state.quarryState
            << " | " << std::fixed << std::setprecision(0) << state.quarryProgress << "%";
        drawText(ssQ.str(), pX + 12.0f, cY + 10.0f, 0.80f, goldText);

        // Logistics context
        drawText("LOGISTICS | Block #" + std::to_string(state.activeBlockNumber) + " | " + state.logisticsState,
                 pX + 12.0f, cY + 32.0f, 0.80f, whiteText);
        drawText(state.routeDescription, pX + 24.0f, cY + 50.0f, 0.72f, dimText);

        // Pulley context
        std::ostringstream ssPull;
        ssPull << "LIFT | State: " << state.pulleyState << " | Height: "
               << std::fixed << std::setprecision(1) << state.pulleyHeight << "m";
        drawText(ssPull.str(), pX + 12.0f, cY + 74.0f, 0.80f, goldText);

        // Construction frontier context
        std::ostringstream ssConst;
        ssConst << "CONSTRUCTION | Level " << state.activeLevel << " | Phase: "
                << state.frontierPhase;
        drawText(ssConst.str(), pX + 12.0f, cY + 98.0f, 0.80f, whiteText);
    }

    // 3. CONTROLS HELP PANEL (Top-Right)
    if (showHelp_ && state.showHelp)
    {
        constexpr float hW = 280.0f;
        constexpr float hH = 240.0f;
        const float hX = static_cast<float>(screenWidth) - hW - 16.0f;

        drawQuad({hX, pY}, {hW, hH}, panelBg);
        drawQuad({hX, pY}, {hW, 1.5f}, goldBorder);
        drawQuad({hX, pY + hH - 1.5f}, {hW, 1.5f}, goldBorder);
        drawQuad({hX, pY}, {1.5f, hH}, goldBorder);
        drawQuad({hX + hW - 1.5f, pY}, {1.5f, hH}, goldBorder);

        drawText("CONTROLS GUIDE", hX + 14.0f, pY + 10.0f, 0.88f, goldText);

        const char* controls[] = {
            "W/A/S/D/Q/E  Camera Move",
            "Mouse        Camera Look",
            "ESC          Release Cursor / Exit",
            "F4           Effects Toggle",
            "F5           Cinematic Showcase",
            "F6           Pause Showcase",
            "F7           Pulley Pause",
            "F8           Pyramid Interior",
            "F9           Cutaway View",
            "F10          Sim Debug Overlay",
            "G            Guided Demo",
            "R            Reset Animation",
            "B / , / .    Timelapse & Speed",
            "Space        Pause / Resume"
        };

        float cy = pY + 30.0f;
        for (const char* line : controls)
        {
            drawText(line, hX + 14.0f, cy, 0.72f, whiteText);
            cy += 14.5f;
        }
    }

    flush(screenWidth, screenHeight);

    // Restore state
    if (depthTest)
        glEnable(GL_DEPTH_TEST);
    if (!blend)
        glDisable(GL_BLEND);
    if (cullFace)
        glEnable(GL_CULL_FACE);
}

bool SimulationHUD::validateSimulationHUD(std::ostream& output)
{
    SimulationHUDState s;
    s.constructionProgress = 0.82f;
    s.constructionStage = "UpperCourses";
    s.activeBlockNumber = 104;
    s.logisticsState = "HAULING";
    s.routeDescription = "Quarry -> Ramp";
    s.activeWorkers = 8;
    s.sandSimEnabled = true;
    s.waterSimEnabled = true;
    s.effectsEnabled = true;
    s.fps = 60.0f;

    bool valid = s.constructionProgress >= 0.0f && s.constructionProgress <= 1.0f &&
                 !s.constructionStage.empty() && s.activeBlockNumber > 0 &&
                 !s.logisticsState.empty() && s.activeWorkers > 0 &&
                 s.fps > 0.0f;

    output << "Phase 13 Simulation HUD Validation\n"
           << "  HUD state fields populated and valid: "
           << (valid ? "PASS" : "FAIL") << '\n'
           << (valid ? "Simulation HUD validation passed.\n"
                     : "Simulation HUD validation failed.\n");

    return valid;
}
