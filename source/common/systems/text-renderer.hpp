#pragma once

#include <map>
#include <string>
#include <application.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <systems/audio-system.hpp>
#include <glad/gl.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <shader/shader.hpp>
#include <stdexcept>
#include <GLFW/glfw3.h>
#include <queue>
#include <tuple>

namespace our {

struct Character {
    GLuint textureID;
    glm::ivec2 size;
    glm::ivec2 bearing;
    GLuint advance;
};

class TextRenderer {
    std::map<std::string, std::map<char, Character>> fonts;
    GLuint VAO, VBO;
    ShaderProgram* textShader;
    glm::mat4 projection;
    float screenWidth, screenHeight;

    float fadeStartTime = -1.0f;
    float fadeDuration = 1.0f;
    float visibleDuration = 1.0f;
    float fadeOutDuration = 0.25f;
    std::string fadeText = "";
    std::string fadeFont = "";
    float fadeScale = 1.0f;
    glm::vec3 fadeColor = glm::vec3(1.0f);
    bool active = false;

    struct TextQueueItem {
        std::string text;
        std::string font;
        float fadeIn;
        float visible;
        float fadeOut;
        float scale;
        glm::vec3 color;
    };
    std::queue<TextQueueItem> textQueue;
    bool isQueueActive = false;

public:
    static TextRenderer& getInstance() {
        static TextRenderer instance;
        return instance;
    }

    void loadFont(const std::string& fontName, const std::string& fontPath, unsigned int fontSize = 2048) {
        FT_Library ft;
        if (FT_Init_FreeType(&ft)) throw std::runtime_error("Could not init FreeType");

        FT_Face face;
        if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
            FT_Done_FreeType(ft);
            throw std::runtime_error("Failed to load font: " + fontPath);
        }

        FT_Set_Pixel_Sizes(face, 0, fontSize);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        std::map<char, Character> characters;
        for (unsigned char c = 0; c < 128; c++) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) continue;

            GLuint tex;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, face->glyph->bitmap.width, face->glyph->bitmap.rows, 
                          0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            characters[c] = {
                tex,
                glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                static_cast<GLuint>(face->glyph->advance.x)
            };
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        FT_Done_Face(face);
        FT_Done_FreeType(ft);

        fonts[fontName] = characters;
    }

    void initialize(float w, float h) {
        textShader = new our::ShaderProgram();
        textShader->attach("assets/shaders/text.vert", GL_VERTEX_SHADER);
        textShader->attach("assets/shaders/text.frag", GL_FRAGMENT_SHADER);
        textShader->link();
        textShader->use();
        screenWidth = w;
        screenHeight = h;
        projection = glm::ortho(0.0f, screenWidth, 0.0f, screenHeight);
        textShader->use();
        textShader->set("projection", projection);
        textShader->set("text", 0);

        loadFont("game", "assets/fonts/font2.ttf"); 
        loadFont("window", "assets/fonts/font3.ttf");
        loadFont("default", "assets/fonts/font4.ttf"); 

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void clearTextQueue() {
        while (!textQueue.empty()) {
            textQueue.pop();
        }
        isQueueActive = false;
    }

    glm::vec2 getTextSize(const std::string& text, const std::string& fontName, float scale) {
        if (fonts.find(fontName) == fonts.end()) {
            throw std::runtime_error("Font not loaded: " + fontName);
        }

        const auto& characters = fonts[fontName];
        float width = 0.0f, height = 0.0f;
        for (char c : text) {
            Character ch = characters.at(c);
            width += (ch.advance >> 6) * scale;
            float h = ch.size.y * scale;
            if (h > height) height = h;
        }
        return glm::vec2(width, height);
    }

    void renderTextWithBackground(const std::string& text, const std::string& fontName, float x, float y, float scale, float scaleFirst,
        glm::vec4 textColor, glm::vec4 backgroundColor, bool boxAroundFirstLetter = false) {
        if (fonts.find(fontName) == fonts.end()) {
            throw std::runtime_error("Font not loaded: " + fontName);
        }

        glm::vec2 textSize = getTextSize(text, fontName, scale);
    
        float xpadding = 8.0f;
        float ypadding = 10.0f;
        float boxWidth = textSize.x + xpadding * 2.0f;
        float boxHeight = textSize.y + ypadding * 2.0f;
        float skewAmount = -10.0f;
    
        float boxX = x - boxWidth / 2.0f;
        float boxY = screenHeight - y - boxHeight / 2.0f;
    
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
    
        textShader->use();
        textShader->set("textColor", backgroundColor);
        textShader->set("projection", projection);
        textShader->set("useTexture", false);
    
        float vertices[6][4] = {
            { boxX - skewAmount, boxY + boxHeight,   0.0f, 1.0f }, 
            { boxX,              boxY,               0.0f, 0.0f },  
            { boxX + boxWidth,   boxY,               1.0f, 0.0f }, 
        
            { boxX - skewAmount, boxY + boxHeight,   0.0f, 1.0f }, 
            { boxX + boxWidth,   boxY,               1.0f, 0.0f },  
            { boxX + boxWidth - skewAmount, boxY + boxHeight, 1.0f, 1.0f }  
        };
    
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    
        float textStartX = x - textSize.x / 2.0f - skewAmount / 2.0f;
        float textStartY = screenHeight - y - textSize.y / 2.0f;
    
        if (boxAroundFirstLetter && !text.empty()) {
            float firstPadding = 10.0f;
            std::string firstLetter = text.substr(0, 1);
            std::string restText = text.substr(1);
            glm::vec2 firstLetterSize = getTextSize(firstLetter, fontName, scaleFirst);
    
            float firstBoxX = textStartX - firstPadding / 2.0f + 4.0f;
            float firstBoxY = textStartY - firstPadding / 2.0f + 2.0f;
            float firstBoxW = firstLetterSize.x + firstPadding;
            float firstBoxH = firstLetterSize.y + firstPadding;
    
            float firstVertices[6][4] = {
                { firstBoxX,               firstBoxY + firstBoxH, 0.0f, 1.0f },
                { firstBoxX,               firstBoxY,             0.0f, 0.0f },
                { firstBoxX + firstBoxW,   firstBoxY,             1.0f, 0.0f },
    
                { firstBoxX,               firstBoxY + firstBoxH, 0.0f, 1.0f },
                { firstBoxX + firstBoxW,   firstBoxY,             1.0f, 0.0f },
                { firstBoxX + firstBoxW,   firstBoxY + firstBoxH, 1.0f, 1.0f }
            };
    
            textShader->set("textColor", glm::vec4(1.0f));
            glm::vec4 firstLetterColor = backgroundColor;
            firstLetterColor.a = 1.0f;
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(firstVertices), firstVertices);
            glDrawArrays(GL_TRIANGLES, 0, 6);
    
            renderText(firstLetter, "default", textStartX + 4.0f, textStartY + 2.0f, scaleFirst, firstLetterColor);
            renderText(restText, fontName, textStartX + firstLetterSize.x + 4.0f, textStartY, scale, textColor);
        } else {
            renderText(text, fontName, textStartX, textStartY, scale, textColor);
        }
    
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }    

    void renderText(const std::string& text, const std::string& fontName, float x, float y, float scale, glm::vec4 color) {
        if (fonts.find(fontName) == fonts.end()) {
            throw std::runtime_error("Font not loaded: " + fontName);
        }

        if (x < 0 || x > screenWidth || y < 0 || y > screenHeight) {
            std::cout << "[text renderer]: Text position out of bounds: (" << x << ", " << y << ")" << std::endl;
            return;
        }

        const auto& characters = fonts[fontName];
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        textShader->use();
        textShader->set("textColor", color);
        textShader->set("projection", projection);
        textShader->set("useTexture", true);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(VAO);

        for (char c : text) {
            Character ch = characters.at(c);
            float xpos = x + ch.bearing.x * scale;
            float ypos = y - (ch.size.y - ch.bearing.y) * scale;
            float w = ch.size.x * scale;
            float h = ch.size.y * scale;

            float vertices[6][4] = {
                { xpos,     ypos + h,   0.0f, 0.0f },
                { xpos,     ypos,       0.0f, 1.0f },
                { xpos + w, ypos,       1.0f, 1.0f },
                { xpos,     ypos + h,   0.0f, 0.0f },
                { xpos + w, ypos,       1.0f, 1.0f },
                { xpos + w, ypos + h,   1.0f, 0.0f }
            };

            glBindTexture(GL_TEXTURE_2D, ch.textureID);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            x += (ch.advance >> 6) * scale;
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

    void showCenteredText(const std::string& text, const std::string& fontName, 
                         float scale = 0.1f, glm::vec3 color = glm::vec3(1.0f),
                         float fadeIn = 0.05f, float visible = 1.0f, float fadeOut = 0.005f) {
        textQueue.push({text, fontName, fadeIn, visible, fadeOut, scale, color});

        if (!isQueueActive) {
            isQueueActive = true;
            showNextText();  
        }
    }

    void showNextText() {
        if (textQueue.empty()) {
            isQueueActive = false; 
            return;
        }

        const auto item = textQueue.front();
        textQueue.pop();

        AudioSystem::getInstance().playSfx("shutter", false, 3.0f);
        fadeStartTime = static_cast<float>(glfwGetTime());
        fadeDuration = item.fadeIn;
        visibleDuration = item.visible;
        fadeOutDuration = item.fadeOut;
        fadeText = item.text;
        fadeFont = item.font;
        fadeScale = item.scale;
        fadeColor = item.color;
        active = true;
    }

    void renderCenteredText() {
        if (!active) return;

        float currentTime = static_cast<float>(glfwGetTime());
        float elapsed = currentTime - fadeStartTime;
        float totalDuration = fadeDuration + visibleDuration + fadeOutDuration;

        if (elapsed > totalDuration) {
            active = false;
            showNextText();
            return;
        }

        float alpha = 1.0f;

        if (elapsed < fadeDuration) {
            alpha = elapsed / fadeDuration;
        }
        else if (elapsed < fadeDuration + visibleDuration) {
            alpha = 1.0f;
        }
        else {
            float fadeOutTime = elapsed - fadeDuration - visibleDuration;
            alpha = 1.0f - (fadeOutTime / fadeOutDuration);
            if (alpha < 0.0f) alpha = 0.0f;
        }
        glm::vec2 size = getTextSize(fadeText, fadeFont, fadeScale);
        float x = (screenWidth - size.x) / 2.0f;
        float y = (screenHeight - size.y) / 2.0f;
        
        renderText(fadeText, fadeFont, x, y, fadeScale, glm::vec4(fadeColor, alpha));
    }

    void destroy() {
        for (auto& [fontName, characters] : fonts) {
            for (auto& [_, ch] : characters) {
                glDeleteTextures(1, &ch.textureID);
            }
        }
        fonts.clear();
        glDeleteBuffers(1, &VBO);
        glDeleteVertexArrays(1, &VAO);
        delete textShader;
        textShader = nullptr;
    }
};

}