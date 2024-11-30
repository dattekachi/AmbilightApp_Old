#ifndef PCH_ENABLED
	#include <QColor>
	#include <QSettings>
	#include <list>
#endif

#ifndef _WIN32
	#include <unistd.h>
#endif


#include <QPixmap>
#include <QWindow>
#include <QApplication>
#include <QDesktopServices>
#include <QMenu>
#include <QWidget>
#include <QColorDialog>
#include <QCloseEvent>
#include <QSettings>
#include <QProcess>
#include <QStringList>
#include <QActionGroup>

#include <AmbilightappConfig.h>

#include <utils/ColorRgb.h>
#include <effectengine/EffectDefinition.h>
#include <webserver/WebServer.h>
#include <utils/Logger.h>

#include "AmbilightAppDaemon.h"
#include "systray.h"

SysTray::SysTray(AmbilightAppDaemon* ambilightappDaemon, quint16 webPort)
	: QObject(),
	_quitAction(nullptr),
	_startAction(nullptr),
	_stopAction(nullptr),
	_colorAction(nullptr),
	_settingsAction(nullptr),
	_openscreencapAction(nullptr),
	_clearAction(nullptr),
	_runmusicledAction(nullptr),
	_restartappAction(nullptr),
	_autorunAction(nullptr),
	_trayIcon(nullptr),
	_trayIconMenu(nullptr),
	_trayIconEfxMenu(nullptr),
	_colorDlg(nullptr),
	_selectedInstance(-1),
	_webPort(webPort)
{
	Q_INIT_RESOURCE(resources);

	std::shared_ptr<AmbilightAppManager> instanceManager;
	ambilightappDaemon->getInstanceManager(instanceManager);
	_instanceManager = instanceManager;
	connect(instanceManager.get(), &AmbilightAppManager::SignalInstanceStateChanged, this, &SysTray::signalInstanceStateChangedHandler);
	connect(instanceManager.get(), &AmbilightAppManager::SignalSettingsChanged, this, &SysTray::signalSettingsChangedHandler);
	_toggleLedAction = nullptr;
	currentState = true;
}

SysTray::~SysTray()
{
	printf("Releasing SysTray\n");

	if (_trayIconEfxMenu != nullptr)
	{
		for (QAction*& effect : _trayIconEfxMenu->actions())
			delete effect;
		_trayIconEfxMenu->clear();
	}

	if (_trayIconMenu != nullptr)
		_trayIconMenu->clear();

	delete _quitAction;
	delete _startAction;
	delete _stopAction;
	delete _colorAction;
	delete _settingsAction;
	delete _openscreencapAction;
	delete _clearAction;
	delete _runmusicledAction;
	delete _restartappAction;
	delete _brightnessMenu;
	delete _autorunAction;
	delete _trayIcon;
	delete _trayIconEfxMenu;
	delete _trayIconMenu;
	delete _colorDlg;
	delete _toggleLedAction;
}

void SysTray::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
	switch (reason)
	{
#ifdef _WIN32
		case QSystemTrayIcon::Context:
			getCurrentAutorunState();
			break;
#endif
		case QSystemTrayIcon::Trigger:
			break;
		case QSystemTrayIcon::DoubleClick:
			settings();
			break;
		case QSystemTrayIcon::MiddleClick:
			break;
		default:;
	}
}

void SysTray::createTrayIcon()
{

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))

#else
	QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

	_trayIconMenu = new QMenu();
	_trayIcon = new QSystemTrayIcon();
	_trayIcon->setContextMenu(_trayIconMenu);

    // if (_trayIcon == nullptr)
    // {
    //     _trayIconMenu = new QMenu();
    //     _trayIcon = new QSystemTrayIcon();
    //     _trayIcon->setContextMenu(_trayIconMenu);
    //     connect(_trayIcon, &QSystemTrayIcon::activated, this, &SysTray::iconActivated);
    //     _trayIcon->setIcon(QIcon(":/ambilightapp-icon-32px.png"));
    //     _trayIcon->show();
    // }
	// _trayIconMenu->clear();

	_brightnessMenu = new QMenu(tr("&Độ sáng LED"));
	_brightnessMenu->setIcon(QPixmap(":/brightness.svg")); 

	for(int brightness = 100; brightness >= 10; brightness -= 10) {
		QAction* brightnessAction = new QAction(QString::number(brightness) + "%", this);
		connect(brightnessAction, &QAction::triggered, this, [this, brightness]() {
			this->setBrightness(brightness);
		});
		_brightnessMenu->addAction(brightnessAction);
	}

	_quitAction = new QAction(tr("&Thoát"));
	_quitAction->setIcon(QPixmap(":/quit.svg"));
	connect(_quitAction, &QAction::triggered, this, &SysTray::menuQuit);

	_colorAction = new QAction(tr("&Chọn màu sắc"));
	_colorAction->setIcon(QPixmap(":/color.svg"));
	connect(_colorAction, &QAction::triggered, this, &SysTray::showColorDialog);

	_settingsAction = new QAction(tr("&Cài đặt"));
	_settingsAction->setIcon(QPixmap(":/settings.svg"));
	connect(_settingsAction, &QAction::triggered, this, &SysTray::settings);

#ifdef _WIN32
	_openscreencapAction = new QAction(tr("&Trình quét màu màn hình"));
	_openscreencapAction->setIcon(QPixmap(":/settings.svg"));
	connect(_openscreencapAction, &QAction::triggered, this, &SysTray::openScreenCap);
#endif

	_clearAction = new QAction(tr("&Theo màu màn hình"));
	_clearAction->setIcon(QPixmap(":/clear.svg"));
	connect(_clearAction, &QAction::triggered, this, &SysTray::clearEfxColor);

	_runmusicledAction = new QAction(tr("&Mở nháy theo nhạc"));
	_runmusicledAction->setIcon(QPixmap(":/music.svg"));
	connect(_runmusicledAction, &QAction::triggered, this, &SysTray::runMusicLed);

	_restartappAction = new QAction(tr("&Khởi động lại"));
	_restartappAction->setIcon(QPixmap(":/restart.svg"));
	connect(_restartappAction, &QAction::triggered, this, &SysTray::restartApp);

	_toggleLedAction = new QAction(tr("&Tắt LED"));
    _toggleLedAction->setIcon(QPixmap(":/toggle.svg")); 
    connect(_toggleLedAction, &QAction::triggered, this, &SysTray::toggleLedState);

    // QMenu* instancesMenu = new QMenu(tr("&Chọn thiết bị LED"), _trayIconMenu);
    // instancesMenu->setIcon(QIcon(":/instance.svg")); 

    // QActionGroup* instanceGroup = new QActionGroup(this);
    // instanceGroup->setExclusive(true);

    // QAction* allAction = new QAction("Tất cả", instanceGroup);
    // allAction->setCheckable(true);
    // allAction->setChecked(_selectedInstance == -1);
    // connect(allAction, &QAction::triggered, this, [this]() {
    //     _selectedInstance = -1;
    //     selectInstance();
    // });
    // instancesMenu->addAction(allAction);
    
    // instancesMenu->addSeparator();

    // std::shared_ptr<AmbilightAppManager> instanceManager = _instanceManager.lock();
    // if (instanceManager != nullptr)
    // {
    //     QVector<QVariantMap> instanceData = instanceManager->getInstanceData();
        
    //     for (const QVariantMap& instance : instanceData)
    //     {
    //         if (instance["enabled"].toBool())
    //         {
    //             int index = instance["instance"].toInt();
    //             QString name = instance["friendly_name"].toString();
                
    //             QAction* instanceAction = new QAction(name, instanceGroup);
    //             instanceAction->setCheckable(true);
    //             instanceAction->setChecked(index == _selectedInstance);
    //             connect(instanceAction, &QAction::triggered, this, [this, index]() {
    //                 _selectedInstance = index;
    //                 selectInstance();
    //             });
    //             instancesMenu->addAction(instanceAction);
    //         }
    //     }
    // }

	std::shared_ptr<AmbilightAppManager> instanceManager = _instanceManager.lock();
	std::list<EffectDefinition> efxs;

	if (instanceManager)
		efxs = instanceManager->getEffects();

	_trayIconEfxMenu = new QMenu();
	_trayIconEfxMenu->setIcon(QPixmap(":/effects.svg"));
	_trayIconEfxMenu->setTitle(tr("Chọn hiệu ứng"));

	for (const EffectDefinition& efx : efxs)
	{
		QString effectName = efx.name;
		QAction* efxAction = new QAction(effectName);
		connect(efxAction, &QAction::triggered, this, [this, effectName]() {
			this->setEffect(effectName);
			});
		_trayIconEfxMenu->addAction(efxAction);
	}

	_trayIconMenu->addAction(_colorAction);
	_trayIconMenu->addMenu(_trayIconEfxMenu);
	_trayIconMenu->addAction(_clearAction);
	_trayIconMenu->addAction(_runmusicledAction);
	_trayIconMenu->addSeparator();
	_trayIconMenu->addMenu(_brightnessMenu);
	_trayIconMenu->addAction(_toggleLedAction);
	// _trayIconMenu->addMenu(instancesMenu);
	_trayIconMenu->addSeparator();
	_trayIconMenu->addAction(_settingsAction);
#ifdef _WIN32
	_trayIconMenu->addAction(_openscreencapAction);
	_trayIconMenu->addSeparator();
	_autorunAction = new QAction(tr("&Tắt tự khởi động"));
	_autorunAction->setIcon(QPixmap(":/autorun.svg"));
	connect(_autorunAction, &QAction::triggered, this, &SysTray::setAutorunState);
	_trayIconMenu->addAction(_autorunAction);
#endif
	_trayIconMenu->addSeparator();
	_trayIconMenu->addAction(_restartappAction);
	_trayIconMenu->addAction(_quitAction);
}

void SysTray::selectInstance()
{
    createTrayIcon();
}

#ifdef _WIN32
bool SysTray::getCurrentAutorunState()
{
	QSettings reg("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
	if (reg.value("Ambilightapp", 0).toString() == qApp->applicationFilePath().replace('/', '\\'))
	{
		_autorunAction->setText(tr("&Tắt tự khởi động"));
		return true;
	}

	_autorunAction->setText(tr("&Bật tự khởi động"));
	return false;
}
#endif

void SysTray::setAutorunState()
{
#ifdef _WIN32
	bool currentState = getCurrentAutorunState();
	QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
	(currentState)
		? reg.remove("Ambilightapp")
		: (reg.setValue("Ambilightapp", qApp->applicationFilePath().replace('/', '\\')), reg.remove("MusicLedStudio"));
#endif
}

void SysTray::setColor(const QColor& color)
{
	ColorRgb rgbColor{ (uint8_t)color.red(), (uint8_t)color.green(), (uint8_t)color.blue() };

	std::shared_ptr<AmbilightAppManager> instanceManager = _instanceManager.lock();
	if (instanceManager)
		instanceManager->setInstanceColor(_selectedInstance, 1, rgbColor, 0);
}

void SysTray::showColorDialog()
{
	if (_colorDlg == nullptr)
	{
		_colorDlg = new QColorDialog();
		_colorDlg->setOptions(QColorDialog::NoButtons);
		connect(_colorDlg, SIGNAL(currentColorChanged(const QColor&)), this, SLOT(setColor(const QColor&)));
	}

	if (_colorDlg->isVisible())
	{
		_colorDlg->hide();
	}
	else
	{
		_colorDlg->show();
		_colorDlg->raise();
		_colorDlg->activateWindow();
	}
}

void SysTray::settings()
{
#ifndef _WIN32
	// Hide error messages when opening webbrowser

	int out_pipe[2];
	int saved_stdout;
	int saved_stderr;

	// saving stdout and stderr file descriptor
	saved_stdout = ::dup(STDOUT_FILENO);
	saved_stderr = ::dup(STDERR_FILENO);

	if (::pipe(out_pipe) == 0)
	{
		// redirecting stdout to pipe
		::dup2(out_pipe[1], STDOUT_FILENO);
		::close(out_pipe[1]);
		// redirecting stderr to stdout
		::dup2(STDOUT_FILENO, STDERR_FILENO);
	}
#endif

	QDesktopServices::openUrl(QUrl("http://localhost:" + QString::number(_webPort) + "/", QUrl::TolerantMode));

#ifndef _WIN32
	// restoring stdout
	::dup2(saved_stdout, STDOUT_FILENO);
	// restoring stderr
	::dup2(saved_stderr, STDERR_FILENO);
#endif
}

void SysTray::setEffect(QString effect)
{
	std::shared_ptr<AmbilightAppManager> instanceManager = _instanceManager.lock();
	if (instanceManager)
		instanceManager->setInstanceEffect(_selectedInstance, effect, 1);
}

void SysTray::clearEfxColor()
{
	std::shared_ptr<AmbilightAppManager> instanceManager = _instanceManager.lock();
	if (instanceManager)
		instanceManager->clearInstancePriority(_selectedInstance, 1);
}

void SysTray::openScreenCap()
{
	QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "HyperionScreenCap.exe");
	QProcess::startDetached("C:\\Program Files\\Ambilight App\\Hyperion Screen Capture\\HyperionScreenCap.exe", QStringList() << "-show");
}

void SysTray::runMusicLed()
{
#ifdef _WIN32
	QProcess::startDetached("C:\\Program Files\\Ambilight App\\MusicLedStudio\\MusicLedStudio.exe");
#else
	QProcess process;
	process.start("open", QStringList() << "/Applications/MusicLedStudio.app");
	process.waitForFinished();
#endif
}

void SysTray::restartApp()
{
#ifdef _WIN32
	QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "HyperionScreenCap.exe");
	QProcess::startDetached("C:\\Program Files\\Ambilight App\\Hyperion Screen Capture\\HyperionScreenCap.exe");
#endif

	auto arguments = QCoreApplication::arguments();
	if (!arguments.contains("--wait-ambilightapp"))
		arguments << "--wait-ambilightapp";

	QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments);
	QCoreApplication::exit(12);
}

void SysTray::setBrightness(int brightness) 
{
    std::shared_ptr<AmbilightAppManager> instanceManager = _instanceManager.lock();
    if (instanceManager)
    {
        instanceManager->setInstanceBrightness(_selectedInstance, brightness);
    }
}

void SysTray::toggleLedState()
{
    std::shared_ptr<AmbilightAppManager> instanceManager = _instanceManager.lock();
    if (instanceManager)
    {
        currentState = !currentState;
        instanceManager->setInstanceComponentState(_selectedInstance, ambilightapp::COMP_LEDDEVICE, currentState);
        _toggleLedAction->setText(currentState ? tr("&Tắt LED") : tr("&Bật LED"));
    }
}

void SysTray::menuQuit()
{
#ifdef _WIN32
	QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "HyperionScreenCap.exe");
#endif
	QApplication::quit();
}

void SysTray::signalInstanceStateChangedHandler(InstanceState state, quint8 instance, const QString& name)
{
	switch (state) {
		case InstanceState::START:
			if (instance == 0)
			{
				std::shared_ptr<AmbilightAppManager> instanceManager = _instanceManager.lock();

				if (instanceManager == nullptr)
					return;

				createTrayIcon();

				connect(_trayIcon, &QSystemTrayIcon::activated, this, &SysTray::iconActivated);				

				_trayIcon->setIcon(QIcon(":/ambilightapp-icon-32px.png"));
				_trayIcon->show();		
			}
			break;
		
		// case InstanceState::STOP:
        //     if (instance == _selectedInstance)
        //     {
        //         _selectedInstance = -1;
        //     }
        //     break;

		default:
			break;
	}
	// createTrayIcon();
}

void SysTray::signalSettingsChangedHandler(settings::type type, const QJsonDocument& data)
{
	if (type == settings::type::WEBSERVER)
	{
		_webPort = data.object()["port"].toInt();
	}
}
