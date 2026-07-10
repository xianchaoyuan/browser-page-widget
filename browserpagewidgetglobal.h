#ifndef BROWSERPAGEWIDGETGLOBAL_H
#define BROWSERPAGEWIDGETGLOBAL_H

#include <QtCore/qglobal.h>

/**
 * @brief 静态库构建时不需要导入导出修饰。
 */
#if defined(BROWSERPAGEWIDGET_STATIC)
#define BROWSERPAGEWIDGET_EXPORT
/**
 * @brief 动态库自身编译时导出公开符号。
 */
#elif defined(BROWSERPAGEWIDGET_LIBRARY)
#define BROWSERPAGEWIDGET_EXPORT Q_DECL_EXPORT
/**
 * @brief 动态库使用方编译时导入公开符号。
 */
#else
#define BROWSERPAGEWIDGET_EXPORT Q_DECL_IMPORT
#endif

#endif // BROWSERPAGEWIDGETGLOBAL_H
