# BrowserPageWidget

`BrowserPageWidget` 是一个基于 Qt WebEngine 的通用浏览器控件库。它把 `QWebEngineView` 封装成一个可直接嵌入 Qt Widgets 程序的组件，并提供工具栏、状态栏、加载超时、导航白名单、弹窗策略、下载策略、网页权限策略和证书错误策略。

项目目标是：让业务程序可以像使用普通 `QWidget` 一样嵌入网页页面，同时保留必要的安全控制和扩展能力。

## 功能特性

- 基于 Qt 6 `QWebEngineView`
- 支持顶部工具栏和底部状态栏
- 支持隐藏工具栏、隐藏状态栏、设置地址栏只读
- 支持页面加载超时
- 支持 URL 协议和主机白名单
- 支持弹窗处理策略
- 支持下载处理策略
- 支持自动保存下载文件
- 支持下载进度和下载完成信号
- 支持网页权限请求处理
- 支持 HTTPS 证书错误处理
- 支持 JavaScript console 日志转发
- 支持访问底层 `QWebEngineView`、`QWebEnginePage`、`QWebEngineProfile`
- 支持作为 CMake 库安装和 `find_package()` 引用

## 环境要求

- CMake 3.21 或更高
- C++17
- Qt 6
- Qt 组件：
  - `Widgets`
  - `WebEngineCore`
  - `WebEngineWidgets`

当前项目不固定 Qt 小版本。只要你的环境能找到 Qt 6，并且安装了 Qt WebEngine，就可以配置。

## 目录结构

```text
browser-page-widget/
├── CMakeLists.txt
├── main.cpp
├── browserpagewidget.h
├── browserpagewidget.cpp
├── browserpagewidget_p.h
├── browserpagewidget_p.cpp
├── browserpagepolicy.h
├── browserpagepolicy.cpp
├── browserwebpage_p.h
├── browserwebpage_p.cpp
├── browserdownloadmanager_p.h
├── browserdownloadmanager_p.cpp
├── browserpagewidgetglobal.h
└── cmake/
    └── BrowserPageWidgetConfig.cmake.in
```

主要文件说明：

| 文件 | 作用 |
| --- | --- |
| `browserpagewidget.h/.cpp` | 对外公开的浏览器控件类 |
| `browserpagewidget_p.h/.cpp` | 控件内部状态和 UI/WebEngine 连接逻辑 |
| `browserpagepolicy.h/.cpp` | URL 协议和主机白名单策略 |
| `browserwebpage_p.h/.cpp` | 内部 WebPage，用于接管导航、弹窗和 console 日志 |
| `browserdownloadmanager_p.h/.cpp` | 下载策略、保存路径和下载进度处理 |
| `browserpagewidgetglobal.h` | 动态库导入导出宏 |
| `main.cpp` | Demo 程序入口 |
| `cmake/BrowserPageWidgetConfig.cmake.in` | 安装后供 `find_package()` 使用的 CMake 配置模板 |

## 快速编译

如果你的系统环境已经能找到 Qt 6，可以直接执行：

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

如果 CMake 找不到 Qt，可以通过 `BROWSER_PAGE_WIDGET_QT_ROOT` 指定 Qt 安装目录：

```powershell
cmake -S . -B build -DBROWSER_PAGE_WIDGET_QT_ROOT=D:/Qt/Qt6.11.1/6.11.1/msvc2022_64
cmake --build build --config Debug
```

生成的 Demo 程序通常位于：

```text
build/Debug/BrowserPageWidgetDemo.exe
```

如果使用的是 Visual Studio 多配置生成器，`Debug`、`Release` 会分别生成到对应配置目录。

## CMake 选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `BROWSER_PAGE_WIDGET_BUILD_DEMO` | `ON` | 是否编译 Demo 程序 |
| `BROWSER_PAGE_WIDGET_QT_ROOT` | 空 | 可选 Qt 6 安装目录提示 |

当前项目固定生成动态库，不再通过 `BUILD_SHARED_LIBS` 切换静态库。

示例：只编译库，不编译 Demo：

```powershell
cmake -S . -B build -DBROWSER_PAGE_WIDGET_BUILD_DEMO=OFF
cmake --build build --config Release
```


## Demo 说明

Demo 入口在 `main.cpp`。

当前 Demo 默认配置：

```cpp
browser.setHomeUrl(QStringLiteral("http://127.0.0.1:19001/"));
browser.setLoadTimeoutMs(30000);
browser.setToolbarVisible(false);
browser.setPopupPolicy(bm::BrowserPageWidget::PopupPolicy::OpenInCurrentView);
browser.setDownloadPolicy(bm::BrowserPageWidget::DownloadPolicy::AutoSave);
```

含义：

- 默认加载 `http://127.0.0.1:19001/`
- 页面加载超时时间为 30 秒
- 顶部浏览器工具栏隐藏
- 网页弹窗在当前页面打开
- 网页下载自动保存到系统下载目录

自动下载默认保存到系统下载目录，例如：

```text
C:/Users/<用户名>/Downloads
```

## 在项目中直接引用

如果你的项目和本项目在同一个源码树中，可以使用 `add_subdirectory()`：

```cmake
add_subdirectory(path/to/browser-page-widget)

target_link_libraries(your_app PRIVATE bm::browserpagewidget)
```

代码中使用：

```cpp
#include "browserpagewidget.h"

bm::BrowserPageWidget *browser = new bm::BrowserPageWidget(parent);
browser->setHomeUrl("https://example.com");
browser->loadUrl(browser->homeUrl());
```

## 安装后引用

先安装库：

```powershell
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=D:/sdk/browserpagewidget
cmake --build build --config Release
cmake --install build --config Release
```

其他项目中使用：

```cmake
list(APPEND CMAKE_PREFIX_PATH "D:/sdk/browserpagewidget")

find_package(BrowserPageWidget REQUIRED)

target_link_libraries(your_app PRIVATE bm::browserpagewidget)
```

代码中使用：

```cpp
#include "browserpagewidget.h"
```

## 基本用法

```cpp
#include "browserpagewidget.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    bm::BrowserPageWidget browser;
    browser.setHomeUrl("https://example.com");
    browser.setLoadTimeoutMs(30000);
    browser.setToolbarVisible(false);
    browser.setStatusBarVisible(true);
    browser.setDownloadPolicy(bm::BrowserPageWidget::DownloadPolicy::AutoSave);

    browser.loadUrl(browser.homeUrl());
    browser.resize(1200, 760);
    browser.show();

    return app.exec();
}
```

## 常用配置

### 隐藏顶部工具栏

```cpp
browser.setToolbarVisible(false);
```

### 隐藏底部状态栏

```cpp
browser.setStatusBarVisible(false);
```

### 禁止用户编辑地址栏

```cpp
browser.setAddressEditable(false);
```

### 设置页面加载超时

```cpp
browser.setLoadTimeoutMs(30000);
```

设置为 `0` 表示禁用超时：

```cpp
browser.setLoadTimeoutMs(0);
```

### 设置缩放比例

```cpp
browser.setZoomFactor(1.25);
```

### 设置 User-Agent

```cpp
browser.setUserAgent("YourApp/1.0");
```

## 导航白名单

默认允许的协议：

- `about`
- `blob`
- `data`
- `http`
- `https`
- `qrc`

默认不允许 `file` 和未知协议。

设置允许协议：

```cpp
browser.setAllowedUrlSchemes({"http", "https", "blob", "data"});
```

设置允许主机：

```cpp
browser.setAllowedHosts({"example.com", "*.example.com"});
```

空主机列表表示不限制主机。

## 弹窗策略

```cpp
browser.setPopupPolicy(bm::BrowserPageWidget::PopupPolicy::Block);
```

可选值：

| 策略 | 说明 |
| --- | --- |
| `Block` | 阻止弹窗 |
| `OpenInCurrentView` | 在当前页面打开弹窗目标 |
| `DelegateToApplication` | 交给宿主程序处理 |

监听弹窗请求：

```cpp
QObject::connect(&browser, &bm::BrowserPageWidget::popupRequested,
                 [](const QUrl &url) {
                     qDebug() << "popup:" << url;
                 });
```

## 下载策略

```cpp
browser.setDownloadPolicy(bm::BrowserPageWidget::DownloadPolicy::AutoSave);
```

可选值：

| 策略 | 说明 |
| --- | --- |
| `Deny` | 拒绝下载 |
| `DelegateToApplication` | 交给宿主程序处理 |
| `AutoSave` | 自动保存到下载目录 |

设置下载目录：

```cpp
browser.setDownloadDirectory("D:/Downloads");
```

监听下载状态：

```cpp
QObject::connect(&browser, &bm::BrowserPageWidget::downloadStarted,
                 [](quint32 id, const QUrl &url, const QString &filePath) {
                     qDebug() << "download started" << id << url << filePath;
                 });

QObject::connect(&browser, &bm::BrowserPageWidget::downloadFinished,
                 [](quint32 id, const QString &filePath) {
                     qDebug() << "download finished" << id << filePath;
                 });

QObject::connect(&browser, &bm::BrowserPageWidget::downloadFailed,
                 [](quint32 id, const QString &reason) {
                     qWarning() << "download failed" << id << reason;
                 });
```

## 网页权限策略

默认拒绝网页权限请求。

```cpp
browser.setPermissionPolicy(
    bm::BrowserPageWidget::PermissionPolicy::DelegateToApplication);
```

委托给宿主后，宿主必须调用 `grant()` 或 `deny()`：

```cpp
QObject::connect(&browser, &bm::BrowserPageWidget::permissionRequested,
                 [](QWebEnginePermission permission) {
                     permission.deny();
                 });
```

## 证书错误策略

默认拒绝证书错误。

```cpp
browser.setCertificatePolicy(
    bm::BrowserPageWidget::CertificatePolicy::DelegateToApplication);
```

委托给宿主后，宿主必须调用 `acceptCertificate()` 或 `rejectCertificate()`。

## 访问底层 WebEngine 对象

如果业务侧需要更底层的能力，可以访问原生对象：

```cpp
QWebEngineView *view = browser.view();
QWebEnginePage *page = browser.page();
QWebEngineProfile *profile = browser.profile();
```

常见用途：

- 配置 WebChannel
- 注入 JavaScript
- 设置请求拦截器
- 设置自定义 Profile 行为

## 默认安全策略

默认配置偏保守：

- 下载默认拒绝
- 网页权限默认拒绝
- 证书错误默认拒绝
- 未知 URL 协议默认拒绝
- `file` 协议默认不允许
- 弹窗默认在当前页面打开

如果业务需要放开能力，应显式调用对应配置函数。

## 目标名称

CMake 库目标为：

```cmake
browserpagewidget
```

推荐链接别名：

```cmake
bm::browserpagewidget
```

当前项目固定生成动态库。Windows 下通常会生成：

```text
browserpagewidget.dll
browserpagewidget.lib
```

Debug 配置下会带 `d` 后缀，例如：

```text
browserpagewidgetd.dll
browserpagewidgetd.lib
```

其中 `.dll` 是运行时动态库，`.lib` 是链接动态库时使用的导入库，不是静态库。

## 注意事项

- `QWebEngineProfile` 可以理解为浏览器用户数据环境，管理 Cookie、缓存、本地存储、下载和权限等。
- 使用外部传入的 `QWebEngineProfile` 时，Profile 必须比 `BrowserPageWidget` 活得更久。
- 如果隐藏了工具栏，用户无法通过地址栏输入地址，但仍可以通过代码调用 `loadUrl()`。
- `AutoSave` 下载会自动处理同名文件，避免覆盖已有文件。
- 如果网页使用 `target="_blank"` 触发下载，控件会用临时页面接管并在下载结束后释放。

## 许可证

当前仓库未包含许可证文件。正式发布前建议补充 `LICENSE`。

