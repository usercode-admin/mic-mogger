#pragma once
#include "imgui.h"

namespace Theme {
    inline ImVec4 V(int r, int g, int b, int a = 255) {
        return ImVec4(r/255.f, g/255.f, b/255.f, a/255.f);
    }
    const ImVec4 bg       = V(0, 0, 0);
    const ImVec4 surface  = V(14, 14, 14);
    const ImVec4 surface2 = V(28, 28, 28);
    const ImVec4 surface3 = V(46, 46, 46);
    const ImVec4 border   = V(54, 54, 54);
    const ImVec4 borderHi = V(220, 220, 220);
    const ImVec4 text     = V(210, 210, 210);
    const ImVec4 textHi   = V(255, 255, 255);
    const ImVec4 muted    = V(160, 160, 160);
    const ImVec4 dim      = V(105, 105, 105);
    const ImVec4 accent   = V(245, 245, 245);
    const ImVec4 green    = V(220, 220, 220);
    const ImVec4 red      = V(170, 170, 170);
    const ImVec4 yellow   = V(200, 200, 200);

    inline void Apply() {
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 0; s.ChildRounding = 10;
        s.FrameRounding = 6;  s.PopupRounding = 8;
        s.ScrollbarRounding = 10; s.GrabRounding = 6;
        s.WindowPadding = ImVec2(20, 20);
        s.FramePadding  = ImVec2(12, 7);
        s.ItemSpacing   = ImVec2(10, 9);
        s.ItemInnerSpacing = ImVec2(8, 6);
        s.ScrollbarSize = 10; s.GrabMinSize = 10;
        s.WindowBorderSize = 0; s.ChildBorderSize = 1;
        s.PopupBorderSize = 1;  s.FrameBorderSize = 0;

        auto& c = s.Colors;
        c[ImGuiCol_WindowBg]      = bg;
        c[ImGuiCol_ChildBg]       = surface;
        c[ImGuiCol_PopupBg]       = surface;
        c[ImGuiCol_Border]        = border;
        c[ImGuiCol_BorderShadow]  = ImVec4(0,0,0,0);
        c[ImGuiCol_FrameBg]       = surface2;
        c[ImGuiCol_FrameBgHovered]= surface3;
        c[ImGuiCol_FrameBgActive] = surface3;
        c[ImGuiCol_MenuBarBg]     = surface;
        c[ImGuiCol_ScrollbarBg]   = ImVec4(0,0,0,0);
        c[ImGuiCol_ScrollbarGrab] = surface3;
        c[ImGuiCol_ScrollbarGrabHovered] = dim;
        c[ImGuiCol_ScrollbarGrabActive]  = muted;
        c[ImGuiCol_CheckMark]     = accent;
        c[ImGuiCol_SliderGrab]    = accent;
        c[ImGuiCol_SliderGrabActive] = textHi;
        c[ImGuiCol_Button]        = surface2;
        c[ImGuiCol_ButtonHovered] = surface3;
        c[ImGuiCol_ButtonActive]  = surface3;
        c[ImGuiCol_Header]        = surface2;
        c[ImGuiCol_HeaderHovered] = surface3;
        c[ImGuiCol_HeaderActive]  = surface3;
        c[ImGuiCol_Separator]     = border;
        c[ImGuiCol_Text]          = text;
        c[ImGuiCol_TextDisabled]  = muted;
        c[ImGuiCol_PlotHistogram] = accent;
    }

    inline void BeginPanel(const char* id, ImVec2 size = ImVec2(0, 0), float round = 10) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, surface);
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, round);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
        ImGui::BeginChild(id, size, true, ImGuiWindowFlags_None);
    }
    inline void EndPanel() {
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

    inline void SectionTitle(const char* t) {
        ImGui::PushStyleColor(ImGuiCol_Text, muted);
        ImGui::TextUnformatted(t);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    inline bool PrimaryButton(const char* label, ImVec2 size = ImVec2(0, 0)) {
        ImGui::PushStyleColor(ImGuiCol_Button, accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, V(220, 220, 220));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  V(180, 180, 180));
        ImGui::PushStyleColor(ImGuiCol_Text, bg);
        bool r = ImGui::Button(label, size);
        ImGui::PopStyleColor(4);
        return r;
    }
}
