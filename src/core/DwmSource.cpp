#include "DwmSource.h"

#include <windows.h>
#include <dwmapi.h>

// 实测结论（别想当然）：
//   DWM_TIMING_INFO 里的帧计数【不是】每个 vblank 都 +1，
//   而是只在桌面真的有内容更新时才递增。桌面完全静止时，
//   2 秒内 cFrame / cRefresh 都只 +1，cFramesDisplayed 甚至不动。
//
//   所以直接拿 cFramesDisplayed 求差，静止时会得到 0——界面上看就是坏的。
//
// 处理方式：
//   有内容更新时 → 用实测的合成帧率
//   连续 3 次几乎没帧（桌面静止）→ 回落到刷新率
//   这样静止时显示 75Hz（显示器确实在以这个频率刷新），
//   放动画 / 跑窗口化游戏时显示真实合成帧率。
//
// 仍然测不到的情况：游戏用独占全屏会绕过 DWM，此时读数为刷新率而非游戏帧率。
// 要拿游戏内真实帧率必须接 PresentMon/RTSS，那是另一套东西。

void DwmSource::poll(Metrics& io) {
    DWM_TIMING_INFO ti{};
    ti.cbSize = sizeof(ti);

    if (FAILED(DwmGetCompositionTimingInfo(nullptr, &ti))) {
        io.fps = -1.f;
        return;
    }

    // 刷新率：rateRefresh 是分数形式，例如 75/1 = 75Hz
    if (ti.rateRefresh.uiDenominator != 0) {
        io.refreshHz = static_cast<float>(ti.rateRefresh.uiNumerator) /
                       static_cast<float>(ti.rateRefresh.uiDenominator);
    }

    const uint64_t frames = ti.cFramesDisplayed;
    const uint64_t nowMs  = GetTickCount64();

    if (!hasPrev_) {
        prevFrame_ = frames;
        prevMs_    = nowMs;
        hasPrev_   = true;
        io.fps     = (io.refreshHz > 0.f) ? io.refreshHz : -1.f;
        return;
    }

    const double dt = static_cast<double>(nowMs - prevMs_) / 1000.0;
    const uint64_t prev = prevFrame_;
    prevFrame_ = frames;
    prevMs_    = nowMs;

    if (dt <= 0.05) return;

    double f = (frames >= prev)
             ? static_cast<double>(frames - prev) / dt
             : 0.0;
    if (f > 400.0) f = 400.0;

    if (f >= 1.0) {
        idleCount_ = 0;
        lastGood_  = f;
    } else if (idleCount_ < 1000) {
        ++idleCount_;
    }

    if (idleCount_ >= 3) {
        // 桌面持续静止：按刷新率显示（显示器确实在以这个频率刷新）
        io.fps = (io.refreshHz > 0.f) ? io.refreshHz : -1.f;
    } else if (f >= 1.0) {
        io.fps = static_cast<float>(f);
    } else {
        // 只有一两个静止样本：用上一次有效值过渡，避免 75↔0 来回跳
        io.fps = (lastGood_ > 0.0) ? static_cast<float>(lastGood_)
                                   : ((io.refreshHz > 0.f) ? io.refreshHz : -1.f);
    }
}
