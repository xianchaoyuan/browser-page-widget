#include "browserpagewidget.h"
#include "agentstartupsplash.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QTcpSocket>
#include <QUrl>

#include <functional>

namespace {

constexpr int kServiceStartupTimeoutMs = 120000;
constexpr int kEndpointProbeTimeoutMs = 300;

// 允许命令行传入页面地址；未传入时使用默认本地 agent 页面。
QUrl agentPageUrl(const QStringList &arguments)
{
    if (arguments.size() > 1) {
        return QUrl::fromUserInput(arguments.at(1));
    }

    return QUrl(QStringLiteral("http://127.0.0.1:19001/"));
}

// 返回服务目录候选路径，优先满足发布包结构，再兼容开发目录结构。
QStringList openClawServiceDirectoryCandidates()
{
    QStringList candidates;
    QDir appDir(QCoreApplication::applicationDirPath());

    // 发布包推荐结构：AgentPageViewer.exe 与 openclaw-service 在同一层目录。
    candidates.append(QDir::cleanPath(appDir.filePath(QStringLiteral("openclaw-service"))));

    // 兼容开发环境：bin/dist/openclaw-service。
    candidates.append(QDir::cleanPath(appDir.filePath(QStringLiteral("../dist/openclaw-service"))));

    return candidates;
}

// 从候选路径中选择真正包含 openclaw.cmd 的服务目录。
QString openClawServiceDirectory()
{
    const QStringList candidates = openClawServiceDirectoryCandidates();
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(QDir(candidate).filePath(QStringLiteral("openclaw.cmd")))) {
            return candidate;
        }
    }

    return candidates.isEmpty()
               ? QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("openclaw-service"))
               : candidates.first();
}

// 启动画面里优先显示相对路径，避免暴露固定安装盘符。
QString pathForDisplay(const QString &path)
{
    QDir appDir(QCoreApplication::applicationDirPath());
    const QString relativePath = appDir.relativeFilePath(path);
    if (!relativePath.isEmpty() && !QDir::isAbsolutePath(relativePath)) {
        return QDir::toNativeSeparators(relativePath);
    }

    return QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
}

int defaultPortForUrl(const QUrl &url)
{
    if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0) {
        return 443;
    }
    return 80;
}

QString elapsedSecondsText(qint64 elapsedMs)
{
    return QStringLiteral("已等待 %1 秒").arg(elapsedMs / 1000);
}

/**
 * @brief 这里只探测 TCP 端口是否可连接，不下载页面内容
 */
bool waitForEndpoint(const QUrl &url,
                     int timeoutMs,
                     const std::function<void(qint64 elapsedMs)> &onProbe = {})
{
    if (url.host().isEmpty()) {
        return true;
    }

    const int port = url.port(defaultPortForUrl(url));
    if (port <= 0) {
        return true;
    }

    QElapsedTimer timer;
    timer.start();
    do {
        QTcpSocket socket;
        socket.connectToHost(url.host(), port);
        if (socket.waitForConnected(kEndpointProbeTimeoutMs)) {
            socket.disconnectFromHost();
            return true;
        }

        if (onProbe) {
            onProbe(timer.elapsed());
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    } while (timer.elapsed() < timeoutMs);

    return false;
}

// openclaw-service/state 是服务自己的状态目录，也是日志的首选位置。
QString openClawStateDirectory()
{
    return QDir(openClawServiceDirectory()).filePath(QStringLiteral("state"));
}

QString serviceLogFileName()
{
    return QStringLiteral("agentpageviewer-service.log");
}

// 尝试创建日志目录并验证是否真的可写。
QString writableLogPathInDirectory(const QString &directory)
{
    if (directory.isEmpty() || !QDir().mkpath(directory)) {
        return QString();
    }

    const QString logPath = QDir(directory).filePath(serviceLogFileName());
    QFile file(logPath);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        return QString();
    }

    file.close();
    return logPath;
}

// 首选日志目录不可写时，依次尝试几个更容易写入的位置。
QString fallbackServiceLogPath()
{
    QString logPath = writableLogPathInDirectory(QCoreApplication::applicationDirPath());
    if (!logPath.isEmpty()) {
        return logPath;
    }

    const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!appDataDir.isEmpty()) {
        logPath = writableLogPathInDirectory(QDir(appDataDir).filePath(QStringLiteral("logs")));
        if (!logPath.isEmpty()) {
            return logPath;
        }
    }

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (!tempDir.isEmpty()) {
        logPath = writableLogPathInDirectory(QDir(tempDir).filePath(QStringLiteral("AgentPageViewer")));
        if (!logPath.isEmpty()) {
            return logPath;
        }
    }

    return QDir(QCoreApplication::applicationDirPath()).filePath(serviceLogFileName());
}

QString openClawServiceLogPath()
{
    const QString primaryPath = writableLogPathInDirectory(openClawStateDirectory());
    if (!primaryPath.isEmpty()) {
        return primaryPath;
    }

    return fallbackServiceLogPath();
}

// 向服务诊断日志追加一行中文信息。
void appendServiceLog(const QString &message)
{
    const QString logPath = openClawServiceLogPath();
    QFile file(logPath);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qWarning().noquote() << "无法打开 AgentPageViewer 服务日志：" << logPath;
        return;
    }

    const QString line = QStringLiteral("[%1] %2\n")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                                  message);
    file.write(line.toLocal8Bit());
}

// 程序启动时先记录基础环境，方便定位服务目录和启动脚本问题。
void appendStartupDiagnostics(const QUrl &pageUrl)
{
    const QString serviceDir = openClawServiceDirectory();
    const QString commandPath = QDir(serviceDir).filePath(QStringLiteral("openclaw.cmd"));

    appendServiceLog(QStringLiteral("---------------- AgentPageViewer 启动 ----------------"));
    appendServiceLog(QStringLiteral("程序目录：%1")
                         .arg(QDir::toNativeSeparators(QCoreApplication::applicationDirPath())));
    appendServiceLog(QStringLiteral("OpenClaw 服务目录：%1")
                         .arg(QDir::toNativeSeparators(serviceDir)));
    appendServiceLog(QStringLiteral("服务日志路径：%1")
                         .arg(pathForDisplay(openClawServiceLogPath())));
    appendServiceLog(QStringLiteral("目标页面：%1").arg(pageUrl.toString()));
    appendServiceLog(QStringLiteral("服务目录候选列表："));
    for (const QString &candidate : openClawServiceDirectoryCandidates()) {
        appendServiceLog(QStringLiteral("  %1").arg(QDir::toNativeSeparators(candidate)));
    }
    appendServiceLog(QStringLiteral("openclaw.cmd 是否存在：%1")
                         .arg(QFileInfo::exists(commandPath) ? QStringLiteral("是") : QStringLiteral("否")));
}

// 通过 cmd.exe 执行 openclaw.cmd gateway。
bool startOpenClawGateway(QProcess *process, const QUrl &pageUrl)
{
    const QString serviceDir = openClawServiceDirectory();
    const QString commandPath = QDir(serviceDir).filePath(QStringLiteral("openclaw.cmd"));

    appendServiceLog(QStringLiteral("---------------- 请求启动 OpenClaw ----------------"));

    if (!QFileInfo::exists(commandPath)) {
        appendServiceLog(QStringLiteral("未找到启动脚本：%1").arg(commandPath));
        qWarning().noquote() << "未找到 OpenClaw 服务启动脚本：" << commandPath;
        return false;
    }

    process->setWorkingDirectory(serviceDir);
    process->setProgram(QStringLiteral("cmd.exe"));
    process->setArguments({QStringLiteral("/d"),
                           QStringLiteral("/c"),
                           QStringLiteral("openclaw.cmd"),
                           QStringLiteral("gateway")});

    QObject::connect(process, &QProcess::errorOccurred,
                     [](QProcess::ProcessError error) {
                         appendServiceLog(QStringLiteral("OpenClaw 进程错误：%1").arg(static_cast<int>(error)));
                         qWarning().noquote() << "OpenClaw 服务进程错误：" << static_cast<int>(error);
                     });

    QObject::connect(process,
                     QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     [](int exitCode, QProcess::ExitStatus exitStatus) {
                         appendServiceLog(QStringLiteral("OpenClaw 进程结束，退出码=%1，退出状态=%2")
                                              .arg(exitCode)
                                              .arg(static_cast<int>(exitStatus)));
                         qWarning().noquote() << "OpenClaw 服务进程结束："
                                              << "退出码=" << exitCode
                                              << "退出状态=" << static_cast<int>(exitStatus);
                     });

    appendServiceLog(QStringLiteral("正在通过 cmd.exe 启动 OpenClaw"));
    appendServiceLog(QStringLiteral("程序：%1").arg(process->program()));
    appendServiceLog(QStringLiteral("参数：%1").arg(process->arguments().join(QLatin1Char(' '))));
    appendServiceLog(QStringLiteral("工作目录：%1").arg(QDir::toNativeSeparators(serviceDir)));

    process->start();
    if (!process->waitForStarted(5000)) {
        appendServiceLog(QStringLiteral("启动失败：%1").arg(process->errorString()));
        qWarning().noquote() << "启动 OpenClaw 服务失败：" << process->errorString();
        return false;
    }

    // 进程如果立刻退出，通常说明启动环境或命令不对。
    if (process->waitForFinished(1500)) {
        appendServiceLog(QStringLiteral("OpenClaw 进程过早退出，退出码=%1，退出状态=%2")
                             .arg(process->exitCode())
                             .arg(static_cast<int>(process->exitStatus())));
        if (waitForEndpoint(pageUrl, 500)) {
            appendServiceLog(QStringLiteral("进程提前退出后，目标页面已经可以连接。"));
            return true;
        }
        return false;
    }

    appendServiceLog(QStringLiteral("OpenClaw 进程已启动并保持运行。"));
    qInfo().noquote() << "OpenClaw 服务已通过 cmd.exe 启动";
    return true;
}

// 程序退出时尝试停止由本程序拉起的服务进程。
void stopOpenClawGateway(QProcess *process)
{
    if (!process || process->state() == QProcess::NotRunning) {
        return;
    }

    process->terminate();
    if (!process->waitForFinished(3000)) {
        process->kill();
        process->waitForFinished(1000);
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("AgentPageViewer"));
    QApplication::setOrganizationName(QStringLiteral("BM"));

    const QUrl pageUrl = agentPageUrl(QCoreApplication::arguments());
    QProcess openClawGateway;

    appendStartupDiagnostics(pageUrl);

    AgentStartupSplash splash(pageUrl);
    splash.setStatus(QStringLiteral("正在检查 OpenClaw 服务..."),
                     QStringLiteral("检测目标页面端口是否已经可连接。"));
    splash.show();
    app.processEvents();

    appendServiceLog(QStringLiteral("启动服务前检查目标页面端口。"));
    if (!waitForEndpoint(pageUrl,
                         500,
                         [&splash](qint64) {
                             splash.setBusyProgress();
                         })) {
        appendServiceLog(QStringLiteral("目标页面暂不可连接，准备启动 OpenClaw 服务。"));
        splash.setStatus(QStringLiteral("正在启动 OpenClaw 服务..."),
                         QStringLiteral("执行 openclaw.cmd gateway。"));
        splash.setBusyProgress();

        const bool serviceStarted = startOpenClawGateway(&openClawGateway, pageUrl);
        if (serviceStarted) {
            splash.setStatus(QStringLiteral("正在等待 OpenClaw 服务就绪..."),
                             QStringLiteral("服务已启动，正在等待页面端口响应。"));
            const bool endpointReady = waitForEndpoint(pageUrl,
                                                       kServiceStartupTimeoutMs,
                                                       [&splash](qint64 elapsedMs) {
                                                           splash.setStatus(QStringLiteral("正在等待 OpenClaw 服务就绪..."),
                                                                            elapsedSecondsText(elapsedMs));
                                                       });
            if (!endpointReady) {
                qWarning().noquote() << "Agent 页面端口暂未就绪：" << pageUrl;
                splash.setStatus(QStringLiteral("服务暂未就绪，正在尝试打开页面..."),
                                 QStringLiteral("日志：%1").arg(pathForDisplay(openClawServiceLogPath())));
            } else {
                splash.setStatus(QStringLiteral("服务已就绪，正在打开页面..."),
                                 pageUrl.toString());
            }
        } else {
            splash.setStatus(QStringLiteral("服务启动失败，正在尝试打开页面..."),
                             QStringLiteral("日志：%1").arg(pathForDisplay(openClawServiceLogPath())));
        }
    } else {
        appendServiceLog(QStringLiteral("目标页面已可连接，跳过 OpenClaw 服务启动。"));
        qInfo().noquote() << "Agent 页面端口已可连接：" << pageUrl;
        splash.setStatus(QStringLiteral("服务已启动，正在打开页面..."),
                         pageUrl.toString());
    }

    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     [&openClawGateway]() {
                         stopOpenClawGateway(&openClawGateway);
                     });

    bm::BrowserPageWidget browser;
    browser.setWindowTitle(QStringLiteral("Agent Page Viewer"));
    browser.setHomeUrl(pageUrl);
    browser.setLoadTimeoutMs(30000);
    browser.setToolbarVisible(false);
    browser.setStatusBarVisible(true);
    browser.setPopupPolicy(bm::BrowserPageWidget::PopupPolicy::OpenInCurrentView);
    browser.setDownloadPolicy(bm::BrowserPageWidget::DownloadPolicy::AutoSave);

    QObject::connect(&browser, &bm::BrowserPageWidget::loadStarted,
                     [&splash]() {
                         splash.setStatus(QStringLiteral("正在加载 agent 页面..."));
                         splash.setProgress(0);
                     });

    QObject::connect(&browser, &bm::BrowserPageWidget::loadProgress,
                     [&splash](int progress) {
                         splash.setProgress(progress);
                     });

    QObject::connect(&browser, &bm::BrowserPageWidget::loadFinished,
                     [&splash](bool) {
                         splash.close();
                     });

    QObject::connect(&browser, &bm::BrowserPageWidget::loadFailed,
                     [&splash](const QUrl &url, int domain, int code, const QString &message) {
                         splash.close();
                         qWarning().noquote() << "Agent 页面加载失败：" << url
                                              << "错误域=" << domain
                                              << "错误码=" << code
                                              << message;
                     });

    QObject::connect(&browser, &bm::BrowserPageWidget::loadTimedOut,
                     [&splash](const QUrl &) {
                         splash.close();
                     });

    QObject::connect(&browser, &bm::BrowserPageWidget::renderProcessTerminated,
                     [&splash](int status, int exitCode) {
                         splash.close();
                         qWarning().noquote() << "Agent 页面渲染进程异常终止："
                                              << "状态=" << status
                                              << "退出码=" << exitCode;
                     });

    QObject::connect(&browser, &bm::BrowserPageWidget::downloadStarted,
                     [](quint32 id, const QUrl &url, const QString &filePath) {
                         qInfo().noquote() << "下载开始：" << id << url << filePath;
                     });

    QObject::connect(&browser, &bm::BrowserPageWidget::downloadFinished,
                     [](quint32 id, const QString &filePath) {
                         qInfo().noquote() << "下载完成：" << id << filePath;
                     });

    QObject::connect(&browser, &bm::BrowserPageWidget::downloadFailed,
                     [](quint32 id, const QString &reason) {
                         qWarning().noquote() << "下载失败：" << id << reason;
                     });

    browser.loadUrl(browser.homeUrl());
    browser.resize(1280, 820);
    browser.show();

    return app.exec();
}
