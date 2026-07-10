#include "browserpagepolicy.h"

#include <QCoreApplication>

namespace bm {

BrowserPagePolicy::BrowserPagePolicy()
    // 默认允许现代前端常用协议；file 和未知协议需要调用方显式放开。
    : allowedUrlSchemes_({
          QStringLiteral("about"),
          QStringLiteral("blob"),
          QStringLiteral("data"),
          QStringLiteral("http"),
          QStringLiteral("https"),
          QStringLiteral("qrc")
      })
{
}

QSet<QString> BrowserPagePolicy::allowedUrlSchemes() const
{
    return allowedUrlSchemes_;
}

void BrowserPagePolicy::setAllowedUrlSchemes(const QSet<QString> &schemes)
{
    // 统一转小写并去掉冒号，避免 "HTTP" 和 "http:" 这类写法造成误判。
    QSet<QString> normalized;
    for (const QString &scheme : schemes) {
        const QString value = normalizedScheme(scheme);
        if (!value.isEmpty()) {
            normalized.insert(value);
        }
    }
    allowedUrlSchemes_ = normalized;
}

QStringList BrowserPagePolicy::allowedHosts() const
{
    return allowedHosts_;
}

void BrowserPagePolicy::setAllowedHosts(const QStringList &hosts)
{
    // 主机规则保持有序列表，便于调试时按配置顺序查看。
    QStringList normalized;
    for (const QString &host : hosts) {
        const QString value = normalizedHost(host);
        if (!value.isEmpty() && !normalized.contains(value)) {
            normalized.append(value);
        }
    }
    allowedHosts_ = normalized;
}

bool BrowserPagePolicy::isUrlAllowed(const QUrl &url, QString *reason) const
{
    if (url.isEmpty() || !url.isValid()) {
        if (reason) {
            *reason = QCoreApplication::translate("BrowserPagePolicy", "地址无效");
        }
        return false;
    }

    // 先检查协议，再检查主机；协议不允许时没有必要继续解析主机。
    const QString scheme = normalizedScheme(url.scheme());
    if (!allowedUrlSchemes_.contains(scheme)) {
        if (reason) {
            *reason = QCoreApplication::translate(
                          "BrowserPagePolicy", "协议“%1”未被允许").arg(scheme);
        }
        return false;
    }

    const QString host = normalizedHost(url.host());
    // 空主机通常来自 about/data/blob/qrc，主机白名单不限制这些内部协议。
    if (allowedHosts_.isEmpty() || host.isEmpty()) {
        return true;
    }

    for (const QString &rule : allowedHosts_) {
        // "*.example.com" 同时匹配 example.com 和它的任意子域名。
        if (rule.startsWith(QStringLiteral("*."))) {
            const QString base = rule.mid(2);
            if (host == base || host.endsWith(QStringLiteral(".") + base)) {
                return true;
            }
        } else if (host == rule) {
            return true;
        }
    }

    if (reason) {
        *reason = QCoreApplication::translate(
                      "BrowserPagePolicy", "主机“%1”不在白名单中").arg(host);
    }
    return false;
}

QString BrowserPagePolicy::normalizedScheme(const QString &scheme)
{
    QString value = scheme.trimmed().toLower();
    while (value.endsWith(QLatin1Char(':'))) {
        value.chop(1);
    }
    return value;
}

QString BrowserPagePolicy::normalizedHost(const QString &host)
{
    QString value = host.trimmed().toLower();
    while (value.endsWith(QLatin1Char('.'))) {
        value.chop(1);
    }
    return value;
}

} // namespace bm
