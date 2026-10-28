#include "MetricsHub.h"

#include <QThread>
#include <QTimer>

#include <windows.h>   // GetTickCount64

#include "WinSource.h"
#include "GpuSource.h"
#include "NetSource.h"
#include "DiskSource.h"
#include "FreqSource.h"
#include "DwmSource.h"
#include "HwinfoSource.h"

// ---------------------------------------------------------------- Collector

Collector::Collector(QObject* parent) : QObject(parent) {}

Collector::~Collector() = default;

void Collector::begin(int intervalMs) {
    // 这个函数在工作线程里执行（由 QThread::started 触发），
    // 所以这里创建的 QTimer 天然属于工作线程，不会和 UI 抢事件循环。
    if (!timer_) {
        timer_ = new QTimer(this);
        connect(timer_, &QTimer::timeout, this, &Collector::tick);
    }
    timer_->start(intervalMs);

    // 先立刻采一次，让界面不用空等一个周期
    tick();
}

void Collector::shutdown() {
    if (timer_) {
        timer_->stop();
        timer_->deleteLater();
        timer_ = nullptr;
    }
}

void Collector::tick() {
    // 数据源在工作线程里懒加载：
    // PDH 的 query 句柄有线程亲和性，必须在用它的线程里创建。
    if (!win_) {
        win_    = std::make_unique<WinSource>();
        gpu_    = std::make_unique<GpuSource>();
        net_    = std::make_unique<NetSource>();
        disk_   = std::make_unique<DiskSource>();
        freq_   = std::make_unique<FreqSource>();
        dwm_    = std::make_unique<DwmSource>();
        hwinfo_ = std::make_unique<HwinfoSource>();

        win_->init();
        gpu_->init();      // 失败是正常的（老卡不上报 GPU 计数器），不是错误
        net_->init();
        disk_->init();
        freq_->init();
        dwm_->init();
        hwinfo_->init();   // 没装 HWiNFO 时必然失败，属正常情况
    }

    Metrics m;
    win_->poll(m);      // CPU / 内存 / 自身占用
    gpu_->poll(m);      // GPU 占用
    net_->poll(m);      // 网络
    disk_->poll(m);     // 磁盘
    freq_->poll(m);     // CPU 频率
    dwm_->poll(m);      // FPS / 刷新率
    hwinfo_->poll(m);   // 温度 / GPU 频率（需 HWiNFO）
    m.tsMs = GetTickCount64();

    emit metricsReady(m);
}

// -------------------------------------------------------------- MetricsHub

MetricsHub::MetricsHub(QObject* parent) : QObject(parent) {}

MetricsHub::~MetricsHub() {
    stop();
}

void MetricsHub::start(int intervalMs) {
    if (thread_) return;

    thread_    = new QThread;
    thread_->setObjectName("MetricsCollector");
    collector_ = new Collector;
    collector_->moveToThread(thread_);

    // 信号槽传值：collector 在工作线程 emit，lambda 在主线程执行，
    // Metrics 被 Qt 自动深拷贝一份，主线程拿到的永远是完整一致的快照。
    connect(collector_, &Collector::metricsReady, this,
            [this](const Metrics& m) { emit metricsReady(m); });

    connect(thread_, &QThread::started, collector_,
            [this, intervalMs] { collector_->begin(intervalMs); });

    thread_->start();
}

void MetricsHub::stop() {
    if (!thread_) return;

    // 先让 collector 在自己的线程里把定时器停掉并释放，
    // 否则从主线程删它会在析构时触发 "Timers cannot be stopped from another thread"
    QMetaObject::invokeMethod(collector_, [this] { collector_->shutdown(); },
                              Qt::BlockingQueuedConnection);

    thread_->quit();
    thread_->wait(3000);

    delete collector_;
    collector_ = nullptr;

    delete thread_;
    thread_ = nullptr;
}
