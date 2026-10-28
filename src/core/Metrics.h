#pragma once
#include <cstdint>

// ============================================================================
// Metrics —— 全项目唯一的耦合点
//
// 设计约束（很重要，别破坏）：
//   1. 纯 POD，不含任何 Qt / Win32 类型。将来原样复制进 STM32 固件也能编译。
//   2. 字段语义固定。加字段可以，改字段含义不行——MCU 端按这个协议解析。
//   3. -1 表示"该机器没有这项数据"，不是错误。UI 层据此显示 "—"。
// ============================================================================
struct Metrics {
    // ---- 主指标（对应参考图上的四行）----
    float    cpuUsage    = 0.f;    // 0-100，全核平均
    float    memUsage    = 0.f;    // 0-100，物理内存占用百分比
    float    gpuUsage    = -1.f;   // 0-100，-1 = 无数据
    float    netDownKBs  = 0.f;    // 下行 KB/s
    float    netUpKBs    = 0.f;    // 上行 KB/s

    // ---- 附加信息（参考图右侧的小字）----
    float    cpuTemp     = -1.f;   // 摄氏度，需 HWiNFO
    float    gpuTemp     = -1.f;   // 摄氏度，需 HWiNFO
    float    cpuFreqMHz  = -1.f;   // 当前主频（含睿频）
    float    gpuClockMHz = -1.f;   // GPU 频率，需 HWiNFO
    float    ramUsedGB   = 0.f;    // 已用内存 GB
    float    ramTotalGB  = 0.f;    // 总内存 GB
    float    fps         = -1.f;   // 桌面合成帧率
    float    refreshHz   = -1.f;   // 显示器刷新率

    // ---- 其他 ----
    float    diskUsedPct = 0.f;    // 0-100，系统盘已用百分比

    // 用 uint64 而不是 uint32：
    // uint32 毫秒 49.7 天回绕，而桌宠是常驻程序，跑一个月就会误判"数据过期"。
    uint64_t tsMs        = 0;      // GetTickCount64()，单调递增

    float    selfMemMB   = 0.f;    // 自身占用，用于监控自己别吃太多
};
