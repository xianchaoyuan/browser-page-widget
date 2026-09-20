#ifndef BROWSERWEBPAGE_P_H
#define BROWSERWEBPAGE_P_H

#include <QWebEnginePage>

namespace bm {

class BrowserPageWidgetPrivate;

/**
 * @brief 应用导航策略并接管弹窗、控制台输出的内部 WebPage。
 *
 * 该类型只服务于 BrowserPageWidget，不属于公开 API。
 */
class BrowserWebPage final : public QWebEnginePage
{
public:
    /**
     * @brief 创建内部 WebPage。
     */
    BrowserWebPage(QWebEngineProfile *profile,
                   BrowserPageWidgetPrivate *browser,
                   QObject *parent,
                   bool transientPopup = false);

protected:
    /**
     * @brief 拦截页面导航请求并应用导航白名单。
     */
    bool acceptNavigationRequest(const QUrl &url,
                                 NavigationType type,
                                 bool isMainFrame) override;

    /**
     * @brief 接管网页创建新窗口的请求。
     */
    QWebEnginePage *createWindow(WebWindowType type) override;

    /**
     * @brief 接收网页 JavaScript 控制台消息。
     */
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceId) override;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    /**
     * @brief 接管 HTTPS 证书错误并应用证书策略（仅 Qt 5）。
     *
     * Qt 5 通过虚函数（而不是 Qt 6 的 certificateError 信号）处理证书错误，
     * 统一的策略判断和应答逻辑见 BrowserPageWidgetPrivate::handleCertificateError()。
     */
    bool certificateError(const QWebEngineCertificateError &error) override;
#endif

private:
    BrowserPageWidgetPrivate *browser_; ///< 所属浏览器控件的私有状态对象。
    bool transientPopup_;               ///< 当前页面是否是为弹窗临时创建的页面。
    bool popupTargetHandled_;           ///< 弹窗目标 URL 是否已经处理过。
};

} // namespace bm

#endif // BROWSERWEBPAGE_P_H
