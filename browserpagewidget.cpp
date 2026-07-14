#include "browserpagewidget.h"

#include "browserdownloadmanager_p.h"
#include "browserpagewidget_p.h"

#include <QDir>
#include <QLineEdit>
#include <QProgressBar>
#include <QTimer>
#include <QToolBar>
#include <QtGlobal>
#include <QWebEngineCookieStore>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

namespace bm {

BrowserPageWidget::BrowserPageWidget(QWidget *parent)
    : BrowserPageWidget(new QWebEngineProfile(), true, parent)
{}

BrowserPageWidget::BrowserPageWidget(QWebEngineProfile &profile, QWidget *parent)
    : BrowserPageWidget(&profile, false, parent)
{}

BrowserPageWidget::BrowserPageWidget(QWebEngineProfile *profile,
                                     bool ownsProfile,
                                     QWidget *parent)
    : QWidget(parent)
    , d_ptr(new BrowserPageWidgetPrivate(this, profile, ownsProfile))
{
    Q_D(BrowserPageWidget);
    d->initialize();
}

BrowserPageWidget::~BrowserPageWidget() noexcept = default;

QWebEngineView *BrowserPageWidget::view() const noexcept
{
    Q_D(const BrowserPageWidget);
    return d->view_;
}

QWebEnginePage *BrowserPageWidget::page() const noexcept
{
    Q_D(const BrowserPageWidget);
    return d->view_ ? d->view_->page() : nullptr;
}

QWebEngineProfile *BrowserPageWidget::profile() const noexcept
{
    Q_D(const BrowserPageWidget);
    return d->profile_;
}

QUrl BrowserPageWidget::currentUrl() const
{
    Q_D(const BrowserPageWidget);
    return d->view_ ? d->view_->url() : QUrl();
}

QString BrowserPageWidget::currentTitle() const
{
    Q_D(const BrowserPageWidget);
    return d->view_ ? d->view_->title() : QString();
}

bool BrowserPageWidget::isLoading() const noexcept
{
    Q_D(const BrowserPageWidget);
    return d->loading_;
}

BrowserPageWidget::LoadState BrowserPageWidget::loadState() const noexcept
{
    Q_D(const BrowserPageWidget);
    return d->loadState_;
}

QUrl BrowserPageWidget::homeUrl() const
{
    Q_D(const BrowserPageWidget);
    return d->homeUrl_;
}

void BrowserPageWidget::setHomeUrl(const QUrl &url)
{
    Q_D(BrowserPageWidget);

    QUrl value = url;
    // 支持用户传入 "example.com" 这类不带协议的主页地址。
    if (value.scheme().isEmpty()) {
        value = BrowserPageWidgetPrivate::normalizedUrl(value.toString());
    }
    if (value.isEmpty() || !value.isValid()) {
        value = QUrl(QStringLiteral("about:blank"));
    }
    if (value == d->homeUrl_) {
        return;
    }

    d->homeUrl_ = value;
    emit homeUrlChanged(d->homeUrl_);
}

void BrowserPageWidget::setHomeUrl(const QString &url)
{
    setHomeUrl(BrowserPageWidgetPrivate::normalizedUrl(url));
}

bool BrowserPageWidget::toolbarVisible() const noexcept
{
    Q_D(const BrowserPageWidget);
    return d->toolbar_ && !d->toolbar_->isHidden();
}

void BrowserPageWidget::setToolbarVisible(bool visible)
{
    Q_D(BrowserPageWidget);
    if (!d->toolbar_ || toolbarVisible() == visible) {
        return;
    }

    d->toolbar_->setVisible(visible);
    emit toolbarVisibleChanged(visible);
}

bool BrowserPageWidget::statusBarVisible() const noexcept
{
    Q_D(const BrowserPageWidget);
    return d->statusBar_ && !d->statusBar_->isHidden();
}

void BrowserPageWidget::setStatusBarVisible(bool visible)
{
    Q_D(BrowserPageWidget);
    if (!d->statusBar_ || statusBarVisible() == visible) {
        return;
    }

    d->statusBar_->setVisible(visible);
    emit statusBarVisibleChanged(visible);
}

bool BrowserPageWidget::addressEditable() const noexcept
{
    Q_D(const BrowserPageWidget);
    return d->addressEdit_ && !d->addressEdit_->isReadOnly();
}

void BrowserPageWidget::setAddressEditable(bool editable)
{
    Q_D(BrowserPageWidget);
    if (!d->addressEdit_ || addressEditable() == editable) {
        return;
    }

    d->addressEdit_->setReadOnly(!editable);
    emit addressEditableChanged(editable);
}

qreal BrowserPageWidget::zoomFactor() const noexcept
{
    Q_D(const BrowserPageWidget);
    return d->view_ ? d->view_->zoomFactor() : 1.0;
}

void BrowserPageWidget::setZoomFactor(qreal factor)
{
    Q_D(BrowserPageWidget);
    // WebEngine 支持较大缩放范围，这里收窄到桌面软件中比较可控的区间。
    const qreal value = qBound<qreal>(0.25, factor, 5.0);
    if (!d->view_ || qFuzzyCompare(d->view_->zoomFactor(), value)) {
        return;
    }

    d->view_->setZoomFactor(value);
    emit zoomFactorChanged(value);
}

int BrowserPageWidget::loadTimeoutMs() const noexcept
{
    Q_D(const BrowserPageWidget);
    return d->loadTimeoutMs_;
}

void BrowserPageWidget::setLoadTimeoutMs(int timeoutMs)
{
    Q_D(BrowserPageWidget);
    const int value = qMax(0, timeoutMs);
    if (value == d->loadTimeoutMs_) {
        return;
    }

    d->loadTimeoutMs_ = value;
    if (d->loading_) {
        if (d->loadTimeoutMs_ > 0) {
            d->loadTimeoutTimer_->start(d->loadTimeoutMs_);
        } else {
            d->loadTimeoutTimer_->stop();
        }
    }

    emit loadTimeoutChanged(d->loadTimeoutMs_);
}

QSet<QString> BrowserPageWidget::allowedUrlSchemes() const
{
    Q_D(const BrowserPageWidget);
    return d->navigationPolicy_.allowedUrlSchemes();
}

void BrowserPageWidget::setAllowedUrlSchemes(const QSet<QString> &schemes)
{
    Q_D(BrowserPageWidget);
    d->navigationPolicy_.setAllowedUrlSchemes(schemes);
}

QStringList BrowserPageWidget::allowedHosts() const
{
    Q_D(const BrowserPageWidget);
    return d->navigationPolicy_.allowedHosts();
}

void BrowserPageWidget::setAllowedHosts(const QStringList &hosts)
{
    Q_D(BrowserPageWidget);
    d->navigationPolicy_.setAllowedHosts(hosts);
}

bool BrowserPageWidget::isUrlAllowed(const QUrl &url) const
{
    Q_D(const BrowserPageWidget);
    return d->canNavigate(url);
}

BrowserPageWidget::PopupPolicy BrowserPageWidget::popupPolicy() const noexcept
{
    Q_D(const BrowserPageWidget);
    return d->popupPolicy_;
}

void BrowserPageWidget::setPopupPolicy(PopupPolicy policy)
{
    Q_D(BrowserPageWidget);
    if (d->popupPolicy_ == policy) {
        return;
    }

    d->popupPolicy_ = policy;
    if (d->view_) {
        d->view_->settings()->setAttribute(
            QWebEngineSettings::JavascriptCanOpenWindows,
            d->popupPolicy_ != PopupPolicy::Block);
    }
}

BrowserPageWidget::DownloadPolicy BrowserPageWidget::downloadPolicy() const noexcept
{
    Q_D(const BrowserPageWidget);
    return d->downloadPolicy_;
}

void BrowserPageWidget::setDownloadPolicy(DownloadPolicy policy)
{
    Q_D(BrowserPageWidget);
    d->downloadPolicy_ = policy;
}

QString BrowserPageWidget::downloadDirectory() const
{
    Q_D(const BrowserPageWidget);
    return d->downloadDirectory_;
}

void BrowserPageWidget::setDownloadDirectory(const QString &directory)
{
    Q_D(BrowserPageWidget);
    // 空目录表示回到系统默认下载目录，避免调用方必须自己查询 QStandardPaths。
    d->downloadDirectory_ = directory.trimmed().isEmpty()
                                ? BrowserDownloadManager::defaultDownloadDirectory()
                                : QDir::cleanPath(directory);
}

BrowserPageWidget::PermissionPolicy BrowserPageWidget::permissionPolicy() const noexcept
{
    Q_D(const BrowserPageWidget);
    return d->permissionPolicy_;
}

void BrowserPageWidget::setPermissionPolicy(PermissionPolicy policy)
{
    Q_D(BrowserPageWidget);
    d->permissionPolicy_ = policy;
}

BrowserPageWidget::CertificatePolicy BrowserPageWidget::certificatePolicy() const noexcept
{
    Q_D(const BrowserPageWidget);
    return d->certificatePolicy_;
}

void BrowserPageWidget::setCertificatePolicy(CertificatePolicy policy)
{
    Q_D(BrowserPageWidget);
    d->certificatePolicy_ = policy;
}

QString BrowserPageWidget::userAgent() const
{
    Q_D(const BrowserPageWidget);
    return d->profile_ ? d->profile_->httpUserAgent() : QString();
}

void BrowserPageWidget::setUserAgent(const QString &userAgent)
{
    Q_D(BrowserPageWidget);
    if (d->profile_) {
        d->profile_->setHttpUserAgent(userAgent);
    }
}

void BrowserPageWidget::loadUrl(const QUrl &url)
{
    Q_D(BrowserPageWidget);

    QUrl target = url;
    // 所有加载入口都统一补全地址，减少地址栏输入和代码调用的行为差异。
    if (target.scheme().isEmpty()) {
        target = BrowserPageWidgetPrivate::normalizedUrl(target.toString());
    }

    QString reason;
    // 真正加载前先检查导航策略，防止 file:// 或未知协议绕过控件限制。
    if (!d->canNavigate(target, &reason)) {
        d->handleNavigationBlocked(target, reason);
        return;
    }

    d->view_->load(target);
}

void BrowserPageWidget::loadUrl(const QString &url)
{
    loadUrl(BrowserPageWidgetPrivate::normalizedUrl(url));
}

void BrowserPageWidget::loadHtml(const QString &html, const QUrl &baseUrl)
{
    Q_D(BrowserPageWidget);

    // baseUrl 会影响 HTML 内相对资源的访问，也要经过同一套导航策略。
    if (!baseUrl.isEmpty()) {
        QString reason;
        if (!d->canNavigate(baseUrl, &reason)) {
            d->handleNavigationBlocked(baseUrl, reason);
            return;
        }
    }

    d->view_->setHtml(html, baseUrl);
}

void BrowserPageWidget::goHome()
{
    Q_D(BrowserPageWidget);
    loadUrl(d->homeUrl_);
}

void BrowserPageWidget::back()
{
    Q_D(BrowserPageWidget);
    d->view_->back();
}

void BrowserPageWidget::forward()
{
    Q_D(BrowserPageWidget);
    d->view_->forward();
}

void BrowserPageWidget::reloadPage()
{
    Q_D(BrowserPageWidget);
    d->view_->reload();
}

void BrowserPageWidget::stop()
{
    Q_D(BrowserPageWidget);
    d->view_->stop();
}

void BrowserPageWidget::zoomIn()
{
    setZoomFactor(zoomFactor() + 0.1);
}

void BrowserPageWidget::zoomOut()
{
    setZoomFactor(zoomFactor() - 0.1);
}

void BrowserPageWidget::resetZoom()
{
    setZoomFactor(1.0);
}

void BrowserPageWidget::clearBrowsingData()
{
    Q_D(BrowserPageWidget);
    if (!d->profile_) {
        return;
    }

    // localStorage/sessionStorage 属于当前页面，只能通过页面脚本清理。
    if (page()) {
        page()->runJavaScript(QStringLiteral(
            "try { localStorage.clear(); sessionStorage.clear(); } catch (e) {}"));
    }
    // history、cookie、http cache 分别属于不同对象，需要分开清理。
    if (d->view_ && d->view_->history()) {
        d->view_->history()->clear();
    }
    if (d->profile_->cookieStore()) {
        d->profile_->cookieStore()->deleteAllCookies();
    }

    // HTTP 缓存清理是异步的，完成后再通知宿主。
    connect(d->profile_, &QWebEngineProfile::clearHttpCacheCompleted,
            this, &BrowserPageWidget::browsingDataCleared,
            Qt::SingleShotConnection);
    d->profile_->clearHttpCache();
}

void BrowserPageWidget::commitAddressBar()
{
    Q_D(BrowserPageWidget);
    loadUrl(d->addressEdit_->text());
}

void BrowserPageWidget::onLoadStarted()
{
    Q_D(BrowserPageWidget);

    // 每次开始加载都重置状态，避免上一次超时或失败影响本次页面。
    d->loading_ = true;
    d->timedOut_ = false;
    d->setLoadState(LoadState::Loading);

    d->progressBar_->setValue(0);
    d->progressBar_->show();
    d->updateStatusMessage(tr("正在加载..."));

    if (d->loadTimeoutMs_ > 0) {
        d->loadTimeoutTimer_->start(d->loadTimeoutMs_);
    }

    d->updateNavigationState();
    emit loadStarted();
}

void BrowserPageWidget::onLoadProgress(int progress)
{
    Q_D(BrowserPageWidget);
    d->progressBar_->setValue(progress);
    emit loadProgress(progress);
}

void BrowserPageWidget::onLoadFinished(bool ok)
{
    Q_D(BrowserPageWidget);

    d->loading_ = false;
    d->loadTimeoutTimer_->stop();
    d->progressBar_->setValue(100);
    d->progressBar_->hide();

    // 超时后的失败回调可能晚到，这里避免把 TimedOut 状态覆盖成 Failed。
    if (!d->timedOut_) {
        if (ok) {
            d->setLoadState(LoadState::Succeeded);
            d->updateStatusMessage(tr("加载完成"));
        } else if (d->loadState_ != LoadState::Failed) {
            d->setLoadState(LoadState::Failed);
            d->updateStatusMessage(tr("加载失败"));
        }
    }

    d->updateNavigationState();
    emit loadFinished(ok && !d->timedOut_);
}

void BrowserPageWidget::onTitleChanged(const QString &title)
{
    if (isWindow()) {
        // setWindowTitle(title);
    }
    emit titleChanged(title);
}

void BrowserPageWidget::onUrlChanged(const QUrl &url)
{
    Q_D(BrowserPageWidget);

    // 用户正在编辑地址栏时不覆盖输入，避免打字过程中被 urlChanged 打断。
    if (!d->addressEdit_->hasFocus()) {
        d->addressEdit_->setText(url.toDisplayString());
    }

    d->updateNavigationState();
    emit urlChanged(url);
}

void BrowserPageWidget::onLoadTimeout()
{
    Q_D(BrowserPageWidget);
    if (!d->loading_) {
        return;
    }

    // 超时后主动 stop()，让 WebEngine 尽快停止网络和渲染工作。
    d->timedOut_ = true;
    d->loading_ = false;
    d->setLoadState(LoadState::TimedOut);
    d->updateStatusMessage(tr("加载超时"));
    d->updateNavigationState();

    const QUrl url = currentUrl();
    d->view_->stop();
    emit loadTimedOut(url);
}

} // namespace bm


