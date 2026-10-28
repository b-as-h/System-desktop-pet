#pragma once
#include "core/Metrics.h"

// ============================================================================
// 情绪枚举
// ============================================================================
enum class Mood {
    Idle,     // 正常
    Busy,     // 忙碌（CPU/GPU 高负载）
    Hot,      // 过热（优先级最高）
    Sleepy    // 困倦（长时间无交互且负载低）
};

// ============================================================================
// PetState —— 情绪状态机
//
// 核心是【迟滞 hysteresis】：
//   如果写成 cpu > 70 ? Busy : Idle，CPU 在 70% 上下抖动时角色会每帧变脸。
//   正确做法是进入阈值和退出阈值不同（进 70%、退 60%），
//   并且要求候选状态【持续 N 秒】才真正切换。
//
// 注意：本期没有温度数据（需要第三方驱动），所以 Hot 暂时用负载近似。
//       阶段 5 接入 cpuTemp 后，把 hotIn/hotOut 改成温度阈值即可，
//       逻辑一行都不用动。
// ============================================================================
class PetState {
public:
    void update(const Metrics& m, double dtSec);

    Mood mood() const { return mood_; }

    // 用户碰了桌宠 → 重置休眠计时
    void poke() { idleSec_ = 0.0; }

private:
    Mood evaluate(const Metrics& m) const;

    Mood   mood_         = Mood::Idle;
    Mood   candidate_    = Mood::Idle;
    double candidateSec_ = 0.0;
    double idleSec_      = 0.0;
};
