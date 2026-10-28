#pragma once
#include <QElapsedTimer>
#include <QPoint>
#include <QWidget>

#include "core/Metrics.h"
#include "PetRenderer.h"
#include "PetState.h"

class QTimer;

// ============================================================================
// PetWindow —— 透明、置顶、无边框、可拖动
//
// 窗口本身不做任何采集，只负责画和接收鼠标。
// 数据从 MetricsHub 通过信号槽送进来。
// ============================================================================
class PetWindow : public QWidget {
    Q_OBJECT
public:
    explicit PetWindow(QWidget* parent = nullptr);

    void setMetrics(const Metrics& m);

    bool clickThrough() const { return clickThrough_; }
    bool noActivate()   const { return noActivate_; }

public slots:
    void setClickThrough(bool on);
    void setNoActivate(bool on);
    void setAlwaysOnTop(bool on);

signals:
    void movedTo(const QPoint& pos);
    void contextMenuRequested(const QPoint& globalPos);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void moveEvent(QMoveEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    void tick();
    void applyNativeFlags();
    void refreshWindow();   // 改完窗口标志后需要重新 show 才生效

    QTimer*       animTimer_ = nullptr;
    QElapsedTimer clock_;
    double        tSec_      = 0.0;
    double        lastSec_   = 0.0;

    Metrics      metrics_;
    PetState     state_;
    PetRenderer  renderer_;

    QPoint dragOffset_;
    bool   dragging_     = false;
    bool   clickThrough_ = false;
    bool   noActivate_   = true;
    int    forcedMood_   = -1;   // 调试用：>=0 时强制该情绪（见 SIP_MOOD）
};
