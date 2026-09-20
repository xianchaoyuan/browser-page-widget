# BrowserPageWidget Qt 5 / Qt 6 单源码整合设计

| 项目 | 内容 |
| --- | --- |
| 文档版本 | v1.0（设计稿，待评审） |
| 整合对象 | `D:\mycode\browser-page-widget`（Qt 6.11.2）+ `D:\mycode\browser-page-widget-qt5`（Qt 5.15.2） |
| 整合后位置 | `D:\mycode\browser-page-widget`（Qt6 项目为基准改造，Qt5 项目完成后删除） |
| 事实依据 | 所有 API 结论均已对照本机 `C:\Qt\5.15.2` 与 `C:\Qt\6.11.2` 头文件逐一核实 |

---

## 1. 目标与范围

**目标**：一套源码，同时支持 Qt 5.15.2 与 Qt 6.x（≥6.2），宿主程序调用同一套公开 API，
不需要写任何 `#if QT_VERSION` 分支。

**范围**：库本体（browserpagewidget）、Example、AgentPageViewer、CMake 构建体系、构建脚本。
`agentpageviewer` 的启动/Job Object/EndpointWaiter 逻辑在移植中已验证双版本通用，不在改动范围内。

**非目标**：
- 不追求抹平能力差异——Qt5 构建下 `loadFailed` 的 errorDomain/errorCode 恒为 -1、
  `clearBrowsingData()` 同步通知（无完成信号）、无 PersistentPermissionsPolicy 等，
  这些是 Qt 5.15 WebEngine（Chromium 83）的固有限制，按「能力降级、语义不破」原则处理。

---

## 2. 结论摘要

- 两项目忽略空白后的真实差异约 **380 行**，可收敛为 **8 个版本分支点** + 1 个兼容头文件。
- 公开 API 统一到「双版本交集」形态：权限用 `permissionRequested(origin, feature)` 二元组模型，
  下载对象用兼容别名 `bm::BrowserDownloadItem`。
- 权限应答内部实现推荐「双轨」：Qt6 走 `QWebEnginePermission`（新 API，deprecated 风险为零），
  Qt5 走 `setFeaturePermission`（Qt5 的正规 API），对外形态完全一致。

---

## 3. 整合后项目布局

```
browser-page-widget/                  # 唯一源码树
├── CMakeLists.txt                    # 版本探测 + 统一 target 别名
├── browserqtcompat.h                 # 【新增】版本兼容层（唯一的 #if 集中地之一）
├── browserpagewidget.h/.cpp          # 公开 API（交集形态，无版本分支）
├── browserpagewidget_p.h/.cpp        # 私有实现（4 处 #if 分支）
├── browserwebpage_p.h/.cpp           # 证书入口（1 处 #if 分支）
├── browserdownloadmanager_p.h/.cpp   # 下载（2 处 #if 分支）
├── browserpagepolicy.h/.cpp          # 无差异，直接沿用
├── agentpageviewer/                  # 无差异，直接沿用
├── example/                          # 无差异
├── cmake/BrowserPageWidgetConfig.cmake.in
├── build.bat                         # 参数化：build.bat <5|6> <Debug|Release>
└── docs/qt5-qt6-unified-source-design.md
```

原则：`#if QT_VERSION` 只允许出现在 `browserqtcompat.h`、私有类 `_p.h/.cpp` 内部，
**公开头文件 `browserpagewidget.h` 中除类型别名外不得出现版本分支**。

---

## 4. 兼容层设计：browserqtcompat.h

```cpp
#pragma once
#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QWebEngineDownloadRequest>
namespace bm {
/// 下载对象统一别名：Qt6 真名 QWebEngineDownloadRequest，
/// Qt5 真名 QWebEngineDownloadItem（Qt 6.11 已删除别名，故必须自定义）。
using BrowserDownloadItem = QWebEngineDownloadRequest;
} // namespace bm
#else
#include <QWebEngineDownloadItem>
namespace bm {
using BrowserDownloadItem = QWebEngineDownloadItem;
} // namespace bm
#endif

namespace bm {

/// 证书错误应答：Qt5 叫 ignoreCertificateError()，Qt6 叫 acceptCertificate()。
inline void acceptCertificateError(QWebEngineCertificateError &error)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    error.acceptCertificate();
#else
    error.ignoreCertificateError();
#endif
}

} // namespace bm
```

说明：
- 两个下载类型在 5.15.2 已拥有相同 API 表面（`setDownloadDirectory/setDownloadFileName/
  suggestedFileName/page()/id()/state()` 均已核实），别名安全。
- `defer()` / `rejectCertificate()` 双版本同名同义，无需包装。

---

## 5. 八个分支点逐项设计

### 5.1 权限模型（公开 API 统一，内部实现双轨）

**公开 API（两版本完全一致，无分支）**：

```cpp
// 信号：网页请求权限时发出
void permissionRequested(const QUrl &origin, int feature);
// 槽：宿主应答
void grantPermission(const QUrl &origin, int feature);
void denyPermission(const QUrl &origin, int feature);
```

**内部实现（推荐方案 B：双轨）**：

| 版本 | 监听入口 | 应答方式 |
| --- | --- | --- |
| Qt 6 | `QWebEnginePage::permissionRequested(QWebEnginePermission)` 信号 | 私有类缓存 pending `QWebEnginePermission`（QHash，键 = origin+type），`grantPermission()` 查表调 `permission.grant()` |
| Qt 5 | `featurePermissionRequested(origin, feature)` 信号 | `setFeaturePermission(origin, feature, PermissionGranted/Denied)` |

选型理由：Qt 6.11 中 `featurePermissionRequested`/`setFeaturePermission` 已标 deprecated
（`QT_DEPRECATED_VERSION_X_6_8`，Qt 7 将移除）。方案 B 让 Qt6 构建完全走新 API，
零弃用警告、面向 Qt7 安全；代价是私有类多约 40 行（一个 QHash + 查表应答）。

备选方案 A（更省事）：Qt6 也连 `featurePermissionRequested` + `setFeaturePermission`，
代码与 Qt5 完全相同、零分支，但 Qt6 构建产生弃用警告且 Qt7 出来要返工。
**若求最小改动可选 A，本设计默认推荐 B。**

### 5.2 证书错误入口（BrowserWebPage，1 处分支）

| 版本 | 入口 |
| --- | --- |
| Qt 6 | 连接 `QWebEnginePage::certificateError` 信号（虚函数在 6.11 已删除） |
| Qt 5 | 重写 `certificateError(QWebEngineCertificateError&)` 虚函数（无信号） |

两个入口的函数体完全相同：拷贝 error → 按策略 `defer()` 后发 `certificateErrorRequested()`，
或直接 `rejectCertificate()`。差异仅在外壳，抽成一个共用私有函数 `handleCertificateError()`。

### 5.3 证书应答方法名（compat 层）

见第 4 节 `acceptCertificateError()`。`defer()/rejectCertificate()` 直接通用。
宿主侧文档注明：应答副本调用 `rejectCertificate()`（拒绝）或 `acceptCertificateError()`（放行，两版通用）。

### 5.4 下载类型名（compat 层）

公开信号签名统一为：

```cpp
void downloadRequested(bm::BrowserDownloadItem *download);
```

宿主从 `QWebEngineDownloadRequest*` 改为 `bm::BrowserDownloadItem*`——机械替换一行。

### 5.5 下载进度信号（BrowserDownloadManager，1 处分支）

| 版本 | 监听 |
| --- | --- |
| Qt 6 | `receivedBytesChanged` + `totalBytesChanged` |
| Qt 5 | `downloadProgress(qint64 received, qint64 total)`（5.15 无前者） |

Qt6 下两信号合并回一次 `emit q->downloadProgress(id, received, total)`（原 Qt6 版已如此处理）。

### 5.6 加载失败明细（BrowserPageWidgetPrivate，1 处分支）

| 版本 | 监听 | errorDomain/errorCode |
| --- | --- | --- |
| Qt 6 | `loadingChanged(QWebEngineLoadingInfo)` | 从 LoadingInfo 取真实值 |
| Qt 5 | `loadFinished(bool)` 兜底 | 恒 -1（Qt5 无对应 API），errorString 保留 |

### 5.7 清缓存完成通知（1 处分支）

| 版本 | 行为 |
| --- | --- |
| Qt 6 | 连 `clearHttpCacheCompleted`，完成后发 `browsingDataCleared()`（准确异步语义） |
| Qt 5 | `clearHttpCache()` 后立即发 `browsingDataCleared()`（无完成信号，文档注明） |

### 5.8 仅 Qt6 的设置项（2 处 `#if` 守卫）

```cpp
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    settings->setAttribute(QWebEngineSettings::NavigateOnDropEnabled, false);
    if (ownsProfile_)
        profile_->setPersistentPermissionsPolicy(QWebEngineProfile::AskEveryTime);
#endif
```

Qt5 构建下这些增强静默缺失，行为 = 当前 Qt5 版，无语义破坏。

### 5.9 伪差异（不需要分支，统一写法即可）

| 项 | 统一写法 |
| --- | --- |
| `Qt::SingleShotConnection`（仅 Qt6） | 手动 `QMetaObject::Connection` 一次性断开模式（Qt5 版已实现，Qt6 下同样合法） |
| `QIcon` 隐式传递包含（仅 Qt6） | 显式 `#include <QIcon>`（无害） |
| `createStandardContextMenu`（仅 Qt6） | 不使用（Qt5 版已放弃接管右键菜单，保持一致） |
| DevTools / `setDevToolsPage()` | 双版本同名同义，F12/`toggleDevTools()`/`inspectElement()` 代码直接通用 |

---

## 6. 公开 API 迁移对照（宿主程序改动清单）

| Qt6 原版 API | 整合版 API（双版本一致） | 宿主改动量 |
| --- | --- | --- |
| `permissionRequested(QWebEnginePermission)` | `permissionRequested(QUrl, int feature)` | 改信号槽签名 |
| `permission->grant() / deny()` | `grantPermission(origin, feature)` / `denyPermission(...)` | 改应答调用 |
| `downloadRequested(QWebEngineDownloadRequest*)` | `downloadRequested(bm::BrowserDownloadItem*)` | 类型名替换 |
| 证书应答 `acceptCertificate()` | `bm::acceptCertificateError(err)` | 函数名替换 |
| 其余全部 API | 不变 | 零改动 |

注：整合后 Qt6 宿主如仍想用 `QWebEnginePermission` 对象模型，可通过 `#if` 自行分支连接
Qt6 原生 `QWebEnginePage::permissionRequested`——库不拦截，只是不再代发。

---

## 7. CMake 与构建体系

```cmake
cmake_minimum_required(VERSION 3.21)

# 版本选择：显式指定优先，否则自动探测 Qt6 → Qt5
set(BPW_QT_MAJOR "" CACHE STRING "Major Qt version (5 or 6), empty = auto-detect")
if(NOT BPW_QT_MAJOR)
    find_package(Qt6 QUIET COMPONENTS WebEngineWidgets Network)
    if(Qt6_FOUND)
        set(BPW_QT_MAJOR 6)
    else()
        set(BPW_QT_MAJOR 5)
    endif()
endif()

find_package(Qt${BPW_QT_MAJOR} REQUIRED COMPONENTS WebEngineWidgets Network)

# 统一 target 别名：屏蔽 Qt5::/Qt6:: 命名差异
add_library(BrowserPageWidget::QtDeps INTERFACE IMPORTED)
target_link_libraries(BrowserPageWidget::QtDeps INTERFACE
    Qt${BPW_QT_MAJOR}::WebEngineWidgets
    Qt${BPW_QT_MAJOR}::Network)

# 统一使用双版本通用命令：add_library + AUTOMOC
# （qt_add_library / qt_standard_project_setup 为 Qt6 专属，不使用）
add_library(browserpagewidget SHARED ...)
set_target_properties(browserpagewidget PROPERTIES AUTOMOC ON ...)
target_link_libraries(browserpagewidget PRIVATE BrowserPageWidget::QtDeps)
```

要点：
- `QT_DEPRECATED_WARN` 不需要设置（方案 B 不触碰 deprecated API）。
- AUTOMOC、C++17（Qt 6 要求 ≥C++17，Qt 5.15 支持 C++17，统一 `CMAKE_CXX_STANDARD 17`）。
- Config 模板中记录实际使用的 Qt 版本，供下游校验。
- WebEngine 运行时部署：`windeployqt` 需对 **库 DLL 和两个 exe 分别执行**（上次踩过的坑：
  它不递归扫描 DLL 依赖）；构建脚本中固化这三条命令 + openclaw-service 包复制。

**build.bat**（参数化，替换现有 build_qt5.bat）：

```
build.bat 5 Debug     # Qt 5.15.2 msvc2019_64 + VS2022
build.bat 6 Debug     # Qt 6.11.2 msvc2022_64 + VS2022
build.bat 5 Release
...
```

MSVC 环境变量组装逻辑沿用 build_qt5.bat 已验证的实现；Qt 路径按版本号映射。

---

## 8. 实施步骤（四阶段，每阶段有独立验证点）

| 阶段 | 内容 | 验证点 |
| --- | --- | --- |
| **P1 兼容层落地** | 以 Qt6 项目为基准，新增 `browserqtcompat.h`；公开 API 改为交集形态（权限二元组、下载别名、证书应答包装）；`Qt::SingleShotConnection` 改手动断开 | Qt 6.11.2 编译通过，/W4 零警告；Example + AgentPageViewer 运行正常 |
| **P2 分支点收编** | 把 Qt5 项目里 8 处差异逻辑以 `#if QT_VERSION` 合入私有实现（权限双轨、证书入口、进度信号、加载明细、清缓存、Qt6 增强守卫） | Qt 6.11.2 再次编译 + 运行回归（确认合入未破坏 Qt6 行为） |
| **P3 Qt5 验证** | 用同一源码切 Qt 5.15.2 配置构建 | Qt 5.15.2 编译通过，/W4 零警告；windeployqt 三件套部署；AgentPageViewer 连 Hermes Dashboard 冒烟（F12 DevTools、下载、页面加载） |
| **P4 收尾** | 参数化 build.bat、README 更新（构建矩阵 + API 迁移对照）、删除 `browser-page-widget-qt5` 目录 | 双版本完整构建矩阵全绿 |

每阶段一个 git commit，出问题可独立回退。

---

## 9. 风险与对策

| 风险 | 等级 | 对策 |
| --- | --- | --- |
| Qt6 下 `QWebEnginePermission` 查表应答存在信号多次触发/对象过期问题 | 中 | pending 表在应答后立即移除；permission 对象为值语义可安全拷贝缓存；P1 阶段加日志验证一问一答 |
| Qt5 构建遗漏 WebEngine 运行时部署（windeployqt 不递归） | 低 | 脚本固化三条 windeployqt 命令，上次已验证 |
| 两版本行为差异被误当成 bug | 低 | README 附「已知能力差异表」（第 1 节非目标内容展开） |
| 未来 Qt6.x 新弃用（如 6.11 已删 certificateError 虚函数的先例） | 中 | 分支点集中在私有类，升级 Qt 只动 `_p` 文件；CI/发版前双版本编译验证作为流程固定下来 |

---

## 10. 验收标准

1. 单一源码树，`grep -r "QT_VERSION_CHECK"` 命中 ≤ 10 处，且全部位于 compat 头与 `_p` 文件。
2. `build.bat 5 Debug` / `build.bat 6 Debug` / 两版本 Release，全部 /W4 零警告。
3. 双版本 Example 可浏览网页、下载文件；AgentPageViewer 可启动 openclaw 服务并加载 Dashboard。
4. 宿主侧 demo 代码（Example）不含任何 `#if QT_VERSION`。
5. `browser-page-widget-qt5` 目录删除，README 载明双版本构建方法。
