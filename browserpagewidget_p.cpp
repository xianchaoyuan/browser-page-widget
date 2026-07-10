#include "browserpagewidget_p.h"

#include "browserdownloadmanager_p.h"
#include "browserwebpage_p.h"

#include <QAction>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QSize>
#include <QSizePolicy>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWebEngineHistory>
#include <QWebEngineLoadingInfo>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

namespace bm {

BrowserPageWidgetPrivate::BrowserPageWidgetPrivate(BrowserPageWidget *q,
                                                   QWebEngineProfile *profile,
                                                   bool ownsProfile)
    : q_ptr(q)
    , profile_(profile)
    , ownsProfile_(ownsProfile)
    , view_(nullptr)
    , toolbar_(nullptr)
    , statusBar_(nullptr)
    , addressEdit_(nullptr)
    , progressBar_(nullptr)
    , statusLabel_(nullptr)
    , backAction_(nullptr)
    , forwardAction_(nullptr)
    , reloadAction_(nullptr)
    , stopAction_(nullptr)
    , homeAction_(nullptr)
    , loadTimeoutTimer_(nullptr)
    , loading_(false)
    , timedOut_(false)
    , loadState_(BrowserPageWidget::LoadState::Idle)
    , homeUrl_(QStringLiteral("about:blank"))
    , loadTimeoutMs_(30000)
    , navigationPolicy_()
    , popupPolicy_(BrowserPageWidget::PopupPolicy::OpenInCurrentView)
    , downloadPolicy_(BrowserPageWidget::DownloadPolicy::Deny)
    , permissionPolicy_(BrowserPageWidget::PermissionPolicy::Deny)
    , certificatePolicy_(BrowserPageWidget::CertificatePolicy::Reject)
    , downloadDirectory_(BrowserDownloadManager::defaultDownloadDirectory())
    , downloadManager_(new BrowserDownloadManager(this))
{
    Q_ASSERT(q_ptr != nullptr);
    Q_ASSERT(profile_ != nullptr);
}

BrowserPageWidgetPrivate::~BrowserPageWidgetPrivate() noexcept
{
    // QWebEnginePage 必须先于其 Profile 销毁，否则 Chromium 可能访问失效资源。
    delete view_;
    view_ = nullptr;

    if (ownsProfile_) {
        delete profile_;
    }
    profile_ = nullptr;
}

void BrowserPageWidgetPrivate::initialize()
{
    Q_Q(BrowserPageWidget);

    // 初始化顺序固定：先创建 UI，再安装 WebEngine Page，最后连接信号槽。
    setupUi();
    setupWebEngine();
    setupConnections();
    updateNavigationState();
    updateStatusMessage(q->tr("就绪"));
}

QUrl BrowserPageWidgetPrivate::normalizedUrl(const QString &text)
{
    const QString value = text.trimmed();
    if (value.isEmpty()) {
        return QUrl();
    }
    return QUrl::fromUserInput(value);
}

void BrowserPageWidgetPrivate::setupUi()
{
    Q_Q(BrowserPageWidget);

    // 顶部工具栏提供浏览器常见操作：后退、前进、刷新、停止、主页和地址输入。
    toolbar_ = new QToolBar(q);
    toolbar_->setObjectName(QStringLiteral("browserToolBar"));
    toolbar_->setIconSize(QSize(18, 18));
    toolbar_->setMovable(false);
    toolbar_->setFloatable(false);
    toolbar_->setToolButtonStyle(Qt::ToolButtonIconOnly);

    backAction_ = toolbar_->addAction(
        q->style()->standardIcon(QStyle::SP_ArrowBack), q->tr("后退"));
    backAction_->setObjectName(QStringLiteral("browserBackAction"));

    forwardAction_ = toolbar_->addAction(
        q->style()->standardIcon(QStyle::SP_ArrowForward), q->tr("前进"));
    forwardAction_->setObjectName(QStringLiteral("browserForwardAction"));

    reloadAction_ = toolbar_->addAction(
        q->style()->standardIcon(QStyle::SP_BrowserReload), q->tr("刷新"));
    reloadAction_->setObjectName(QStringLiteral("browserReloadAction"));

    stopAction_ = toolbar_->addAction(
        q->style()->standardIcon(QStyle::SP_BrowserStop), q->tr("停止"));
    stopAction_->setObjectName(QStringLiteral("browserStopAction"));

    homeAction_ = toolbar_->addAction(
        q->style()->standardIcon(QStyle::SP_DirHomeIcon), q->tr("主页"));
    homeAction_->setObjectName(QStringLiteral("browserHomeAction"));

    // 地址栏只是 UI 输入入口，真正的地址规范化和策略检查在 loadUrl() 中完成。
    addressEdit_ = new QLineEdit(q);
    addressEdit_->setObjectName(QStringLiteral("browserAddressEdit"));
    addressEdit_->setClearButtonEnabled(true);
    addressEdit_->setPlaceholderText(q->tr("输入网址后按 Enter"));
    addressEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar_->addWidget(addressEdit_);

    // QWebEngineView 是真正承载网页渲染的控件。
    view_ = new QWebEngineView(q);
    view_->setObjectName(QStringLiteral("browserWebEngineView"));

    // 底部状态栏用于显示加载状态、链接悬停地址和下载进度。
    statusBar_ = new QWidget(q);
    statusBar_->setObjectName(QStringLiteral("browserStatusBar"));

    QHBoxLayout *statusLayout = new QHBoxLayout(statusBar_);
    statusLayout->setContentsMargins(8, 2, 8, 4);
    statusLayout->setSpacing(8);

    statusLabel_ = new QLabel(statusBar_);
    statusLabel_->setObjectName(QStringLiteral("browserStatusLabel"));
    statusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    progressBar_ = new QProgressBar(statusBar_);
    progressBar_->setObjectName(QStringLiteral("browserProgressBar"));
    progressBar_->setRange(0, 100);
    progressBar_->setFixedWidth(120);
    progressBar_->hide();

    statusLayout->addWidget(statusLabel_);
    statusLayout->addWidget(progressBar_);

    // 加载超时用单次定时器实现，避免网页长期卡在 loading 状态。
    loadTimeoutTimer_ = new QTimer(q);
    loadTimeoutTimer_->setSingleShot(true);

    QVBoxLayout *rootLayout = new QVBoxLayout(q);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(toolbar_);
    rootLayout->addWidget(view_, 1);
    rootLayout->addWidget(statusBar_);
}

void BrowserPageWidgetPrivate::setupWebEngine()
{
    // 自定义 Page 负责导航拦截、弹窗接管和控制台日志转发。
    view_->setPage(new BrowserWebPage(profile_, this, view_));

    // 默认关闭高风险能力，只按桌面业务需要保留基础浏览能力。
    QWebEngineSettings *settings = view_->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, popupPolicy_ != BrowserPageWidget::PopupPolicy::Block);
    settings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, false);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
    settings->setAttribute(QWebEngineSettings::ErrorPageEnabled, true);
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    settings->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, false);
    settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, false);
    settings->setAttribute(QWebEngineSettings::AllowGeolocationOnInsecureOrigins, false);
    settings->setAttribute(QWebEngineSettings::PdfViewerEnabled, true);
    settings->setAttribute(QWebEngineSettings::NavigateOnDropEnabled, false);
    settings->setUnknownUrlSchemePolicy(QWebEngineSettings::DisallowUnknownUrlSchemes);

    // 只有控件自建的 Profile 才在这里设置权限持久化策略，避免改动宿主传入的共享 Profile。
    if (ownsProfile_) {
        profile_->setPersistentPermissionsPolicy(QWebEngineProfile::PersistentPermissionsPolicy::AskEveryTime);
    }
}

void BrowserPageWidgetPrivate::setupConnections()
{
    Q_Q(BrowserPageWidget);

    // 工具栏按钮连接到公开槽函数，外部代码和 UI 操作走同一套逻辑。
    QObject::connect(backAction_, &QAction::triggered,
                     q, &BrowserPageWidget::back);
    QObject::connect(forwardAction_, &QAction::triggered,
                     q, &BrowserPageWidget::forward);
    QObject::connect(reloadAction_, &QAction::triggered,
                     q, &BrowserPageWidget::reloadPage);
    QObject::connect(stopAction_, &QAction::triggered,
                     q, &BrowserPageWidget::stop);
    QObject::connect(homeAction_, &QAction::triggered,
                     q, &BrowserPageWidget::goHome);
    QObject::connect(addressEdit_, &QLineEdit::returnPressed,
                     q, &BrowserPageWidget::commitAddressBar);
    QObject::connect(loadTimeoutTimer_, &QTimer::timeout,
                     q, &BrowserPageWidget::onLoadTimeout);

    // WebEngine 加载信号统一汇总到 BrowserPageWidget，再由控件维护状态栏和公开信号。
    QObject::connect(view_, &QWebEngineView::loadStarted,
                     q, &BrowserPageWidget::onLoadStarted);
    QObject::connect(view_, &QWebEngineView::loadProgress,
                     q, &BrowserPageWidget::onLoadProgress);
    QObject::connect(view_, &QWebEngineView::loadFinished,
                     q, &BrowserPageWidget::onLoadFinished);
    QObject::connect(view_, &QWebEngineView::titleChanged,
                     q, &BrowserPageWidget::onTitleChanged);
    QObject::connect(view_, &QWebEngineView::urlChanged,
                     q, &BrowserPageWidget::onUrlChanged);

    // 鼠标悬停链接时临时显示 URL，离开链接后恢复原状态消息。
    QObject::connect(view_->page(), &QWebEnginePage::linkHovered,
                     q, [this](const QString &url) {
                         statusLabel_->setText(url.isEmpty() ? statusMessage_ : url);
                     });
    // loadingChanged 能提供更详细的失败信息，比 loadFinished(false) 更适合生成错误信号。
    QObject::connect(view_->page(), &QWebEnginePage::loadingChanged,
                     q, [this](const QWebEngineLoadingInfo &loadingInfo) {
                         handleLoadingInfo(loadingInfo);
                     });
    QObject::connect(
        // Chromium 渲染进程崩溃时要立即停止加载状态，避免 UI 一直显示正在加载。
        view_->page(), &QWebEnginePage::renderProcessTerminated,
        q,
        [this](QWebEnginePage::RenderProcessTerminationStatus status, int exitCode) {
            Q_Q(BrowserPageWidget);
            loading_ = false;
            loadTimeoutTimer_->stop();
            setLoadState(BrowserPageWidget::LoadState::Failed);
            updateStatusMessage(q->tr("网页渲染进程异常终止"));
            updateNavigationState();
            emit q->renderProcessTerminated(static_cast<int>(status), exitCode);
        });

    QObject::connect(
        // 证书错误默认拒绝；只有显式委托时才把决定权交给宿主。
        view_->page(), &QWebEnginePage::certificateError,
        q, [this](const QWebEngineCertificateError &error) {
            Q_Q(BrowserPageWidget);
            QWebEngineCertificateError request(error);

            // defer() 将决定权移交给宿主；未委托时始终执行明确拒绝。
            if (certificatePolicy_
                == BrowserPageWidget::CertificatePolicy::DelegateToApplication) {
                request.defer();
                emit q->certificateErrorRequested(request);
            } else {
                request.rejectCertificate();
                emit q->certificateRejected(request.url(), request.description());
            }
        });
    QObject::connect(
        // 网页权限默认拒绝，避免网页默认拿到摄像头、麦克风等敏感能力。
        view_->page(), &QWebEnginePage::permissionRequested,
        q, [this](QWebEnginePermission permission) {
            Q_Q(BrowserPageWidget);
            if (permissionPolicy_
                == BrowserPageWidget::PermissionPolicy::DelegateToApplication) {
                emit q->permissionRequested(permission);
            } else {
                permission.deny();
                emit q->permissionDenied(
                    permission.origin(),
                    static_cast<int>(permission.permissionType()));
            }
        });

    QObject::connect(
        // 下载请求属于 Profile 级别信号，统一交给下载管理器按策略处理。
        profile_, &QWebEngineProfile::downloadRequested,
        q, [this](QWebEngineDownloadRequest *download) {
            downloadManager_->handleDownloadRequested(download);
        });
}

void BrowserPageWidgetPrivate::updateNavigationState()
{
    // 导航按钮状态完全由 WebEngine 历史记录和当前加载状态决定。
    backAction_->setEnabled(view_->history()->canGoBack());
    forwardAction_->setEnabled(view_->history()->canGoForward());
    reloadAction_->setEnabled(!loading_);
    stopAction_->setEnabled(loading_);
}

void BrowserPageWidgetPrivate::updateStatusMessage(const QString &message)
{
    Q_Q(BrowserPageWidget);

    // 相同消息也刷新 QLabel，避免链接悬停文本覆盖后无法恢复。
    if (statusMessage_ == message) {
        statusLabel_->setText(message);
        return;
    }

    statusMessage_ = message;
    statusLabel_->setText(statusMessage_);
    emit q->statusMessageChanged(statusMessage_);
}

void BrowserPageWidgetPrivate::setLoadState(BrowserPageWidget::LoadState state)
{
    Q_Q(BrowserPageWidget);
    if (loadState_ == state) {
        return;
    }

    loadState_ = state;
    emit q->loadStateChanged(loadState_);
}

bool BrowserPageWidgetPrivate::canNavigate(const QUrl &url, QString *reason) const
{
    return navigationPolicy_.isUrlAllowed(url, reason);
}

void BrowserPageWidgetPrivate::handleNavigationBlocked(const QUrl &url,
                                                       const QString &reason)
{
    Q_Q(BrowserPageWidget);

    updateStatusMessage(q->tr("已阻止访问：%1").arg(reason));
    qWarning().noquote() << "BrowserPageWidget blocked navigation:"
                         << url << reason;
    emit q->navigationBlocked(url, reason);
}

void BrowserPageWidgetPrivate::handleLoadingInfo(const QWebEngineLoadingInfo &loadingInfo)
{
    Q_Q(BrowserPageWidget);

    // 下载和错误页不是普通页面失败，避免重复发出 loadFailed()。
    if (loadingInfo.isDownload() || loadingInfo.isErrorPage()) {
        return;
    }

    if (loadingInfo.status() == QWebEngineLoadingInfo::LoadFailedStatus
        && !timedOut_) {
        setLoadState(BrowserPageWidget::LoadState::Failed);
        const QString error = loadingInfo.errorString().isEmpty()
                                  ? q->tr("网页加载失败")
                                  : loadingInfo.errorString();
        updateStatusMessage(error);
        emit q->loadFailed(loadingInfo.url(),
                           static_cast<int>(loadingInfo.errorDomain()),
                           loadingInfo.errorCode(),
                           error);
    }
}

void BrowserPageWidgetPrivate::handleJavaScriptConsoleMessage(int level,
                                                              const QString &message,
                                                              int lineNumber,
                                                              const QString &sourceId)
{
    Q_Q(BrowserPageWidget);

    // 控制台日志既打印到 Qt 日志，也通过信号交给宿主做可视化或持久化。
    const QString log = QStringLiteral("Web console [%1] %2:%3 - %4")
                            .arg(level)
                            .arg(sourceId)
                            .arg(lineNumber)
                            .arg(message);
    if (level <= 0) {
        qInfo().noquote() << log;
    } else if (level == 1) {
        qWarning().noquote() << log;
    } else {
        qCritical().noquote() << log;
    }

    emit q->javaScriptConsoleMessage(
        level, message, lineNumber, sourceId);
}

} // namespace bm

