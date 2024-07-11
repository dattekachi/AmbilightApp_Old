#pragma once

#ifndef PCH_ENABLED
	#include <QTimer>

	#include <utils/Image.h>
	#include <utils/Logger.h>
	#include <utils/settings.h>
	#include <utils/Components.h>
#endif

class AmbilightAppInstance;

class SystemControl : public QObject
{
	Q_OBJECT

public:
	SystemControl(AmbilightAppInstance* ambilightapp);
	~SystemControl();

	quint8 getCapturePriority();

	bool isCEC();

	void setSysCaptureEnable(bool enable);

private slots:
	void handleCompStateChangeRequest(ambilightapp::Components component, bool enable);
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);
	void handleSysImage(const QString& name, const Image<ColorRgb>& image);
	void setSysInactive();

private:
	AmbilightAppInstance* _ambilightapp;

	bool	_sysCaptEnabled;
	bool	_alive;
	quint8	_sysCaptPrio;
	QString	_sysCaptName;
	QTimer*	_sysInactiveTimer;
	bool	_isCEC;
};
