#include "browserpagewidget.h"
#include "agentstartupcontroller.h"
#include "agentstartupsplash.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QUrl>

#include <memory>

namespace {

// 允许命令行传入页面地址；未传入时使用默认本地 Agent 页面。
QUrl agentPageUrl(const QStringList &arguments)
{
    if (arguments.size() > 1) {
        return QUrl::fromUserInput(arguments.at(1));
    }

    return QUrl(QStringLiteral("http://127.0.0.1:19001/"));
}

// 统一连接浏览器事件，主流程只关心“什么时候打开浏览器”。
void connectBrowserSignals(bm::BrowserPageWidget *browser, AgentStartupSplash *splash)
{
    QObject::connect(browser, &bm::BrowserPageWidget::loadStarted,
                     splash, [splash]() {
                         splash->setStatus(QStringLiteral("正在加载 Agent 页面..."));
                         splash->setProgress(0);
                     });

    QObject::connect(browser, &bm::BrowserPageWidget::loadProgress,
                     splash, [splash](int progress) {
                         splash->setProgress(progress);
                     });

    QObject::connect(browser, &bm::BrowserPageWidget::loadFinished,
                     splash, [splash](bool) {
                         splash->close();
                     });

    QObject::connect(browser, &bm::BrowserPageWidget::loadFailed,
                     splash, [splash](const QUrl &url, int domain, int code, const QString &message) {
                         splash->close();
                         qWarning().noquote() << "Agent 页面加载失败：" << url
                                              << "错误域=" << domain
                                              << "错误码=" << code
                                              << message;
                     });

    QObject::connect(browser, &bm::BrowserPageWidget::loadTimedOut,
                     splash, [splash](const QUrl &) {
                         splash->close();
                     });

    QObject::connect(browser, &bm::BrowserPageWidget::renderProcessTerminated,
                     splash, [splash](int status, int exitCode) {
                         splash->close();
                         qWarning().noquote() << "Agent 页面渲染进程异常终止："
                                              << "状态=" << status
                                              << "退出码=" << exitCode;
                     });

    QObject::connect(browser, &bm::BrowserPageWidget::downloadStarted,
                     browser, [](quint32 id, const QUrl &url, const QString &filePath) {
                         qInfo().noquote() << "下载开始：" << id << url << filePath;
                     });

    QObject::connect(browser, &bm::BrowserPageWidget::downloadFinished,
                     browser, [](quint32 id, const QString &filePath) {
                         qInfo().noquote() << "下载完成：" << id << filePath;
                     });

    QObject::connect(browser, &bm::BrowserPageWidget::downloadFailed,
                     browser, [](quint32 id, const QString &reason) {
                         qWarning().noquote() << "下载失败：" << id << reason;
                     });
}

// 延迟创建浏览器，避免服务等待阶段提前初始化 WebEngine。
std::unique_ptr<bm::BrowserPageWidget> createBrowser(const QUrl &pageUrl, AgentStartupSplash *splash)
{
    auto browser = std::make_unique<bm::BrowserPageWidget>();
    browser->setWindowTitle(QStringLiteral("RF Claw"));
    browser->setWindowIcon(QIcon(":/resources/logo.ico"));
    browser->setHomeUrl(pageUrl);
    browser->setLoadTimeoutMs(30000);
    browser->setToolbarVisible(false);
    browser->setStatusBarVisible(true);
    browser->setPopupPolicy(bm::BrowserPageWidget::PopupPolicy::OpenInCurrentView);
    browser->setDownloadPolicy(bm::BrowserPageWidget::DownloadPolicy::AutoSave);

    connectBrowserSignals(browser.get(), splash);
    return browser;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("AgentPageViewer"));
    QApplication::setOrganizationName(QStringLiteral("BM"));

    const QUrl pageUrl = agentPageUrl(QCoreApplication::arguments());

    AgentStartupSplash splash(pageUrl);
    splash.setStatus(QStringLiteral("正在检查 Agent 服务..."),
                     QStringLiteral("检测目标页面端口是否已经可连接。"));
    splash.show();

    AgentStartupController startupController(pageUrl, &splash, &app);
    std::unique_ptr<bm::BrowserPageWidget> browser;

    QObject::connect(&startupController, &AgentStartupController::readyToOpenPage,
                     &app, [&]() {
                         if (!browser) {
                             browser = createBrowser(pageUrl, &splash);
                             browser->loadUrl(browser->homeUrl());
                             browser->resize(1280, 820);
                         }

                         browser->show();
                         browser->raise();
                     });

    // 从事件循环开始后再执行启动流程，保证启动画面可以先绘制并持续响应。
    QTimer::singleShot(0, &startupController, &AgentStartupController::start);

    return app.exec();
}