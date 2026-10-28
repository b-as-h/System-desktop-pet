#include "TrayIcon.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>

#include "pet/PetWindow.h"

namespace {

// 程序化生成托盘图标，省掉一个 .ico 素材
QIcon makeTrayIcon() {
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient g(0, 8, 0, 56);
    g.setColorAt(0.0, QColor(111, 224, 212));
    g.setColorAt(1.0, QColor(38, 138, 128));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(QRectF(8, 8, 48, 48));

    p.setBrush(QColor(38, 48, 59));
    p.drawEllipse(QPointF(25, 31), 5.5, 6.5);
    p.drawEllipse(QPointF(39, 31), 5.5, 6.5);

    p.setBrush(QColor(255, 255, 255, 230));
    p.drawEllipse(QPointF(23, 28.5), 2.0, 2.0);
    p.drawEllipse(QPointF(37, 28.5), 2.0, 2.0);

    return QIcon(pm);
}

} // namespace

TrayIcon::TrayIcon(PetWindow* pet, QObject* parent)
    : QObject(parent), pet_(pet) {

    tray_ = new QSystemTrayIcon(makeTrayIcon(), this);
    tray_->setToolTip(QStringLiteral("系统信息"));

    menu_ = new QMenu;

    // 桌宠默认隐藏，这里控制显隐
    actPet_ = menu_->addAction(QStringLiteral("显示桌宠"));
    actPet_->setCheckable(true);
    actPet_->setChecked(pet_ && pet_->isVisible());
    connect(actPet_, &QAction::toggled, this, [this](bool on) {
        if (!pet_) return;
        pet_->setVisible(on);
        if (on) pet_->raise();
    });

    menu_->addSeparator();

    actTop_ = menu_->addAction(QStringLiteral("始终置顶"));
    actTop_->setCheckable(true);
    actTop_->setChecked(true);
    connect(actTop_, &QAction::toggled, this, [this](bool on) {
        if (pet_) pet_->setAlwaysOnTop(on);
    });

    actThrough_ = menu_->addAction(QStringLiteral("桌宠点击穿透"));
    actThrough_->setCheckable(true);
    connect(actThrough_, &QAction::toggled, this, [this](bool on) {
        if (!pet_) return;
        pet_->setClickThrough(on);
        if (on) {
            // 提醒用户后门在哪，否则窗口就"救不回来"了
            tray_->showMessage(QStringLiteral("点击穿透已开启"),
                               QStringLiteral("桌宠不再响应鼠标。\n要关掉它，请右键这个托盘图标。"),
                               QSystemTrayIcon::Information, 5000);
        }
    });

    actNoAct_ = menu_->addAction(QStringLiteral("桌宠不抢焦点"));
    actNoAct_->setCheckable(true);
    actNoAct_->setChecked(true);
    connect(actNoAct_, &QAction::toggled, this, [this](bool on) {
        if (pet_) pet_->setNoActivate(on);
    });

    menu_->addSeparator();

    QAction* actQuit = menu_->addAction(QStringLiteral("退出"));
    connect(actQuit, &QAction::triggered, qApp, &QApplication::quit);

    tray_->setContextMenu(menu_);

    // 左键点托盘图标也弹菜单，比"只能右键"顺手
    connect(tray_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason r) {
                if (r == QSystemTrayIcon::Trigger) showMenuAt(QCursor::pos());
            });
}

TrayIcon::~TrayIcon() {
    tray_->hide();
    delete menu_;
    menu_ = nullptr;
}

void TrayIcon::show() {
    tray_->show();
}

void TrayIcon::showMenuAt(const QPoint& globalPos) {
    if (!menu_) return;
    if (actPet_) actPet_->setChecked(pet_ && pet_->isVisible());
    menu_->popup(globalPos);
}
