#ifndef BROWSERPAGEWIDGETGLOBAL_H
#define BROWSERPAGEWIDGETGLOBAL_H

#include <QtCore/qglobal.h>

#if defined(BROWSERPAGEWIDGET_LIBRARY)
#  define BROWSERPAGEWIDGET_PUBLIC __declspec(dllexport)
#else
#  define BROWSERPAGEWIDGET_PUBLIC __declspec(dllimport)
#endif

#endif // BROWSERPAGEWIDGETGLOBAL_H
