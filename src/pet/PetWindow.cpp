#include "PetWindow.h"

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

PetWindow::PetWindow(QWidget* parent) : QWidget(parent) {
    // Qt::Tool 是关键：不出现在任务栏、不出现在 Alt+Tab
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    // 千万别设 autoFillBackground，设了透明就失效，变成一块黑方块
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(false);

    resize(200, 220);
    setCursor(Qt::OpenHandCursor);

    // 调试用：SIP_MOOD=idle|busy|hot|sleepy 可强制情绪，
    // 方便一次性截图检查四种表情画得对不对，不用真的把 CPU 跑满。
    const QString forced = qEnvironmentVariable("SIP_MOOD").toLower();
    if (forced == QLatin1String("idle"))   forcedMood_ = static_cast<int>(Mood::Idle);
    if (forced == QLatin1String("busy"))   forcedMood_ = static_cast<int>(Mood::Busy);
    if (forced == QLatin1String("hot"))    forcedMood_ = static_cast<int>(Mood::Hot);
    if (forced == QLatin1String("sleepy")) forcedMood_ = static_cast<int>(Mood::Sleepy);

    // 动画循环 ~30fps。
    // 注意：Windows 定时器精度约 15ms，不能假设每次都是 33ms，
    // 所以 tSec_ 用 QElapsedTimer 的真实时间差累加，动画才匀速。
    clock_.start();
    animTimer_ = new QTimer(this);
    animTimer_->setTimerType(Qt::PreciseTimer);
    connect(animTimer_, &QTimer::timeout, this, &PetWindow::tick);
    animTimer_->start(33);
}

void PetWindow::setMetrics(const Metrics& m) {
    metrics_ = m;
}

void PetWindow::tick() {
    const double now = clock_.elapsed() / 1000.0;
    double dt = now - lastSec_;
    lastSec_ = now;

    // 系统休眠 / 主线程卡顿后 dt 会很大，钳一下避免动画瞬移
    if (dt <= 0.0)  dt = 0.001;
    if (dt > 0.25)  dt = 0.25;

    tSec_ += dt;
    state_.update(metrics_, dt);
    update();
}

void PetWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    const Mood mood = (forcedMood_ >= 0) ? static_cast<Mood>(forcedMood_) : state_.mood();
    renderer_.paint(p, size(), metrics_, mood, tSec_);
}

// ---------------------------------------------------------------- 鼠标交互

void PetWindow::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        dragOffset_ = e->globalPosition().toPoint() - frameGeometry().topLeft();
        dragging_   = true;
        state_.poke();          // 碰一下就重置休眠计时
        setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void PetWindow::mouseMoveEvent(QMouseEvent* e) {
    if (dragging_ && (e->buttons() & Qt::LeftButton)) {
        move(e->globalPosition().toPoint() - dragOffset_);
        e->accept();
        return;
    }
    QWidget::mouseMoveEvent(e);
}

void PetWindow::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        state_.poke();
        setCursor(Qt::OpenHandCursor);
        e->accept();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void PetWindow::contextMenuEvent(QContextMenuEvent* e) {
    // 右键菜单由 TrayIcon 统一提供，避免两处维护同一份菜单
    emit contextMenuRequested(e->globalPos());
    e->accept();
}

// ---------------------------------------------------------------- 窗口标志

void PetWindow::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    applyNativeFlags();
}

void PetWindow::moveEvent(QMoveEvent* e) {
    QWidget::moveEvent(e);
    emit movedTo(pos());
}

void PetWindow::applyNativeFlags() {
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

    ex |= WS_EX_TOOLWINDOW;     // 不进任务栏（Qt::Tool 已经设了，双保险）

    // 不抢焦点：点桌宠拖动时，不会把用户正在打字的窗口切走。
    // 万一在某些机器上影响拖动，托盘菜单里可以关掉。
    if (noActivate_) ex |=  WS_EX_NOACTIVATE;
    else             ex &= ~WS_EX_NOACTIVATE;

    // 点击穿透：鼠标事件穿到下面的窗口。
    // 开了之后窗口就"点不动"了，所以托盘菜单必须能关回来——这是后门。
    if (clickThrough_) {
        ex |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
    } else {
        ex &= ~WS_EX_TRANSPARENT;
    }

    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex);
#endif
}

void PetWindow::refreshWindow() {
    // setWindowFlag 会隐藏窗口，必须重新 show 才生效；顺便把位置找回来
    const QPoint p = pos();
    show();
    move(p);
}

void PetWindow::setClickThrough(bool on) {
    if (clickThrough_ == on) return;
    clickThrough_ = on;
    // Qt 6 原生支持，不用手写 SetWindowLong
    setWindowFlag(Qt::WindowTransparentForInput, on);
    refreshWindow();
    applyNativeFlags();
}

void PetWindow::setNoActivate(bool on) {
    if (noActivate_ == on) return;
    noActivate_ = on;
    applyNativeFlags();
}

void PetWindow::setAlwaysOnTop(bool on) {
    setWindowFlag(Qt::WindowStaysOnTopHint, on);
    refreshWindow();
    applyNativeFlags();
}
