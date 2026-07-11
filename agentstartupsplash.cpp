#include "agentstartupsplash.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QScreen>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QString compactUrlText(const QUrl &url)
{
    const QString text = url.toString(QUrl::RemovePassword);
    constexpr int kMaxLength = 86;
    if (text.size() <= kMaxLength) {
        return text;
    }

    return text.left(58) + QStringLiteral(" ... ") + text.right(22);
}

QLabel *createLabel(const QString &objectName, const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(objectName);
    return label;
}

} // namespace

AgentStartupSplash::AgentStartupSplash(const QUrl &pageUrl)
    : QWidget(nullptr)
    , statusLabel_(new QLabel(this))
    , detailLabel_(new QLabel(this))
    , progressTextLabel_(new QLabel(this))
    , progressBar_(new QProgressBar(this))
{
    setWindowFlags(Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(640, 340);
    setStyleSheet(QStringLiteral(R"(
        QWidget {
            color: #1f2937;
            font-family: "Microsoft YaHei", "Segoe UI";
            font-size: 13px;
        }
        QFrame#ShellFrame {
            background: #ffffff;
            border: 1px solid #dbe3ef;
            border-radius: 14px;
        }
        QFrame#BrandPanel {
            background: #0f172a;
            border-top-left-radius: 14px;
            border-bottom-left-radius: 14px;
        }
        QLabel {
            border: none;
            background: transparent;
        }
        QLabel#IconLabel {
            background: #2563eb;
            border-radius: 14px;
            color: #ffffff;
            font-size: 25px;
            font-weight: 700;
        }
        QLabel#BrandTitleLabel {
            color: #f8fafc;
            font-size: 17px;
            font-weight: 600;
        }
        QLabel#BrandSubtitleLabel {
            color: #cbd5e1;
            font-size: 12px;
        }
        QLabel#TitleLabel {
            color: #0f172a;
            font-size: 22px;
            font-weight: 600;
        }
        QLabel#SubtitleLabel {
            color: #64748b;
            font-size: 13px;
        }
        QLabel#BadgeLabel {
            background: #e0f2fe;
            border: 1px solid #bae6fd;
            border-radius: 11px;
            color: #0369a1;
            font-size: 12px;
            padding: 3px 10px;
        }
        QFrame#StatusCard {
            background: #f8fafc;
            border: 1px solid #e2e8f0;
            border-radius: 10px;
        }
        QLabel#StatusLabel {
            color: #111827;
            font-size: 16px;
            font-weight: 600;
        }
        QLabel#DetailLabel {
            color: #64748b;
            font-size: 12px;
        }
        QLabel#ProgressTextLabel {
            color: #475569;
            font-size: 12px;
        }
        QFrame#TargetFrame {
            background: #f1f5f9;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
        }
        QLabel#TargetTagLabel {
            background: #dbeafe;
            border-radius: 6px;
            color: #1d4ed8;
            font-size: 11px;
            font-weight: 600;
            padding: 2px 6px;
        }
        QLabel#TargetUrlLabel {
            color: #475569;
            font-size: 12px;
        }
        QProgressBar {
            height: 9px;
            border: none;
            border-radius: 4px;
            background: #e2e8f0;
        }
        QProgressBar::chunk {
            border-radius: 4px;
            background: #2563eb;
        }
    )"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 18, 18, 18);

    auto *shellFrame = new QFrame(this);
    shellFrame->setObjectName(QStringLiteral("ShellFrame"));
    auto *shadowEffect = new QGraphicsDropShadowEffect(shellFrame);
    shadowEffect->setBlurRadius(30);
    shadowEffect->setOffset(0, 8);
    shadowEffect->setColor(QColor(15, 23, 42, 48));
    shellFrame->setGraphicsEffect(shadowEffect);
    rootLayout->addWidget(shellFrame);

    auto *shellLayout = new QHBoxLayout(shellFrame);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    auto *brandPanel = new QFrame(shellFrame);
    brandPanel->setObjectName(QStringLiteral("BrandPanel"));
    brandPanel->setFixedWidth(180);

    auto *brandLayout = new QVBoxLayout(brandPanel);
    brandLayout->setContentsMargins(24, 28, 22, 26);
    brandLayout->setSpacing(12);

    auto *iconLabel = createLabel(QStringLiteral("IconLabel"), QStringLiteral("A"), brandPanel);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedSize(52, 52);

    auto *brandTitleLabel = createLabel(QStringLiteral("BrandTitleLabel"), QStringLiteral("Agent Viewer"), brandPanel);
    auto *brandSubtitleLabel = createLabel(QStringLiteral("BrandSubtitleLabel"), QStringLiteral("OpenClaw Gateway"), brandPanel);
    brandSubtitleLabel->setWordWrap(true);

    brandLayout->addWidget(iconLabel);
    brandLayout->addSpacing(10);
    brandLayout->addWidget(brandTitleLabel);
    brandLayout->addWidget(brandSubtitleLabel);
    brandLayout->addStretch();
    brandLayout->addWidget(createLabel(QStringLiteral("BrandSubtitleLabel"), QStringLiteral("正在建立本地服务连接"), brandPanel));

    auto *contentWidget = new QWidget(shellFrame);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(30, 28, 30, 26);
    contentLayout->setSpacing(14);

    auto *headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(12);

    auto *titleBlockLayout = new QVBoxLayout;
    titleBlockLayout->setContentsMargins(0, 0, 0, 0);
    titleBlockLayout->setSpacing(4);

    auto *titleLabel = createLabel(QStringLiteral("TitleLabel"), QStringLiteral("正在准备 agent 页面"), contentWidget);
    auto *subtitleLabel = createLabel(QStringLiteral("SubtitleLabel"), QStringLiteral("请稍候，服务启动完成后会自动进入页面"), contentWidget);
    titleBlockLayout->addWidget(titleLabel);
    titleBlockLayout->addWidget(subtitleLabel);

    auto *badgeLabel = createLabel(QStringLiteral("BadgeLabel"), QStringLiteral("启动中"), contentWidget);
    badgeLabel->setAlignment(Qt::AlignCenter);
    badgeLabel->setFixedHeight(24);

    headerLayout->addLayout(titleBlockLayout, 1);
    headerLayout->addWidget(badgeLabel, 0, Qt::AlignTop);

    auto *statusCard = new QFrame(contentWidget);
    statusCard->setObjectName(QStringLiteral("StatusCard"));
    auto *statusLayout = new QVBoxLayout(statusCard);
    statusLayout->setContentsMargins(18, 16, 18, 16);
    statusLayout->setSpacing(7);

    statusLabel_->setObjectName(QStringLiteral("StatusLabel"));
    detailLabel_->setObjectName(QStringLiteral("DetailLabel"));
    detailLabel_->setWordWrap(true);
    statusLayout->addWidget(statusLabel_);
    statusLayout->addWidget(detailLabel_);

    auto *progressRowLayout = new QHBoxLayout;
    progressRowLayout->setContentsMargins(0, 0, 0, 0);
    progressRowLayout->setSpacing(10);

    progressBar_->setRange(0, 0);
    progressBar_->setTextVisible(false);
    progressTextLabel_->setObjectName(QStringLiteral("ProgressTextLabel"));
    progressTextLabel_->setText(QStringLiteral("准备中"));
    progressTextLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    progressTextLabel_->setFixedWidth(68);

    progressRowLayout->addWidget(progressBar_, 1);
    progressRowLayout->addWidget(progressTextLabel_);

    auto *targetFrame = new QFrame(contentWidget);
    targetFrame->setObjectName(QStringLiteral("TargetFrame"));
    auto *targetLayout = new QHBoxLayout(targetFrame);
    targetLayout->setContentsMargins(12, 9, 12, 9);
    targetLayout->setSpacing(10);

    auto *targetTagLabel = createLabel(QStringLiteral("TargetTagLabel"), QStringLiteral("URL"), targetFrame);
    targetTagLabel->setAlignment(Qt::AlignCenter);

    auto *targetUrlLabel = createLabel(QStringLiteral("TargetUrlLabel"), compactUrlText(pageUrl), targetFrame);
    targetUrlLabel->setWordWrap(false);

    targetLayout->addWidget(targetTagLabel);
    targetLayout->addWidget(targetUrlLabel, 1);

    contentLayout->addLayout(headerLayout);
    contentLayout->addSpacing(2);
    contentLayout->addWidget(statusCard);
    contentLayout->addLayout(progressRowLayout);
    contentLayout->addWidget(targetFrame);
    contentLayout->addStretch();

    shellLayout->addWidget(brandPanel);
    shellLayout->addWidget(contentWidget, 1);

    centerOnScreen();
    setStatus(QStringLiteral("正在初始化..."));
}

void AgentStartupSplash::setStatus(const QString &status, const QString &detail)
{
    statusLabel_->setText(status);
    if (!detail.isNull()) {
        detailLabel_->setText(detail);
    }
    repaint();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

void AgentStartupSplash::setBusyProgress()
{
    progressBar_->setRange(0, 0);
    progressTextLabel_->setText(QStringLiteral("准备中"));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

void AgentStartupSplash::setProgress(int value)
{
    if (progressBar_->maximum() != 100) {
        progressBar_->setRange(0, 100);
    }
    progressBar_->setValue(value);
    progressTextLabel_->setText(QStringLiteral("%1%").arg(value));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

void AgentStartupSplash::centerOnScreen()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }

    const QRect availableGeometry = screen->availableGeometry();
    move(availableGeometry.center().x() - width() / 2,
         availableGeometry.center().y() - height() / 2);
}

