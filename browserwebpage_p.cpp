#include "browserwebpage_p.h"

#include "browserpagewidget.h"
#include "browserpagewidget_p.h"

namespace bm {

namespace {

// Qt 创建新窗口时常先给 about:blank，真正目标地址会稍后通过 urlChanged 到达。
bool isIgnorablePopupUrl(const QUrl &url)
{
    return url.isEmpty()
           || url == QUrl(QStringLiteral("about:blank"));
}

} // namespace

BrowserWebPage::BrowserWebPage(QWebEngineProfile *profile,
                               BrowserPageWidgetPrivate *browser,
                               QObject *parent,
                               bool transientPopup)
    : QWebEnginePage(profile, parent)
    , browser_(browser)
    , transientPopup_(transientPopup)
    , popupTargetHandled_(false)
{
    Q_ASSERT(browser_ != nullptr);
}

bool BrowserWebPage::acceptNavigationRequest(const QUrl &url,
                                             NavigationType type,
                                             bool isMainFrame)
{
    // 所有主框架和子框架导航都会进入这里，统一套用导航白名单。
    QString reason;
    if (!browser_->canNavigate(url, &reason)) {
        browser_->handleNavigationBlocked(url, reason);

        // 被拒绝的弹窗页不会再产生有效 URL，应尽早回收。
        if (transientPopup_) {
            deleteLater();
        }
        return false;
    }

    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}

QWebEnginePage *BrowserWebPage::createWindow(WebWindowType type)
{
    Q_UNUSED(type)

    BrowserPageWidget *q = browser_->q_ptr;
    if (browser_->popupPolicy_
        == BrowserPageWidget::PopupPolicy::Block) {
        emit q->popupBlocked();
        return nullptr;
    }

    // 临时 Page 以当前 Page 为 parent，便于下载管理器识别它属于本控件。
    BrowserWebPage *popupPage =
        new BrowserWebPage(profile(), browser_, this, true);
    connect(
        popupPage, &QWebEnginePage::urlChanged,
        q, [popupPage, q, browser = browser_](const QUrl &url) {
            if (popupPage->popupTargetHandled_ || isIgnorablePopupUrl(url)) {
                return;
            }

            // 一个弹窗只处理第一个有效目标，避免重定向过程中重复通知宿主。
            popupPage->popupTargetHandled_ = true;
            if (browser->popupPolicy_
                == BrowserPageWidget::PopupPolicy::OpenInCurrentView) {
                // OpenInCurrentView 模式复用主视图，外部不需要管理额外窗口。
                q->loadUrl(url);
            } else {
                // DelegateToApplication 模式只发信号，是否打开新窗口由宿主决定。
                emit q->popupRequested(url);
            }
            popupPage->deleteLater();
        });

    return popupPage;
}

void BrowserWebPage::javaScriptConsoleMessage(
    JavaScriptConsoleMessageLevel level,
    const QString &message,
    int lineNumber,
    const QString &sourceId)
{
    // 控制台消息不在这里直接打印，统一交给 BrowserPageWidgetPrivate 分级处理。
    browser_->handleJavaScriptConsoleMessage(
        static_cast<int>(level), message, lineNumber, sourceId);
}

} // namespace bm
