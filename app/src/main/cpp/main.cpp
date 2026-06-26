#include "include/main.h"
#include <android/asset_manager.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sstream>
#include <iomanip>
#include <ctime>

GLuint programId;
GLuint uScreenSizeLoc;
GLuint uUseTextureLoc;
GLuint fontTextureId;
GLuint vao, vbo;

struct Vertex {
    float x, y;
    float u, v;
    float r, g, b, a;
};

std::vector<Vertex> vertexBuffer;

void push_quad(float x, float y, float w, float h, float u, float v, float uw, float vh, float r, float g, float b, float a) {
    Vertex v1 = {x, y, u, v, r, g, b, a};
    Vertex v2 = {x + w, y, u + uw, v, r, g, b, a};
    Vertex v3 = {x, y + h, u, v + vh, r, g, b, a};
    Vertex v4 = {x + w, y + h, u + uw, v + vh, r, g, b, a};
    vertexBuffer.push_back(v1); vertexBuffer.push_back(v2); vertexBuffer.push_back(v3);
    vertexBuffer.push_back(v3); vertexBuffer.push_back(v2); vertexBuffer.push_back(v4);
}

void render_text(float x, float y, const std::string& text, float r, float g, float b, float scale = 2.0f) {
    float curX = x;
    float charW = 8.0f * scale;
    float charH = 16.0f * scale;
    for (char c : text) {
        if (c < 32 || c > 126) c = '?';
        int idx = c - 32;
        int col = idx % 16;
        int row = idx / 16;
        float u = col * 8.0f / 128.0f;
        float v = row * 16.0f / 128.0f;
        push_quad(curX, curY, charW, charH, u, v, 8.0f/128.0f, 16.0f/128.0f, r, g, b, 1.0f);
        curX += charW;
    }
}

void load_font_texture(ANativeActivity* activity) {
    glGenTextures(1, &fontTextureId);
    glBindTexture(2D, fontTextureId);
    unsigned char font_bmp[128 * 128];
    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 128; x++) {
            int cx = x / 8, cy = y / 16;
            int idx = cy * 16 + cx;
            char c = idx + 32;
            int lx = x % 8, ly = y % 16;
            font_bmp[y * 128 + x] = ((lx == 0 || lx == 7 || ly == 0 || ly == 15)) ? 0x00 : 0xFF;
        }
    }
    glTexImage2D(2D, 0, RED, 128, 128, 0, RED, UNSIGNED_BYTE, font_bmp);
    glTexParameteri(2D, TEXTURE_MIN_FILTER, NEAREST);
    glTexParameteri(2D, TEXTURE_MAG_FILTER, NEAREST);
}

GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    return shader;
}

void init_gl(AppState* state) {
    state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(state->display, NULL, NULL);
    const EGLint attribs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_NONE};
    EGLConfig config; EGLint numConfigs;
    eglChooseConfig(state->display, attribs, &config, 1, &numConfigs);
    state->surface = eglCreateWindowSurface(state->display, config, state->activity->surface, NULL);
    const EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    state->context = eglCreateContext(state->display, config, EGL_NO_CONTEXT, ctx_attribs);
    eglMakeCurrent(state->display, state->surface, state->surface, state->context);
    eglQuerySurface(state->display, state->surface, EGL_WIDTH, &state->width);
    eglQuerySurface(state->display, state->surface, EGL_HEIGHT, &state->height);

    const char* vtxSrc = "#version 300 es\nlayout(location=0) in vec2 p; layout(location=1) in vec2 t; layout(location=2) in vec4 c; uniform vec2 s; out vec2 vt; out vec4 vc; void main() { gl_Position = vec4((p/s)*2.0-1.0, 0.0, 1.0); gl_Position.y *= -1.0; vt = t; vc = c; }";
    const char* fragSrc = "#version 300 es\nprecision mediump float; in vec2 vt; in vec4 vc; uniform sampler2D tex; uniform bool useTex; out vec4 o; void main() { o = useTex ? vec4(vc.rgb, vc.a * texture(tex, vt).r) : vc; }";
    
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vtxSrc);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragSrc);
    programId = glCreateProgram();
    glAttachShader(programId, vs); glAttachShader(programId, fs); glLinkProgram(programId);
    uScreenSizeLoc = glGetUniformLocation(programId, "s");
    uUseTextureLoc = glGetUniformLocation(programId, "useTex");

    glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
    load_font_texture(state->activity);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    state->initialized = true;
}

void term_gl(AppState* state) {
    if (state->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (state->context != EGL_NO_CONTEXT) eglDestroyContext(state->display, state->context);
        if (state->surface != EGL_NO_SURFACE) eglDestroySurface(state->display, state->surface);
        eglTerminate(state->display);
    }
    state->initialized = false;
}

void draw_frame(AppState* state) {
    if (!state->initialized) return;
    glViewport(0, 0, state->width, state->height);
    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(programId);
    glUniform2f(uScreenSizeLoc, (float)state->width, (float)state->height);

    vertexBuffer.clear();

    if (state->screenState == 0) {
        render_text(50, 200, "ENTER SERVER IP:", 1.0f, 1.0f, 1.0f, 3.0f);
        push_quad(50, 280, state->width - 100, 60, 0,0,0,0, 0.2f, 0.2f, 0.2f, 1.0f);
        render_text(60, 295, state->inputText + "_", 0.0f, 1.0f, 0.0f, 2.5f);
    } else if (state->screenState == 1) {
        render_text(50, 200, "ENTER NICKNAME:", 1.0f, 1.0f, 1.0f, 3.0f);
        push_quad(50, 280, state->width - 100, 60, 0,0,0,0, 0.2f, 0.2f, 0.2f, 1.0f);
        render_text(60, 295, state->inputText + "_", 0.0f, 1.0f, 0.0f, 2.5f);
    } else {
        push_quad(0, 0, state->width, 80, 0,0,0,0, 0.18f, 0.18f, 0.2f, 1.0f);
        render_text(20, 25, "ROOM: GLOBAL  |  STATUS: " + std::string(state->isConnected ? "ONLINE" : "OFFLINE"), 1.0f, 1.0f, 1.0f, 2.0f);
        
        float currentY = 100 + state->scrollOffset;
        state->msgMutex.lock();
        for (const auto& msg : state->messages) {
            std::string line = "[" + msg.timestamp + "] " + msg.sender + ": " + msg.content;
            size_t maxChars = state->width / 16;
            for (size_t i = 0; i < line.length(); i += maxChars) {
                render_text(20, currentY, line.substr(i, maxChars), 0.9f, 0.9f, 0.9f, 2.0f);
                currentY += 40;
            }
            currentY += 10;
        }
        state->msgMutex.unlock();

        push_quad(0, state->height - 120, state->width, 120, 0,0,0,0, 0.15f, 0.15f, 0.15f, 1.0f);
        push_quad(20, state->height - 100, state->width - 40, 80, 0,0,0,0, 0.25f, 0.25f, 0.25f, 1.0f);
        render_text(40, state->height - 75, state->inputText + "_", 1.0f, 1.0f, 1.0f, 2.0f);
    }

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertexBuffer.size() * sizeof(Vertex), vertexBuffer.data(), GL_STREAM_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));

    glUniform1i(uUseTextureLoc, GL_FALSE);
    glDrawArrays(GL_TRIANGLES, 0, vertexBuffer.size());
    glUniform1i(uUseTextureLoc, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D, fontTextureId);
    glDrawArrays(GL_TRIANGLES, 0, vertexBuffer.size());

    eglSwapBuffers(state->display, state->surface);
}

void* network_worker(void* arg) {
    AppState* state = (AppState*)arg;
    while (state->running) {
        if (!state->isConnected && !state->serverIp.empty()) {
            state->sockFd = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(5000);
            if (inet_pton(AF_INET, state->serverIp.c_str(), &serv_addr.sin_addr) > 0) {
                if (connect(state->sockFd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) >= 0) {
                    state->isConnected = true;
                    std::string joinReq = "GET /ws HTTP/1.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";
                    send(state->sockFd, joinReq.c_str(), joinReq.length(), 0);
                }
            }
        }
        if (state->isConnected) {
            char buffer[1024];
            int len = recv(state->sockFd, buffer, sizeof(buffer) - 1, 0);
            if (len <= 0) {
                state->isConnected = false;
                close(state->sockFd);
            } else {
                buffer[len] = '\0';
                std::string raw(buffer);
                if (raw.find("\r\n\r\n") != std::string::npos) {
                    state->msgMutex.lock();
                    time_t now = time(0); tm *ltm = localtime(&now);
                    std::stringstream ss; ss << std::setw(2) << std::setfill('0') << ltm->tm_hour << ":" << std::setw(2) << std::setfill('0') << ltm->tm_min;
                    state->messages.push_back({"SYSTEM", "Connected to Server", ss.str()});
                    state->msgMutex.unlock();
                }
            }
        }
        sleep(1);
    }
    return NULL;
}

void send_chat_message(AppState* state, const std::string& text) {
    if (!state->isConnected) return;
    std::string packet = text;
    send(state->sockFd, packet.c_str(), packet.length(), 0);
}

void handle_input_char(AppState* state, char c) {
    state->inputText += c;
}
void handle_backspace(AppState* state) {
    if (!state->inputText.empty()) state->inputText.pop_back();
}
void handle_enter(AppState* state) {

