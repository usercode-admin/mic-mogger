#pragma once
#include <atomic>
#include <string>
#include <mutex>

enum class AppPhase { Install, Main };
extern AppPhase g_appPhase;

struct InstallerState {
    std::atomic<bool>  running{false};
    std::atomic<bool>  done{false};
    std::atomic<bool>  success{false};
    std::atomic<float> progress{0.0f};

    std::atomic<bool> optDriver{true};
    std::atomic<bool> optShortcut{true};

    void startInstall();
    void setStatus(const std::string& s);
    std::string getStatus();

private:
    std::mutex mu_;
    std::string status_;
};

extern InstallerState g_installer;

void CreateDesktopShortcut();
