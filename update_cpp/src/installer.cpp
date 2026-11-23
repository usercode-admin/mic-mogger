#include "installer.h"
#include "driver.h"
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <thread>
#include <string>

AppPhase g_appPhase = AppPhase::Install;
InstallerState g_installer;

void InstallerState::setStatus(const std::string& s) {
    std::lock_guard<std::mutex> lk(mu_);
    status_ = s;
}
std::string InstallerState::getStatus() {
    std::lock_guard<std::mutex> lk(mu_);
    return status_;
}

void CreateDesktopShortcut() {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    wchar_t desktop[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktop)))
        return;
    std::wstring lnk = std::wstring(desktop) + L"\\BuffMic.lnk";

    CoInitialize(nullptr);
    IShellLinkW* sl = nullptr;
    CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                     IID_IShellLinkW, (void**)&sl);
    if (sl) {
        sl->SetPath(exe);
        IPersistFile* pf = nullptr;
        sl->QueryInterface(IID_IPersistFile, (void**)&pf);
        if (pf) { pf->Save(lnk.c_str(), TRUE); pf->Release(); }
        sl->Release();
    }
    CoUninitialize();
}

void InstallerState::startInstall() {
    if (running.exchange(true)) return;
    done = false;
    success = false;
    progress = 0.0f;
    setStatus("Preparing...");

    std::thread([this]{
        if (optDriver.load()) {
            if (IsVBCableInstalled()) {
                setStatus("VB-Cable already installed");
                progress = 0.6f;
            } else {
                setStatus("Installing VB-Cable driver...");
                progress = 0.15f;
                std::wstring err;
                bool ok = EnsureVBCableInstalled(&err);
                progress = 0.6f;
                if (ok) setStatus("Driver installed successfully");
                else    setStatus("Driver install incomplete - may need reboot");
            }
        } else {
            progress = 0.6f;
        }

        if (optShortcut.load()) {
            setStatus("Creating desktop shortcut...");
            progress = 0.85f;
            CreateDesktopShortcut();
        }

        progress = 1.0f;
        setStatus("Ready");
        success = true;
        done = true;
        running = false;
    }).detach();
}
