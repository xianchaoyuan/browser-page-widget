#ifndef ENDPOINTWAITER_H
#define ENDPOINTWAITER_H

#include <QElapsedTimer>
#include <QObject>
#include <QUrl>

#include <functional>

class QTcpSocket;

/**
 * @brief 异步等待某个 URL 对应的主机端口可连接。
 *
 * 这个类只做 TCP 端口探测，不请求网页内容。调用方创建对象后设置回调，
 * 再调用 start() 即可。等待结束后对象会自动 deleteLater()。
 */
class EndpointWaiter : public QObject
{
public:
    /**
     * @brief 创建端口等待器。
     * @param url 需要探测的页面地址。
     * @param timeoutMs 最大等待时间，单位毫秒。
     * @param parent Qt 父对象，用于跟随启动控制器一起释放。
     */
    explicit EndpointWaiter(const QUrl &url, int timeoutMs, QObject *parent = nullptr);

    /**
     * @brief 设置每次探测失败后的回调。
     * @param callback 回调参数是已经等待的毫秒数。
     */
    void setProbeCallback(const std::function<void(qint64 elapsedMs)> &callback);

    /**
     * @brief 设置等待完成后的回调。
     * @param callback 回调参数表示端口是否已经可连接。
     */
    void setFinishedCallback(const std::function<void(bool available)> &callback);

    /** @brief 开始异步等待，函数会立刻返回，不阻塞界面。 */
    void start();

private:
    /** @brief 发起一次 TCP 连接探测。 */
    void startProbe();

    /** @brief 处理一次 TCP 探测结果。 */
    void completeProbe(int probeId, QTcpSocket *socket, bool available);

    /** @brief 统一结束出口，确保完成回调只触发一次。 */
    void finish(bool available);

private:
    /** @brief 要探测的页面地址。 */
    QUrl url_;

    /** @brief 最大等待时间，单位毫秒。 */
    int timeoutMs_ = 0;

    /** @brief 当前正在执行的 TCP 探测 socket。 */
    QTcpSocket *socket_ = nullptr;

    /** @brief 已经等待的时间计时器。 */
    QElapsedTimer elapsedTimer_;

    /** @brief 当前探测编号，用于忽略过期回调。 */
    int probeId_ = 0;

    /** @brief 防止完成回调重复触发。 */
    bool finished_ = false;

    /** @brief 每次探测失败后的进度回调。 */
    std::function<void(qint64 elapsedMs)> onProbe_;

    /** @brief 等待完成后的结果回调。 */
    std::function<void(bool available)> onFinished_;

    Q_DISABLE_COPY(EndpointWaiter)
};

#endif // ENDPOINTWAITER_H
