#include "driver.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <vector>
#include <cstring>
#include <sstream>
#include "miniaudio.h"

namespace fs = std::filesystem;

static std::wstring g_logPath;

static void InitLog() {
    wchar_t buf[MAX_PATH];
    GetTempPathW(MAX_PATH, buf);
    fs::path p = fs::path(buf) / L"BuffMicVB";
    std::error_code ec;
    fs::create_directories(p, ec);
    g_logPath = (p / L"install.log").wstring();
    FILE* f = _wfopen(g_logPath.c_str(), L"w, ccs=UTF-8");
    if (f) {
        fwprintf(f, L"=== BuffMic VB-Cable install log ===\n");
        fclose(f);
    }
}

static void Log(const std::wstring& msg) {
    if (g_logPath.empty()) return;
    FILE* f = _wfopen(g_logPath.c_str(), L"a, ccs=UTF-8");
    if (!f) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fwprintf(f, L"[%02d:%02d:%02d] %ls\n", st.wHour, st.wMinute, st.wSecond, msg.c_str());
    fclose(f);
}

static fs::path TempDir() {
    wchar_t buf[MAX_PATH];
    GetTempPathW(MAX_PATH, buf);
    fs::path p = fs::path(buf) / L"BuffMicVB";
    std::error_code ec;
    fs::create_directories(p, ec);
    return p;
}

static bool ExtractRes(int id, const fs::path& dest) {
    HMODULE h = GetModuleHandleW(nullptr);
    HRSRC r = FindResourceW(h, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!r) return false;
    HGLOBAL g = LoadResource(h, r);
    if (!g) return false;
    void* d = LockResource(g);
    DWORD sz = SizeofResource(h, r);
    if (!d || !sz) return false;
    std::ofstream f(dest, std::ios::binary);
    if (!f) return false;
    f.write(static_cast<const char*>(d), sz);
    return f.good();
}

bool IsVBCableInstalled() {
    ma_context ctx;
    ma_context_config cfg = ma_context_config_init();
    if (ma_context_init(nullptr, 0, &cfg, &ctx) != MA_SUCCESS) return false;

    ma_device_info* pb = nullptr; ma_uint32 pbc = 0;
    ma_device_info* cp = nullptr; ma_uint32 cpc = 0;
    ma_context_get_devices(&ctx, &pb, &pbc, &cp, &cpc);

    bool foundIn = false, foundOut = false;
    for (ma_uint32 i = 0; i < pbc; ++i)
        if (strstr(pb[i].name, "CABLE Input")) { foundOut = true; break; }
    for (ma_uint32 i = 0; i < cpc; ++i)
        if (strstr(cp[i].name, "CABLE Output")) { foundIn = true; break; }

    ma_context_uninit(&ctx);
    return foundIn && foundOut;
}

static bool SysFileExists() {
    return fs::exists(L"C:\\Windows\\System32\\drivers\\vbaudio_cable64_win10.sys");
}

bool EnsureVBCableInstalled(std::wstring* errOut) {
    if (g_logPath.empty()) InitLog();
    Log(L"=== EnsureVBCableInstalled ===");

    if (IsVBCableInstalled()) {
        Log(L"Already installed");
        return true;
    }

    fs::path dir = TempDir();
    Log(L"Temp: " + dir.wstring());

    ExtractRes(101, dir / L"vbMmeCable64_win10.inf");
    ExtractRes(102, dir / L"vbaudio_cable64_win10.sys");
    ExtractRes(103, dir / L"vbaudio_cable64_win10.cat");
    ExtractRes(105, dir / L"VBCABLE_ControlPanel.exe");

    if (!ExtractRes(104, dir / L"VBCABLE_Setup_x64.exe")) {
        Log(L"Extract setup exe failed");
        if (errOut) *errOut = L"Extract setup exe failed";
        return false;
    }

    std::wstring setup = (dir / L"VBCABLE_Setup_x64.exe").wstring();
    Log(L"Launch setup: " + setup);

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"open";
    sei.lpFile = setup.c_str();
    sei.lpParameters = L"-i -h";
    sei.nShow = SW_HIDE;

    if (ShellExecuteExW(&sei)) {
        WaitForSingleObject(sei.hProcess, 180000);
        DWORD code = 0;
        GetExitCodeProcess(sei.hProcess, &code);
        CloseHandle(sei.hProcess);
        Log(L"Setup exit: " + std::to_wstring(code));

        for (int i = 0; i < 40; ++i) {
            Sleep(500);
            if (IsVBCableInstalled()) {
                Log(L"SUCCESS after " + std::to_wstring(i * 500) + L"ms");
                return true;
            }
        }
    } else {
        Log(L"ShellExecuteEx failed: " + std::to_wstring(GetLastError()));
    }

    std::wstring inf = (dir / L"vbMmeCable64_win10.inf").wstring();
    std::wstring cmd = L"pnputil /add-driver \"" + inf + L"\" /install";
    Log(L"Fallback pnputil");
    {
        STARTUPINFOW si{}; si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        std::wstring c = cmd;
        if (CreateProcessW(nullptr, &c[0], nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 60000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
    Sleep(2500);
    if (IsVBCableInstalled()) {
        Log(L"SUCCESS via pnputil");
        return true;
    }

    if (SysFileExists()) {
        Log(L"Driver file installed, needs reboot");
        if (errOut) *errOut = L"Driver installed, may need reboot. Log: " + g_logPath;
        return false;
    }

    Log(L"ALL methods failed");
    if (errOut) *errOut = L"Install failed. Log: " + g_logPath;
    return false;
}

void OpenVBCableControlPanel() {
    if (g_logPath.empty()) InitLog();
    fs::path dir = TempDir();
    fs::path exe = dir / L"VBCABLE_ControlPanel.exe";
    if (!fs::exists(exe)) ExtractRes(105, exe);
    ShellExecuteW(nullptr, L"open", exe.wstring().c_str(),
                  nullptr, nullptr, SW_SHOWNORMAL);
}
