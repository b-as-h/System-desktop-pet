#include "FreqSource.h"

#include <windows.h>
#include <pdh.h>

FreqSource::~FreqSource() {
    if (query_) {
        PdhCloseQuery(static_cast<PDH_HQUERY>(query_));
        query_ = nullptr;
    }
}

bool FreqSource::init() {
    // 标称频率：注册表里每个逻辑核都有一个 ~MHz，取 0 号即可
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0, KEY_READ, &key) == ERROR_SUCCESS) {
        DWORD mhz = 0, size = sizeof(mhz), type = 0;
        if (RegQueryValueExW(key, L"~MHz", nullptr, &type,
                             reinterpret_cast<LPBYTE>(&mhz), &size) == ERROR_SUCCESS) {
            baseMHz_ = static_cast<float>(mhz);
        }
        RegCloseKey(key);
    }

    PDH_HQUERY q = nullptr;
    if (PdhOpenQueryW(nullptr, 0, &q) != ERROR_SUCCESS) return false;

    PDH_HCOUNTER h = nullptr;
    if (PdhAddCounterW(q, L"\\Processor Information(_Total)\\% Processor Performance",
                       0, &h) != ERROR_SUCCESS) {
        PdhCloseQuery(q);
        return false;
    }

    PdhCollectQueryData(q);   // 建立基线
    query_   = q;
    counter_ = h;
    return true;
}

void FreqSource::poll(Metrics& io) {
    if (!query_ || !counter_ || baseMHz_ <= 0.f) return;

    PDH_HQUERY q = static_cast<PDH_HQUERY>(query_);
    if (PdhCollectQueryData(q) != ERROR_SUCCESS) return;

    PDH_FMT_COUNTERVALUE v{};
    if (PdhGetFormattedCounterValue(static_cast<PDH_HCOUNTER>(counter_),
                                    PDH_FMT_DOUBLE, nullptr, &v) != ERROR_SUCCESS) {
        return;
    }
    if (v.CStatus != ERROR_SUCCESS) return;

    // % Processor Performance 可能 > 100（睿频）
    const double mhz = static_cast<double>(baseMHz_) * v.doubleValue / 100.0;
    if (mhz > 0.0 && mhz < 20000.0) {
        io.cpuFreqMHz = static_cast<float>(mhz);
    }
}
