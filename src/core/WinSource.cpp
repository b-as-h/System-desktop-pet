#include "WinSource.h"

#include <windows.h>
#include <psapi.h>

namespace {

inline uint64_t fileTimeToU64(const FILETIME& ft) {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

} // namespace

bool WinSource::init() {
    // 第一次采样必须建立基线：
    // GetSystemTimes 返回的是【开机以来】的累计值，没有前值可比，
    // 第一次算出来的差值要么是 0 要么是垃圾数据。
    FILETIME idle{}, kernel{}, user{};
    if (GetSystemTimes(&idle, &kernel, &user)) {
        prevIdle_   = fileTimeToU64(idle);
        prevKernel_ = fileTimeToU64(kernel);
        prevUser_   = fileTimeToU64(user);
        hasPrev_    = true;
    }
    return true;   // 这几个 API 在 Win10 上不会失败，失败也能靠后续 poll 自愈
}

void WinSource::poll(Metrics& io) {
    // ---------- CPU ----------
    FILETIME idle{}, kernel{}, user{};
    if (GetSystemTimes(&idle, &kernel, &user)) {
        const uint64_t curIdle   = fileTimeToU64(idle);
        const uint64_t curKernel = fileTimeToU64(kernel);
        const uint64_t curUser   = fileTimeToU64(user);

        if (hasPrev_) {
            const uint64_t dIdle   = curIdle   - prevIdle_;
            const uint64_t dKernel = curKernel - prevKernel_;
            const uint64_t dUser   = curUser   - prevUser_;

            // kernel time 里【已经包含】idle time，所以：
            //   total = kernel + user        （不是 kernel + user + idle）
            //   busy  = total - idle
            // 忘了减 idle 会导致占用率永远偏低。
            const uint64_t total = dKernel + dUser;
            if (total > 0) {
                double busy = static_cast<double>(total) - static_cast<double>(dIdle);
                double pct  = busy * 100.0 / static_cast<double>(total);
                if (pct < 0.0)   pct = 0.0;     // Win8+ 深度 C-state 可能让 idle 略超 total
                if (pct > 100.0) pct = 100.0;
                io.cpuUsage = static_cast<float>(pct);
            }
        }

        prevIdle_   = curIdle;
        prevKernel_ = curKernel;
        prevUser_   = curUser;
        hasPrev_    = true;
    }

    // ---------- 内存 ----------
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        io.memUsage   = static_cast<float>(ms.dwMemoryLoad);
        io.ramTotalGB = static_cast<float>(ms.ullTotalPhys) / 1073741824.0f;
        io.ramUsedGB  = static_cast<float>(ms.ullTotalPhys - ms.ullAvailPhys) / 1073741824.0f;
    }

    // ---------- 自身占用 ----------
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                             sizeof(pmc))) {
        io.selfMemMB = static_cast<float>(pmc.WorkingSetSize) / 1048576.0f;
    }
}
