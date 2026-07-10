#include "browserpagewidget.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("BrowserPageWidgetDemo"));
    QApplication::setOrganizationName(QStringLiteral("BM"));

    bm::BrowserPageWidget browser;
    browser.setWindowTitle(QStringLiteral("BrowserPageWidget Qt6 Demo"));
    browser.setHomeUrl(QStringLiteral("http://127.0.0.1:19001/"));
    browser.setLoadTimeoutMs(30000);
    browser.setToolbarVisible(false);
    browser.setPopupPolicy(bm::BrowserPageWidget::PopupPolicy::OpenInCurrentView);
    browser.setDownloadPolicy(bm::BrowserPageWidget::DownloadPolicy::AutoSave);

    QObject::connect(&browser, &bm::BrowserPageWidget::loadFailed,
                     [](const QUrl &url, int domain, int code, const QString &message) {
                         qWarning().noquote() << "Page load failed:" << url
                                              << "domain=" << domain
                                              << "code=" << code
                                              << message;
                     });

    QObject::connect(&browser, &bm::BrowserPageWidget::downloadRejected,
                     [](const QUrl &url, const QString &reason) {
                         qWarning().noquote() << "Download rejected:" << url << reason;
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
    browser.resize(1200, 760);
    browser.show();

    return app.exec();
}
