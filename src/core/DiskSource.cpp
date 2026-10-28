#include "DiskSource.h"

#include <windows.h>
#include <string>

void DiskSource::poll(Metrics& io) {
    // 取系统盘（Windows 目录所在盘），而不是硬编码 C:
    wchar_t winDir[MAX_PATH]{};
    const UINT n = GetWindowsDirectoryW(winDir, MAX_PATH);
    if (n < 3 || n >= MAX_PATH) return;

    // "C:\Windows" → "C:\"
    std::wstring root(winDir, 3);

    ULARGE_INTEGER freeForUser{}, total{}, totalFree{};
    if (!GetDiskFreeSpaceExW(root.c_str(), &freeForUser, &total, &totalFree)) return;
    if (total.QuadPart == 0) return;

    const double used = static_cast<double>(total.QuadPart - totalFree.QuadPart);
    io.diskUsedPct = static_cast<float>(used * 100.0 / static_cast<double>(total.QuadPart));
}
