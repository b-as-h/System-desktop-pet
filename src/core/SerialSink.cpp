#include "SerialSink.h"

#include <algorithm>

namespace {

uint8_t u8FromPct(float v) {
    if (v < 0.f) return 0xFF;                       // -1 = 无数据
    const int i = static_cast<int>(v + 0.5f);
    return static_cast<uint8_t>(std::clamp(i, 0, 100));
}

// 温度：0-150℃，0xFF = 无数据
uint8_t u8FromTemp(float v) {
    if (v < 0.f) return 0xFF;
    const int i = static_cast<int>(v + 0.5f);
    return static_cast<uint8_t>(std::clamp(i, 0, 150));
}

// 频率：以 100MHz 为单位，0xFF = 无数据
uint8_t u8FromMHz(float mhz) {
    if (mhz <= 0.f) return 0xFF;
    const int i = static_cast<int>(mhz / 100.0f + 0.5f);
    return static_cast<uint8_t>(std::clamp(i, 0, 254));
}

uint16_t u16FromKBs(float v) {
    if (v < 0.f) return 0xFFFF;
    const int i = static_cast<int>(v + 0.5f);
    return static_cast<uint16_t>(std::clamp(i, 0, 65534));
}

void putU16LE(uint8_t* dst, uint16_t v) {
    dst[0] = static_cast<uint8_t>(v & 0xFF);
    dst[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

} // namespace

uint8_t SerialSink::crc8(const uint8_t* data, int len) {
    uint8_t crc = 0x00;
    for (int i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07)
                               : static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}

SerialSink::Frame SerialSink::build(const Metrics& m) {
    Frame f{};
    f[0]  = 0xAA;
    f[1]  = 0x55;
    f[2]  = 0x02;                                   // 协议版本
    f[3]  = u8FromPct(m.cpuUsage);
    f[4]  = u8FromPct(m.memUsage);
    f[5]  = u8FromPct(m.gpuUsage);
    f[6]  = u8FromTemp(m.cpuTemp);
    f[7]  = u8FromTemp(m.gpuTemp);
    putU16LE(&f[8],  u16FromKBs(m.netDownKBs));
    putU16LE(&f[10], u16FromKBs(m.netUpKBs));
    f[12] = u8FromMHz(m.cpuFreqMHz);
    f[13] = (m.fps < 0.f) ? 0xFF
                          : static_cast<uint8_t>(std::clamp(static_cast<int>(m.fps + 0.5f), 0, 254));
    f[14] = crc8(f.data(), kFrameSize - 1);         // CRC 覆盖前 14 字节
    return f;
}

void SerialSink::send(const Metrics& /*m*/) {
    // TODO(阶段 7)：CreateFileW(L"\\\\.\\COM5") → 发 build(m) 的 15 字节
}
