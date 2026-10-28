#pragma once
#include <QObject>

class QAction;
class QMenu;
class QPoint;
class QSystemTrayIcon;
class PetWindow;

// ============================================================================
// TrayIcon —— 托盘菜单
//
// 桌宠默认不显示（主界面是圆环仪表盘），要在这里能打开。
// 同时承担"后门"职责：点击穿透开启后窗口就点不动了，只能靠这里的菜单关回来。
// ============================================================================
class TrayIcon : public QObject {
    Q_OBJECT
public:
    explicit TrayIcon(PetWindow* pet, QObject* parent = nullptr);
    ~TrayIcon() override;

    void show();

public slots:
    void showMenuAt(const QPoint& globalPos);

private:
    PetWindow*       pet_  = nullptr;
    QSystemTrayIcon* tray_ = nullptr;
    QMenu*           menu_ = nullptr;

    QAction* actPet_     = nullptr;
    QAction* actTop_     = nullptr;
    QAction* actThrough_ = nullptr;
    QAction* actNoAct_   = nullptr;
};
