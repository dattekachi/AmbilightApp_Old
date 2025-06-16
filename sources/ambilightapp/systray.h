#pragma once

#ifndef PCH_ENABLED		
	#include <memory>
	#include <vector>
#endif

#include <base/AmbilightAppInstance.h>
#include <base/AmbilightAppManager.h>
#include <QSystemTrayIcon>
#include <QWidget>
#include <utils/Components.h>

class AmbilightAppDaemon;
class QMenu;
class QAction;
class QColorDialog;


class SysTray : public QObject
{
	Q_OBJECT

public:
	SysTray(AmbilightAppDaemon* ambilightappDaemon, quint16 webPort);
	~SysTray();

public slots:
	void showColorDialog();
	void setColor(const QColor& color);
	void settings();
	void openScreenCap();
	void setEffect(QString effect);
	void clearEfxColor();
	void runMusicLed();
	void restartApp();
	void menuQuit();
	void setAutorunState();
	void setBrightness(int brightness);
	void toggleLedState();
	void selectInstance();
	void cleanup();

private slots:
	void iconActivated(QSystemTrayIcon::ActivationReason reason);

	///
	/// @brief is called whenever a ambilightapp instance state changes
	///
	void signalInstanceStateChangedHandler(InstanceState state, quint8 instance, const QString& name);
	void signalSettingsChangedHandler(settings::type type, const QJsonDocument& data);

    void updateBrightnessMenu();

private:
	void createTrayIcon();

#ifdef _WIN32
	///
	/// @brief Checks whether Ambilight App should start at Windows system start.
	/// @return True on success, otherwise false
	///
	bool getCurrentAutorunState();
#endif

	QAction* _quitAction;
	QAction* _startAction;
	QAction* _stopAction;
	QAction* _colorAction;
	QAction* _settingsAction;
	QAction* _openscreencapAction;
	QAction* _clearAction;
	QAction* _runmusicledAction;
	QAction* _restartappAction;
	QAction* _autorunAction;
	QAction* _toggleLedAction;

	QSystemTrayIcon* _trayIcon;
	QMenu*           _trayIconMenu;
	QMenu*           _trayIconEfxMenu;
	QMenu*           _brightnessMenu;

	QColorDialog*		_colorDlg;

	std::weak_ptr<AmbilightAppManager> _instanceManager;
	quint16				_webPort;
	int					_selectedInstance;
	bool				currentState;
};
