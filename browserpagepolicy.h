#ifndef BROWSERPAGEPOLICY_H
#define BROWSERPAGEPOLICY_H

#include "browserpagewidgetglobal.h"

#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace bm {

/**
 * @brief 浏览器导航白名单策略。
 *
 * 该类不依赖 QWidget 或 WebEngine 进程，可独立用于配置校验和单元测试。
 */
class BROWSERPAGEWIDGET_EXPORT BrowserPagePolicy final
{
public:
    /**
     * @brief 创建默认导航策略。
     *
     * 默认允许 about、blob、data、http、https、qrc 协议，不限制主机。
     */
    BrowserPagePolicy();

    /**
     * @brief 返回允许访问的 URL 协议集合。
     */
    QSet<QString> allowedUrlSchemes() const;

    /**
     * @brief 设置允许访问的 URL 协议集合。
     */
    void setAllowedUrlSchemes(const QSet<QString> &schemes);

    /**
     * @brief 返回允许访问的主机规则列表。
     */
    QStringList allowedHosts() const;

    /**
     * @brief 设置允许访问的主机规则列表。
     *
     * 空列表表示不限制主机；支持 "*.example.com" 通配规则。
     */
    void setAllowedHosts(const QStringList &hosts);

    /**
     * @brief 判断 URL 是否满足当前协议和主机白名单。
     */
    bool isUrlAllowed(const QUrl &url, QString *reason = nullptr) const;

private:
    /**
     * @brief 规范化 URL 协议字符串。
     */
    static QString normalizedScheme(const QString &scheme);

    /**
     * @brief 规范化主机名字符串。
     */
    static QString normalizedHost(const QString &host);

    QSet<QString> allowedUrlSchemes_; ///< 允许访问的 URL 协议集合。
    QStringList allowedHosts_;        ///< 允许访问的主机规则列表。
};

} // namespace bm

#endif // BROWSERPAGEPOLICY_H
