#pragma once
#include <string>

bool IsVBCableInstalled();
bool EnsureVBCableInstalled(std::wstring* errOut = nullptr);
void OpenVBCableControlPanel();