#include "browserpagewidget.h"
#include "agentstartupcontroller.h"
#include "agentstartupsplash.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QMessageBox>
#include <QCoreApplication>
#include <QDebug>
#include <QIcon>
#include <QTimer>
#include <QUrl>

#include <memory>

namespace {

// 统一连接浏览器事件，主流程只关心“什么时候打开浏览器”。
void connectBrowserSignals(bm::BrowserPageWidget *browser, AgentStartupSplash *splash)
{
    QObject::connect(browser, &bm::BrowserPageWidget::loadStarted,
                     splash, [splash]() {
                         splash->setStatus(QStringLiteral("加载页面..."), QStringLiteral(""));
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
    QApplication::setApplicationName(QStringLiteral("RFClaw"));
    QApplication::setOrganizationName(QStringLiteral("BM"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("DeepSeek Harness Qt viewer"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("service-dir"), QStringLiteral("DSH portable directory"), QStringLiteral("directory")});
    parser.addPositionalArgument(QStringLiteral("url"), QStringLiteral("Optional existing service URL; do not start or stop a service"));
    parser.process(app);
    if (parser.positionalArguments().size() > 1) parser.showHelp(1);
    const QUrl pageUrl = parser.positionalArguments().isEmpty() ? QUrl() : QUrl(parser.positionalArguments().first());

    AgentStartupSplash splash(pageUrl);
    splash.setStatus(QStringLiteral("检查服务..."), QStringLiteral(""));
    splash.show();

    AgentStartupController startupController(pageUrl, &splash, &app, parser.value(QStringLiteral("service-dir")));
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &startupController, &AgentStartupController::stopService);
    QObject::connect(&startupController, &AgentStartupController::startupFailed, &app, [&](const QString &message) {
        splash.hide();
        QMessageBox::critical(nullptr, QStringLiteral("DeepSeek Harness"), message);
        app.exit(1);
    });
    std::unique_ptr<bm::BrowserPageWidget> browser;

    QObject::connect(&splash, &AgentStartupSplash::cancelRequested,
                     &app, [&]() {
                         startupController.stopService();
                         app.quit();
                     });

    QObject::connect(&startupController, &AgentStartupController::readyToOpenPage,
                     &app, [&](const QUrl &readyUrl) {
                         if (!browser) {
                             browser = createBrowser(readyUrl, &splash);
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
