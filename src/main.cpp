#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

constexpr int WORLD_WIDTH = 800;
constexpr int WORLD_HEIGHT = 600;
constexpr float START_X = 400.0f;
constexpr float START_Y = 330.0f;
constexpr int TILE = 16;

struct Color {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;
};

namespace Palette {
    constexpr Color dark{0x0f, 0x38, 0x0f, 0xff};
    constexpr Color shadow{0x1f, 0x4a, 0x1f, 0xff};
    constexpr Color midDark{0x30, 0x62, 0x30, 0xff};
    constexpr Color mid{0x5f, 0x8f, 0x25, 0xff};
    constexpr Color midLight{0x8b, 0xac, 0x0f, 0xff};
    constexpr Color light{0x9b, 0xbc, 0x0f, 0xff};
    constexpr Color glow{0xcf, 0xe8, 0x8f, 0xff};
    constexpr Color tileAlt{0xa5, 0xc5, 0x1a, 0xff};
    constexpr Color path{0xa4, 0xbd, 0x22, 0xff};
    constexpr Color transparent{0, 0, 0, 0};
}

struct Image {
    int width;
    int height;
    std::vector<std::uint8_t> pixels;

    Image(int w, int h, Color fill = Palette::transparent)
        : width(w), height(h), pixels(static_cast<std::size_t>(w * h * 4), 0) {
        clear(fill);
    }

    void clear(Color color) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                setPixel(x, y, color);
            }
        }
    }

    void setPixel(int x, int y, Color color) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        const std::size_t i = static_cast<std::size_t>((y * width + x) * 4);
        pixels[i + 0] = color.r;
        pixels[i + 1] = color.g;
        pixels[i + 2] = color.b;
        pixels[i + 3] = color.a;
    }

    void rect(float xf, float yf, float wf, float hf, Color color) {
        const int x0 = static_cast<int>(std::round(xf));
        const int y0 = static_cast<int>(std::round(yf));
        const int w = static_cast<int>(std::round(wf));
        const int h = static_cast<int>(std::round(hf));
        for (int y = y0; y < y0 + h; ++y) {
            for (int x = x0; x < x0 + w; ++x) {
                setPixel(x, y, color);
            }
        }
    }
};

struct Texture {
    GLuint id = 0;
    int width = 0;
    int height = 0;

    Texture() = default;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&& other) noexcept {
        id = other.id;
        width = other.width;
        height = other.height;
        other.id = 0;
    }

    Texture& operator=(Texture&& other) noexcept {
        if (this != &other) {
            release();
            id = other.id;
            width = other.width;
            height = other.height;
            other.id = 0;
        }
        return *this;
    }

    ~Texture() { release(); }

    void release() {
        if (id != 0) {
            glDeleteTextures(1, &id);
            id = 0;
        }
    }
};

struct Layer {
    Texture texture;
    float speed;
    float yParallax;
    float opacity;
};

struct Player {
    float x = START_X;
    float y = START_Y;
    float width = 64.0f;
    float height = 80.0f;
    float speed = 190.0f;
    bool flipH = false;
    Texture texture;
};

class Keys {
public:
    void set(int key, bool pressed) {
        if (key >= 0 && key < static_cast<int>(state.size())) {
            state[static_cast<std::size_t>(key)] = pressed;
        }
    }

    bool down(int key) const {
        return key >= 0 && key < static_cast<int>(state.size()) && state[static_cast<std::size_t>(key)];
    }

private:
    std::array<bool, 512> state{};
};

Keys gKeys;

std::string shaderTypeName(GLenum type) {
    return type == GL_VERTEX_SHADER ? "vertex" : "fragment";
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(length), '\0');
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("Erro ao compilar shader " + shaderTypeName(type) + ": " + log);
    }

    return shader;
}

GLuint createProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    GLuint program = glCreateProgram();

    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(length), '\0');
        glGetProgramInfoLog(program, length, nullptr, log.data());
        glDeleteProgram(program);
        throw std::runtime_error("Erro ao linkar programa: " + log);
    }

    return program;
}

Texture uploadTexture(const Image& image) {
    Texture texture;
    texture.width = image.width;
    texture.height = image.height;

    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());

    return texture;
}

void drawTree(Image& img, int x, int y) {
    img.rect(x + 13, y + 31, 14, 20, Palette::dark);
    img.rect(x + 7, y + 17, 30, 22, Palette::midDark);
    img.rect(x, y + 24, 44, 23, Palette::midDark);
    img.rect(x + 8, y + 7, 28, 21, Palette::shadow);
    img.rect(x + 14, y, 16, 14, Palette::dark);
    img.rect(x + 12, y + 20, 7, 7, Palette::midLight);
    img.rect(x + 28, y + 28, 6, 6, Palette::light);
}

Texture makeGroundTexture() {
    Image img(WORLD_WIDTH, WORLD_HEIGHT, Palette::light);

    for (int y = 0; y < img.height; y += TILE) {
        for (int x = 0; x < img.width; x += TILE) {
            const bool alt = ((x / TILE + y / TILE) % 2) == 0;
            img.rect(x, y, TILE, TILE, alt ? Palette::light : Palette::tileAlt);

            if ((x * 3 + y * 5) % 112 == 0) {
                img.rect(x + 2, y + 3, 3, 4, Palette::midLight);
                img.rect(x + 9, y + 11, 4, 2, Palette::midDark);
            }

            if ((x * 7 + y) % 208 == 0) {
                img.rect(x + 7, y + 6, 2, 2, Palette::dark);
            }
        }
    }

    for (int y = -24; y < img.height + 40; y += TILE) {
        const float wobble = std::sin(static_cast<float>(y) * 0.032f) * 26.0f;
        for (float x = 312.0f + wobble; x <= 472.0f + wobble; x += TILE) {
            img.rect(x, y, TILE, TILE, Palette::midLight);
            img.rect(x + 1, y + 1, 14, 14, Palette::path);
            img.rect(x + 2, y + 12, 6, 2, Palette::midDark);
            img.rect(x + 11, y + 4, 2, 2, Palette::glow);
        }
    }

    return uploadTexture(img);
}

Texture makeWaterTexture() {
    Image img(WORLD_WIDTH, WORLD_HEIGHT, Palette::transparent);

    for (int y = -16; y < img.height + 16; y += TILE) {
        const float curve = std::sin(static_cast<float>(y) * 0.021f) * 24.0f;
        for (float x = -16.0f; x < 116.0f + curve; x += TILE) {
            img.rect(x, y, TILE, TILE, Palette::midLight);
            img.rect(x + 1, y + 1, 14, 14, Palette::mid);
            img.rect(x + 3, y + 5, 9, 2, Palette::glow);
            img.rect(x + 7, y + 11, 8, 2, Palette::dark);
        }

        img.rect(116.0f + curve, y, TILE, TILE, Palette::dark);
        img.rect(124.0f + curve, y + 4, 8, 8, Palette::midDark);
    }

    for (int y = 64; y < img.height; y += 96) {
        img.rect(640, y, 96, 16, Palette::dark);
        img.rect(656, y - 16, 64, 16, Palette::midDark);
        img.rect(672, y - 32, 32, 16, Palette::shadow);
        img.rect(684, y - 20, 8, 8, Palette::midLight);
    }

    return uploadTexture(img);
}

Texture makeDistantTexture() {
    Image img(WORLD_WIDTH, WORLD_HEIGHT, Palette::transparent);

    for (int x = 120; x < img.width; x += 170) {
        img.rect(x, 44, 128, 16, Palette::midDark);
        img.rect(x + 16, 28, 96, 16, Palette::shadow);
        img.rect(x + 32, 12, 64, 16, Palette::dark);
        img.rect(x + 48, 60, 24, 30, Palette::dark);
        img.rect(x + 56, 68, 8, 14, Palette::midLight);
    }

    for (int x = 172; x < img.width; x += 240) {
        img.rect(x, 498, 88, 16, Palette::midDark);
        img.rect(x + 16, 482, 56, 16, Palette::dark);
        img.rect(x + 32, 466, 24, 16, Palette::shadow);
        img.rect(x + 40, 486, 8, 12, Palette::light);
    }

    return uploadTexture(img);
}

Texture makeObjectsTexture() {
    Image img(WORLD_WIDTH, WORLD_HEIGHT, Palette::transparent);

    const std::vector<std::pair<int, int>> trees = {
        {74, 62}, {184, 138}, {602, 70}, {704, 170},
        {58, 408}, {162, 502}, {604, 405}, {718, 498},
        {520, 290}, {230, 285}
    };

    for (const auto& [x, y] : trees) {
        drawTree(img, x, y);
    }

    const std::vector<std::pair<int, int>> rocks = {
        {252, 92}, {530, 138}, {246, 466}, {536, 516}, {604, 252}, {342, 202}
    };

    for (const auto& [x, y] : rocks) {
        img.rect(x, y + 10, 40, 22, Palette::dark);
        img.rect(x + 5, y + 2, 30, 16, Palette::midDark);
        img.rect(x + 10, y + 5, 10, 5, Palette::midLight);
        img.rect(x + 25, y + 12, 8, 4, Palette::shadow);
    }

    for (int x = 300; x <= 500; x += 48) {
        img.rect(x, 166, 32, 12, Palette::dark);
        img.rect(x + 4, 154, 24, 12, Palette::midDark);
        img.rect(x + 10, 146, 12, 8, Palette::shadow);
    }

    return uploadTexture(img);
}

Texture makeForegroundTexture() {
    Image img(WORLD_WIDTH, WORLD_HEIGHT, Palette::transparent);

    for (int x = -8; x < img.width + 16; x += 40) {
        const float y = 552.0f + std::sin(static_cast<float>(x) * 0.075f) * 12.0f;
        img.rect(x, y, 42, 22, Palette::midDark);
        img.rect(x + 7, y - 10, 10, 18, Palette::dark);
        img.rect(x + 24, y - 6, 10, 14, Palette::dark);
        img.rect(x + 14, y + 3, 8, 6, Palette::midLight);
    }

    for (int x = 36; x < img.width; x += 112) {
        img.rect(x, 250, 38, 16, Palette::midDark);
        img.rect(x + 7, 238, 24, 18, Palette::dark);
        img.rect(x + 15, 232, 8, 8, Palette::shadow);
    }

    return uploadTexture(img);
}

Texture makePlayerTexture() {
    Image img(64, 80, Palette::transparent);

    img.rect(20, 3, 24, 8, Palette::dark);
    img.rect(17, 11, 30, 10, Palette::shadow);
    img.rect(21, 21, 22, 13, Palette::light);
    img.rect(18, 34, 28, 28, Palette::midDark);
    img.rect(22, 38, 20, 6, Palette::midLight);
    img.rect(14, 39, 8, 18, Palette::dark);
    img.rect(42, 39, 8, 18, Palette::dark);
    img.rect(22, 62, 8, 13, Palette::dark);
    img.rect(34, 62, 8, 13, Palette::dark);
    img.rect(48, 34, 11, 23, Palette::midLight);
    img.rect(50, 38, 7, 15, Palette::light);
    img.rect(52, 42, 3, 7, Palette::dark);
    img.rect(25, 26, 4, 4, Palette::dark);
    img.rect(35, 26, 4, 4, Palette::dark);
    img.rect(29, 31, 6, 3, Palette::midDark);
    img.rect(16, 24, 5, 10, Palette::shadow);
    img.rect(43, 24, 5, 10, Palette::shadow);

    return uploadTexture(img);
}

Texture makeHudTexture() {
    Image img(WORLD_WIDTH, WORLD_HEIGHT, Palette::transparent);

    img.rect(10, 10, 166, 42, Palette::dark);
    img.rect(14, 14, 158, 34, Palette::light);
    img.rect(20, 22, 14, 14, Palette::dark);
    img.rect(38, 22, 14, 14, Palette::dark);
    img.rect(56, 22, 14, 14, Palette::dark);
    img.rect(82, 22, 74, 6, Palette::dark);
    img.rect(82, 32, 54, 6, Palette::midDark);

    img.rect(624, 10, 166, 42, Palette::dark);
    img.rect(628, 14, 158, 34, Palette::light);
    img.rect(642, 23, 18, 18, Palette::dark);
    img.rect(666, 23, 18, 18, Palette::midDark);
    img.rect(690, 23, 76, 6, Palette::dark);
    img.rect(690, 34, 52, 5, Palette::midDark);

    return uploadTexture(img);
}

float positiveModulo(float value, float modulo) {
    return std::fmod(std::fmod(value, modulo) + modulo, modulo);
}

class Renderer {
public:
    Renderer() {
        program = createProgram(vertexShaderSource, fragmentShaderSource);
        setupQuad();
        cacheUniforms();
    }

    ~Renderer() {
        if (vbo != 0) glDeleteBuffers(1, &vbo);
        if (vao != 0) glDeleteVertexArrays(1, &vao);
        if (program != 0) glDeleteProgram(program);
    }

    void begin() const {
        glUseProgram(program);
        glBindVertexArray(vao);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(uTexture, 0);
        glUniform2f(uResolution, static_cast<float>(WORLD_WIDTH), static_cast<float>(WORLD_HEIGHT));
    }

    void drawQuad(const Texture& texture, float centerX, float centerY, float width, float height,
                  float rotation = 0.0f, bool flipH = false, float opacity = 1.0f) const {
        glBindTexture(GL_TEXTURE_2D, texture.id);
        glUniform2f(uCenter, centerX, centerY);
        glUniform2f(uSize, width, height);
        glUniform1f(uRotation, rotation);
        glUniform1i(uFlipH, flipH ? 1 : 0);
        glUniform1f(uOpacity, opacity);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

private:
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLint uResolution = -1;
    GLint uCenter = -1;
    GLint uSize = -1;
    GLint uRotation = -1;
    GLint uTexture = -1;
    GLint uFlipH = -1;
    GLint uOpacity = -1;

    static constexpr const char* vertexShaderSource = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;

uniform vec2 uResolution;
uniform vec2 uCenter;
uniform vec2 uSize;
uniform float uRotation;

out vec2 vTexCoord;

void main() {
    vec2 p = aPosition * uSize;

    float c = cos(uRotation);
    float s = sin(uRotation);
    mat2 r = mat2(c, -s, s, c);
    p = r * p;

    p += uCenter;

    vec2 zeroToOne = p / uResolution;
    vec2 clip = zeroToOne * 2.0 - 1.0;
    clip.y = -clip.y;

    gl_Position = vec4(clip, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)glsl";

    static constexpr const char* fragmentShaderSource = R"glsl(
#version 330 core
uniform sampler2D uTexture;
uniform bool uFlipH;
uniform float uOpacity;

in vec2 vTexCoord;
out vec4 outColor;

void main() {
    vec2 uv = vTexCoord;
    if (uFlipH) uv.x = 1.0 - uv.x;

    vec4 tex = texture(uTexture, uv);
    if (tex.a < 0.05) discard;

    outColor = vec4(tex.rgb, tex.a * uOpacity);
}
)glsl";

    void setupQuad() {
        const float vertices[] = {
            -0.5f, -0.5f, 0.0f, 0.0f,
             0.5f, -0.5f, 1.0f, 0.0f,
            -0.5f,  0.5f, 0.0f, 1.0f,
            -0.5f,  0.5f, 0.0f, 1.0f,
             0.5f, -0.5f, 1.0f, 0.0f,
             0.5f,  0.5f, 1.0f, 1.0f
        };

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    }

    void cacheUniforms() {
        uResolution = glGetUniformLocation(program, "uResolution");
        uCenter = glGetUniformLocation(program, "uCenter");
        uSize = glGetUniformLocation(program, "uSize");
        uRotation = glGetUniformLocation(program, "uRotation");
        uTexture = glGetUniformLocation(program, "uTexture");
        uFlipH = glGetUniformLocation(program, "uFlipH");
        uOpacity = glGetUniformLocation(program, "uOpacity");
    }
};

bool isMoving() {
    return gKeys.down(GLFW_KEY_LEFT) || gKeys.down(GLFW_KEY_A) ||
           gKeys.down(GLFW_KEY_RIGHT) || gKeys.down(GLFW_KEY_D) ||
           gKeys.down(GLFW_KEY_UP) || gKeys.down(GLFW_KEY_W) ||
           gKeys.down(GLFW_KEY_DOWN) || gKeys.down(GLFW_KEY_S);
}

void updatePlayer(Player& player, float deltaTime) {
    float vx = 0.0f;
    float vy = 0.0f;

    if (gKeys.down(GLFW_KEY_LEFT) || gKeys.down(GLFW_KEY_A)) vx -= 1.0f;
    if (gKeys.down(GLFW_KEY_RIGHT) || gKeys.down(GLFW_KEY_D)) vx += 1.0f;
    if (gKeys.down(GLFW_KEY_UP) || gKeys.down(GLFW_KEY_W)) vy -= 1.0f;
    if (gKeys.down(GLFW_KEY_DOWN) || gKeys.down(GLFW_KEY_S)) vy += 1.0f;

    if (vx != 0.0f && vy != 0.0f) {
        const float inv = 1.0f / std::sqrt(2.0f);
        vx *= inv;
        vy *= inv;
    }

    player.x += vx * player.speed * deltaTime;
    player.y += vy * player.speed * deltaTime;
    player.x = std::clamp(player.x, 64.0f, static_cast<float>(WORLD_WIDTH - 64));
    player.y = std::clamp(player.y, 76.0f, static_cast<float>(WORLD_HEIGHT - 76));

    if (vx < 0.0f) player.flipH = true;
    if (vx > 0.0f) player.flipH = false;
}

void drawLayer(const Renderer& renderer, const Layer& layer, const Player& player) {
    const float dx = player.x - START_X;
    const float dy = player.y - START_Y;
    const float offsetX = -dx * layer.speed;
    const float offsetY = -dy * layer.yParallax;
    const float startX = positiveModulo(offsetX, static_cast<float>(WORLD_WIDTH)) - static_cast<float>(WORLD_WIDTH);

    for (int i = 0; i < 3; ++i) {
        renderer.drawQuad(
            layer.texture,
            startX + static_cast<float>(i * WORLD_WIDTH) + WORLD_WIDTH / 2.0f,
            WORLD_HEIGHT / 2.0f + offsetY,
            static_cast<float>(WORLD_WIDTH),
            static_cast<float>(WORLD_HEIGHT),
            0.0f,
            false,
            layer.opacity
        );
    }
}

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }

    if (action == GLFW_PRESS) gKeys.set(key, true);
    if (action == GLFW_RELEASE) gKeys.set(key, false);
}

void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    const float targetAspect = static_cast<float>(WORLD_WIDTH) / static_cast<float>(WORLD_HEIGHT);
    int viewportWidth = width;
    int viewportHeight = static_cast<int>(std::round(static_cast<float>(width) / targetAspect));

    if (viewportHeight > height) {
        viewportHeight = height;
        viewportWidth = static_cast<int>(std::round(static_cast<float>(height) * targetAspect));
    }

    const int viewportX = (width - viewportWidth) / 2;
    const int viewportY = (height - viewportHeight) / 2;
    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
}

int main() {
    try {
        if (!glfwInit()) {
            throw std::runtime_error("Falha ao iniciar GLFW.");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        GLFWwindow* window = glfwCreateWindow(WORLD_WIDTH, WORLD_HEIGHT, "Game Boy Parallax", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Falha ao criar janela GLFW.");
        }

        glfwMakeContextCurrent(window);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
        glfwSwapInterval(1);

        glewExperimental = GL_TRUE;
        const GLenum glewStatus = glewInit();
        if (glewStatus != GLEW_OK) {
            glfwDestroyWindow(window);
            glfwTerminate();
            throw std::runtime_error(reinterpret_cast<const char*>(glewGetErrorString(glewStatus)));
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        int fbWidth = 0;
        int fbHeight = 0;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        framebufferSizeCallback(window, fbWidth, fbHeight);

        Renderer renderer;

        std::vector<Layer> layers;
        layers.push_back({makeGroundTexture(), 0.10f, 0.04f, 1.0f});
        layers.push_back({makeWaterTexture(), 0.22f, 0.08f, 1.0f});
        layers.push_back({makeDistantTexture(), 0.34f, 0.12f, 1.0f});
        layers.push_back({makeObjectsTexture(), 0.62f, 0.24f, 1.0f});
        layers.push_back({makeForegroundTexture(), 0.92f, 0.34f, 0.98f});

        Player player;
        player.texture = makePlayerTexture();
        Texture hudTexture = makeHudTexture();

        double lastTime = glfwGetTime();

        while (!glfwWindowShouldClose(window)) {
            const double now = glfwGetTime();
            const float deltaTime = std::min(static_cast<float>(now - lastTime), 0.05f);
            lastTime = now;

            glfwPollEvents();
            updatePlayer(player, deltaTime);

            glClearColor(0.61f, 0.74f, 0.06f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            renderer.begin();

            for (const Layer& layer : layers) {
                drawLayer(renderer, layer, player);
            }

            const float seconds = static_cast<float>(now);
            const float bob = isMoving() ? std::sin(seconds * 14.0f) * 2.0f : 0.0f;
            const float walkRotation = isMoving() ? std::sin(seconds * 12.0f) * 0.045f : 0.0f;

            renderer.drawQuad(player.texture, player.x, player.y + bob, player.width, player.height, walkRotation, player.flipH, 1.0f);
            renderer.drawQuad(hudTexture, WORLD_WIDTH / 2.0f, WORLD_HEIGHT / 2.0f, WORLD_WIDTH, WORLD_HEIGHT, 0.0f, false, 0.95f);

            glfwSwapBuffers(window);
        }

        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        glfwTerminate();
        return 1;
    }
}
