#include "RingPanel.h"

#include <QContextMenuEvent>
#include <QFont>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QSettings>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace {

// ---------------------------------------------------------------- 尺寸常量
constexpr int kPanelW = 570;
constexpr int kPanelH = 200;

constexpr double kRingY  = 108.0;
constexpr double kRingR  = 42.0;
constexpr double kRingX[4] = { 72.0, 184.0, 296.0, 408.0 };

constexpr double kFpsCx = 513.0;
constexpr double kFpsCy = 108.0;

constexpr double kArcW = 9.0;

// ---------------------------------------------------------------- 配色
const QColor kCpuColor(78, 205, 196);
const QColor kGpuColor(255, 169, 77);
const QColor kRamColor(167, 139, 250);
const QColor kNetColor(110, 231, 183);
const QColor kFpsColor(230, 237, 247);

const QColor kTextMain(240, 245, 252);
const QColor kTextSub(255, 255, 255, 130);
const QColor kTextLabel(255, 255, 255, 170);

// ---------------------------------------------------------------- 格式化
QString fmtTemp(float t) {
    if (t < 0.f) return QStringLiteral("—");
    return QString::number(t, 'f', 0) + QStringLiteral("°C");
}

QString fmtGhz(float mhz) {
    if (mhz <= 0.f) return QStringLiteral("—");
    return QString::number(mhz / 1000.0f, 'f', 2) + QStringLiteral("GHz");
}

QString fmtMhz(float mhz) {
    if (mhz <= 0.f) return QStringLiteral("—");
    return QString::number(mhz, 'f', 0) + QStringLiteral("MHz");
}

// 紧凑速率：小于 1MB/s 用 K，否则用 M
QString fmtSpeedShort(float kbs) {
    if (kbs >= 1024.f) return QString::number(kbs / 1024.0f, 'f', 1) + QStringLiteral("M");
    return QString::number(kbs, 'f', 1) + QStringLiteral("K");
}

QString fmtSpeedFull(float kbs) {
    if (kbs >= 1024.f) return QString::number(kbs / 1024.0f, 'f', 1) + QStringLiteral("M/s");
    return QString::number(kbs, 'f', 1) + QStringLiteral("K/s");
}

QString joinParts(const QStringList& parts) {
    return parts.isEmpty() ? QStringLiteral("—") : parts.join(QStringLiteral(" · "));
}

// ---------------------------------------------------------------- 画环
void drawRing(QPainter& p,
              const QPointF& c, double r,
              double ratio,
              const QColor& col,
              const QString& centerText,
              int centerPx) {
    const QRectF box(c.x() - r, c.y() - r, r * 2.0, r * 2.0);

    // 底槽
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 22), kArcW, Qt::SolidLine, Qt::FlatCap));
    p.drawEllipse(box);

    // 进度弧：从 12 点方向顺时针
    if (ratio > 0.002) {
        const double clamped = std::clamp(ratio, 0.0, 1.0);
        p.setPen(QPen(col, kArcW, Qt::SolidLine, Qt::RoundCap));
        const int start = 90 * 16;
        const int span  = static_cast<int>(-clamped * 360.0 * 16.0);
        p.drawArc(box, start, span);
    }

    // 中心文字
    QFont f = p.font();
    f.setPixelSize(centerPx);
    f.setBold(true);
    p.setFont(f);
    p.setPen(kTextMain);
    p.drawText(QRectF(c.x() - r + 6, c.y() - r + 6, (r - 6) * 2, (r - 6) * 2),
               Qt::AlignCenter, centerText);
}

void drawCaption(QPainter& p, double cx, double y, const QString& text,
                 int px, const QColor& color, bool bold = false) {
    QFont f = p.font();
    f.setPixelSize(px);
    f.setBold(bold);
    p.setFont(f);
    p.setPen(color);
    p.drawText(QRectF(cx - 80, y - 10, 160, 20), Qt::AlignCenter, text);
}

} // namespace

RingPanel::RingPanel(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    resize(kPanelW, kPanelH);
    setCursor(Qt::OpenHandCursor);
    setWindowTitle(QStringLiteral("系统信息"));
    // 位置在 showEvent 里恢复，见那里的注释
}

void RingPanel::setMetrics(const Metrics& m) {
    m_ = m;
    update();
}

// ---------------------------------------------------------------- 绘制

void RingPanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // ---- 背板 ----
    // 用不透明色：留一点透明度会让桌面文字透出来，在深色壁纸上看着像重影。
    // 圆角仍由 QPainter 抗锯齿处理，边缘依然是平滑的。
    const QRectF box(0.5, 0.5, width() - 1.0, height() - 1.0);
    p.setPen(QPen(QColor(255, 255, 255, 30), 1.0));
    p.setBrush(QColor(13, 17, 24));
    p.drawRoundedRect(box, 16.0, 16.0);

    // 顶部一条极淡的渐隐线，让面板不那么"死"
    {
        QLinearGradient g(0, 0, width(), 0);
        g.setColorAt(0.0, QColor(78, 205, 196, 0));
        g.setColorAt(0.5, QColor(78, 205, 196, 70));
        g.setColorAt(1.0, QColor(78, 205, 196, 0));
        p.setPen(QPen(g, 1.0));
        p.drawLine(QPointF(18, 1.0), QPointF(width() - 18, 1.0));
    }

    // ---- 四条环 ----
    const bool gpuOk = m_.gpuUsage >= 0.f;

    struct Item {
        QString label;
        QString center;
        QString sub;
        double  ratio;
        QColor  color;
        bool    hasData;
    };

    QStringList cpuParts;
    if (m_.cpuTemp >= 0.f)    cpuParts << fmtTemp(m_.cpuTemp);
    if (m_.cpuFreqMHz > 0.f)  cpuParts << fmtGhz(m_.cpuFreqMHz);

    QStringList gpuParts;
    if (m_.gpuTemp >= 0.f)    gpuParts << fmtTemp(m_.gpuTemp);
    if (m_.gpuClockMHz > 0.f) gpuParts << fmtMhz(m_.gpuClockMHz);

    const QString ramSub = (m_.ramTotalGB > 0.f)
        ? QStringLiteral("%1/%2G")
              .arg(static_cast<double>(m_.ramUsedGB), 0, 'f', 1)
              .arg(static_cast<double>(m_.ramTotalGB), 0, 'f', 1)
        : QStringLiteral("—");

    const double netTotal = static_cast<double>(m_.netDownKBs) + m_.netUpKBs;

    // 网速用对数刻度：线性刻度下几 KB/s 的日常流量在环上几乎看不见，
    // 而 10MB/s 才满格。log 之后 3KB/s 也有 16%，观感更合理。
    const double netRatio = std::log10(1.0 + netTotal) / std::log10(1.0 + 10240.0);

    const Item items[4] = {
        { QStringLiteral("CPU"),
          QString::number(m_.cpuUsage, 'f', 0) + QStringLiteral("%"),
          joinParts(cpuParts),
          m_.cpuUsage / 100.0, kCpuColor, true },

        { QStringLiteral("GPU"),
          gpuOk ? QString::number(m_.gpuUsage, 'f', 0) + QStringLiteral("%")
                : QStringLiteral("—"),
          gpuOk ? joinParts(gpuParts) : QStringLiteral("—"),
          gpuOk ? m_.gpuUsage / 100.0 : 0.0, kGpuColor, gpuOk },

        { QStringLiteral("RAM"),
          QString::number(m_.memUsage, 'f', 0) + QStringLiteral("%"),
          ramSub,
          m_.memUsage / 100.0, kRamColor, true },

        { QStringLiteral("NET"),
          QStringLiteral("↓") + fmtSpeedShort(m_.netDownKBs),
          QStringLiteral("↑") + fmtSpeedFull(m_.netUpKBs),
          std::min(1.0, netRatio), kNetColor, true },
    };

    for (int i = 0; i < 4; ++i) {
        const Item& it = items[i];
        const QPointF c(kRingX[i], kRingY);

        drawCaption(p, c.x(), 44.0, it.label, 11, kTextLabel, true);

        // NET 的中心文字更宽，字号小一点
        drawRing(p, c, kRingR, it.ratio, it.color, it.center,
                 (i == 3) ? 14 : 19);

        drawCaption(p, c.x(), 172.0, it.sub, 11, kTextSub);
    }

    // ---- FPS ----
    {
        // 分隔竖线
        p.setPen(QPen(QColor(255, 255, 255, 24), 1.0));
        p.drawLine(QPointF(462.0, 40.0), QPointF(462.0, 160.0));

        drawCaption(p, kFpsCx, 44.0, QStringLiteral("FPS"), 11, kTextLabel, true);

        const bool fpsOk = m_.fps >= 0.f;
        QFont f = p.font();
        f.setPixelSize(44);
        f.setBold(true);
        p.setFont(f);
        p.setPen(fpsOk ? kFpsColor : QColor(255, 255, 255, 70));
        p.drawText(QRectF(kFpsCx - 60, kFpsCy - 32, 120, 60), Qt::AlignCenter,
                   fpsOk ? QString::number(m_.fps, 'f', 0) : QStringLiteral("—"));

        if (m_.refreshHz > 0.f) {
            drawCaption(p, kFpsCx, 172.0,
                        QString::number(m_.refreshHz, 'f', 0) + QStringLiteral("Hz"),
                        11, kTextSub);
        }
    }
}

// ---------------------------------------------------------------- 交互

void RingPanel::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        dragOffset_ = e->globalPosition().toPoint() - frameGeometry().topLeft();
        dragging_   = true;
        setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void RingPanel::mouseMoveEvent(QMouseEvent* e) {
    if (dragging_ && (e->buttons() & Qt::LeftButton)) {
        move(e->globalPosition().toPoint() - dragOffset_);
        e->accept();
        return;
    }
    QWidget::mouseMoveEvent(e);
}

void RingPanel::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        setCursor(Qt::OpenHandCursor);
        savePos();
        e->accept();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void RingPanel::contextMenuEvent(QContextMenuEvent* e) {
    emit contextMenuRequested(e->globalPos());
    e->accept();
}

// ---------------------------------------------------------------- 位置记忆

void RingPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    // 位置恢复必须放在 showEvent 里、而且直接用【存下来的值】做判断。
    // 踩过的坑：在构造函数里 move() 是延迟应用的，showEvent 里读 frameGeometry()
    // 拿到的还是 (0,0)，边界校验形同虚设，结果面板会被恢复到屏幕外（比如 x=-719）。
    if (!posReady_) {
        posReady_ = true;
        restorePos();
    }
}

void RingPanel::restorePos() {
    QSettings s;
    const QPoint saved = s.value(QStringLiteral("ring/pos")).toPoint();

    QScreen* scr = nullptr;
    if (!saved.isNull()) {
        scr = QGuiApplication::screenAt(QRect(saved, size()).center());
    }
    if (!scr) scr = QGuiApplication::primaryScreen();
    if (!scr) return;

    const QRect a = scr->availableGeometry();

    QPoint p = saved;
    if (saved.isNull()) {
        // 首次运行：右上角
        p = QPoint(a.right() - width() - 40, a.top() + 40);
    } else {
        // 换过显示器 / 分辨率、或用户把面板拖出屏幕，都要夹回来
        p.setX(std::clamp(p.x(), a.left(), a.right()  - width()  + 1));
        p.setY(std::clamp(p.y(), a.top(),  a.bottom() - height() + 1));
    }

    move(p);
}

void RingPanel::savePos() {
    QSettings s;
    s.setValue(QStringLiteral("ring/pos"), pos());
}
