#include "agentstartupsplash.h"

#include <QColor>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QPixmap>
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
    , cancelButton_(new QPushButton(QStringLiteral("取消"), this))
{
    setWindowFlags(Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    resize(600, 300);
    setStyleSheet(QStringLiteral(R"(
        QWidget {
            color: #111827;
            font-family: "Microsoft YaHei", "Segoe UI";
            font-size: 13px;
        }
        QFrame#ShellFrame {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 16px;
        }
        QFrame#BrandPanel {
            background: #f9fafb;
            border-top-left-radius: 16px;
            border-bottom-left-radius: 16px;
            border-right: 1px solid #e5e7eb;
        }
        QLabel {
            border: none;
            background: transparent;
        }
        QLabel#IconLabel {
            background: #eff6ff;
            border: 1px solid #bfdbfe;
            border-radius: 16px;
            color: #2563eb;
            font-size: 26px;
            font-weight: 700;
        }
        QLabel#BrandTitleLabel {
            color: #111827;
            font-size: 18px;
            font-weight: 600;
        }
        QLabel#BrandSubtitleLabel {
            color: #64748b;
            font-size: 12px;
        }
        QLabel#TitleLabel {
            color: #111827;
            font-size: 23px;
            font-weight: 600;
        }
        QLabel#SubtitleLabel {
            color: #6b7280;
            font-size: 13px;
        }
        QLabel#BadgeLabel {
            background: #eff6ff;
            border: 1px solid #bfdbfe;
            border-radius: 11px;
            color: #1d4ed8;
            font-size: 12px;
            padding: 3px 10px;
        }
        QFrame#StatusCard {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 12px;
        }
        QLabel#StatusLabel {
            color: #111827;
            font-size: 16px;
            font-weight: 600;
        }
        QLabel#DetailLabel {
            color: #6b7280;
            font-size: 12px;
        }
        QLabel#ProgressTextLabel {
            color: #4b5563;
            font-size: 12px;
        }
        QFrame#TargetFrame {
            background: #f9fafb;
            border: 1px solid #e5e7eb;
            border-radius: 8px;
        }
        QLabel#TargetTagLabel {
            background: #eff6ff;
            border-radius: 6px;
            color: #1d4ed8;
            font-size: 11px;
            font-weight: 600;
            padding: 2px 6px;
        }
        QLabel#TargetUrlLabel {
            color: #4b5563;
            font-size: 12px;
        }
        QProgressBar {
            height: 9px;
            border: none;
            border-radius: 4px;
            background: #e5e7eb;
        }
        QProgressBar::chunk {
            border-radius: 4px;
            background: #2563eb;
        }
        QPushButton {
            min-width: 76px;
            min-height: 30px;
            padding: 0 14px;
            color: #374151;
            background: #ffffff;
            border: 1px solid #d1d5db;
            border-radius: 6px;
        }
        QPushButton:hover {
            color: #1d4ed8;
            background: #eff6ff;
            border-color: #93c5fd;
        }
        QPushButton:pressed {
            background: #dbeafe;
        }
        QPushButton:disabled {
            color: #9ca3af;
            background: #f3f4f6;
            border-color: #e5e7eb;
        }
    )"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);

    auto *shellFrame = new QFrame(this);
    shellFrame->setObjectName(QStringLiteral("ShellFrame"));

    auto *shadowEffect = new QGraphicsDropShadowEffect(shellFrame);
    shadowEffect->setBlurRadius(34.0);
    shadowEffect->setOffset(0.0, 8.0);
    shadowEffect->setColor(QColor(15, 23, 42, 46));
    shellFrame->setGraphicsEffect(shadowEffect);

    rootLayout->addWidget(shellFrame);

    auto *shellLayout = new QHBoxLayout(shellFrame);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    auto *brandPanel = new QFrame(shellFrame);
    brandPanel->setObjectName(QStringLiteral("BrandPanel"));
    brandPanel->setMinimumWidth(135);
    brandPanel->setMaximumWidth(190);

    auto *brandLayout = new QVBoxLayout(brandPanel);
    brandLayout->setContentsMargins(22, 24, 18, 22);
    brandLayout->setSpacing(8);

    auto *iconLabel = createLabel(QStringLiteral("IconLabel"), QString(), brandPanel);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedSize(48, 48);
    const QPixmap logoPixmap(QStringLiteral(":/resources/logo.ico"));
    iconLabel->setPixmap(logoPixmap.scaled(36, 36,
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));

    auto *brandTitleLabel = createLabel(QStringLiteral("BrandTitleLabel"), QStringLiteral("RFClaw"), brandPanel);
    auto *brandSubtitleLabel = createLabel(QStringLiteral("BrandSubtitleLabel"), QStringLiteral("智能体"), brandPanel);
    brandSubtitleLabel->setWordWrap(true);

    brandLayout->addWidget(iconLabel);
    brandLayout->addSpacing(4);
    brandLayout->addWidget(brandTitleLabel);
    brandLayout->addWidget(brandSubtitleLabel);
    brandLayout->addStretch();
    brandLayout->addWidget(createLabel(QStringLiteral("BrandSubtitleLabel"), QStringLiteral("本地运行"), brandPanel));

    auto *contentWidget = new QWidget(shellFrame);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(24, 22, 24, 20);
    contentLayout->setSpacing(10);

    auto *headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(12);

    auto *titleBlockLayout = new QVBoxLayout;
    titleBlockLayout->setContentsMargins(0, 0, 0, 0);
    titleBlockLayout->setSpacing(4);

    auto *titleLabel = createLabel(QStringLiteral("TitleLabel"), QStringLiteral("正在启动"), contentWidget);
    auto *subtitleLabel = createLabel(QStringLiteral("SubtitleLabel"), QStringLiteral("请稍候"), contentWidget);
    titleBlockLayout->addWidget(titleLabel);
    titleBlockLayout->addWidget(subtitleLabel);

    headerLayout->addLayout(titleBlockLayout, 1);

    auto *statusCard = new QFrame(contentWidget);
    statusCard->setObjectName(QStringLiteral("StatusCard"));
    auto *statusLayout = new QVBoxLayout(statusCard);
    statusLayout->setContentsMargins(16, 13, 16, 13);
    statusLayout->setSpacing(5);

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
    progressTextLabel_->setText(QStringLiteral("连接中"));
    progressTextLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    progressTextLabel_->setFixedWidth(72);

    progressRowLayout->addWidget(progressBar_, 1);
    progressRowLayout->addWidget(progressTextLabel_);

    auto *targetFrame = new QFrame(contentWidget);
    targetFrame->setObjectName(QStringLiteral("TargetFrame"));
    auto *targetLayout = new QHBoxLayout(targetFrame);
    targetLayout->setContentsMargins(10, 7, 10, 7);
    targetLayout->setSpacing(10);

    auto *targetTagLabel = createLabel(QStringLiteral("TargetTagLabel"), QStringLiteral("目标"), targetFrame);
    targetTagLabel->setAlignment(Qt::AlignCenter);

    auto *targetUrlLabel = createLabel(QStringLiteral("TargetUrlLabel"), compactUrlText(pageUrl), targetFrame);
    targetUrlLabel->setWordWrap(false);

    targetLayout->addWidget(targetTagLabel);
    targetLayout->addWidget(targetUrlLabel, 1);

    auto *actionLayout = new QHBoxLayout;
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->addStretch();
    actionLayout->addWidget(cancelButton_);

    connect(cancelButton_, &QPushButton::clicked, this, [this]() {
        cancelButton_->setEnabled(false);
        cancelButton_->setText(QStringLiteral("正在取消..."));
        emit cancelRequested();
    });

    contentLayout->addLayout(headerLayout);
    contentLayout->addWidget(statusCard);
    contentLayout->addLayout(progressRowLayout);
    contentLayout->addWidget(targetFrame);
    contentLayout->addLayout(actionLayout);

    shellLayout->addWidget(brandPanel, 2);
    shellLayout->addWidget(contentWidget, 5);

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
    progressTextLabel_->setText(QStringLiteral("连接中"));
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

