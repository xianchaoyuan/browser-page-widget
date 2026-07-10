#include "browserdownloadmanager_p.h"

#include "browserpagewidget.h"
#include "browserpagewidget_p.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QProgressBar>
#include <QStandardPaths>
#include <QtGlobal>
#include <QWebEngineDownloadRequest>
#include <QWebEnginePage>
#include <QWebEngineView>

#include <memory>

namespace bm {

namespace {

// 记录上一次 UI 进度，避免下载过程中高频刷新导致界面卡顿。
struct DownloadProgressState
{
    int lastPercent = -1;
    bool indeterminateShown = false;
};

// 下载进入这些状态后不会再恢复，可用于收尾和释放临时对象。
bool isTerminalDownloadState(QWebEngineDownloadRequest::DownloadState state)
{
    return state == QWebEngineDownloadRequest::DownloadCompleted
           || state == QWebEngineDownloadRequest::DownloadInterrupted
           || state == QWebEngineDownloadRequest::DownloadCancelled;
}

// 判断下载是否来自当前控件，防止共享 Profile 时误处理其他页面的下载。
bool pageBelongsToCurrentView(QWebEnginePage *page, QWebEnginePage *mainPage)
{
    if (!page || !mainPage) {
        return true;
    }

    // 弹窗页以主页面为 parent，沿 parent 链向上找即可确认归属。
    for (QObject *object = page; object; object = object->parent()) {
        if (object == mainPage) {
            return true;
        }
    }

    return false;
}

void deleteTransientPageWhenDownloadDone(QWebEngineDownloadRequest *download,
                                         QWebEnginePage *mainPage)
{
    QWebEnginePage *downloadPage = download ? download->page() : nullptr;
    if (!downloadPage || downloadPage == mainPage) {
        return;
    }

    // target="_blank" 直接触发下载时不会再产生有效 URL，需要在下载结束后回收临时 Page。
    QObject::connect(download, &QWebEngineDownloadRequest::stateChanged,
                     downloadPage,
                     [downloadPage](QWebEngineDownloadRequest::DownloadState state) {
                         if (isTerminalDownloadState(state)) {
                             downloadPage->deleteLater();
                         }
                     });
}

// 更新状态栏进度；只有百分比变化或首次进入未知总大小模式时才刷新 UI。
void updateDownloadProgress(BrowserPageWidgetPrivate *browser,
                            BrowserPageWidget *q,
                            const QString &fileName,
                            qint64 receivedBytes,
                            qint64 totalBytes,
                            DownloadProgressState *state)
{
    if (!browser->progressBar_ || !state) {
        return;
    }

    // totalBytes 不可用时使用忙碌进度条，不强行计算百分比。
    if (totalBytes <= 0) {
        if (state->indeterminateShown) {
            return;
        }

        state->indeterminateShown = true;
        browser->progressBar_->setRange(0, 0);
        browser->progressBar_->show();
        browser->updateStatusMessage(q->tr("正在下载：%1").arg(fileName));
        return;
    }

    // 只按整数百分比刷新，可显著降低大文件下载时的 UI 更新频率。
    const int percent = qBound(0,
                               static_cast<int>((static_cast<double>(receivedBytes) * 100.0)
                                                / static_cast<double>(totalBytes)),
                               100);
    if (state->lastPercent == percent) {
        return;
    }

    state->lastPercent = percent;
    browser->progressBar_->setRange(0, 100);
    browser->progressBar_->setValue(percent);
    browser->progressBar_->show();
    browser->updateStatusMessage(q->tr("正在下载：%1（%2%）").arg(fileName).arg(percent));
}

// 下载结束时恢复普通状态栏；如果页面仍在加载，则不隐藏页面加载进度。
void finishDownloadProgress(BrowserPageWidgetPrivate *browser,
                            const QString &message)
{
    if (browser->progressBar_ && !browser->loading_) {
        browser->progressBar_->hide();
        browser->progressBar_->setRange(0, 100);
    }
    browser->updateStatusMessage(message);
}

} // namespace

BrowserDownloadManager::BrowserDownloadManager(BrowserPageWidgetPrivate *browser)
    : browser_(browser)
{
    Q_ASSERT(browser_ != nullptr);
}

QString BrowserDownloadManager::defaultDownloadDirectory()
{
    // 优先使用系统下载目录；极端情况下再退回到用户目录下的 Downloads。
    QString directory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (directory.isEmpty()) {
        directory = QDir::homePath() + QStringLiteral("/Downloads");
    }
    return QDir::cleanPath(directory);
}

void BrowserDownloadManager::handleDownloadRequested(QWebEngineDownloadRequest *download)
{
    if (!download) {
        return;
    }

    // 共享 Profile 会广播所有下载请求，这里只接收本控件页面和它创建的临时弹窗页。
    if (!pageBelongsToCurrentView(download->page(), browser_->view_->page())) {
        return;
    }

    BrowserPageWidget *q = browser_->q_ptr;
    if (browser_->downloadPolicy_ == BrowserPageWidget::DownloadPolicy::Deny) {
        const QString reason = q->tr("下载策略禁止保存文件");
        download->cancel();
        browser_->updateStatusMessage(reason);
        emit q->downloadRejected(download->url(), reason);
        return;
    }

    if (browser_->downloadPolicy_ == BrowserPageWidget::DownloadPolicy::DelegateToApplication) {
        browser_->updateStatusMessage(q->tr("下载请求已交给宿主程序处理"));
        emit q->downloadRequested(download);
        return;
    }

    // AutoSave 模式需要确保目录存在，目录不可写时明确拒绝下载。
    QDir directory(browser_->downloadDirectory_);
    if (!directory.mkpath(QStringLiteral("."))) {
        const QString reason = q->tr("无法创建下载目录：%1").arg(browser_->downloadDirectory_);
        download->cancel();
        browser_->updateStatusMessage(reason);
        emit q->downloadRejected(download->url(), reason);
        return;
    }

    // 只采用建议路径中的文件名部分，并生成未占用名称，避免目录穿越和覆盖。
    const QString fileName = uniqueDownloadFileName(browser_->downloadDirectory_,
                                                    download->suggestedFileName());
    download->setDownloadDirectory(browser_->downloadDirectory_);
    download->setDownloadFileName(fileName);

    const quint32 id = download->id();
    const QUrl url = download->url();
    const QString filePath = directory.filePath(fileName);
    auto progressState = std::make_shared<DownloadProgressState>();

    // receivedBytes 和 totalBytes 都可能变化，两个信号共用同一个进度处理逻辑。
    auto emitProgress = [this, q, download, id, fileName, progressState]() {
        updateDownloadProgress(browser_, q, fileName,
                               download->receivedBytes(),
                               download->totalBytes(),
                               progressState.get());
        emit q->downloadProgress(
            id, download->receivedBytes(), download->totalBytes());
    };

    QObject::connect(download,
                     &QWebEngineDownloadRequest::receivedBytesChanged,
                     q,
                     emitProgress);
    QObject::connect(download,
                     &QWebEngineDownloadRequest::totalBytesChanged,
                     q,
                     emitProgress);
    QObject::connect(download, &QWebEngineDownloadRequest::stateChanged, q, [this, q, download, id, filePath](
                         QWebEngineDownloadRequest::DownloadState state) {
                         if (state == QWebEngineDownloadRequest::DownloadCompleted) {
                             finishDownloadProgress(browser_, q->tr("下载完成：%1").arg(filePath));
                             emit q->downloadFinished(id, filePath);
                             return;
                         }

                         if (state == QWebEngineDownloadRequest::DownloadInterrupted) {
                             const QString reason = download->interruptReasonString();
                             finishDownloadProgress(browser_, q->tr("下载失败：%1").arg(reason));
                             emit q->downloadFailed(id, reason);
                             return;
                         }

                         if (state == QWebEngineDownloadRequest::DownloadCancelled) {
                             const QString reason = q->tr("下载已取消");
                             finishDownloadProgress(browser_, q->tr("下载失败：%1").arg(reason));
                             emit q->downloadFailed(id, reason);
                         }
                     });

    // accept() 前先挂好收尾逻辑，避免非常快的下载错过完成信号。
    deleteTransientPageWhenDownloadDone(download, browser_->view_->page());

    download->accept();
    updateDownloadProgress(browser_, q, fileName,
                           download->receivedBytes(),
                           download->totalBytes(),
                           progressState.get());
    emit q->downloadStarted(id, url, filePath);
}

QString BrowserDownloadManager::uniqueDownloadFileName(const QString &directory,
                                                       const QString &suggestedName)
{
    // QFileInfo(...).fileName() 会丢弃路径部分，避免网页把文件保存到指定目录外。
    QString fileName = QFileInfo(suggestedName).fileName();
    if (fileName.isEmpty()) {
        fileName = QStringLiteral("download.bin");
    }

    const QDir dir(directory);
    if (!dir.exists(fileName)) {
        return fileName;
    }

    const QFileInfo info(fileName);
    const QString baseName = info.completeBaseName().isEmpty()
                                 ? QStringLiteral("download")
                                 : info.completeBaseName();
    const QString suffix = info.suffix();

    // 同名文件按 "name (1).ext" 递增，避免覆盖用户已有文件。
    for (int index = 1; index < 10000; ++index) {
        const QString candidate = suffix.isEmpty() ? QStringLiteral("%1 (%2)").arg(baseName).arg(index)
                                                   : QStringLiteral("%1 (%2).%3")
                .arg(baseName)
                .arg(index)
                .arg(suffix);
        if (!dir.exists(candidate)) {
            return candidate;
        }
    }

    // 极少数情况下编号耗尽，用时间戳兜底生成一个基本唯一的文件名。
    return QStringLiteral("%1-%2.%3")
        .arg(baseName)
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(suffix.isEmpty() ? QStringLiteral("bin") : suffix);
}

} // namespace bm
