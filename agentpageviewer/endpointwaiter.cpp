#include "endpointwaiter.h"

#include <QAbstractSocket>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

namespace {

constexpr int kEndpointProbeTimeoutMs = 300;
constexpr int kEndpointProbeIntervalMs = 100;

int defaultPortForUrl(const QUrl &url)
{
    if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0) {
        return 443;
    }
    return 80;
}

} // namespace

EndpointWaiter::EndpointWaiter(const QUrl &url, int timeoutMs, QObject *parent)
    : QObject(parent)
    , url_(url)
    , timeoutMs_(timeoutMs)
{
}

void EndpointWaiter::setProbeCallback(const std::function<void(qint64 elapsedMs)> &callback)
{
    onProbe_ = callback;
}

void EndpointWaiter::setFinishedCallback(const std::function<void(bool available)> &callback)
{
    onFinished_ = callback;
}

void EndpointWaiter::start()
{
    if (url_.host().isEmpty()) {
        QTimer::singleShot(0, this, [this]() {
            finish(true);
        });
        return;
    }

    elapsedTimer_.start();
    QTimer::singleShot(0, this, [this]() {
        startProbe();
    });
}

void EndpointWaiter::startProbe()
{
    if (finished_) {
        return;
    }

    const int currentProbeId = ++probeId_;
    const int port = url_.port(defaultPortForUrl(url_));
    if (port <= 0) {
        finish(true);
        return;
    }

    QTcpSocket *socket = new QTcpSocket(this);
    socket_ = socket;

    QObject::connect(socket, &QTcpSocket::connected,
                     this, [this, currentProbeId, socket]() {
                         completeProbe(currentProbeId, socket, true);
                     });

    QObject::connect(socket, &QTcpSocket::errorOccurred,
                     this, [this, currentProbeId, socket](QAbstractSocket::SocketError) {
                         completeProbe(currentProbeId, socket, false);
                     });

    QTimer::singleShot(kEndpointProbeTimeoutMs, socket, [this, currentProbeId, socket]() {
        completeProbe(currentProbeId, socket, false);
    });

    socket->connectToHost(url_.host(), port);
}

void EndpointWaiter::completeProbe(int probeId, QTcpSocket *socket, bool available)
{
    if (finished_ || probeId != probeId_ || socket != socket_) {
        return;
    }

    socket_->deleteLater();
    socket_ = nullptr;

    if (available) {
        finish(true);
        return;
    }

    const qint64 elapsedMs = elapsedTimer_.elapsed();
    if (onProbe_) {
        onProbe_(elapsedMs);
    }

    if (elapsedMs >= timeoutMs_) {
        finish(false);
        return;
    }

    QTimer::singleShot(kEndpointProbeIntervalMs, this, [this]() {
        startProbe();
    });
}

void EndpointWaiter::finish(bool available)
{
    if (finished_) {
        return;
    }

    finished_ = true;
    if (socket_) {
        socket_->deleteLater();
        socket_ = nullptr;
    }

    const std::function<void(bool)> finishedCallback = onFinished_;
    deleteLater();

    if (finishedCallback) {
        finishedCallback(available);
    }
}
