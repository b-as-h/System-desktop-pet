#pragma once
#include "IDataSource.h"

// ============================================================================
// DiskSource —— 系统盘已用百分比
// ============================================================================
class DiskSource : public IDataSource {
public:
    bool init() override { return true; }
    void poll(Metrics& io) override;
    const char* name() const override { return "DiskSource"; }
};
