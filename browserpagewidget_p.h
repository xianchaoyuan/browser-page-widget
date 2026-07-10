#ifndef BROWSERPAGEWIDGET_P_H
#define BROWSERPAGEWIDGET_P_H

#include "browserpagewidget.h"
#include "browserpagepolicy.h"

#include <QScopedPointer>

class QAction;
class QLabel;
class QLineEdit;
class QProgressBar;
class QTimer;
class QToolBar;
class QWebEngineLoadingInfo;
class QWebEngineProfile;
class QWebEngineView;

namespace bm {

class BrowserDownloadManager;

/**
 * @brief BrowserPageWidget 的内部状态。
 *
 * 该文件不是公开 API，不应被业务工程直接包含。
 */
class BrowserPageWidgetPrivate final
{
    Q_DECLARE_PUBLIC(BrowserPageWidget)

public:
    BrowserPageWidgetPrivate(BrowserPageWidget *q,
                             QWebEngineProfile *profile,
                             bool ownsProfile);
    ~BrowserPageWidgetPrivate() noexcept;

    /**
     * @brief 初始化 UI、WebEngine 和信号槽连接。
     */
    void initialize();

    /**
     * @brief 将用户输入转换为可加载的 URL。
     */
    static QUrl normalizedUrl(const QString &text);

    /**
     * @brief 创建工具栏、浏览器视图、状态栏和布局。
     */
    void setupUi();

    /**
     * @brief 安装自定义 WebPage 并配置 WebEngine 安全选项。
     */
    void setupWebEngine();

    /**
     * @brief 连接 UI、WebEngine 和公开信号槽。
     */
    void setupConnections();

    /**
     * @brief 根据历史记录和加载状态更新导航按钮可用性。
     */
    void updateNavigationState();

    /**
     * @brief 更新底部状态栏消息并发出通知信号。
     */
    void updateStatusMessage(const QString &message);

    /**
     * @brief 更新页面加载状态并发出通知信号。
     */
    void setLoadState(BrowserPageWidget::LoadState state);

    /**
     * @brief 判断 URL 是否满足当前导航策略。
     */
    bool canNavigate(const QUrl &url, QString *reason = nullptr) const;

    /**
     * @brief 处理被导航策略阻止的 URL。
     */
    void handleNavigationBlocked(const QUrl &url, const QString &reason);

    /**
     * @brief 处理 WebEngine 提供的详细加载信息。
     */
    void handleLoadingInfo(const QWebEngineLoadingInfo &loadingInfo);

    /**
     * @brief 处理网页 JavaScript 控制台消息。
     */
    void handleJavaScriptConsoleMessage(int level, const QString &message,
                                        int lineNumber, const QString &sourceId);

    BrowserPageWidget *q_ptr; ///< 公共对象指针，用于私有实现访问 BrowserPageWidget。

    QWebEngineProfile *profile_; ///< 当前 WebEngine Profile。
    bool ownsProfile_;           ///< 是否由本控件负责释放 profile_。
    QWebEngineView *view_;       ///< 实际承载网页渲染的视图控件。

    QToolBar *toolbar_;         ///< 顶部浏览器工具栏。
    QWidget *statusBar_;        ///< 底部状态栏容器。
    QLineEdit *addressEdit_;    ///< 地址输入框。
    QProgressBar *progressBar_; ///< 加载和下载进度条。
    QLabel *statusLabel_;       ///< 状态栏文本标签。

    QAction *backAction_;    ///< 后退按钮动作。
    QAction *forwardAction_; ///< 前进按钮动作。
    QAction *reloadAction_;  ///< 刷新按钮动作。
    QAction *stopAction_;    ///< 停止加载按钮动作。
    QAction *homeAction_;    ///< 回到主页按钮动作。

    QTimer *loadTimeoutTimer_;                 ///< 页面加载超时定时器。
    bool loading_;                             ///< 当前页面是否正在加载。
    bool timedOut_;                            ///< 当前加载是否已经超时。
    BrowserPageWidget::LoadState loadState_;   ///< 当前页面加载状态。
    QUrl homeUrl_;                             ///< 主页地址。
    QString statusMessage_;                    ///< 状态栏当前常驻消息。
    int loadTimeoutMs_;                        ///< 加载超时时间，单位毫秒。

    BrowserPagePolicy navigationPolicy_;                     ///< URL 协议和主机白名单策略。
    BrowserPageWidget::PopupPolicy popupPolicy_;             ///< 网页弹窗处理策略。
    BrowserPageWidget::DownloadPolicy downloadPolicy_;       ///< 网页下载处理策略。
    BrowserPageWidget::PermissionPolicy permissionPolicy_;   ///< 网页权限请求处理策略。
    BrowserPageWidget::CertificatePolicy certificatePolicy_; ///< HTTPS 证书错误处理策略。
    QString downloadDirectory_;                              ///< 自动保存下载目录。

    QScopedPointer<BrowserDownloadManager> downloadManager_; ///< 下载请求处理器。
};

} // namespace bm

#endif // BROWSERPAGEWIDGET_P_H
