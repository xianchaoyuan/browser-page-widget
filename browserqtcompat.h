#ifndef BROWSERQTCOMPAT_H
#define BROWSERQTCOMPAT_H

// 版本兼容层：Qt 5.15 与 Qt 6.x 的 WebEngine API 差异集中在这里。
// 公开头文件 browserpagewidget.h 只引用本文件提供的统一类型与辅助函数，
// 自身不出现任何 #if QT_VERSION 分支。
//
// 事实依据：所有 API 结论均已对照本机 C:\Qt\5.15.2 与 C:\Qt\6.11.2 头文件核实。

#include <QtGlobal>
#include <QString>
#include <QWebEngineCertificateError>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QWebEngineDownloadRequest>
#else
#include <QWebEngineDownloadItem>
#endif

namespace bm {

/**
 * @brief 下载对象统一别名。
 *
 * Qt 6 的真名是 QWebEngineDownloadRequest，Qt 5 的真名是 QWebEngineDownloadItem
 * （Qt 6.11 已删除后者别名，因此必须自定义）。两个类型在 5.15.2 上已拥有相同
 * 的 API 表面（setDownloadDirectory / setDownloadFileName / suggestedFileName /
 * page / id / state 均已核实），别名安全。
 */
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using BrowserDownloadItem = QWebEngineDownloadRequest;
#else
using BrowserDownloadItem = QWebEngineDownloadItem;
#endif

/**
 * @brief 证书错误应答：放行该证书错误。
 *
 * Qt 5 的应答函数名是 ignoreCertificateError()，Qt 6 是 acceptCertificate()。
 * 宿主在 certificateErrorRequested() 信号收到的错误对象副本上调用本函数。
 */
inline void acceptCertificateError(QWebEngineCertificateError &error)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    error.acceptCertificate();
#else
    error.ignoreCertificateError();
#endif
}

/**
 * @brief 返回证书错误的人类可读描述。
 *
 * Qt 5 叫 errorDescription()，Qt 6 叫 description()。
 */
inline QString certificateErrorDescription(const QWebEngineCertificateError &error)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return error.description();
#else
    return error.errorDescription();
#endif
}

} // namespace bm

#endif // BROWSERQTCOMPAT_H
