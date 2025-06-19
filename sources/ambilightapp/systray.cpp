#ifndef PCH_ENABLED
	#include <QColor>
	#include <QSettings>
	#include <list>
	#include <QJsonObject>
	#include <QJsonDocument>
	#include <utils/settings.h>
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
#include <QMutex>

#include <AmbilightappConfig.h>

#include <utils/ColorRgb.h>
#include <effectengine/EffectDefinition.h>
#include <webserver/WebServer.h>
#include <utils/Logger.h>

#include "AmbilightAppDaemon.h"
#include "systray.h"

QMutex SysTray::s_colorAccessMutex;
QMap<QString, QColor> SysTray::s_lastSelectedColors;
QSettings SysTray::s_settings("HKEY_CURRENT_USER\\Software\\AmbilightApp", QSettings::NativeFormat);
QMutex SysTray::s_effectAccessMutex;
QMap<QString, QString> SysTray::s_lastSelectedEffects;

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
	_toggleLedAction(nullptr),
	_brightnessMenu(nullptr),
	_trayIcon(nullptr),
	_trayIconMenu(nullptr),
	_trayIconEfxMenu(nullptr),
	_colorDlg(nullptr),
	_selectedInstance(-1),
	_webPort(webPort),
	currentState(true)
{
	Q_INIT_RESOURCE(resources);

	// Đọc màu đã lưu khi khởi tạo instance đầu tiên
	static bool isFirstInstance = true;
	if (isFirstInstance)
	{
		QMutexLocker locker(&s_colorAccessMutex);
		s_settings.beginGroup("Instances");
		QStringList instances = s_settings.childGroups();
		for (const QString& instance : instances)
		{
			s_settings.beginGroup(instance);
			QString colorStr = s_settings.value("Color").toString();
			if (!colorStr.isEmpty())
				s_lastSelectedColors[instance] = QColor(colorStr);
			s_settings.endGroup();
		}
		s_settings.endGroup();
		isFirstInstance = false;
	}

	// Đọc effect đã lưu khi khởi tạo instance đầu tiên
	static bool isFirstEffectInstance = true;
	if (isFirstEffectInstance)
	{
		QMutexLocker locker(&s_effectAccessMutex);
		s_settings.beginGroup("Instances");
		QStringList instances = s_settings.childGroups();
		for (const QString& instance : instances)
		{
			s_settings.beginGroup(instance);
			QString effectStr = s_settings.value("Effect").toString();
			if (!effectStr.isEmpty())
				s_lastSelectedEffects[instance] = effectStr;
			s_settings.endGroup();
		}
		s_settings.endGroup();
		isFirstEffectInstance = false;
	}

	std::shared_ptr<AmbilightAppManager> instanceManager;
	ambilightappDaemon->getInstanceManager(instanceManager);
	_instanceManager = instanceManager;
	connect(instanceManager.get(), &AmbilightAppManager::SignalInstanceStateChanged, this, &SysTray::signalInstanceStateChangedHandler);
	connect(instanceManager.get(), &AmbilightAppManager::SignalSettingsChanged, this, &SysTray::signalSettingsChangedHandler);
}

SysTray::~SysTray()
{
	printf("Releasing SysTray\n");

	cleanup();

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

void SysTray::cleanup()
{
    if (_trayIcon != nullptr)
    {
        _trayIcon->hide();
        delete _trayIcon;
        _trayIcon = nullptr;
    }
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

	if (_trayIconEfxMenu != nullptr) {
		delete _trayIconEfxMenu;
		_trayIconEfxMenu = nullptr;
	}
	if (_trayIconMenu != nullptr) {
		delete _trayIconMenu;
		_trayIconMenu = nullptr;
	}
	if (_brightnessMenu != nullptr) {
		delete _brightnessMenu;
		_brightnessMenu = nullptr;
	}

	_trayIconMenu = new QMenu();

	if (_trayIcon == nullptr)
	{
		_trayIcon = new QSystemTrayIcon();
		connect(_trayIcon, &QSystemTrayIcon::activated, this, &SysTray::iconActivated);
		_trayIcon->setIcon(QIcon(":/ambilightapp-icon-32px.png"));
		_trayIcon->show();
	}

	QString instanceKey = QString("Instance_%1").arg(_selectedInstance == -1 ? 0 : _selectedInstance);

	// Menu độ sáng
	std::shared_ptr<AmbilightAppManager> instanceManager = _instanceManager.lock();
	int currentBrightness = 100;
	if (instanceManager)
	{
		int instanceIndex = _selectedInstance;
		if (instanceIndex == -1)
			instanceIndex = 0;

		auto instance = instanceManager->getAmbilightAppInstance(instanceIndex);
		if (instance)
		{
			QJsonDocument configDoc = instance->getSetting(settings::type::COLOR);
			QJsonObject config = configDoc.object();
			
			if (config.contains("channelAdjustment"))
			{
				QJsonArray adjustments = config["channelAdjustment"].toArray();
				if (!adjustments.isEmpty())
				{
					QJsonObject firstAdjustment = adjustments[0].toObject();
					if (firstAdjustment.contains("brightness"))
					{
						currentBrightness = firstAdjustment["brightness"].toInt(100);
					}
				}
			}
		}
	}

	_brightnessMenu = new QMenu(tr("&Độ sáng LED:   %1%").arg(currentBrightness));
	_brightnessMenu->setIcon(QPixmap(":/brightness.svg")); 

	QActionGroup* brightnessGroup = new QActionGroup(this);
	brightnessGroup->setExclusive(true);

	for(int brightness = 100; brightness >= 10; brightness -= 10) {
		QAction* brightnessAction = new QAction(QString::number(brightness) + "%", brightnessGroup);
		brightnessAction->setCheckable(true);
		brightnessAction->setChecked(brightness == currentBrightness);
		connect(brightnessAction, &QAction::triggered, this, [this, brightness]() {
			this->setBrightness(brightness);
		});
		_brightnessMenu->addAction(brightnessAction);
	}

	// Menu thoát
	_quitAction = new QAction(tr("&Thoát"));
	_quitAction->setIcon(QPixmap(":/quit.svg"));
	connect(_quitAction, &QAction::triggered, this, &SysTray::menuQuit);

	// Menu chọn màu sắc
	QColor currentColor = s_lastSelectedColors.value(instanceKey);
	_colorAction = new QAction(tr("&Chọn màu sắc%1").arg(currentColor.isValid() ? ":   " + currentColor.name() : ""));
	_colorAction->setIcon(QPixmap(":/color.svg"));
	connect(_colorAction, &QAction::triggered, this, &SysTray::showColorDialog);

	// Menu cài đặt
	_settingsAction = new QAction(tr("&Cài đặt"));
	_settingsAction->setIcon(QPixmap(":/settings.svg"));
	connect(_settingsAction, &QAction::triggered, this, &SysTray::settings);

	// Menu plugin quét màu
#ifdef _WIN32
	_openscreencapAction = new QAction(tr("&Plugin quét màu"));
	_openscreencapAction->setIcon(QPixmap(":/settings.svg"));
	connect(_openscreencapAction, &QAction::triggered, this, &SysTray::openScreenCap);
#endif

	// Menu chế độ theo màn hình
	_clearAction = new QAction(tr("&Chế độ theo màn hình"));
	_clearAction->setIcon(QPixmap(":/clear.svg"));
	connect(_clearAction, &QAction::triggered, this, &SysTray::clearEfxColor);

	// Menu chế độ nháy theo nhạc
	_runmusicledAction = new QAction(tr("&Chế độ nháy theo nhạc"));
	_runmusicledAction->setIcon(QPixmap(":/music.svg"));
	connect(_runmusicledAction, &QAction::triggered, this, &SysTray::runMusicLed);

	// Menu khởi động lại
	_restartappAction = new QAction(tr("&Khởi động lại"));
	_restartappAction->setIcon(QPixmap(":/restart.svg"));
	connect(_restartappAction, &QAction::triggered, this, &SysTray::restartApp);

	// Menu bật/tắt LED
	_toggleLedAction = new QAction(tr("&Tắt LED"));
    _toggleLedAction->setIcon(QPixmap(":/toggle.svg")); 
    connect(_toggleLedAction, &QAction::triggered, this, &SysTray::toggleLedState);

	// Menu chọn LED cần chỉnh
    QMenu* instancesMenu = new QMenu(tr("&Chọn LED cần chỉnh"), _trayIconMenu);
    instancesMenu->setIcon(QIcon(":/instance.svg")); 

    QActionGroup* instanceGroup = new QActionGroup(this);
    instanceGroup->setExclusive(true);

    QAction* allAction = new QAction("Tất cả", instanceGroup);
    allAction->setCheckable(true);
    allAction->setChecked(_selectedInstance == -1);
    connect(allAction, &QAction::triggered, this, [this]() {
        _selectedInstance = -1;
        selectInstance();
    });
    instancesMenu->addAction(allAction);
    
    instancesMenu->addSeparator();

    if (instanceManager != nullptr)
    {
        QVector<QVariantMap> instanceData = instanceManager->getInstanceData();
        
        for (const QVariantMap& instance : instanceData)
        {
            if (instance["enabled"].toBool())
            {
                int index = instance["instance"].toInt();
                QString name = instance["friendly_name"].toString();
                
                QAction* instanceAction = new QAction(name, instanceGroup);
                instanceAction->setCheckable(true);
                instanceAction->setChecked(index == _selectedInstance);
                connect(instanceAction, &QAction::triggered, this, [this, index]() {
                    _selectedInstance = index;
                    selectInstance();
                });
                instancesMenu->addAction(instanceAction);
            }
        }
    }

	// Menu chọn hiệu ứng
	std::list<EffectDefinition> efxs;
	if (instanceManager)
		efxs = instanceManager->getEffects();

	QString currentEffect = s_lastSelectedEffects.value(instanceKey);

	_trayIconEfxMenu = new QMenu(tr("&Chọn hiệu ứng%1").arg(currentEffect.isEmpty() ? "" : ":   " + currentEffect));
	_trayIconEfxMenu->setIcon(QPixmap(":/effects.svg"));

	QActionGroup* effectGroup = new QActionGroup(this);
	effectGroup->setExclusive(true);

	for (const EffectDefinition& efx : efxs)
	{
		QString effectName = efx.name;
		QAction* efxAction = new QAction(effectName, effectGroup);
		efxAction->setCheckable(true);
		efxAction->setChecked(effectName == currentEffect && !currentEffect.isEmpty());
		connect(efxAction, &QAction::triggered, this, [this, effectName]() {
			this->setEffect(effectName);
		});
		_trayIconEfxMenu->addAction(efxAction);
	}

	// Thêm các menu vào menu chính
	_trayIconMenu->addMenu(instancesMenu);
	_trayIconMenu->addSeparator();
	_trayIconMenu->addAction(_colorAction);
	_trayIconMenu->addMenu(_trayIconEfxMenu);
	_trayIconMenu->addAction(_clearAction);
	_trayIconMenu->addAction(_runmusicledAction);
	_trayIconMenu->addSeparator();
	_trayIconMenu->addMenu(_brightnessMenu);
	_trayIconMenu->addAction(_toggleLedAction);
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

	_trayIcon->setContextMenu(_trayIconMenu);
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
	{
		if (_selectedInstance == -1) {
			QVector<QVariantMap> instanceData = instanceManager->getInstanceData();
			for (const QVariantMap& inst : instanceData) {
				if (inst["running"].toBool()) {
					int idx = inst["instance"].toInt();
					instanceManager->setInstanceColor(idx, 1, rgbColor, 0);

					QString instanceKey = QString("Instance_%1").arg(idx);
					QString savedColor = s_lastSelectedColors.value(instanceKey).name();
					QString newColor = color.name();
					if (savedColor != newColor) {
						s_lastSelectedColors[instanceKey] = color;
						saveInstanceSetting(instanceKey, "Color", newColor, s_colorAccessMutex);
						// Xóa key Effect nếu có
						s_lastSelectedEffects.remove(instanceKey);
						removeInstanceSetting(instanceKey, "Effect", s_effectAccessMutex);
					}
				}
			}
		} else {
			instanceManager->setInstanceColor(_selectedInstance, 1, rgbColor, 0);

			QString instanceKey = QString("Instance_%1").arg(_selectedInstance);
			QString savedColor = s_lastSelectedColors.value(instanceKey).name();
			QString newColor = color.name();
			if (savedColor != newColor) {
				s_lastSelectedColors[instanceKey] = color;
				saveInstanceSetting(instanceKey, "Color", newColor, s_colorAccessMutex);
				// Xóa key Effect nếu có
				s_lastSelectedEffects.remove(instanceKey);
				removeInstanceSetting(instanceKey, "Effect", s_effectAccessMutex);
			}
		}
	}
	// Cập nhật tiêu đề menu color
	_colorAction->setText(tr("&Chọn màu sắc:   %1").arg(color.name()));
	createTrayIcon();
}

void SysTray::showColorDialog()
{
    if (_colorDlg == nullptr)
    {
        _colorDlg = new QColorDialog();
        
        #ifdef __APPLE__
            _colorDlg->setOption(QColorDialog::DontUseNativeDialog);
            _colorDlg->setOption(QColorDialog::ShowAlphaChannel, false);
        // #else
        //     _colorDlg->setOptions(QColorDialog::NoButtons);
        #endif
        
        connect(_colorDlg, SIGNAL(currentColorChanged(const QColor&)), this, SLOT(setColor(const QColor&)));
        connect(_colorDlg, SIGNAL(colorSelected(const QColor&)), this, SLOT(setColor(const QColor&)));
    }

    if (_colorDlg->isVisible())
    {
        _colorDlg->hide();
    }
    else
    {
        #ifdef __APPLE__
            _colorDlg->setWindowFlags(Qt::WindowStaysOnTopHint);
            _colorDlg->exec();
        #else
            _colorDlg->show();
            _colorDlg->raise();
            _colorDlg->activateWindow();
        #endif
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
	{
		if (_selectedInstance == -1) {
			QVector<QVariantMap> instanceData = instanceManager->getInstanceData();
			for (const QVariantMap& inst : instanceData) {
				if (inst["running"].toBool()) {
					int idx = inst["instance"].toInt();
					instanceManager->setInstanceEffect(idx, effect, 1);

					QString instanceKey = QString("Instance_%1").arg(idx);
					QString savedEffect = s_lastSelectedEffects.value(instanceKey);
					if (savedEffect != effect) {
						s_lastSelectedEffects[instanceKey] = effect;
						saveInstanceSetting(instanceKey, "Effect", effect, s_effectAccessMutex);
						// Xóa key Color nếu có
						s_lastSelectedColors.remove(instanceKey);
						removeInstanceSetting(instanceKey, "Color", s_colorAccessMutex);
					}
				}
			}
		} else {
			instanceManager->setInstanceEffect(_selectedInstance, effect, 1);

			QString instanceKey = QString("Instance_%1").arg(_selectedInstance);
			QString savedEffect = s_lastSelectedEffects.value(instanceKey);
			if (savedEffect != effect) {
				s_lastSelectedEffects[instanceKey] = effect;
				saveInstanceSetting(instanceKey, "Effect", effect, s_effectAccessMutex);
				// Xóa key Color nếu có
				s_lastSelectedColors.remove(instanceKey);
				removeInstanceSetting(instanceKey, "Color", s_colorAccessMutex);
			}
		}
	}
	// Cập nhật tiêu đề menu effect
	_trayIconEfxMenu->setTitle(tr("&Chọn hiệu ứng: %1").arg(effect));
	createTrayIcon();
}

void SysTray::clearEfxColor()
{
	std::shared_ptr<AmbilightAppManager> instanceManager = _instanceManager.lock();
	if (instanceManager)
		instanceManager->clearInstancePriority(_selectedInstance, 1);

	if (_selectedInstance == -1 && instanceManager) {
		QVector<QVariantMap> instanceData = instanceManager->getInstanceData();
		for (const QVariantMap& inst : instanceData) {
			if (inst["running"].toBool()) {
				int idx = inst["instance"].toInt();
				QString instanceKey = QString("Instance_%1").arg(idx);
				s_lastSelectedColors.remove(instanceKey);
				s_lastSelectedEffects.remove(instanceKey);
				removeInstanceSetting(instanceKey, "Color", s_colorAccessMutex);
				removeInstanceSetting(instanceKey, "Effect", s_effectAccessMutex);
			}
		}
	} else {
		QString instanceKey = QString("Instance_%1").arg(_selectedInstance == -1 ? 0 : _selectedInstance);
		s_lastSelectedColors.remove(instanceKey);
		s_lastSelectedEffects.remove(instanceKey);
		removeInstanceSetting(instanceKey, "Color", s_colorAccessMutex);
		removeInstanceSetting(instanceKey, "Effect", s_effectAccessMutex);
	}
	createTrayIcon();
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
        if (_selectedInstance == -1)
        {
            // Lấy danh sách tất cả instance đang chạy
            QVector<QVariantMap> instanceData = instanceManager->getInstanceData();
            for (const QVariantMap& inst : instanceData)
            {
                if (inst["running"].toBool())
                {
                    int idx = inst["instance"].toInt();
                    auto instance = instanceManager->getAmbilightAppInstance(idx);
                    if (instance)
                    {
                        QJsonDocument configDoc = instance->getSetting(settings::type::COLOR);
                        QJsonObject config = configDoc.object();
                        if (config.contains("channelAdjustment"))
                        {
                            QJsonArray adjustments = config["channelAdjustment"].toArray();
                            if (!adjustments.isEmpty())
                            {
                                QJsonObject firstAdjustment = adjustments[0].toObject();
                                firstAdjustment["brightness"] = brightness;
                                adjustments[0] = firstAdjustment;
                                config["channelAdjustment"] = adjustments;
                            }
                        }
                        instance->setSetting(settings::type::COLOR, QJsonDocument(config).toJson());
                    }
                }
            }
        }
        else
        {
            auto instance = instanceManager->getAmbilightAppInstance(_selectedInstance);
            if (instance)
            {
                QJsonDocument configDoc = instance->getSetting(settings::type::COLOR);
                QJsonObject config = configDoc.object();
                if (config.contains("channelAdjustment"))
                {
                    QJsonArray adjustments = config["channelAdjustment"].toArray();
                    if (!adjustments.isEmpty())
                    {
                        QJsonObject firstAdjustment = adjustments[0].toObject();
                        firstAdjustment["brightness"] = brightness;
                        adjustments[0] = firstAdjustment;
                        config["channelAdjustment"] = adjustments;
                    }
                }
                instance->setSetting(settings::type::COLOR, QJsonDocument(config).toJson());
            }
        }
    }
	createTrayIcon();
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
				_trayIcon->setIcon(QIcon(":/ambilightapp-icon-32px.png"));
				_trayIcon->show();		
			}
			// Áp dụng color/effect khi instance được khởi động
			applySavedColorEffect(instance);
			break;
		
		case InstanceState::STOP:
            if (instance == _selectedInstance)
            {
                _selectedInstance = -1;
            }
            break;

		default:
			break;
	}
	createTrayIcon();
}

void SysTray::applySavedColorEffect(quint8 instance)
{
	std::shared_ptr<AmbilightAppManager> instanceManager = _instanceManager.lock();
	if (!instanceManager)
		return;

	QString instanceKey = QString("Instance_%1").arg(instance);
	
	// Áp dụng color nếu có
	if (s_lastSelectedColors.contains(instanceKey))
	{
		QColor color = s_lastSelectedColors[instanceKey];
		ColorRgb rgbColor{ (uint8_t)color.red(), (uint8_t)color.green(), (uint8_t)color.blue() };
		instanceManager->setInstanceColor(instance, 1, rgbColor, 0);
	}
	// Áp dụng effect nếu có
	else if (s_lastSelectedEffects.contains(instanceKey))
	{
		QString effect = s_lastSelectedEffects[instanceKey];
		instanceManager->setInstanceEffect(instance, effect, 1);
	}
}

void SysTray::signalSettingsChangedHandler(settings::type type, const QJsonDocument& data)
{
	if (type == settings::type::WEBSERVER)
	{
		_webPort = data.object()["port"].toInt();
	}
	else if (type == settings::type::COLOR)
	{
		// Cập nhật menu độ sáng khi cấu hình màu thay đổi
		QJsonObject config = data.object();
		if (config.contains("channelAdjustment"))
		{
			QJsonArray adjustments = config["channelAdjustment"].toArray();
			if (!adjustments.isEmpty())
			{
				QJsonObject firstAdjustment = adjustments[0].toObject();
				if (firstAdjustment.contains("brightness"))
				{
					int brightness = firstAdjustment["brightness"].toInt(100);
					if (_brightnessMenu != nullptr)
					{
						_brightnessMenu->setTitle(tr("&Độ sáng LED:   %1%").arg(brightness));
						// Cập nhật trạng thái checked cho các action
						for(QAction* action : _brightnessMenu->actions())
						{
							QString text = action->text();
							text.remove("%");
							int actionBrightness = text.toInt();
							action->setChecked(actionBrightness == brightness);
						}
					}
				}
			}
		}
	}
}

void SysTray::saveInstanceSetting(const QString& instanceKey, const QString& key, const QVariant& value, QMutex& mutex)
{
	QMutexLocker locker(&mutex);
	s_settings.beginGroup("Instances");
	s_settings.beginGroup(instanceKey);
	s_settings.setValue(key, value);
	s_settings.endGroup();
	s_settings.endGroup();
	s_settings.sync();
}

void SysTray::removeInstanceSetting(const QString& instanceKey, const QString& key, QMutex& mutex)
{
	QMutexLocker locker(&mutex);
	s_settings.beginGroup("Instances");
	s_settings.beginGroup(instanceKey);
	s_settings.remove(key);
	s_settings.endGroup();
	s_settings.endGroup();
	s_settings.sync();
}
