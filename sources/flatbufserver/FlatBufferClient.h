#pragma once

// util
#include <utils/Logger.h>
#include <utils/Image.h>
#include <utils/ColorRgb.h>
#include <utils/Components.h>

// flatbuffer FBS
#include "ambilightapp_reply_generated.h"
#include "ambilightapp_request_generated.h"

class QTcpSocket;
class QLocalSocket;
class QTimer;

class FlatBufferClient : public QObject
{
	Q_OBJECT

public:
	explicit FlatBufferClient(QTcpSocket* socket, QLocalSocket* domain, int timeout, QObject* parent = nullptr);

signals:
	void SignalClearGlobalInput(int priority, bool forceClearAll);
	void SignalImageReceived(int priority, const Image<ColorRgb>& image, int timeout_ms, ambilightapp::Components origin, QString clientDescription);
	void SignalSetGlobalColor(int priority, const std::vector<ColorRgb>& ledColor, int timeout_ms, ambilightapp::Components origin, QString clientDescription);
	void SignalClientDisconnected(FlatBufferClient* client);

public slots:
	void forceClose();

private slots:
	void readyRead();
	void disconnected();

private:
	void handleMessage(const ambilightappnet::Request* req);
	void handleRegisterCommand(const ambilightappnet::Register* regReq);
	void handleColorCommand(const ambilightappnet::Color* colorReq);
	void handleImageCommand(const ambilightappnet::Image* image);
	void handleClearCommand(const ambilightappnet::Clear* clear);
	void handleNotImplemented();
	void sendMessage();
	void sendSuccessReply();
	void sendErrorReply(const std::string& error);

private:
	Logger*			_log;
	QTcpSocket*		_socket;
	QLocalSocket*	_domain;
	QString			_clientAddress;
	QTimer*			_timeoutTimer;
	int				_timeout;
	int				_priority;
	QString			_clientDescription;

	QByteArray		_receiveBuffer;

	flatbuffers::FlatBufferBuilder _builder;
};
