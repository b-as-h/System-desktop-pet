#pragma once
#include "IDataSource.h"
#include <string>
#include <utility>
#include <vector>

// ============================================================================
// GpuSource —— 用 Windows 自带的 PDH 性能计数器读 GPU 占用
//
// 为什么不用 NVML：
//   nvml.dll 只认 NVIDIA 显卡。本机是 AMD R7 430，LoadLibrary 必然失败，
//   gpuUsage 会永远停在 -1。而 PDH 的 \GPU Engine(*)\Utilization Percentage
//   是 WDDM 2.x 提供的系统级计数器，AMD / Intel / NVIDIA 全通用，
//   用户态、零依赖、不用装任何厂商 SDK。
//
// 句柄用 void* 存，避免 windows.h / pdh.h 污染这个头文件。
// ============================================================================
class GpuSource : public IDataSource {
public:
    ~GpuSource() override;

    bool init() override;
    void poll(Metrics& io) override;
    const char* name() const override { return "GpuSource"; }

private:
    void* query_ = nullptr;                                        // PDH_HQUERY
    std::vector<std::pair<void*, std::string>> counters_;           // PDH_HCOUNTER + 引擎类型
};
