// project includes
#include "JsonClientConnection.h"
#include <api/AmbilightAPI.h>

// qt inc
#include <QTcpSocket>
#include <QHostAddress>

JsonClientConnection::JsonClientConnection(QTcpSocket* socket, bool localConnection)
	: QObject()
	, _socket(socket)
	, _receiveBuffer()
	, _log(Logger::getInstance("JSONCLIENTCONNECTION"))
{
	connect(_socket, &QTcpSocket::disconnected, this, &JsonClientConnection::disconnected);
	connect(_socket, &QTcpSocket::readyRead, this, &JsonClientConnection::readRequest);
	// create a new instance of JsonAPI
	_ambilightAPI = new AmbilightAPI(socket->peerAddress().toString(), _log, localConnection, this);
	// get the callback messages from JsonAPI and send it to the client
	connect(_ambilightAPI, &AmbilightAPI::SignalCallbackJsonMessage, this, &JsonClientConnection::sendMessage);
	connect(_ambilightAPI, &AmbilightAPI::SignalPerformClientDisconnection, this, [&]() { _socket->close(); });

	_ambilightAPI->initialize();
}

void JsonClientConnection::readRequest()
{
	_receiveBuffer += _socket->readAll();
	// raw socket data, handling as usual
	int bytes = _receiveBuffer.indexOf('\n') + 1;
	while (bytes > 0)
	{
		// create message string
		QString message(QByteArray(_receiveBuffer.data(), bytes));

		// remove message data from buffer
		_receiveBuffer = _receiveBuffer.mid(bytes);

		// handle message
		_ambilightAPI->handleMessage(message);

		// try too look up '\n' again
		bytes = _receiveBuffer.indexOf('\n') + 1;
	}
}

qint64 JsonClientConnection::sendMessage(QJsonObject message)
{
	QJsonDocument writer(message);
	QByteArray data = writer.toJson(QJsonDocument::Compact) + "\n";

	if (!_socket || (_socket->state() != QAbstractSocket::ConnectedState))
		return 0;
	else
		return _socket->write(data.data(), data.size());
}

void JsonClientConnection::disconnected()
{
	emit SignalClientConnectionClosed(this);
}
