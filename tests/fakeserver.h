#pragma once

#include <QHash>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <functional>

// Just enough HTTP to stand in for Todoist's API in tests: one request per
// connection, answered by a handler that sees the method, path and query,
// headers, and body.
class FakeServer : public QTcpServer {
    Q_OBJECT

public:
    struct Request {
        QByteArray method, target, body;
        QHash<QByteArray, QByteArray> headers;
        QByteArray path() const { return target.split('?').first(); }
    };
    struct Response {
        int status = 200;
        QByteArray body;
        QByteArray extraHeaders = {};
    };
    using Handler = std::function<Response(const Request &)>;

    explicit FakeServer(Handler handler) : m_handler(std::move(handler)) {
        listen(QHostAddress::LocalHost);
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = nextPendingConnection())
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] { read(socket); });
        });
    }

    QString url() const { return QString("http://127.0.0.1:%1").arg(serverPort()); }
    QList<Request> requests;

private:
    void read(QTcpSocket *socket) {
        QByteArray &buffer = m_buffers[socket];
        buffer += socket->readAll();
        const int end = buffer.indexOf("\r\n\r\n");
        if (end < 0)
            return;
        Request request;
        const QList<QByteArray> lines = buffer.left(end).split('\n');
        const QList<QByteArray> first = lines.first().trimmed().split(' ');
        request.method = first.value(0);
        request.target = first.value(1);
        for (int i = 1; i < lines.size(); ++i) {
            const int colon = lines[i].indexOf(':');
            if (colon > 0)
                request.headers.insert(lines[i].left(colon).trimmed().toLower(), lines[i].mid(colon + 1).trimmed());
        }
        const int length = request.headers.value("content-length").toInt();
        if (buffer.size() < end + 4 + length)
            return;
        request.body = buffer.mid(end + 4, length);
        m_buffers.remove(socket);
        requests << request;

        const Response response = m_handler(request);
        QByteArray reply = "HTTP/1.1 " + QByteArray::number(response.status) + " X\r\n"
                           "Content-Type: application/json\r\n"
                           "Connection: close\r\n" + response.extraHeaders +
                           "Content-Length: " + QByteArray::number(response.body.size()) + "\r\n\r\n" + response.body;
        socket->write(reply);
        socket->disconnectFromHost();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }

    Handler m_handler;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};
