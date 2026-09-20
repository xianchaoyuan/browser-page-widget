#ifndef BROWSERPAGEWIDGET_H
#define BROWSERPAGEWIDGET_H

#include "browserpagewidgetglobal.h"
#include "browserqtcompat.h"

#include <QScopedPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QWebEngineCertificateError>
#include <QWidget>

class QWebEnginePage;
class QWebEngineProfile;
class QWebEngineView;

namespace bm {

class BrowserPageWidgetPrivate;

/**
 * @brief 可嵌入工业桌面软件的通用 WebEngine 浏览器控件。
 *
 * 这个类把 QWebEngineView 包装成一个更完整的 QWidget：自带工具栏、地址栏、
 * 状态栏、加载超时、导航白名单、弹窗策略、下载策略、权限策略和证书策略。
 * 默认策略遵循最小权限原则：拒绝未知协议、证书错误、网页权限与下载。
 *
 * 一套源码同时支持 Qt 5.15 与 Qt 6（≥6.2），宿主程序不需要写任何
 * #if QT_VERSION 分支；版本差异全部收敛在私有实现与 browserqtcompat.h 中。
 */
class BROWSERPAGEWIDGET_PUBLIC BrowserPageWidget final : public QWidget
{
    Q_OBJECT

    /** @brief 主页地址属性。 */
    Q_PROPERTY(QUrl homeUrl READ homeUrl WRITE setHomeUrl NOTIFY homeUrlChanged)
    /** @brief 工具栏是否可见。 */
    Q_PROPERTY(bool toolbarVisible READ toolbarVisible WRITE setToolbarVisible NOTIFY toolbarVisibleChanged)
    /** @brief 状态栏是否可见。 */
    Q_PROPERTY(bool statusBarVisible READ statusBarVisible WRITE setStatusBarVisible NOTIFY statusBarVisibleChanged)
    /** @brief 地址栏是否允许用户编辑。 */
    Q_PROPERTY(bool addressEditable READ addressEditable WRITE setAddressEditable NOTIFY addressEditableChanged)
    /** @brief 当前页面缩放比例。 */
    Q_PROPERTY(qreal zoomFactor READ zoomFactor WRITE setZoomFactor NOTIFY zoomFactorChanged)
    /** @brief 页面加载超时时间，单位为毫秒。 */
    Q_PROPERTY(int loadTimeoutMs READ loadTimeoutMs WRITE setLoadTimeoutMs NOTIFY loadTimeoutChanged)
    /** @brief 当前页面加载状态。 */
    Q_PROPERTY(LoadState loadState READ loadState NOTIFY loadStateChanged)

public:
    /**
     * @brief 页面加载状态。
     */
    enum class LoadState {
        Idle,      ///< 尚未开始加载。
        Loading,   ///< 正在加载页面。
        Succeeded, ///< 页面加载成功。
        Failed,    ///< 页面加载失败。
        TimedOut   ///< 页面加载超过设定时间。
    };
    Q_ENUM(LoadState)

    /**
     * @brief 网页请求打开新窗口时的处理方式。
     */
    enum class PopupPolicy {
        Block,                 ///< 直接阻止弹窗。
        OpenInCurrentView,     ///< 把弹窗目标地址加载到当前页面。
        DelegateToApplication  ///< 发出 popupRequested()，交给宿主程序处理。
    };
    Q_ENUM(PopupPolicy)

    /**
     * @brief 网页触发下载时的处理方式。
     */
    enum class DownloadPolicy {
        Deny,                  ///< 直接取消下载。
        DelegateToApplication, ///< 发出 downloadRequested()，交给宿主程序处理。
        AutoSave               ///< 自动保存到 downloadDirectory()。
    };
    Q_ENUM(DownloadPolicy)

    /**
     * @brief 网页请求摄像头、麦克风、剪贴板等权限时的处理方式。
     */
    enum class PermissionPolicy {
        Deny,                 ///< 直接拒绝权限。
        DelegateToApplication ///< 发出 permissionRequested()，交给宿主程序处理。
    };
    Q_ENUM(PermissionPolicy)

    /**
     * @brief HTTPS 证书错误的处理方式。
     */
    enum class CertificatePolicy {
        Reject,               ///< 直接拒绝有证书错误的页面。
        DelegateToApplication ///< 发出 certificateErrorRequested()，交给宿主程序处理。
    };
    Q_ENUM(CertificatePolicy)

    /**
     * @brief 创建使用独立 Profile 的浏览器控件。
     *
     * 适合大多数场景：控件自己管理 WebEngine Profile 生命周期。
     */
    explicit BrowserPageWidget(QWidget *parent = nullptr);

    /**
     * @brief 使用宿主提供的 Profile。
     *
     * 适合多个浏览器控件共享 Cookie、缓存、请求拦截器或 WebChannel 的场景。
     * @note profile 的生命周期必须长于本控件。
     */
    explicit BrowserPageWidget(QWebEngineProfile &profile, QWidget *parent = nullptr);

    BrowserPageWidget(const BrowserPageWidget &other) = delete;
    ~BrowserPageWidget() noexcept override;

    /**
     * @brief 禁止赋值浏览器控件。
     */
    BrowserPageWidget &operator=(const BrowserPageWidget &rhs) = delete;

    /**
     * @brief 返回底层 QWebEngineView。
     *
     * 用于高级配置，例如注入脚本、配置 WebChannel、设置请求拦截器等。
     */
    QWebEngineView *view() const noexcept;

    /**
     * @brief 返回当前页面对象。
     */
    QWebEnginePage *page() const noexcept;

    /**
     * @brief 返回当前 WebEngine Profile。
     */
    QWebEngineProfile *profile() const noexcept;

    /**
     * @brief 返回当前页面地址。
     */
    QUrl currentUrl() const;

    /**
     * @brief 返回当前页面标题。
     */
    QString currentTitle() const;

    /**
     * @brief 判断页面是否处于加载中。
     */
    bool isLoading() const noexcept;

    /**
     * @brief 返回当前页面加载状态。
     */
    LoadState loadState() const noexcept;

    /**
     * @brief 返回主页地址。
     */
    QUrl homeUrl() const;

    /**
     * @brief 设置主页地址。
     */
    void setHomeUrl(const QUrl &url);

    /**
     * @brief 使用字符串设置主页地址。
     */
    void setHomeUrl(const QString &url);

    /**
     * @brief 判断工具栏是否可见。
     */
    bool toolbarVisible() const noexcept;

    /**
     * @brief 设置工具栏是否可见。
     */
    void setToolbarVisible(bool visible);

    /**
     * @brief 判断状态栏是否可见。
     */
    bool statusBarVisible() const noexcept;

    /**
     * @brief 设置状态栏是否可见。
     */
    void setStatusBarVisible(bool visible);

    /**
     * @brief 判断地址栏是否允许编辑。
     */
    bool addressEditable() const noexcept;

    /**
     * @brief 设置地址栏是否允许编辑。
     */
    void setAddressEditable(bool editable);

    /**
     * @brief 返回页面缩放比例。
     */
    qreal zoomFactor() const noexcept;

    /**
     * @brief 设置页面缩放比例。
     */
    void setZoomFactor(qreal factor);

    /**
     * @brief 返回加载超时时间，0 表示禁用超时。
     */
    int loadTimeoutMs() const noexcept;

    /**
     * @brief 设置加载超时时间，单位为毫秒。
     */
    void setLoadTimeoutMs(int timeoutMs);

    /**
     * @brief 返回允许访问的 URL 协议集合。
     */
    QSet<QString> allowedUrlSchemes() const;

    /**
     * @brief 设置允许访问的 URL 协议。
     *
     * 默认允许 about、blob、data、http、https、qrc。file 和未知协议默认不允许。
     */
    void setAllowedUrlSchemes(const QSet<QString> &schemes);

    /**
     * @brief 返回主机白名单。
     */
    QStringList allowedHosts() const;

    /**
     * @brief 设置主机白名单。
     *
     * 空列表允许任意主机；支持 "*.example.com" 通配规则。
     */
    void setAllowedHosts(const QStringList &hosts);

    /**
     * @brief 判断指定 URL 是否满足当前导航策略。
     */
    bool isUrlAllowed(const QUrl &url) const;

    /**
     * @brief 返回当前弹窗策略。
     */
    PopupPolicy popupPolicy() const noexcept;

    /**
     * @brief 设置网页弹窗处理策略。
     */
    void setPopupPolicy(PopupPolicy policy);

    /**
     * @brief 返回当前下载策略。
     */
    DownloadPolicy downloadPolicy() const noexcept;

    /**
     * @brief 设置网页下载处理策略。
     */
    void setDownloadPolicy(DownloadPolicy policy);

    /**
     * @brief 返回自动保存下载目录。
     */
    QString downloadDirectory() const;

    /**
     * @brief 设置自动保存下载目录。
     *
     * 传入空字符串会恢复为系统默认下载目录。
     */
    void setDownloadDirectory(const QString &directory);

    /**
     * @brief 返回当前网页权限策略。
     */
    PermissionPolicy permissionPolicy() const noexcept;

    /**
     * @brief 设置网页权限请求处理策略。
     */
    void setPermissionPolicy(PermissionPolicy policy);

    /**
     * @brief 返回当前证书错误策略。
     */
    CertificatePolicy certificatePolicy() const noexcept;

    /**
     * @brief 设置 HTTPS 证书错误处理策略。
     */
    void setCertificatePolicy(CertificatePolicy policy);

    /**
     * @brief 返回当前 User-Agent 字符串。
     */
    QString userAgent() const;

    /**
     * @brief 设置当前 Profile 的 User-Agent 字符串。
     */
    void setUserAgent(const QString &userAgent);

public slots:
    /**
     * @brief 加载网页地址。
     *
     * 地址会先经过导航策略检查，被白名单拒绝时不会真正加载。
     */
    void loadUrl(const QUrl &url);

    /**
     * @brief 使用字符串加载网页地址。
     */
    void loadUrl(const QString &url);

    /**
     * @brief 加载一段 HTML 字符串。
     *
     * baseUrl 会影响相对路径解析，也会先经过导航策略检查。
     */
    void loadHtml(const QString &html, const QUrl &baseUrl = QUrl());

    /**
     * @brief 加载主页地址。
     */
    void goHome();

    /**
     * @brief 后退到上一条历史记录。
     */
    void back();

    /**
     * @brief 前进到下一条历史记录。
     */
    void forward();

    /**
     * @brief 重新加载当前页面。
     */
    void reloadPage();

    /**
     * @brief 停止当前页面加载。
     */
    void stop();

    /**
     * @brief 放大页面。
     */
    void zoomIn();

    /**
     * @brief 缩小页面。
     */
    void zoomOut();

    /**
     * @brief 恢复默认缩放比例。
     */
    void resetZoom();

    /**
     * @brief 打开或关闭开发者工具窗口。
     *
     * 首次调用会创建独立的 DevTools 窗口，再次调用则切换显示状态。
     * 默认快捷键 F12。
     */
    void toggleDevTools();

    /**
     * @brief 进入「检查元素」拾取模式。
     *
     * DevTools 未打开时会先打开。拾取模式下点击页面元素，
     * DevTools 会直接定位到对应 DOM 节点，适合排查资源加载类问题。
     */
    void inspectElement();

    /**
     * @brief 清理当前 Profile 的历史、Cookie、缓存和当前页面存储。
     */
    void clearBrowsingData();

    /**
     * @brief 授权一个 permissionRequested() 信号上报的网页权限。
     *
     * 权限模型是「来源 + Feature」二元组；宿主在收到 permissionRequested()
     * 信号后调用本函数完成授权。
     * @param origin 请求来源地址。
     * @param feature QWebEnginePage::Feature 枚举值。
     */
    void grantPermission(const QUrl &origin, int feature);

    /**
     * @brief 拒绝一个 permissionRequested() 信号上报的网页权限。
     * @param origin 请求来源地址。
     * @param feature QWebEnginePage::Feature 枚举值。
     */
    void denyPermission(const QUrl &origin, int feature);

signals:
    /** @brief 主页地址变化时发出。 */
    void homeUrlChanged(const QUrl &url);

    /** @brief 工具栏可见性变化时发出。 */
    void toolbarVisibleChanged(bool visible);

    /** @brief 状态栏可见性变化时发出。 */
    void statusBarVisibleChanged(bool visible);

    /** @brief 地址栏编辑状态变化时发出。 */
    void addressEditableChanged(bool editable);

    /** @brief 页面缩放比例变化时发出。 */
    void zoomFactorChanged(qreal factor);

    /** @brief 加载超时时间变化时发出。 */
    void loadTimeoutChanged(int timeoutMs);

    /** @brief 页面加载状态变化时发出。 */
    void loadStateChanged(bm::BrowserPageWidget::LoadState state);

    /** @brief 页面标题变化时发出。 */
    void titleChanged(const QString &title);

    /** @brief 页面地址变化时发出。 */
    void urlChanged(const QUrl &url);

    /** @brief 页面开始加载时发出。 */
    void loadStarted();

    /** @brief 页面加载进度变化时发出。 */
    void loadProgress(int progress);

    /** @brief 页面加载结束时发出。 */
    void loadFinished(bool ok);

    /**
     * @brief 页面加载失败时发出。
     *
     * @note Qt 5 的 WebEngine（Chromium 83）不提供加载失败明细，
     * 该版本下 errorDomain 与 errorCode 恒为 -1，详细原因请看 errorString。
     */
    void loadFailed(const QUrl &url, int errorDomain, int errorCode,
                    const QString &errorString);

    /** @brief 页面加载超时时发出。 */
    void loadTimedOut(const QUrl &url);

    /** @brief 导航被白名单策略阻止时发出。 */
    void navigationBlocked(const QUrl &url, const QString &reason);

    /** @brief 弹窗被策略阻止时发出。 */
    void popupBlocked();

    /** @brief 弹窗交给宿主程序处理时发出。 */
    void popupRequested(const QUrl &url);

    /** @brief 网页 console 输出被转发时发出。 */
    void javaScriptConsoleMessage(int level, const QString &message,
                                  int lineNumber, const QString &sourceId);

    /** @brief 证书错误被默认拒绝时发出。 */
    void certificateRejected(const QUrl &url, const QString &description);

    /**
     * @brief 将证书错误交给宿主处理。
     * @warning 宿主必须调用 bm::acceptCertificateError()（放行）或
     * rejectCertificate()（拒绝），否则请求会一直挂起。
     */
    void certificateErrorRequested(QWebEngineCertificateError error);

    /** @brief 网页权限请求被默认拒绝时发出。 */
    void permissionDenied(const QUrl &origin, int permissionType);

    /**
     * @brief 将网页权限请求交给宿主处理。
     *
     * 宿主必须调用 grantPermission() 或 denyPermission() 完成应答，
     * 否则请求会一直挂起。
     * @param origin 请求来源地址。
     * @param feature QWebEnginePage::Feature 枚举值。
     */
    void permissionRequested(const QUrl &origin, int feature);

    /** @brief WebEngine 渲染进程异常终止时发出。 */
    void renderProcessTerminated(int terminationStatus, int exitCode);

    /** @brief 下载请求被拒绝时发出。 */
    void downloadRejected(const QUrl &url, const QString &reason);

    /**
     * @brief 将下载请求交给宿主处理。
     * @warning 宿主必须调用 accept() 或 cancel()。
     */
    void downloadRequested(bm::BrowserDownloadItem *download);

    /** @brief 自动保存下载开始时发出。 */
    void downloadStarted(quint32 id, const QUrl &url, const QString &filePath);

    /** @brief 自动保存下载进度变化时发出。 */
    void downloadProgress(quint32 id, qint64 receivedBytes, qint64 totalBytes);

    /** @brief 自动保存下载完成时发出。 */
    void downloadFinished(quint32 id, const QString &filePath);

    /** @brief 自动保存下载失败或取消时发出。 */
    void downloadFailed(quint32 id, const QString &reason);

    /** @brief 浏览数据清理完成时发出。 */
    void browsingDataCleared();

    /** @brief 状态栏消息变化时发出。 */
    void statusMessageChanged(const QString &message);

private slots:
    /** @brief 地址栏按下 Enter 后提交当前输入。 */
    void commitAddressBar();

    /** @brief 处理页面开始加载。 */
    void onLoadStarted();

    /** @brief 处理页面加载进度。 */
    void onLoadProgress(int progress);

    /** @brief 处理页面加载结束。 */
    void onLoadFinished(bool ok);

    /** @brief 处理页面标题变化。 */
    void onTitleChanged(const QString &title);

    /** @brief 处理页面地址变化。 */
    void onUrlChanged(const QUrl &url);

    /** @brief 处理页面加载超时。 */
    void onLoadTimeout();

private:
    Q_DECLARE_PRIVATE(BrowserPageWidget)

    /**
     * @brief 内部统一构造入口。
     */
    BrowserPageWidget(QWebEngineProfile *profile, bool ownsProfile, QWidget *parent);

    QScopedPointer<BrowserPageWidgetPrivate> d_ptr; ///< 私有实现对象，隐藏内部 UI 和 WebEngine 状态。
};

} // namespace bm

#endif // BROWSERPAGEWIDGET_H
