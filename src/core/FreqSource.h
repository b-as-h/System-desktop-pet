#pragma once
#include "IDataSource.h"
#include <cstdint>

// ============================================================================
// FreqSource —— CPU 当前主频
//
// 用 PDH 的 "\Processor Information(_Total)\% Processor Performance"：
//   该值 = 当前频率 / 标称频率 × 100，睿频时会超过 100。
//   乘以注册表里的 ~MHz（标称频率）就得到当前 MHz。
//
// 为什么不直接读 CallNtPowerInformation：
//   它返回的 CurrentMhz 在很多平台上不跟随睿频变化，读数不准。
// ============================================================================
class FreqSource : public IDataSource {
public:
    ~FreqSource() override;

    bool init() override;
    void poll(Metrics& io) override;
    const char* name() const override { return "FreqSource"; }

private:
    float baseMHz_ = 0.f;   // 标称频率
    void* query_   = nullptr;   // PDH_HQUERY
    void* counter_ = nullptr;   // PDH_HCOUNTER
};
