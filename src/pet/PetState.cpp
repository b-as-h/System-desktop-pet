#include "PetState.h"

#include <algorithm>

namespace {
// 进入阈值 / 退出阈值必须分开，这是迟滞的全部意义
constexpr float kHotIn    = 85.f;   // 超过 85% 才开始考虑 Hot
constexpr float kHotOut   = 75.f;   // 但已经 Hot 了要掉到 75% 以下才退出
constexpr float kBusyIn   = 70.f;
constexpr float kBusyOut  = 60.f;

constexpr double kHotHoldSec   = 1.5;   // 过热反应要快
constexpr double kOtherHoldSec = 3.0;   // 其他状态稳一点，避免频繁变脸
constexpr double kSleepySec    = 300.0; // 5 分钟没交互
} // namespace

Mood PetState::evaluate(const Metrics& m) const {
    // 取 CPU 和 GPU 里更忙的那个当负载指标
    float load = m.cpuUsage;
    if (m.gpuUsage > 0.f) load = std::max(load, m.gpuUsage);

    // 1. 过热：优先级最高
    if (mood_ == Mood::Hot) {
        if (load >= kHotOut) return Mood::Hot;
    } else if (load >= kHotIn) {
        return Mood::Hot;
    }

    // 2. 忙碌
    if (mood_ == Mood::Busy) {
        if (load >= kBusyOut) return Mood::Busy;
    } else if (load >= kBusyIn) {
        return Mood::Busy;
    }

    // 3. 困倦：长时间无交互 + 负载低
    if (idleSec_ >= kSleepySec && load < 15.f) return Mood::Sleepy;

    return Mood::Idle;
}

void PetState::update(const Metrics& m, double dtSec) {
    idleSec_ += dtSec;

    const Mood want = evaluate(m);

    // 已经是这个状态：清空候选计时
    if (want == mood_) {
        candidate_    = want;
        candidateSec_ = 0.0;
        return;
    }

    // 候选状态变了，重新计时
    if (want != candidate_) {
        candidate_    = want;
        candidateSec_ = 0.0;
    }

    candidateSec_ += dtSec;

    const double hold = (want == Mood::Hot) ? kHotHoldSec : kOtherHoldSec;
    if (candidateSec_ >= hold) {
        mood_         = want;
        candidateSec_ = 0.0;
    }
}
