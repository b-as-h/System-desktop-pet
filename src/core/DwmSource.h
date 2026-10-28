#pragma once
#include "IDataSource.h"
#include <cstdint>

// ============================================================================
// DwmSource —— 桌面合成帧率（FPS）与刷新率
//
// 原理：DWM 每把一帧合成到屏幕上，DWM_TIMING_INFO.cFramesDisplayed 就 +1。
//       两次采样求差除以时间，就是实际呈现的帧率。
//
// 局限（要说清楚，不能装作能测游戏帧率）：
//   - 反映的是【桌面合成层】的帧率，上限是显示器刷新率
//   - 游戏用独占全屏（fullscreen exclusive）时会绕过 DWM，此时读数为 0
//   - 想要游戏内真实帧率得接 PresentMon/RTSS，那是另一套东西
// ============================================================================
class DwmSource : public IDataSource {
public:
    bool init() override { return true; }
    void poll(Metrics& io) override;
    const char* name() const override { return "DwmSource"; }

private:
    bool     hasPrev_   = false;
    uint64_t prevFrame_ = 0;
    uint64_t prevMs_    = 0;
    int      idleCount_ = 0;   // 连续"几乎没帧"的次数，用于回落到刷新率
    double   lastGood_  = -1.0; // 上一次测到的有效帧率，用于过渡
};
