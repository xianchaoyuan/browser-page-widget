#include "agentstartupcontroller.h"

#include "agentstartupsplash.h"
#include "endpointwaiter.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

#include <functional>

namespace {

constexpr int kServiceStartupTimeoutMs = 120000;

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

QString elapsedSecondsText(qint64 elapsedMs)
{
    return QStringLiteral("已等待 %1 秒").arg(elapsedMs / 1000);
}

// 创建端口等待器，并保持调用处只关心“等待进度”和“等待结果”。
void waitForEndpointAsync(const QUrl &url,
                          int timeoutMs,
                          QObject *context,
                          const std::function<void(qint64 elapsedMs)> &onProbe,
                          const std::function<void(bool available)> &onFinished)
{
    if (!context) {
        if (onFinished) {
            onFinished(true);
        }
        return;
    }

    EndpointWaiter *waiter = new EndpointWaiter(url, timeoutMs, context);
    waiter->setProbeCallback(onProbe);
    waiter->setFinishedCallback(onFinished);
    waiter->start();
}

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
    file.write(line.toUtf8());
}

} // namespace

AgentStartupController::AgentStartupController(const QUrl &pageUrl,
                                               AgentStartupSplash *splash,
                                               QObject *parent)
    : QObject(parent)
    , pageUrl_(pageUrl)
    , splash_(splash)
    , openClawGateway_(new QProcess(this))
{
    QObject::connect(openClawGateway_, &QProcess::started,
                     this, []() {
                         appendServiceLog(QStringLiteral("OpenClaw 启动进程已创建。"));
                         qInfo().noquote() << "OpenClaw 启动进程已创建";
                     });

    QObject::connect(openClawGateway_, &QProcess::errorOccurred,
                     this, [](QProcess::ProcessError error) {
                         appendServiceLog(QStringLiteral("OpenClaw 进程错误：%1").arg(static_cast<int>(error)));
                         qWarning().noquote() << "OpenClaw 服务进程错误：" << static_cast<int>(error);
                     });

    QObject::connect(openClawGateway_,
                     QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     this,
                     [](int exitCode, QProcess::ExitStatus exitStatus) {
                         appendServiceLog(QStringLiteral("OpenClaw 进程结束，退出码=%1，退出状态=%2")
                                              .arg(exitCode)
                                              .arg(static_cast<int>(exitStatus)));
                         qWarning().noquote() << "OpenClaw 服务进程结束："
                                              << "退出码=" << exitCode
                                              << "退出状态=" << static_cast<int>(exitStatus);
                     });
}

AgentStartupController::~AgentStartupController()
{
    stopService();
}

void AgentStartupController::start()
{
    appendStartupDiagnostics();
    checkEndpointBeforeStart();
}

void AgentStartupController::stopService()
{
    if (!openClawGateway_ || openClawGateway_->state() == QProcess::NotRunning) {
        return;
    }

    openClawGateway_->terminate();
}

void AgentStartupController::appendStartupDiagnostics()
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
    appendServiceLog(QStringLiteral("目标页面：%1").arg(pageUrl_.toString()));
    appendServiceLog(QStringLiteral("服务目录候选列表："));
    for (const QString &candidate : openClawServiceDirectoryCandidates()) {
        appendServiceLog(QStringLiteral("  %1").arg(QDir::toNativeSeparators(candidate)));
    }
    appendServiceLog(QStringLiteral("openclaw.cmd 是否存在：%1")
                         .arg(QFileInfo::exists(commandPath) ? QStringLiteral("是") : QStringLiteral("否")));
}

void AgentStartupController::checkEndpointBeforeStart()
{
    if (splash_) {
        splash_->setStatus(QStringLiteral("正在检查 OpenClaw 服务..."),
                           QStringLiteral("检测目标页面端口是否已经可连接。"));
    }

    appendServiceLog(QStringLiteral("启动服务前检查目标页面端口。"));
    waitForEndpointAsync(pageUrl_,
                         500,
                         this,
                         [this](qint64) {
                             if (splash_) {
                                 splash_->setBusyProgress();
                             }
                         },
                         [this](bool endpointAvailable) {
                             if (endpointAvailable) {
                                 appendServiceLog(QStringLiteral("目标页面已可连接，跳过 OpenClaw 服务启动。"));
                                 qInfo().noquote() << "Agent 页面端口已可连接：" << pageUrl_;
                                 if (splash_) {
                                     splash_->setStatus(QStringLiteral("服务已启动，正在打开页面..."),
                                                        pageUrl_.toString());
                                 }
                                 finishStartup();
                                 return;
                             }

                             appendServiceLog(QStringLiteral("目标页面暂不可连接，准备启动 OpenClaw 服务。"));
                             if (splash_) {
                                 splash_->setStatus(QStringLiteral("正在启动 OpenClaw 服务..."),
                                                    QStringLiteral("执行 cmd.exe /d /c openclaw.cmd gateway。"));
                                 splash_->setBusyProgress();
                             }

                             if (startOpenClawGateway()) {
                                 waitForServiceReady();
                             } else {
                                 if (splash_) {
                                     splash_->setStatus(QStringLiteral("服务启动失败，正在尝试打开页面..."),
                                                        QStringLiteral("日志：%1").arg(pathForDisplay(openClawServiceLogPath())));
                                 }
                                 finishStartup();
                             }
                         });
}

bool AgentStartupController::startOpenClawGateway()
{
    const QString serviceDir = openClawServiceDirectory();
    const QString commandPath = QDir(serviceDir).filePath(QStringLiteral("openclaw.cmd"));

    appendServiceLog(QStringLiteral("---------------- 请求启动 OpenClaw ----------------"));

    if (!QFileInfo::exists(commandPath)) {
        appendServiceLog(QStringLiteral("未找到启动脚本：%1").arg(commandPath));
        qWarning().noquote() << "未找到 OpenClaw 服务启动脚本：" << commandPath;
        return false;
    }

    openClawGateway_->setWorkingDirectory(serviceDir);
    openClawGateway_->setProgram(QStringLiteral("cmd.exe"));
    openClawGateway_->setArguments({QStringLiteral("/d"),
                                    QStringLiteral("/c"),
                                    QStringLiteral("openclaw.cmd"),
                                    QStringLiteral("gateway")});

    appendServiceLog(QStringLiteral("正在通过 cmd.exe 启动 OpenClaw"));
    appendServiceLog(QStringLiteral("程序：%1").arg(openClawGateway_->program()));
    appendServiceLog(QStringLiteral("参数：%1").arg(openClawGateway_->arguments().join(QLatin1Char(' '))));
    appendServiceLog(QStringLiteral("工作目录：%1").arg(QDir::toNativeSeparators(serviceDir)));

    openClawGateway_->start();
    return true;
}

void AgentStartupController::waitForServiceReady()
{
    if (splash_) {
        splash_->setStatus(QStringLiteral("正在等待 OpenClaw 服务就绪..."),
                           QStringLiteral("服务启动命令已发送，正在等待页面端口响应。"));
    }

    waitForEndpointAsync(pageUrl_,
                         kServiceStartupTimeoutMs,
                         this,
                         [this](qint64 elapsedMs) {
                             if (splash_) {
                                 splash_->setStatus(QStringLiteral("正在等待 OpenClaw 服务就绪..."),
                                                    elapsedSecondsText(elapsedMs));
                             }
                         },
                         [this](bool endpointReady) {
                             if (!endpointReady) {
                                 appendServiceLog(QStringLiteral("等待 OpenClaw 服务就绪超时，继续尝试打开页面。"));
                                 qWarning().noquote() << "Agent 页面端口暂未就绪：" << pageUrl_;
                                 if (splash_) {
                                     splash_->setStatus(QStringLiteral("服务暂未就绪，正在尝试打开页面..."),
                                                        QStringLiteral("日志：%1").arg(pathForDisplay(openClawServiceLogPath())));
                                 }
                             } else {
                                 appendServiceLog(QStringLiteral("OpenClaw 服务端口已就绪。"));
                                 if (splash_) {
                                     splash_->setStatus(QStringLiteral("服务已就绪，正在打开页面..."),
                                                        pageUrl_.toString());
                                 }
                             }

                             finishStartup();
                         });
}

void AgentStartupController::finishStartup()
{
    if (startupFinished_) {
        return;
    }

    startupFinished_ = true;
    emit readyToOpenPage();
}





