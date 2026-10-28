#pragma once
#include "IDataSource.h"
#include <cstdint>

// ============================================================================
// NetSource —— 网络上下行速率
//
// 坑很多，见 .cpp 里的注释。核心一条：
// GetIfTable2 返回的不只是物理网卡，还包含 NDIS 过滤驱动接口
// （WFP Native MAC Layer LightWeight Filter / QoS Packet Scheduler 等），
// 同一块网卡会出现 4-5 条记录，必须用 HardwareInterface 标志筛掉。
// ============================================================================
class NetSource : public IDataSource {
public:
    bool init() override { return true; }
    void poll(Metrics& io) override;
    const char* name() const override { return "NetSource"; }

private:
    bool     hasPrev_ = false;
    uint64_t prevIn_  = 0;
    uint64_t prevOut_ = 0;
};
