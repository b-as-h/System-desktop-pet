#include "PetRenderer.h"

#include <QFont>
#include <QLinearGradient>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Palette {
    QColor light;
    QColor base;
    QColor edge;
};

Palette paletteFor(Mood m) {
    switch (m) {
    case Mood::Busy:   return { QColor(255, 196, 122), QColor(245, 160,  74), QColor(196, 118,  44) };
    case Mood::Hot:    return { QColor(255, 154, 147), QColor(242, 102,  94), QColor(190,  70,  64) };
    case Mood::Sleepy: return { QColor(168, 188, 216), QColor(126, 150, 184), QColor( 88, 108, 138) };
    default:           return { QColor(111, 224, 212), QColor( 63, 191, 178), QColor( 38, 138, 128) };
    }
}

} // namespace

void PetRenderer::paint(QPainter& p, const QSize& size, const Metrics& m, Mood mood, double tSec) {
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const double w = size.width();
    const double h = size.height();
    const double s = std::min(w, h) / 220.0;      // 统一缩放因子，保证不同尺寸比例一致
    const Palette pal = paletteFor(mood);

    // ---------- 呼吸：sin 波驱动纵向缩放，周期 3 秒 ----------
    const double breath = 1.0 + 0.03 * std::sin(tSec * 2.0 * kPi / 3.0);

    // ---------- 忙碌时轻微抖动（整体位移，不影响结构） ----------
    double shakeX = 0.0, shakeY = 0.0;
    if (mood == Mood::Busy) {
        shakeX = std::sin(tSec * 42.0) * 1.8;
        shakeY = std::cos(tSec * 37.0) * 1.0;
    }

    // ---------- 困倦时整体缓慢下沉 ----------
    const double droop = (mood == Mood::Sleepy) ? 4.0 : 0.0;

    p.save();
    p.translate(w * 0.5 + shakeX, h * 0.52 + shakeY + droop);

    const double rx = 62.0 * s;
    const double ry = 58.0 * s * breath;

    // ---------- 地面阴影 ----------
    {
        QRadialGradient sh(QPointF(0, ry * 0.98), rx * 1.15);
        sh.setColorAt(0.0, QColor(0, 0, 0, 60));
        sh.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(sh);
        p.drawEllipse(QPointF(0, ry * 0.98), rx * 1.15, 12.0 * s);
    }

    // ---------- 过热光晕 ----------
    if (mood == Mood::Hot) {
        const double pulse = 0.5 + 0.5 * std::sin(tSec * 5.0);
        QRadialGradient glow(QPointF(0, 0), rx * (1.35 + 0.08 * pulse));
        glow.setColorAt(0.60, QColor(255, 90, 80, 0));
        glow.setColorAt(1.0, QColor(255, 90, 80, static_cast<int>(26 + 22 * pulse)));
        p.setBrush(glow);
        p.drawEllipse(QPointF(0, 0), rx * 1.35, ry * 1.35);
    }

    // ---------- 尾巴（画在身体后面） ----------
    {
        QPainterPath tail;
        const double sway = std::sin(tSec * 1.6) * 7.0 * s;
        tail.moveTo(rx * 0.72, ry * 0.62);
        tail.quadTo(rx * 1.30, ry * 0.55 + sway, rx * 1.12, ry * 0.05 + sway * 0.6);
        QPen tp(pal.edge, 7.0 * s, Qt::SolidLine, Qt::RoundCap);
        p.setPen(tp);
        p.setBrush(Qt::NoBrush);
        p.drawPath(tail);
    }

    // ---------- 耳朵 ----------
    {
        QPen ep(pal.edge, 2.2 * s);
        p.setPen(ep);
        QLinearGradient eg(0, -ry * 0.9, 0, -ry * 0.5);
        eg.setColorAt(0.0, pal.light);
        eg.setColorAt(1.0, pal.base);
        p.setBrush(eg);
        p.drawEllipse(QPointF(-rx * 0.56, -ry * 0.80), 17.0 * s, 19.0 * s);
        p.drawEllipse(QPointF( rx * 0.56, -ry * 0.80), 17.0 * s, 19.0 * s);
    }

    // ---------- 脚 ----------
    {
        p.setPen(Qt::NoPen);
        p.setBrush(pal.edge.lighter(108));
        p.drawEllipse(QPointF(-rx * 0.38, ry * 0.86), 17.0 * s, 9.0 * s);
        p.drawEllipse(QPointF( rx * 0.38, ry * 0.86), 17.0 * s, 9.0 * s);
    }

    // ---------- 身体 ----------
    {
        QLinearGradient bg(0, -ry, 0, ry);
        bg.setColorAt(0.0, pal.light);
        bg.setColorAt(0.55, pal.base);
        bg.setColorAt(1.0, pal.base.darker(112));
        p.setPen(QPen(pal.edge, 2.4 * s));
        p.setBrush(bg);
        p.drawEllipse(QPointF(0, 0), rx, ry);
    }

    // ---------- 高光 ----------
    {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 62));
        p.drawEllipse(QPointF(-rx * 0.38, -ry * 0.48), rx * 0.30, ry * 0.20);
    }

    // ---------- 腮红 ----------
    {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 120, 120, 52));
        p.drawEllipse(QPointF(-rx * 0.62, ry * 0.22), 13.0 * s, 8.0 * s);
        p.drawEllipse(QPointF( rx * 0.62, ry * 0.22), 13.0 * s, 8.0 * s);
    }

    // ---------- 眼睛 ----------
    // 眨眼：每 3.7 秒眨一次，持续 0.13 秒
    const bool blinking = (mood != Mood::Sleepy) &&
                          (std::fmod(tSec, 3.7) < 0.13);

    const double eyeDX = 24.0 * s;
    const double eyeY  = -6.0 * s;
    // 负载越高瞳孔越大（0.85 ~ 1.25 倍），让角色"看起来更紧张"
    const double loadPct = std::clamp(static_cast<double>(m.cpuUsage), 0.0, 100.0) / 100.0;
    const double pupilK  = 0.85 + 0.40 * loadPct;

    auto drawOpenEye = [&](double cx) {
        // 眼白
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(253, 253, 255, 240));
        p.drawEllipse(QPointF(cx, eyeY), 11.5 * s, 12.5 * s);

        if (blinking) {
            p.setPen(QPen(QColor(38, 48, 59), 3.0 * s, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(cx - 9.0 * s, eyeY), QPointF(cx + 9.0 * s, eyeY));
            return;
        }

        // 瞳孔
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(38, 48, 59));
        const double pr = 5.6 * s * pupilK;
        p.drawEllipse(QPointF(cx, eyeY + 1.0 * s), pr, pr * 1.12);

        // 眼神光
        p.setBrush(QColor(255, 255, 255, 235));
        p.drawEllipse(QPointF(cx - pr * 0.42, eyeY - pr * 0.50), pr * 0.34, pr * 0.34);
    };

    auto drawSquintEye = [&](double cx, double dir) {
        // 过热：画成 ">" "<" 的挤压眼
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(38, 48, 59), 3.4 * s, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath path;
        path.moveTo(cx - 8.0 * s * dir, eyeY - 7.0 * s);
        path.lineTo(cx + 7.0 * s * dir, eyeY);
        path.lineTo(cx - 8.0 * s * dir, eyeY + 7.0 * s);
        p.drawPath(path);
    };

    auto drawClosedEye = [&](double cx) {
        // 困倦：闭眼弧线
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(38, 48, 59), 3.2 * s, Qt::SolidLine, Qt::RoundCap));
        QPainterPath path;
        path.moveTo(cx - 9.0 * s, eyeY);
        path.quadTo(cx, eyeY + 7.0 * s, cx + 9.0 * s, eyeY);
        p.drawPath(path);
    };

    if (mood == Mood::Hot) {
        drawSquintEye(-eyeDX, 1.0);
        drawSquintEye( eyeDX, -1.0);
    } else if (mood == Mood::Sleepy) {
        drawClosedEye(-eyeDX);
        drawClosedEye( eyeDX);
    } else {
        drawOpenEye(-eyeDX);
        drawOpenEye( eyeDX);
    }

    // ---------- 嘴巴 ----------
    {
        const double my = 22.0 * s;
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(38, 48, 59), 3.0 * s, Qt::SolidLine, Qt::RoundCap));

        if (mood == Mood::Busy) {
            // 忙：张个小圆嘴
            p.setBrush(QColor(70, 40, 40));
            p.setPen(QPen(QColor(38, 48, 59), 2.2 * s));
            const double r = 5.0 * s * (1.0 + 0.12 * std::sin(tSec * 9.0));
            p.drawEllipse(QPointF(0, my + 2.0 * s), r, r * 1.15);
        } else if (mood == Mood::Hot) {
            // 热：波浪嘴
            QPainterPath path;
            path.moveTo(-13.0 * s, my);
            path.quadTo(-6.5 * s, my - 5.0 * s, 0, my);
            path.quadTo( 6.5 * s, my + 5.0 * s, 13.0 * s, my);
            p.drawPath(path);
        } else if (mood == Mood::Sleepy) {
            // 困：小小的一条
            p.drawLine(QPointF(-5.0 * s, my), QPointF(5.0 * s, my));
        } else {
            // 正常：微笑
            QPainterPath path;
            path.moveTo(-13.0 * s, my - 2.0 * s);
            path.quadTo(0, my + 9.0 * s, 13.0 * s, my - 2.0 * s);
            p.drawPath(path);
        }
    }

    // ---------- 过热：汗滴 ----------
    // 画在身体外侧，并加一圈浅色描边，否则和红色身体糊在一起看不出来
    if (mood == Mood::Hot) {
        const double phase = std::fmod(tSec * 0.9, 1.0);          // 0→1 循环
        const double dy    = -ry * 0.45 + phase * ry * 1.05;
        const double dx    = rx * 1.02;
        const int    alpha = static_cast<int>(255 * (1.0 - phase * 0.55));

        QPainterPath sweat;
        sweat.moveTo(dx, dy - 10.0 * s);
        sweat.quadTo(dx + 7.5 * s, dy + 1.0 * s, dx, dy + 7.5 * s);
        sweat.quadTo(dx - 7.5 * s, dy + 1.0 * s, dx, dy - 10.0 * s);

        p.setPen(QPen(QColor(255, 255, 255, static_cast<int>(alpha * 0.55)), 1.4 * s));
        p.setBrush(QColor(150, 214, 255, alpha));
        p.drawPath(sweat);

        // 小高光
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, static_cast<int>(alpha * 0.8)));
        p.drawEllipse(QPointF(dx - 2.0 * s, dy - 1.0 * s), 2.0 * s, 2.6 * s);
    }

    // ---------- 困倦：飘 Z ----------
    if (mood == Mood::Sleepy) {
        p.setPen(QColor(230, 238, 248, 210));
        for (int i = 0; i < 3; ++i) {
            const double phase = std::fmod(tSec * 0.45 + i * 0.33, 1.0);
            const int alpha = static_cast<int>(210 * (1.0 - phase));
            if (alpha <= 0) continue;
            const int pt = static_cast<int>((13 + 9 * phase) * s);
            p.setFont(QFont(QStringLiteral("Segoe UI"), std::max(6, pt), QFont::Bold));
            p.setPen(QColor(230, 238, 248, alpha));
            p.drawText(QPointF(rx * 0.75 + phase * 26.0 * s,
                               -ry * 0.75 - phase * 44.0 * s),
                       QStringLiteral("Z"));
        }
    }

    p.restore();
}
