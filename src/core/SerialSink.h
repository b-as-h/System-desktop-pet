#pragma once
#include <array>
#include <cstdint>

#include "Metrics.h"

// ============================================================================
// SerialSink —— 串口输出端（接 STM32 用，目前未启用）
//
// 定长二进制帧，MCU 端不需要 JSON 解析器：
//
//   AA 55 | ver | cpu | mem | gpu | cpuTemp | gpuTemp | netDown(2) | netUp(2)
//     2      1     1     1     1       1         1          2            2
//   | cpuFreq/100 | fps | crc8
//          1         1      1                                  合计 15 字节
//
// 约定：
//   - gpu / 温度 / 频率 无效时写 0xFF（对应 Metrics 里的 -1）
//   - cpuFreq 以 100MHz 为单位（0xFF = 无效），即实际 MHz = 值 × 100
//   - fps 取整，0xFF = 无效
//   - 多字节字段一律小端，MCU 端 memcpy 到 uint16_t 即可
//   - CRC-8/ATM：多项式 0x07，初值 0x00，覆盖前 14 字节
// ============================================================================
class SerialSink {
public:
    static constexpr int kFrameSize = 15;
    using Frame = std::array<uint8_t, kFrameSize>;

    static Frame build(const Metrics& m);

    static uint8_t crc8(const uint8_t* data, int len);

    // 目前未接串口；将来在这里做 CreateFileW + WriteFile
    void send(const Metrics& m);
};
