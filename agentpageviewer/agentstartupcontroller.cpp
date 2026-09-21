#include "agentstartupcontroller.h"

#include "agentstartupsplash.h"
#include "endpointwaiter.h"
#include "processjob.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QTimer>
#include <QStandardPaths>
#include <QStringList>
#include <QtGlobal>

#include <functional>

namespace {

constexpr int kServiceStartupTimeoutMs = 120000;

// 返回服务目录候选路径，优先满足发布包结构，再兼容开发目录结构。
QStringList dshServiceDirectoryCandidates()
{
    QStringList candidates;
    QDir appDir(QCoreApplication::applicationDirPath());

    // 发布包推荐结构：AgentPageViewer.exe 与 dsh-win7-x64 在同一层目录。
    candidates.append(QDir::cleanPath(appDir.filePath(QStringLiteral("dsh-win7-x64"))));

    // 兼容开发环境：bin/dist/dsh-win7-x64。
    candidates.append(QDir::cleanPath(appDir.filePath(QStringLiteral("../dist/dsh-win7-x64"))));

    return candidates;
}

// 从候选路径中选择真正包含 runtime/node.exe 的服务目录。
QString dshServiceDirectory()
{
    const QStringList candidates = dshServiceDirectoryCandidates();
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(QDir(candidate).filePath(QStringLiteral("runtime/node.exe")))) {
            return candidate;
        }
    }

    return candidates.isEmpty()
               ? QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("dsh-win7-x64"))
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

QString dshStateDirectory(const QString &serviceDirectory)
{
    return QDir(serviceDirectory).filePath(QStringLiteral("data"));
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

QString dshServiceLogPath(const QString &serviceDirectory)
{
    const QString primaryPath = writableLogPathInDirectory(dshStateDirectory(serviceDirectory));
    if (!primaryPath.isEmpty()) {
        return primaryPath;
    }

    return fallbackServiceLogPath();
}

} // namespace

// 向服务诊断日志追加一行中文信息。
void AgentStartupController::appendServiceLog(const QString &message) const
{
    const QString logPath = dshServiceLogPath(serviceDirectory_);
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

AgentStartupController::AgentStartupController(const QUrl &pageUrl,
                                               AgentStartupSplash *splash,
                                               QObject *parent,
                                               const QString &serviceDirectory)
    : QObject(parent)
    , pageUrl_(pageUrl)
    , serviceDirectory_(serviceDirectory.isEmpty() ? dshServiceDirectory() : QFileInfo(serviceDirectory).absoluteFilePath())
    , splash_(splash)
    , agentGateway_(new ProcessJob(this))
{
    agentGateway_->setFinishedCallback([this](int exitCode) {
        appendServiceLog(QStringLiteral("Agent 服务进程结束，退出码=%1。").arg(exitCode));
        qWarning().noquote() << "Agent 服务进程结束：退出码=" << exitCode;
        failStartup(QStringLiteral("DSH 服务已退出，退出码=%1。").arg(exitCode));
    });
}

AgentStartupController::~AgentStartupController()
{
    stopService();
}

void AgentStartupController::start()
{
    if (!pageUrl_.isEmpty() && (!pageUrl_.isValid() || pageUrl_.host().isEmpty()
        || (pageUrl_.scheme() != "http" && pageUrl_.scheme() != "https"))) {
        failStartup(QStringLiteral("网页地址必须为有效的 HTTP 或 HTTPS 地址。"));
        return;
    }
    appendStartupDiagnostics();
    checkEndpointBeforeStart();
}

void AgentStartupController::stopService()
{
    startupFinished_ = true;
    if (startupPoll_) startupPoll_->stop();
    if (!agentGateway_ || !agentGateway_->isActive()) {
        return;
    }

    appendServiceLog(QStringLiteral("正在关闭本程序启动的 Agent 进程树。"));
    agentGateway_->close();
}

void AgentStartupController::appendStartupDiagnostics()
{
    const QString serviceDir = serviceDirectory_;
    const QString commandPath = QDir(serviceDir).filePath(QStringLiteral("runtime/node.exe"));

    appendServiceLog(QStringLiteral("---------------- AgentPageViewer 启动 ----------------"));
    appendServiceLog(QStringLiteral("程序目录：%1")
                         .arg(QDir::toNativeSeparators(QCoreApplication::applicationDirPath())));
    appendServiceLog(QStringLiteral("Agent 服务目录：%1")
                         .arg(QDir::toNativeSeparators(serviceDir)));
    appendServiceLog(QStringLiteral("服务日志路径：%1")
                         .arg(pathForDisplay(dshServiceLogPath(serviceDirectory_))));
    appendServiceLog(QStringLiteral("目标页面：%1").arg(pageUrl_.toString(QUrl::RemoveQuery)));
    appendServiceLog(QStringLiteral("服务目录候选列表："));
    for (const QString &candidate : dshServiceDirectoryCandidates()) {
        appendServiceLog(QStringLiteral("  %1").arg(QDir::toNativeSeparators(candidate)));
    }
    appendServiceLog(QStringLiteral("runtime/node.exe 是否存在：%1")
                         .arg(QFileInfo::exists(commandPath) ? QStringLiteral("是") : QStringLiteral("否")));
}

void AgentStartupController::checkEndpointBeforeStart()
{
    if (splash_) {
        splash_->setStatus(QStringLiteral("检查服务..."), QStringLiteral(""));
    }

    if (pageUrl_.isEmpty()) {
        if (startAgentGateway()) {
            waitForStartupAddress();
        } else {
            failStartup(QStringLiteral("启动 DSH 失败，请检查便携包及服务日志。"));
        }
        return;
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
                             if (startupFinished_) return;
                             if (endpointAvailable) {
                                 appendServiceLog(QStringLiteral("目标页面已可连接，跳过 Agent 服务启动。"));
                                 qInfo().noquote() << "Agent 页面端口已可连接：" << pageUrl_.toString(QUrl::RemoveQuery);
                                 if (splash_) {
                                     splash_->setStatus(QStringLiteral("打开页面..."), QStringLiteral(""));
                                 }
                                 finishStartup();
                                 return;
                             }

                             failStartup(QStringLiteral("指定的服务地址无法连接，请先启动该服务。"));
                         });
}

bool AgentStartupController::startAgentGateway()
{
    const QString serviceDir = serviceDirectory_;
    const QString commandPath = QDir(serviceDir).filePath(QStringLiteral("runtime/node.exe"));

    appendServiceLog(QStringLiteral("---------------- 请求启动 Agent ----------------"));

    if (!QFileInfo::exists(commandPath)) {
        appendServiceLog(QStringLiteral("未找到 Node 程序：%1").arg(commandPath));
        qWarning().noquote() << "未找到 Agent 服务 Node 程序：" << commandPath;
        return false;
    }

    const QString program = commandPath;
    const QString entry = QDir(serviceDir).filePath(QStringLiteral("app/node_modules/@deepseek-ai/dsh/lib/bin.js"));
    if (!QFileInfo::exists(entry)) {
        appendServiceLog(QStringLiteral("未找到 DSH 入口：%1").arg(entry));
        return false;
    }
    const QString dataDir = dshStateDirectory(serviceDir);
    if (!QDir().mkpath(dataDir)) return false;
    startupOutput_ = new QTemporaryFile(QDir(dataDir).filePath(QStringLiteral("qt-startup-XXXXXX.log")), this);
    if (!startupOutput_->open()) {
        appendServiceLog(QStringLiteral("无法创建启动输出文件：%1").arg(startupOutput_->errorString()));
        return false;
    }
    startupOutput_->close();
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("DSH_HOME"), dataDir);
    environment.remove(QStringLiteral("NODE_PATH"));
    environment.remove(QStringLiteral("NODE_OPTIONS"));
    environment.insert(QStringLiteral("PATH"), QDir::toNativeSeparators(QDir(serviceDir).filePath("runtime"))
                       + QDir::listSeparator() + environment.value(QStringLiteral("PATH")));
    const QStringList arguments{entry, QStringLiteral("web"), QStringLiteral("--no-open"),
                                QStringLiteral("--host"), QStringLiteral("127.0.0.1"),
                                QStringLiteral("--port"), QStringLiteral("0")};

    appendServiceLog(QStringLiteral("正在通过便携包 Node 启动 DSH"));
    appendServiceLog(QStringLiteral("程序：%1").arg(program));
    appendServiceLog(QStringLiteral("参数：%1").arg(arguments.join(QLatin1Char(' '))));
    appendServiceLog(QStringLiteral("工作目录：%1").arg(QDir::toNativeSeparators(serviceDir)));

    if (!agentGateway_->start(program, arguments, serviceDir, environment, startupOutput_->fileName())) {
        appendServiceLog(QStringLiteral("Agent 启动失败：%1").arg(agentGateway_->lastErrorString()));
        qWarning().noquote() << "Agent 服务启动失败：" << agentGateway_->lastErrorString();
        return false;
    }

    appendServiceLog(QStringLiteral("Agent 启动进程已创建，PID=%1，并已纳入进程作业。")
                         .arg(agentGateway_->processId()));
    qInfo().noquote() << "Agent 启动进程已创建，PID=" << agentGateway_->processId();
    return true;
}

void AgentStartupController::waitForStartupAddress()
{
    startupElapsed_.start();
    startupPoll_ = new QTimer(this);
    startupPoll_->setInterval(100);
    connect(startupPoll_, &QTimer::timeout, this, [this]() {
        if (startupFinished_) return;
        QFile output(startupOutput_->fileName());
        if (output.open(QIODevice::ReadOnly)) {
            if (output.size() > 262144) output.seek(output.size() - 262144);
            static const QRegularExpression pattern(QStringLiteral(
                "(?:^|[\r\n])dsh web: (http://127\\.0\\.0\\.1:[0-9]+/\\?token=[A-Za-z0-9_-]+)[\r\n]"));
            const auto match = pattern.match(QString::fromUtf8(output.readAll()));
            if (match.hasMatch()) {
                const QUrl url(match.captured(1));
                if (url.port() > 0 && url.port() <= 65535) {
                    pageUrl_ = url;
                    startupPoll_->stop();
                    waitForServiceReady();
                    return;
                }
            }
        }
        if (splash_) splash_->setStatus(QStringLiteral("等待服务地址..."), elapsedSecondsText(startupElapsed_.elapsed()));
        if (startupElapsed_.elapsed() >= kServiceStartupTimeoutMs) {
            failStartup(QStringLiteral("等待 DSH 输出启动地址超时。"));
        }
    });
    startupPoll_->start();
}

void AgentStartupController::failStartup(const QString &message)
{
    if (startupOutput_) startupOutput_->setAutoRemove(false);
    appendServiceLog(message);
    stopService();
    emit startupFailed(message + QStringLiteral("\n日志：%1").arg(pathForDisplay(dshServiceLogPath(serviceDirectory_)))
                       + (startupOutput_ ? QStringLiteral("\n启动输出：%1").arg(pathForDisplay(startupOutput_->fileName())) : QString()));
}

void AgentStartupController::waitForServiceReady()
{
    if (splash_) {
        splash_->setStatus(QStringLiteral("等待服务..."), QStringLiteral(""));
    }

    waitForEndpointAsync(pageUrl_,
                         kServiceStartupTimeoutMs,
                         this,
                         [this](qint64 elapsedMs) {
                             if (splash_) {
                                 splash_->setStatus(QStringLiteral("等待服务..."),
                                                    elapsedSecondsText(elapsedMs));
                             }
                         },
                         [this](bool endpointReady) {
                             if (startupFinished_) return;
                             if (!endpointReady) {
                                 failStartup(QStringLiteral("等待 Agent 服务就绪超时。"));
                                 return;
                             } else {
                                 appendServiceLog(QStringLiteral("Agent 服务端口已就绪。"));
                                 if (splash_) {
                                     splash_->setStatus(QStringLiteral("打开页面..."), QStringLiteral(""));
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
    emit readyToOpenPage(pageUrl_);
}
