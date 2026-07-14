#ifndef AGENTSTARTUPCONTROLLER_H
#define AGENTSTARTUPCONTROLLER_H

#include <QObject>
#include <QUrl>

class AgentStartupSplash;
class ProcessJob;

/**
 * @brief 管理 AgentPageViewer 启动阶段的服务检查、服务启动和端口等待。
 *
 * 这个类只负责启动流程，不创建浏览器主界面。所有等待动作都使用异步方式，
 * 避免启动画面因为阻塞等待而卡顿。
 */
class AgentStartupController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 创建启动流程控制器。
     * @param pageUrl 需要打开的 Agent 页面地址。
     * @param splash 启动画面，用于显示当前启动状态。
     * @param parent Qt 父对象。
     */
    explicit AgentStartupController(const QUrl &pageUrl,
                                    AgentStartupSplash *splash,
                                    QObject *parent = nullptr);

    /** @brief 析构时会停止由本程序拉起的 Agent 进程树。 */
    ~AgentStartupController() override;

    /** @brief 开始异步启动流程。 */
    void start();

    /** @brief 停止由本程序拉起的 Agent 进程树。 */
    void stopService();

signals:
    /** @brief 服务已就绪，或者已决定继续打开页面。 */
    void readyToOpenPage();

private:
    /** @brief 记录程序目录、服务目录、目标页面等启动诊断信息。 */
    void appendStartupDiagnostics();

    /** @brief 检查目标页面端口是否已经可连接。 */
    void checkEndpointBeforeStart();

    /** @brief 启动 Agent 服务。 */
    bool startAgentGateway();

    /** @brief 服务启动命令发出后，异步等待目标端口就绪。 */
    void waitForServiceReady();

    /** @brief 只发送一次 readyToOpenPage 信号，防止重复打开主界面。 */
    void finishStartup();

private:
    /** @brief 即将打开的 Agent 页面地址。 */
    QUrl pageUrl_;

    /** @brief 启动画面，不由本类拥有。 */
    AgentStartupSplash *splash_ = nullptr;

    /** @brief 本程序拉起的 Agent 进程树管理器。 */
    ProcessJob *agentGateway_ = nullptr;

    /** @brief 防止异步回调重复触发主界面打开。 */
    bool startupFinished_ = false;
};

#endif // AGENTSTARTUPCONTROLLER_H
