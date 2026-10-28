#pragma once
#include "Metrics.h"

// ============================================================================
// IDataSource —— 采集层统一接口
//
// 约定：每个实现只填自己负责的字段，其他字段一律不动。
// 这样加一个数据源（比如将来的 SerialSource 反向读单片机）不用改任何现有代码。
//
// init() 失败不是致命错误：这台机器没有独显、没装 HWiNFO，都属正常。
// 返回 false 只是表示"我没数据"，MetricsHub 会跳过它。
// ============================================================================
class IDataSource {
public:
    virtual ~IDataSource() = default;

    virtual bool init() = 0;              // 初始化，失败返回 false（非致命）
    virtual void poll(Metrics& io) = 0;   // 只填自己的字段
    virtual const char* name() const = 0;
};
