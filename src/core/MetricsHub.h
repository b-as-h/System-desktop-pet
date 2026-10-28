#pragma once
#include <QObject>
#include <QMetaType>
#include <memory>

#include "Metrics.h"

// Metrics 走跨线程信号槽，需要注册元类型
Q_DECLARE_METATYPE(Metrics)

class QThread;
class QTimer;
class WinSource;
class GpuSource;
class NetSource;
class DiskSource;
class FreqSource;
class DwmSource;
class HwinfoSource;

// ============================================================================
// Collector —— 真正干活的对象，被 moveToThread 到工作线程
//
// 为什么要单独一个类而不是直接 move MetricsHub：
//   MetricsHub 留在主线程当"门面"，Collector 在工作线程里采集，
//   两边只靠信号槽传值（Qt 跨线程自动排队 + 深拷贝 Metrics），
//   不需要任何 mutex，也不会出现"CPU 是新值、GPU 是旧值"的撕裂。
// ============================================================================
class Collector : public QObject {
    Q_OBJECT
public:
    explicit Collector(QObject* parent = nullptr);
    ~Collector() override;

public slots:
    void begin(int intervalMs);
    void shutdown();
    void tick();

signals:
    void metricsReady(const Metrics& m);

private:
    QTimer* timer_ = nullptr;
    std::unique_ptr<WinSource>     win_;
    std::unique_ptr<GpuSource>     gpu_;
    std::unique_ptr<NetSource>     net_;
    std::unique_ptr<DiskSource>    disk_;
    std::unique_ptr<FreqSource>    freq_;
    std::unique_ptr<DwmSource>     dwm_;
    std::unique_ptr<HwinfoSource>  hwinfo_;
};

// ============================================================================
// MetricsHub —— 主线程门面，负责起线程、收数据、转发
// ============================================================================
class MetricsHub : public QObject {
    Q_OBJECT
public:
    explicit MetricsHub(QObject* parent = nullptr);
    ~MetricsHub() override;

    void start(int intervalMs = 1000);
    void stop();

signals:
    void metricsReady(const Metrics& m);

private:
    QThread*   thread_    = nullptr;
    Collector* collector_ = nullptr;
};
