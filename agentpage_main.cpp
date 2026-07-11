#include "browserpagewidget.h"
#include "agentstartupsplash.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QTcpSocket>
#include <QUrl>

#include <functional>

namespace {

constexpr int kServiceStartupTimeoutMs = 120000;
constexpr int kEndpointProbeTimeoutMs = 300;

QUrl agentPageUrl(const QStringList &arguments)
{
    if (arguments.size() > 1) {
        return QUrl::fromUserInput(arguments.at(1));
    }

    return QUrl(QStringLiteral("http://127.0.0.1:19001/"));
}

// AgentPageViewer 和库都会输出到 bin/<Config>，服务固定放在 bin/Release/openclaw-service。
QString openClawServiceDirectory()
{
    QDir binDir(QCoreApplication::applicationDirPath());
    if (binDir.dirName().compare(QStringLiteral("Debug"), Qt::CaseInsensitive) == 0
        || binDir.dirName().compare(QStringLiteral("Release"), Qt::CaseInsensitive) == 0
        || binDir.dirName().compare(QStringLiteral("RelWithDebInfo"), Qt::CaseInsensitive) == 0
        || binDir.dirName().compare(QStringLiteral("MinSizeRel"), Qt::CaseInsensitive) == 0) {
        binDir.cdUp();
    }

    return binDir.filePath(QStringLiteral("Release/openclaw-service"));
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

// 这里只探测 TCP 端口是否可连接，不下载页面内容。
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

// 用 QProcess 启动 openclaw.cmd gateway，让服务和窗口程序一起受控。
bool startOpenClawGateway(QProcess *process)
{
    const QString serviceDir = openClawServiceDirectory();
    const QString commandPath = QDir(serviceDir).filePath(QStringLiteral("openclaw.cmd"));
    if (!QFileInfo::exists(commandPath)) {
        qWarning().noquote() << "OpenClaw service command not found:" << commandPath;
        return false;
    }

    process->setWorkingDirectory(serviceDir);
    process->setProgram(QStringLiteral("cmd.exe"));
    process->setArguments({QStringLiteral("/c"), QStringLiteral("openclaw.cmd"), QStringLiteral("gateway")});
    process->setProcessChannelMode(QProcess::MergedChannels);

    QObject::connect(process, &QProcess::readyReadStandardOutput,
                     [process]() {
                         const QString output = QString::fromLocal8Bit(process->readAllStandardOutput()).trimmed();
                         if (!output.isEmpty()) {
                             qInfo().noquote() << output;
                         }
                     });

    QObject::connect(process, &QProcess::errorOccurred,
                     [](QProcess::ProcessError error) {
                         qWarning().noquote() << "OpenClaw service process error:" << static_cast<int>(error);
                     });

    QObject::connect(process,
                     QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     [](int exitCode, QProcess::ExitStatus exitStatus) {
                         qWarning().noquote() << "OpenClaw service process finished:"
                                              << "exitCode=" << exitCode
                                              << "exitStatus=" << static_cast<int>(exitStatus);
                     });

    process->start();
    if (!process->waitForStarted(5000)) {
        qWarning().noquote() << "Failed to start OpenClaw service:" << process->errorString();
        return false;
    }

    qInfo().noquote() << "OpenClaw service started from:" << serviceDir;
    return true;
}

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

    AgentStartupSplash splash(pageUrl);
    splash.setStatus(QStringLiteral("正在检查 OpenClaw 服务..."),
                     QStringLiteral("检测目标页面端口是否已经可连接。"));
    splash.show();
    app.processEvents();

    if (!waitForEndpoint(pageUrl,
                         500,
                         [&splash](qint64) {
                             splash.setBusyProgress();
                         })) {
        splash.setStatus(QStringLiteral("正在启动 OpenClaw 服务..."),
                         QStringLiteral("执行 openclaw.cmd gateway。"));
        splash.setBusyProgress();

        const bool serviceStarted = startOpenClawGateway(&openClawGateway);
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
                qWarning().noquote() << "Agent endpoint is not ready yet:" << pageUrl;
                splash.setStatus(QStringLiteral("服务暂未就绪，正在尝试打开页面..."),
                                 pageUrl.toString());
            } else {
                splash.setStatus(QStringLiteral("服务已就绪，正在打开页面..."),
                                 pageUrl.toString());
            }
        } else {
            splash.setStatus(QStringLiteral("服务启动失败，正在尝试打开页面..."),
                             pageUrl.toString());
        }
    } else {
        qInfo().noquote() << "Agent endpoint is already available:" << pageUrl;
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
                         qWarning().noquote() << "Agent page load failed:" << url
                                              << "domain=" << domain
                                              << "code=" << code
                                              << message;
                     });

    QObject::connect(&browser, &bm::BrowserPageWidget::loadTimedOut,
                     [&splash](const QUrl &) {
                         splash.close();
                     });

    QObject::connect(&browser, &bm::BrowserPageWidget::renderProcessTerminated,
                     [&splash](int status, int exitCode) {
                         splash.close();
                         qWarning().noquote() << "Agent page render process terminated:"
                                              << "status=" << status
                                              << "exitCode=" << exitCode;
                     });

    QObject::connect(&browser, &bm::BrowserPageWidget::downloadStarted,
                     [](quint32 id, const QUrl &url, const QString &filePath) {
                         qInfo().noquote() << "Download started:" << id << url << filePath;
                     });

    QObject::connect(&browser, &bm::BrowserPageWidget::downloadFinished,
                     [](quint32 id, const QString &filePath) {
                         qInfo().noquote() << "Download finished:" << id << filePath;
                     });

    QObject::connect(&browser, &bm::BrowserPageWidget::downloadFailed,
                     [](quint32 id, const QString &reason) {
                         qWarning().noquote() << "Download failed:" << id << reason;
                     });

    browser.loadUrl(browser.homeUrl());
    browser.resize(1280, 820);
    browser.show();

    return app.exec();
}

