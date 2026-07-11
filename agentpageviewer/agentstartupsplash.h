#ifndef AGENTSTARTUPSPLASH_H
#define AGENTSTARTUPSPLASH_H

#include <QString>
#include <QWidget>

class QLabel;
class QProgressBar;
class QUrl;

/**
 * @brief AgentPageViewer 启动等待画面。
 *
 * 在 OpenClaw 服务启动和 agent 页面加载期间显示，避免用户看到空白窗口或误以为程序卡死。
 */
class AgentStartupSplash : public QWidget
{
public:
    /**
     * @brief 创建启动等待画面。
     * @param pageUrl 即将打开的 agent 页面地址，用于显示目标页面信息。
     */
    explicit AgentStartupSplash(const QUrl &pageUrl);

    /**
     * @brief 更新主状态文字和详细说明。
     * @param status 主状态文字，例如“正在启动 OpenClaw 服务...”。
     * @param detail 详细说明；传入空值时清空，省略时保留当前说明。
     */
    void setStatus(const QString &status, const QString &detail = QString());

    /**
     * @brief 设置为忙碌进度条。
     *
     * 适合无法确定真实进度的阶段，例如等待服务端口就绪。
     */
    void setBusyProgress();

    /**
     * @brief 设置明确进度值。
     * @param value 0 到 100 的页面加载进度。
     */
    void setProgress(int value);

private:
    /** @brief 将启动画面移动到主屏幕中心。 */
    void centerOnScreen();

private:
    /** @brief 显示当前启动阶段的主状态文字。 */
    QLabel *statusLabel_ = nullptr;

    /** @brief 显示当前阶段的补充说明。 */
    QLabel *detailLabel_ = nullptr;

    /** @brief 显示进度条右侧的阶段或百分比。 */
    QLabel *progressTextLabel_ = nullptr;

    /** @brief 显示忙碌状态或页面加载进度。 */
    QProgressBar *progressBar_ = nullptr;
};

#endif // AGENTSTARTUPSPLASH_H
