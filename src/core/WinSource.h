#pragma once
#include "IDataSource.h"
#include <cstdint>

// ============================================================================
// WinSource —— 零依赖 Win32 采集：CPU / 内存 / 自身占用
// ============================================================================
class WinSource : public IDataSource {
public:
    bool init() override;
    void poll(Metrics& io) override;
    const char* name() const override { return "WinSource"; }

private:
    bool     hasPrev_    = false;
    uint64_t prevIdle_   = 0;
    uint64_t prevKernel_ = 0;
    uint64_t prevUser_   = 0;
};
