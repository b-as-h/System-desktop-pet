#pragma once
#include <QPoint>
#include <QWidget>

#include "core/Metrics.h"

// ============================================================================
// RingPanel —— 圆环仪表盘
//
// 呈现参考图上的全部数据：
//   CPU 占用 + 温度 + 频率
//   GPU 占用 + 温度 + 频率
//   RAM 占用 + 已用/总量
//   NET 下行 / 上行
//   FPS
//
// 纯展示 + 可拖动，不做任何采集。数据由 MetricsHub 通过信号槽送进来。
// ============================================================================
class RingPanel : public QWidget {
    Q_OBJECT
public:
    explicit RingPanel(QWidget* parent = nullptr);

    void setMetrics(const Metrics& m);

signals:
    void contextMenuRequested(const QPoint& globalPos);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void showEvent(QShowEvent* e) override;

private:
    void restorePos();
    void savePos();

    Metrics m_;
    QPoint  dragOffset_;
    bool    dragging_ = false;
    bool    posReady_ = false;   // 位置只在第一次 show 时恢复一次
};
