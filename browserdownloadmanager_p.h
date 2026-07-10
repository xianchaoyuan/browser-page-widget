#ifndef BROWSERDOWNLOADMANAGER_P_H
#define BROWSERDOWNLOADMANAGER_P_H

#include <QString>

class QWebEngineDownloadRequest;

namespace bm {

class BrowserPageWidgetPrivate;

/**
 * @brief 下载策略和文件落盘规则的内部实现。
 *
 * 该类型只服务于 BrowserPageWidget，不属于公开 API。
 */
class BrowserDownloadManager final
{
public:
    /**
     * @brief 创建下载管理器。
     */
    explicit BrowserDownloadManager(BrowserPageWidgetPrivate *browser);

    /**
     * @brief 返回系统默认下载目录。
     */
    static QString defaultDownloadDirectory();

    /**
     * @brief 根据控件下载策略处理 WebEngine 下载请求。
     */
    void handleDownloadRequested(QWebEngineDownloadRequest *download);

private:
    /**
     * @brief 根据建议文件名生成不会覆盖已有文件的文件名。
     */
    static QString uniqueDownloadFileName(const QString &directory,
                                          const QString &suggestedName);

    BrowserPageWidgetPrivate *browser_; ///< 所属浏览器控件的私有状态对象。
};

} // namespace bm

#endif // BROWSERDOWNLOADMANAGER_P_H
