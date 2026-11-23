#include <windows.h>
#include "miniaudio.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include "audio_engine.h"
#include "driver.h"
#include "ui.h"
#include "installer.h"
#include "theme.h"
#include <thread>
#include <atomic>
#include <chrono>

static void glfw_error(int, const char* d) { OutputDebugStringA(d); }

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    g_appPhase = IsVBCableInstalled() ? AppPhase::Main : AppPhase::Install;

    AudioEngine eng;

    glfwSetErrorCallback(glfw_error);
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(1220, 620, "BuffMic", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    Theme::Apply();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    bool audioStarted = false;

    std::atomic<bool> alive{true};
    std::thread hotkey([&]{
        while (alive.load()) {
            if (GetAsyncKeyState(VK_F4) & 1) {
                bool b = eng.buffActive.load();
                eng.buffActive.store(!b);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (g_appPhase == AppPhase::Install) {
            RenderInstaller();
        } else {
            if (!audioStarted) {
                auto ins = eng.listInputs();
                auto outs = eng.listOutputs();
                std::string iN, oN;
                if (eng.autoSelect(iN, oN)) {
                    int ii = 0, oi = 0;
                    for (int i = 0; i < (int)ins.size(); ++i)  if (ins[i].name  == iN) ii = i;
                    for (int i = 0; i < (int)outs.size(); ++i) if (outs[i].name == oN) oi = i;
                    eng.start(ins[ii].index, outs[oi].index);
                }
                audioStarted = true;
            }
            RenderUI(eng, IsVBCableInstalled());
        }

        ImGui::Render();
        int w, h; glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);
    }

    alive = false; hotkey.join();
    eng.stop();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
