#pragma once

#ifndef PCH_ENABLED
	#include <QJsonObject>

	#include <utils/Logger.h>
#endif

class QtHttpServer;
class QtHttpRequest;
class QtHttpClientWrapper;
class AmbilightAPI;
class AmbilightAppManager;

class WebJsonRpc : public QObject {
	Q_OBJECT

public:
	WebJsonRpc(QtHttpRequest* request, QtHttpServer* server, bool localConnection, QtHttpClientWrapper* parent);

	void handleMessage(QtHttpRequest* request, QString query = "");

private:
	QtHttpServer* _server;
	QtHttpClientWrapper* _wrapper;
	Logger*		_log;
	AmbilightAPI*	_ambilightAPI;

	bool _stopHandle = false;
	bool _unlocked = false;

private slots:
	void handleCallback(QJsonObject obj);
};
