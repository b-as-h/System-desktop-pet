#pragma once
#include "IDataSource.h"
#include <cstdint>

// ============================================================================
// HwinfoSource —— 从 HWiNFO 的共享内存里读温度 / GPU 频率
//
// 为什么必须靠第三方：
//   CPU 核心温度来自 MSR 寄存器，读它需要内核权限。用户态程序一律读不到，
//   必须借 HWiNFO 或 LibreHardwareMonitor 这类自带驱动的工具。
//   实测本机 MSAcpi_ThermalZoneTemperature 直接返回"拒绝访问"，
//   而且即使提权，桌面主板返回的通常也是 ACPI 热区温度而非核心温度。
//
// 前提：HWiNFO 需要在设置里勾选 "Shared Memory Support"。
//       免费版该功能 12 小时后会冻结，所以这里做了【数据新鲜度检测】：
//       如果 poll_time 长时间不前进，就认为数据已冻结，返回 -1。
//
// 没装 HWiNFO / 没开共享内存 → attach 失败 → 所有字段保持 -1，
// 界面显示 "—"。这是正常情况，不是错误。
// ============================================================================
class HwinfoSource : public IDataSource {
public:
    ~HwinfoSource() override;

    bool init() override;
    void poll(Metrics& io) override;
    const char* name() const override { return "HwinfoSource"; }

    bool attached() const { return view_ != nullptr; }

private:
    bool attach();
    void detach();
    bool rescan();
    double readValue(int32_t offset) const;

    const uint8_t* view_     = nullptr;
    size_t         viewSize_ = 0;

    uint32_t offReadings_ = 0;
    uint32_t numReadings_ = 0;
    uint32_t stride_      = 0;

    int32_t  cpuTempOff_  = -1;
    int32_t  gpuTempOff_  = -1;
    int32_t  gpuClockOff_ = -1;

    int64_t  lastPollTime_ = -1;
    int      staleCount_   = 0;
    uint64_t lastTryMs_    = 0;
};
