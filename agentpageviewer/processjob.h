#ifndef PROCESSJOB_H
#define PROCESSJOB_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <functional>

#ifndef Q_OS_WIN
class QProcess;
#endif

/**
 * @brief 启动并管理一个服务进程树。
 *
 * Windows 下使用 Job Object 管理进程树：主程序退出或 close() 被调用时，
 * 会结束由本类启动的 cmd.exe 以及它创建出的 node.exe 等子进程。
 */
class ProcessJob : public QObject
{
public:
    /**
     * @brief 创建进程树管理器。
     * @param parent Qt 父对象。
     */
    explicit ProcessJob(QObject *parent = nullptr);

    /** @brief 析构时会关闭进程作业并清理进程树。 */
    ~ProcessJob() override;

    /**
     * @brief 启动程序并纳入进程树管理。
     * @param program 程序路径或程序名。
     * @param arguments 启动参数。
     * @param workingDirectory 工作目录。
     * @return 启动并纳入管理成功返回 true。
     */
    bool start(const QString &program,
               const QStringList &arguments,
               const QString &workingDirectory);

    /** @brief 关闭进程作业，结束由本类启动的进程树。 */
    void close();

    /** @brief 是否仍然持有可清理的进程作业。 */
    bool isActive() const;

    /** @brief 返回启动进程的 PID。 */
    quint64 processId() const;

    /** @brief 返回最近一次错误信息。 */
    QString lastErrorString() const;

    /**
     * @brief 设置启动进程结束回调。
     * @param callback 回调参数是启动进程退出码。
     */
    void setFinishedCallback(const std::function<void(int exitCode)> &callback);

private:
#ifdef Q_OS_WIN
    /** @brief 创建带 KILL_ON_JOB_CLOSE 策略的 Windows Job Object。 */
    bool createJob();

    /** @brief 关闭启动进程句柄。 */
    void closeProcessHandle();

    /** @brief 处理启动进程退出通知。 */
    void onProcessFinished();

    /** @brief Windows Job Object 句柄。 */
    void *jobHandle_ = nullptr;

    /** @brief 启动进程句柄。 */
    void *processHandle_ = nullptr;

    /** @brief Windows 进程退出通知器。 */
    QObject *processFinishedNotifier_ = nullptr;
#else
    /** @brief 非 Windows 平台上的普通 QProcess 回退实现。 */
    QProcess *process_ = nullptr;
#endif

    /** @brief 启动进程 PID。 */
    quint64 processId_ = 0;

    /** @brief 启动进程是否仍在运行。 */
    bool running_ = false;

    /** @brief 最近一次错误信息。 */
    QString lastErrorString_;

    /** @brief 启动进程结束回调。 */
    std::function<void(int exitCode)> finishedCallback_;

    Q_DISABLE_COPY(ProcessJob)
};

#endif // PROCESSJOB_H