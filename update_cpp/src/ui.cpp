#include "ui.h"
#include "driver.h"
#include "installer.h"
#include "theme.h"
#include "imgui.h"
#include <windows.h>
#include <commdlg.h>
#include <string>

static std::string OpenFileDialog() {
    OPENFILENAMEA ofn{};
    char buf[MAX_PATH] = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = "Audio\0*.wav;*.mp3;*.flac;*.ogg\0All\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameA(&ofn)) return {};
    return std::string(buf);
}

void RenderInstaller() {
    auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::bg);
    ImGui::Begin("##installer", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar);

    const float W = io.DisplaySize.x;
    const float H = io.DisplaySize.y;
    const float cw = 620.f;

    ImGui::SetCursorPos({ (W - cw) * 0.5f, 80.f });
    ImGui::BeginGroup();

    ImGui::PushStyleColor(ImGuiCol_Text, Theme::textHi);
    ImGui::SetWindowFontScale(2.0f);
    ImGui::TextUnformatted("BUFF MIC");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::TextColored(Theme::muted, "MIC-MOGGER   ·   SETUP");

    ImGui::Spacing(); ImGui::Spacing();

    Theme::BeginPanel("##vb", {cw, 130});
    {
        bool installed = IsVBCableInstalled();
        ImGui::TextColored(Theme::textHi, "VIRTUAL AUDIO DRIVER");
        ImGui::SameLine(cw - 130);
        if (installed)
            ImGui::TextColored(Theme::green, "* INSTALLED");
        else
            ImGui::TextColored(Theme::yellow, "* NOT INSTALLED");

        ImGui::Spacing();
        ImGui::TextColored(Theme::muted,
            "Route BuffMic output into Discord, OBS, games and any app.");

        ImGui::Spacing();
        bool b = g_installer.optDriver.load();
        if (installed) {
            ImGui::BeginDisabled();
            ImGui::Checkbox("VB-Cable already installed", &b);
            ImGui::EndDisabled();
        } else {
            if (ImGui::Checkbox("Install VB-Cable virtual driver", &b))
                g_installer.optDriver.store(b);
        }
    }
    Theme::EndPanel();

    ImGui::Spacing();

    Theme::BeginPanel("##opt", {cw, 88});
    {
        ImGui::TextColored(Theme::textHi, "OPTIONS");
        ImGui::Spacing();
        bool s = g_installer.optShortcut.load();
        if (ImGui::Checkbox("Create desktop shortcut", &s))
            g_installer.optShortcut.store(s);
    }
    Theme::EndPanel();

    ImGui::Spacing(); ImGui::Spacing();

    if (g_installer.running.load() || g_installer.done.load()) {
        ImGui::ProgressBar(g_installer.progress.load(), {cw, 8}, "");
        ImGui::Spacing();
        ImGui::TextColored(
            g_installer.done.load() ? Theme::green : Theme::muted,
            "%s", g_installer.getStatus().c_str());
        ImGui::Spacing();
    }

    if (!g_installer.running.load() && !g_installer.done.load()) {
        float bw = cw * 0.62f;
        if (Theme::PrimaryButton("INSTALL & LAUNCH", {bw, 46}))
            g_installer.startInstall();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, Theme::surface2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::surface3);
        if (ImGui::Button("SKIP", {cw - bw - 10, 46}))
            g_appPhase = AppPhase::Main;
        ImGui::PopStyleColor(2);
    }
    else if (g_installer.done.load()) {
        if (Theme::PrimaryButton("CONTINUE TO BUFFMIC", {cw, 46}))
            g_appPhase = AppPhase::Main;
    }

    ImGui::EndGroup();
    ImGui::End();
    ImGui::PopStyleColor();
}

void RenderUI(AudioEngine& eng, bool vbInstalled) {
    auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##main", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::PushStyleColor(ImGuiCol_Text, Theme::textHi);
    ImGui::SetWindowFontScale(1.3f);
    ImGui::TextUnformatted("BUFF MIC");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextColored(Theme::dim, "v2.1  ·  mic-mogger");

    ImGui::SameLine(ImGui::GetWindowWidth() - 360);
    bool buff = eng.buffActive.load();
    ImGui::TextColored(vbInstalled ? Theme::green : Theme::red,
        "%s", vbInstalled ? "* CABLE READY" : "* NO CABLE");
    ImGui::SameLine();
    ImGui::TextColored(buff ? Theme::green : Theme::dim,
        "    %s  [F4]", buff ? "* BUFF ON" : "* BUFF OFF");

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, Theme::border);
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing(); ImGui::Spacing();

    ImGui::Columns(3, "cols", false);

    Theme::SectionTitle("MASTER");

    if (buff) {
        ImGui::PushStyleColor(ImGuiCol_Button, Theme::surface3);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::V(72, 72, 72));
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::textHi);
        if (ImGui::Button("STOP BUFF   [F4]", {-1, 52}))
            eng.buffActive.store(false);
        ImGui::PopStyleColor(3);
    } else {
        if (Theme::PrimaryButton("START BUFF   [F4]", {-1, 52}))
            eng.buffActive.store(true);
    }

    ImGui::Spacing(); ImGui::Spacing();
    Theme::SectionTitle("LEVEL");

    float g = eng.gain.load() * 100.f;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##gain", &g, 50.f, 1600.f, "%.0f%%"))
        eng.gain.store(g / 100.f);
    ImGui::TextColored(Theme::dim, "50 - 1600%%  real-time");

    ImGui::Spacing();
    Theme::SectionTitle("LOUDNESS");

    float mk = eng.makeup.load() * 100.f;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##mk", &mk, 0.f, 200.f, "%.0f%%"))
        eng.makeup.store(mk / 100.f);
    ImGui::TextColored(Theme::dim, "0 - 200%%  final makeup");

    ImGui::Spacing(); ImGui::Spacing();
    Theme::SectionTitle("OUTPUT METER");
    ImGui::ProgressBar(eng.meterPeak.load(), {-1, 12}, "");
    ImGui::Spacing();
    ImGui::TextColored(Theme::dim, "SR: %d Hz  |  %s",
        eng.streamSampleRate(), eng.running() ? "RUNNING" : "STOPPED");

    ImGui::NextColumn();

    Theme::SectionTitle("EQUALIZER · 3-BAND");

    float l = eng.lowDb.load(), m = eng.midDb.load(), h = eng.highDb.load();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##lo", &l, -12.f, 12.f, "LOW    %+.1f dB")) eng.lowDb.store(l);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##md", &m, -12.f, 12.f, "MID    %+.1f dB")) eng.midDb.store(m);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##hi", &h, -12.f, 12.f, "HIGH   %+.1f dB")) eng.highDb.store(h);

    ImGui::Spacing();
    float mf = eng.midFreq.load();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##mf", &mf, 400.f, 5000.f, "MID FREQ   %.0f Hz"))
        eng.midFreq.store(mf);

    ImGui::SetNextItemWidth(-1);
    float hf = eng.hpfFreq.load();
    if (ImGui::SliderFloat("##hp", &hf, 40.f, 200.f, "HPF        %.0f Hz"))
        eng.hpfFreq.store(hf);

    ImGui::Spacing(); ImGui::Spacing();
    Theme::SectionTitle("COMPRESSOR");

    float th = eng.compThresh.load(), ra = eng.compRatio.load();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##th", &th, 0.05f, 0.6f, "THRESH   %.2f"))
        eng.compThresh.store(th);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##ra", &ra, 1.f, 12.f, "RATIO    %.1f:1"))
        eng.compRatio.store(ra);

    ImGui::NextColumn();

    Theme::SectionTitle("ROUTING");

    static auto ins  = eng.listInputs();
    static auto outs = eng.listOutputs();
    static int inSel = 0, outSel = 0;
    static bool routed = false;

    if (!routed && !ins.empty() && !outs.empty()) {
        std::string iN, oN;
        if (eng.autoSelect(iN, oN)) {
            for (int i = 0; i < (int)ins.size();  ++i) if (ins[i].name  == iN) inSel = i;
            for (int i = 0; i < (int)outs.size(); ++i) if (outs[i].name == oN) outSel = i;
            eng.start(ins[inSel].index, outs[outSel].index);
            routed = true;
        }
    }

    ImGui::TextColored(Theme::muted, "INPUT / MIC");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##in", ins.empty() ? "" : ins[inSel].name.c_str())) {
        for (int i = 0; i < (int)ins.size(); ++i)
            if (ImGui::Selectable(ins[i].name.c_str(), i == inSel)) inSel = i;
        ImGui::EndCombo();
    }

    ImGui::Spacing();
    ImGui::TextColored(Theme::muted, "OUTPUT / CABLE");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##out", outs.empty() ? "" : outs[outSel].name.c_str())) {
        for (int i = 0; i < (int)outs.size(); ++i)
            if (ImGui::Selectable(outs[i].name.c_str(), i == outSel)) outSel = i;
        ImGui::EndCombo();
    }

    ImGui::Spacing();
    if (ImGui::Button("APPLY ROUTING", {-1, 32})) {
        if (!ins.empty() && !outs.empty())
            eng.start(ins[inSel].index, outs[outSel].index);
    }

    ImGui::Spacing(); ImGui::Spacing();
    Theme::SectionTitle("AUDIO FILE");

    std::string fn = eng.fileName();
    ImGui::TextWrapped("%s", fn.empty() ? "(none)" : fn.c_str());
    ImGui::Spacing();

    if (ImGui::Button("SELECT", {110, 32})) {
        std::string p = OpenFileDialog();
        if (!p.empty()) {
            std::string err;
            if (!eng.loadFile(p, err))
                MessageBoxA(nullptr, err.c_str(), "Load error", MB_OK | MB_ICONERROR);
        }
    }
    ImGui::SameLine();
    bool play = eng.isPlaying();
    if (play) {
        ImGui::PushStyleColor(ImGuiCol_Button, Theme::surface3);
        if (ImGui::Button("STOP", {80, 32})) eng.togglePlay();
        ImGui::PopStyleColor();
    } else {
        if (Theme::PrimaryButton("PLAY", {80, 32})) eng.togglePlay();
    }

    ImGui::Spacing();
    bool st = eng.staticOn.load();
    if (ImGui::Checkbox("Static / grit", &st)) eng.staticOn.store(st);

    ImGui::Spacing(); ImGui::Spacing();
    if (!vbInstalled) {
        if (ImGui::Button("RE-RUN SETUP", {-1, 30}))
            g_appPhase = AppPhase::Install;
    } else {
        if (ImGui::Button("VB-CABLE PANEL", {-1, 30}))
            OpenVBCableControlPanel();
    }

    ImGui::Columns(1);
    ImGui::End();
}
