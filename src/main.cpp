#include <QApplication>
#include <QFile>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QtGlobal>

#include "core/MetricsHub.h"
#include "pet/PetWindow.h"
#include "ui/RingPanel.h"
#include "ui/TrayIcon.h"

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {

QFile* g_log = nullptr;

void logLine(const QString& s) {
    if (!g_log) return;
    g_log->write(s.toUtf8());
    g_log->write("\n");
    g_log->flush();
}

} // namespace

int main(int argc, char* argv[]) {
    // 日志必须在 QApplication 之前就绪：
    // 万一 Qt 初始化失败，进程会静默退出，没有日志就完全查不到线索。
    // 用 GetModuleFileNameW 取 exe 目录，避免中文路径在窄字符下被截断。
    if (!qEnvironmentVariableIsEmpty("SIP_DEBUG")) {
        QString dir;
#ifdef Q_OS_WIN
        wchar_t buf[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, buf, MAX_PATH) > 0) {
            dir = QString::fromWCharArray(buf);
            dir = dir.left(dir.lastIndexOf(QLatin1Char('\\')) + 1);
        }
#endif
        g_log = new QFile(dir + QStringLiteral("sip_debug.log"));
        if (g_log->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            logLine(QStringLiteral("[boot] 进程启动"));
        } else {
            delete g_log;
            g_log = nullptr;
        }
    }

    QApplication app(argc, argv);
    logLine(QStringLiteral("[boot] QApplication 构造完成, Qt=%1").arg(qVersion()));

    // QSettings 靠这两个名字定位注册表位置
    QApplication::setOrganizationName(QStringLiteral("b-as-h"));
    QApplication::setApplicationName(QStringLiteral("SystemInfoPet"));
    QApplication::setQuitOnLastWindowClosed(false);   // 关掉窗口不等于退出程序

    // Metrics 要跨线程走信号槽，必须注册元类型
    qRegisterMetaType<Metrics>("Metrics");

    // 主界面：圆环仪表盘
    RingPanel panel;

    // 桌宠：默认不显示，托盘菜单里可以打开。代码完整保留。
    PetWindow pet;
    if (QScreen* scr = QGuiApplication::primaryScreen()) {
        const QRect a = scr->availableGeometry();
        pet.move(a.right() - pet.width() - 40, a.bottom() - pet.height() - 10);
        logLine(QStringLiteral("[boot] screen=%1,%2 %3x%4")
                .arg(a.x()).arg(a.y()).arg(a.width()).arg(a.height()));
    }

    MetricsHub hub;

    QObject::connect(&hub, &MetricsHub::metricsReady, &panel, &RingPanel::setMetrics);
    QObject::connect(&hub, &MetricsHub::metricsReady, &pet,   &PetWindow::setMetrics);

    if (g_log) {
        QObject::connect(&hub, &MetricsHub::metricsReady, [](const Metrics& m) {
            logLine(QStringLiteral(
                "cpu=%1 mem=%2 gpu=%3 cT=%4 gT=%5 cF=%6 gC=%7 "
                "ram=%8/%9 net=%10/%11 fps=%12 hz=%13 disk=%14 self=%15")
                .arg(m.cpuUsage,    0, 'f', 1)
                .arg(m.memUsage,    0, 'f', 1)
                .arg(m.gpuUsage,    0, 'f', 1)
                .arg(m.cpuTemp,     0, 'f', 1)
                .arg(m.gpuTemp,     0, 'f', 1)
                .arg(m.cpuFreqMHz,  0, 'f', 0)
                .arg(m.gpuClockMHz, 0, 'f', 0)
                .arg(m.ramUsedGB,   0, 'f', 1)
                .arg(m.ramTotalGB,  0, 'f', 1)
                .arg(m.netDownKBs,  0, 'f', 1)
                .arg(m.netUpKBs,    0, 'f', 1)
                .arg(m.fps,         0, 'f', 1)
                .arg(m.refreshHz,   0, 'f', 0)
                .arg(m.diskUsedPct, 0, 'f', 1)
                .arg(m.selfMemMB,   0, 'f', 1));
        });
    }

    panel.show();

    // 调试用：SIP_PET=1 让桌宠开机就显示（默认是隐藏的，只能从托盘打开）
    if (qEnvironmentVariable("SIP_PET") == QLatin1String("1")) {
        pet.show();
        logLine(QStringLiteral("[boot] SIP_PET=1，桌宠已强制显示"));
    }

    TrayIcon tray(&pet);
    QObject::connect(&panel, &RingPanel::contextMenuRequested, &tray, &TrayIcon::showMenuAt);
    QObject::connect(&pet,   &PetWindow::contextMenuRequested, &tray, &TrayIcon::showMenuAt);
    tray.show();

    // 启动一会儿后记录一次窗口状态，方便无人值守验证
    QTimer::singleShot(1200, &app, [&] {
        if (!g_log) return;
        logLine(QStringLiteral("[win] panel visible=%1 geom=%2,%3 %4x%5")
                .arg(panel.isVisible() ? 1 : 0).arg(panel.x()).arg(panel.y())
                .arg(panel.width()).arg(panel.height()));
        logLine(QStringLiteral("[win] pet visible=%1 (默认隐藏)")
                .arg(pet.isVisible() ? 1 : 0));
#ifdef Q_OS_WIN
        HWND h = reinterpret_cast<HWND>(panel.winId());
        const LONG_PTR ex = GetWindowLongPtr(h, GWL_EXSTYLE);
        logLine(QStringLiteral("[win] exstyle=0x%1 toolwindow=%2 layered=%3 topmost=%4")
                .arg(static_cast<qlonglong>(ex), 0, 16)
                .arg((ex & WS_EX_TOOLWINDOW) ? 1 : 0)
                .arg((ex & WS_EX_LAYERED)    ? 1 : 0)
                .arg((ex & WS_EX_TOPMOST)    ? 1 : 0));
#endif
    });

    hub.start(1000);

    const int rc = app.exec();

    hub.stop();

    if (g_log) {
        logLine(QStringLiteral("[exit] rc=%1").arg(rc));
        g_log->close();
        delete g_log;
        g_log = nullptr;
    }
    return rc;
}
