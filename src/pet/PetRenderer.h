#pragma once
#include <QPainter>
#include <QSize>

#include "core/Metrics.h"
#include "PetState.h"

// ============================================================================
// PetRenderer —— 纯 QPainter 画出来的几何角色
//
// 不用美术素材的好处：
//   - 零素材依赖，不用担心图片版权
//   - 任意分辨率不糊（矢量绘制）
//   - 表情可以参数化：眼睛大小、嘴巴弧度、颜色全是变量，
//     情绪变化只改几个数，不需要准备四套图
// ============================================================================
class PetRenderer {
public:
    // 在 size 范围内绘制；tSec 是程序运行秒数（用真实时间差累加，不要假设固定步长）
    void paint(QPainter& p, const QSize& size, const Metrics& m, Mood mood, double tSec);
};
