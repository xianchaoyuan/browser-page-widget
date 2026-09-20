# BrowserPageWidget

`BrowserPageWidget` 是一个基于 Qt WebEngine 的通用浏览器控件库。它把 `QWebEngineView` 封装成一个可直接嵌入 Qt Widgets 程序的组件，并提供工具栏、状态栏、加载超时、导航白名单、弹窗策略、下载策略、网页权限策略和证书错误策略。

项目目标是：让业务程序可以像使用普通 `QWidget` 一样嵌入网页页面，同时保留必要的安全控制和扩展能力。

**一套源码同时支持 Qt 5.15.2 与 Qt 6（≥6.2）**：宿主程序通过同一套公开 API 使用控件，不需要编写任何 `#if QT_VERSION` 分支；版本差异全部收敛在私有实现（`*_p.h/.cpp`）与兼容头 `browserqtcompat.h` 中。

## 功能特性

- 基于 Qt WebEngine（Qt 5.15.2 / Qt 6.x 双版本）
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
- 支持 DevTools 开发者工具（F12 切换、`inspectElement()` 元素拾取）
- 支持访问底层 `QWebEngineView`、`QWebEnginePage`、`QWebEngineProfile`
- 支持作为 CMake 库安装和 `find_package()` 引用

## 环境要求

- CMake 3.21 或更高
- C++17
- Qt 5.15.2 **或** Qt 6.x（二选一，或同时安装）
- Qt 组件：
  - `Widgets`
  - `WebEngineCore`
  - `WebEngineWidgets`
  - `Network`（仅 `AgentPageViewer` 用于探测服务端口）

构建时优先使用 Qt 6；没有 Qt 6 时自动回落到 Qt 5.15。也可以显式指定版本（见下文）。

## 快速编译

### 一键脚本（Windows / MSVC 2022）

```bat
build.bat 6 Debug     rem Qt 6.11.2 msvc2022_64 + VS2022（默认）
build.bat 5 Debug     rem Qt 5.15.2 msvc2019_64 + VS2022
build.bat 5 Release
build.bat 6 Release
```

- 第一个参数是 Qt 大版本（`5` 或 `6`，默认 `6`），第二个参数是配置（`Debug`/`Release`，默认 `Debug`）。
- 不同 Qt 版本使用不同构建目录：Qt 6 用 `build/`，Qt 5 用 `build-qt5/`，避免 CMakeCache 冲突。
- 产物输出到按版本隔离的目录：`bin/qt6/<配置>/` 与 `bin/qt5/<配置>/`，两套构建互不覆盖。
- Qt 安装路径硬编码于脚本内（Qt 6 指向 `C:\Qt\6.11.2\6.11.2\msvc2022_64`，Qt 5 指向 `C:\Qt\5.15.2\5.15.2\msvc2019_64`），如路径不同请修改 `build.bat` 中的 `QT_ROOT`。

### 手动 CMake

```powershell
# 自动探测：Qt6 -> Qt5
cmake -S . -B build
cmake --build build --config Debug

# 显式指定 Qt 版本
cmake -S . -B build-qt5 -DBPW_QT_MAJOR=5 -DBROWSER_PAGE_WIDGET_QT_ROOT=C:\Qt\5.15.2\5.15.2\msvc2019_64
cmake --build build-qt5 --config Debug
```

生成的 Example 程序位于：

```text
bin/qt6/debug/BrowserPageWidgetExample.exe    （Qt 6 构建）
bin/qt5/debug/BrowserPageWidgetExample.exe    （Qt 5 构建）
```

如果使用的是 Visual Studio 多配置生成器，`Debug`、`Release` 会分别生成到小写的对应配置目录，例如 `debug`、`release`。

### 运行时部署（WebEngine）

WebEngine 运行需要 Qt 的 DLL、`QtWebEngineProcess( d).exe` 和 `resources/` 目录。使用 `windeployqt` 部署时要注意：**它不会递归扫描 DLL 依赖**，因此必须对库 DLL 和两个可执行文件分别执行：

```bat
set PATH=<Qt 安装目录>\bin;%PATH%
windeployqt --debug --compiler-runtime bin\qt6\debug\browserpagewidgetd.dll
windeployqt --debug --compiler-runtime bin\qt6\debug\BrowserPageWidgetExample.exe
windeployqt --debug --compiler-runtime bin\qt6\debug\AgentPageViewer.exe
```

Qt 5 构建对应使用 `C:\Qt\5.15.2\5.15.2\msvc2019_64\bin\windeployqt.exe`，目标目录换成 `bin\qt5\debug\`。

## 目录结构

```text
browser-page-widget/
├── CMakeLists.txt
├── build.bat                  # 参数化构建脚本：build.bat <5|6> <Debug|Release>
├── example/
│   └── main.cpp
├── agentpageviewer/
│   ├── main.cpp
│   ├── agentstartupcontroller.h
│   ├── agentstartupcontroller.cpp
│   ├── endpointwaiter.h
│   ├── endpointwaiter.cpp
│   ├── processjob.h
│   ├── processjob.cpp
│   ├── agentstartupsplash.h
│   └── agentstartupsplash.cpp
├── browserpagewidget.h
├── browserpagewidget.cpp
├── browserpagewidget_p.h
├── browserpagewidget_p.cpp
├── browserqtcompat.h          # 双版本兼容层：下载类型别名、证书应答包装等
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
| `browserpagewidget.h/.cpp` | 对外公开的浏览器控件类（双版本交集 API，无版本分支） |
| `browserqtcompat.h` | 版本兼容层：`bm::BrowserDownloadItem` 别名、`bm::acceptCertificateError()` 等 |
| `browserpagewidget_p.h/.cpp` | 控件内部状态和 UI/WebEngine 连接逻辑（版本差异集中处） |
| `browserpagepolicy.h/.cpp` | URL 协议和主机白名单策略 |
| `browserwebpage_p.h/.cpp` | 内部 WebPage，用于接管导航、弹窗、证书错误（Qt5）和 console 日志 |
| `browserdownloadmanager_p.h/.cpp` | 下载策略、保存路径和下载进度处理 |
| `browserpagewidgetglobal.h` | 动态库导入导出宏 |
| `example/main.cpp` | Example 程序入口 |
| `agentpageviewer/main.cpp` | AgentPageViewer 程序入口，负责创建启动画面和显示 Agent 页面 |
| `cmake/BrowserPageWidgetConfig.cmake.in` | 安装后供 `find_package()` 使用的 CMake 配置模板 |

## CMake 选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `BPW_QT_MAJOR` | 自动 | Qt 大版本：`5` 或 `6`；留空时自动探测 Qt6 → Qt5 |
| `BROWSER_PAGE_WIDGET_BUILD_EXAMPLE` | `ON` | 是否编译 Example 程序 |
| `BROWSER_PAGE_WIDGET_BUILD_AGENT_VIEWER` | `ON` | 是否编译 AgentPageViewer 程序 |
| `BROWSER_PAGE_WIDGET_QT_ROOT` | 空 | 可选 Qt 5/6 安装目录提示 |

当前项目固定生成动态库，不再通过 `BUILD_SHARED_LIBS` 切换静态库。

示例：只编译库，不编译 Example：

```powershell
cmake -S . -B build -DBROWSER_PAGE_WIDGET_BUILD_EXAMPLE=OFF
cmake --build build --config Release
```

## AgentPageViewer

`AgentPageViewer` 是一个类似 Example 的独立程序，用于直接显示 Agent 页面。

程序显示页面前会先检查目标地址是否已经可连接。如果还没有服务，它会从程序目录附近查找并自动启动 Agent 服务。

推荐发布包结构：

```text
<发布目录>/
├── AgentPageViewer.exe
├── browserpagewidget.dll
└── openclaw-service/
    ├── openclaw.cmd
    ├── runtime/
    └── state/
```

发布包中服务启动命令等价于：

```text
openclaw-service/openclaw.cmd gateway
```

开发调试时也兼容 `bin/qt6/debug/AgentPageViewer.exe` 或 `bin/qt5/debug/AgentPageViewer.exe` 自动查找 `bin/dist/openclaw-service`。

启动过程中会显示启动画面，提示当前正在检查服务、启动服务或等待服务就绪。程序通过 `cmd.exe /d /c openclaw.cmd gateway` 启动服务，工作目录为 `openclaw-service`。端口检查和服务等待使用异步流程，启动画面会保持响应；程序最多等待 120 秒，等目标页面端口可连接后再加载网页。Windows 下会使用 Job Object 管理由本程序拉起的服务进程树，AgentPageViewer 退出时会自动清理对应的 node 服务。

程序启动时就会创建诊断日志，并记录应用目录、服务目录、目标 URL 和服务文件是否存在。默认日志路径为：

```text
openclaw-service/state/agentpageviewer-service.log
```

默认打开：

```text
http://127.0.0.1:19001/
```

程序默认隐藏顶部工具栏，保留底部状态栏，并启用自动下载保存。页面加载完成、加载失败或加载超时时，启动画面会自动关闭。

## Example 说明

Example 入口在 `example/main.cpp`。

当前 Example 默认配置：

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
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=<安装目录>/browserpagewidget
cmake --build build --config Release
cmake --install build --config Release
```

其他项目中使用：

```cmake
list(APPEND CMAKE_PREFIX_PATH "<安装目录>/browserpagewidget")

find_package(BrowserPageWidget REQUIRED)

target_link_libraries(your_app PRIVATE bm::browserpagewidget)
```

代码中使用：

```cpp
#include "browserpagewidget.h"
```

安装导出的 `BrowserPageWidgetConfig.cmake` 会记录构建时实际使用的 Qt 大版本（`BrowserPageWidget_QT_MAJOR`），下游 `find_package()` 会自动引用同版本的 Qt。

## 基本用法

```cpp
#include "browserpagewidget.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

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

### 打开开发者工具

```cpp
browser.toggleDevTools();    // F12 或此方法切换 DevTools 窗口
browser.inspectElement();    // 进入「检查元素」拾取模式
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
browser.setDownloadDirectory("<下载目录>");
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

`DelegateToApplication` 模式下，`downloadRequested()` 信号参数类型是双版本统一的 `bm::BrowserDownloadItem *`（Qt 6 真名 `QWebEngineDownloadRequest`，Qt 5 真名 `QWebEngineDownloadItem`），宿主无需感知具体版本。

## 网页权限策略

默认拒绝网页权限请求。

```cpp
browser.setPermissionPolicy(
    bm::BrowserPageWidget::PermissionPolicy::DelegateToApplication);
```

委托给宿主后，权限模型是「来源 + Feature」二元组，宿主必须调用 `grantPermission()` 或 `denyPermission()` 完成应答：

```cpp
QObject::connect(&browser, &bm::BrowserPageWidget::permissionRequested,
                 [&browser](const QUrl &origin, int feature) {
                     // 例如：允许摄像头、拒绝麦克风
                     if (feature == static_cast<int>(QWebEnginePage::MediaAudioCapture)) {
                         browser.denyPermission(origin, feature);
                     } else {
                         browser.grantPermission(origin, feature);
                     }
                 });
```

内部实现按版本走双轨：Qt 6 走 `QWebEnginePermission` 新 API（Qt 6.8 起旧 API 已弃用），Qt 5 走 `setFeaturePermission`，对外行为完全一致。

## 证书错误策略

默认拒绝证书错误。

```cpp
browser.setCertificatePolicy(
    bm::BrowserPageWidget::CertificatePolicy::DelegateToApplication);
```

委托给宿主后，宿主必须调用 `bm::acceptCertificateError()`（放行）或 `rejectCertificate()`（拒绝）：

```cpp
QObject::connect(&browser, &bm::BrowserPageWidget::certificateErrorRequested,
                 [](QWebEngineCertificateError error) {
                     if (/* 判断是否可信 */) {
                         bm::acceptCertificateError(error);   // 双版本通用：Qt6=acceptCertificate，Qt5=ignoreCertificateError
                     } else {
                         error.rejectCertificate();
                     }
                 });
```

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

## Qt 5 / Qt 6 已知能力差异

| 能力 | Qt 6 | Qt 5.15（Chromium 83） |
| --- | --- | --- |
| `loadFailed` 的 `errorDomain` / `errorCode` | 真实值 | 恒为 `-1`，详见 `errorString` |
| `clearBrowsingData()` 完成通知 | 缓存真正清完后异步通知 | 发起清理后立即通知 |
| 权限应答内部机制 | `QWebEnginePermission`（新 API） | `setFeaturePermission` |
| 拖放导航开关（`NavigateOnDropEnabled`） | 默认关闭 | 无对应 API，保持默认 |
| 权限持久化策略（`setPersistentPermissionsPolicy`） | 自建 Profile 默认 `AskEveryTime` | 无对应 API |
| 证书错误入口 | `certificateError` 信号 | `certificateError()` 虚函数 |

以上差异已由库内部收敛，宿主程序无需（也不应）编写版本分支；如需感知差异（例如展示错误码），请以信号参数实际值为准。

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
- `browser-page-widget-qt5`（旧的独立 Qt5 适配目录）在整合完成后已废弃，请使用本目录的 `build.bat 5`。

## 许可证

当前仓库未包含许可证文件。正式发布前建议补充 `LICENSE`。
