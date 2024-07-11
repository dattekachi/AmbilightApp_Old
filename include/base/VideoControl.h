#pragma once

#ifndef PCH_ENABLED
	#include <QTimer>

	#include <utils/Image.h>
	#include <utils/Logger.h>
	#include <utils/settings.h>
	#include <utils/Components.h>
#endif

class AmbilightAppInstance;

class VideoControl : public QObject
{
	Q_OBJECT

public:
	VideoControl(AmbilightAppInstance* ambilightapp);
	~VideoControl();

	quint8 getCapturePriority();
	bool isCEC();
	void setUsbCaptureEnable(bool enable);

private slots:
	void handleCompStateChangeRequest(ambilightapp::Components component, bool enable);
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);
	void handleUsbImage();
	void handleIncomingUsbImage(const QString& name, const Image<ColorRgb>& image);
	void setUsbInactive();

private:
	AmbilightAppInstance* _ambilightapp;

	bool	_usbCaptEnabled;
	bool	_alive;
	quint8	_usbCaptPrio;
	QString	_usbCaptName;
	QTimer*	_usbInactiveTimer;
	bool	_isCEC;

	struct {
		QMutex mutex;
		QString name;
		Image<ColorRgb> frame;
	} incoming;

	static bool	_stream;
};
