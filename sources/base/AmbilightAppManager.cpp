/* AmbilightAppManager.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
*
*  Project homesite: http://ambilightled.com
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
*/

#ifndef PCH_ENABLED
	#include <QThread>
	#include <QDir>
#endif

#include <QStringList>

#include <base/AmbilightAppManager.h>
#include <base/AmbilightAppInstance.h>
#include <db/InstanceTable.h>
#include <base/GrabberWrapper.h>
#include <base/AccessManager.h>
#include <base/Muxer.h>
#include <utils/GlobalSignals.h>

QString AmbilightAppManager::getRootPath()
{
	return _rootPath;
}

AmbilightAppManager::AmbilightAppManager(const QString& rootPath, bool readonlyMode)
	: _log(Logger::getInstance("AMBILIGHT_MANAGER"))
	, _instanceTable(new InstanceTable(readonlyMode))
	, _rootPath(rootPath)
	, _readonlyMode(readonlyMode)
	, _fireStarter(0)
{
	qRegisterMetaType<InstanceState>("InstanceState");
	connect(this, &AmbilightAppManager::SignalInstanceStateChanged, this, &AmbilightAppManager::handleInstanceStateChange);
}

AmbilightAppManager::~AmbilightAppManager()
{
	Debug(_log, "AmbilightAppManager has been removed");
}

void AmbilightAppManager::handleInstanceStateChange(InstanceState state, quint8 instance, const QString& name)
{
	switch (state)
	{
		case InstanceState::START:
			emit GlobalSignals::getInstance()->SignalPerformanceStateChanged(true, PerformanceReportType::INSTANCE, instance, name);
			break;
		case InstanceState::STOP:
			emit GlobalSignals::getInstance()->SignalPerformanceStateChanged(false, PerformanceReportType::INSTANCE, instance);
			emit GlobalSignals::getInstance()->SignalPerformanceStateChanged(false, PerformanceReportType::LED, instance);
			break;
		default:
			break;
	}
}


std::shared_ptr<AmbilightAppInstance> AmbilightAppManager::getAmbilightAppInstance(quint8 instance)
{
	if (_runningInstances.contains(instance))
		return _runningInstances.value(instance);

	Warning(_log, "The requested instance index '%d' with name '%s' isn't running, return main instance", instance, QSTRING_CSTR(_instanceTable->getNamebyIndex(instance)));
	return _runningInstances.value(0);
}

std::vector<QString> AmbilightAppManager::getInstances()
{
	std::vector<QString> ret;

	for (const quint8& key : _runningInstances.keys())
	{
		ret.push_back(QString::number(key));
		ret.push_back(_instanceTable->getNamebyIndex(key));
	}

	return ret;
}

void AmbilightAppManager::setInstanceColor(int instance, int priority, ColorRgb ledColors, int timeout_ms)
{
	std::vector<ColorRgb> rgbColor{ ledColors };

	for (const auto& selInstance : _runningInstances)
		if (instance == -1 || selInstance->getInstanceIndex() == instance)
		{
			QUEUE_CALL_3(selInstance.get(), setColor, int, 1, std::vector<ColorRgb>, rgbColor, int, 0);
		}
}

void  AmbilightAppManager::setInstanceEffect(int instance, QString effectName, int priority)
{
	for (const auto& selInstance : _runningInstances)
		if (instance == -1 || selInstance->getInstanceIndex() == instance)
		{
			QUEUE_CALL_2(selInstance.get(), setEffect, QString, effectName, int, 1);
		}
}

void AmbilightAppManager::setInstanceBrightness(int instance, int brightness)
{
    for (const auto& selInstance : _runningInstances)
        if (instance == -1 || selInstance->getInstanceIndex() == instance)
        {
            QJsonObject adjustment;
            adjustment["brightness"] = brightness;
            QUEUE_CALL_1(selInstance.get(), updateAdjustments, QJsonObject, adjustment);
        }
}

void AmbilightAppManager::setInstanceComponentState(int instance, ambilightapp::Components component, bool enable)
{
    for (const auto& selInstance : _runningInstances)
    {
        if (instance == -1 || selInstance->getInstanceIndex() == instance)
        {
            emit selInstance->SignalRequestComponent(component, enable);
        }
    }
}

void AmbilightAppManager::clearInstancePriority(int instance, int priority)
{
	for (const auto& selInstance : _runningInstances)
		if (instance == -1 || selInstance->getInstanceIndex() == instance)
		{
			QUEUE_CALL_1(selInstance.get(), clear, int, 1);
		}
}

std::list<EffectDefinition> AmbilightAppManager::getEffects()
{
	std::list<EffectDefinition> efxs;

	if (IsInstanceRunning(0))
	{
		auto inst = getAmbilightAppInstance(0);

		SAFE_CALL_0_RET(inst.get(), getEffects, std::list<EffectDefinition>, efxs);
	}

	return efxs;
}

QVector<QVariantMap> AmbilightAppManager::getInstanceData() const
{
	QVector<QVariantMap> instances = _instanceTable->getAllInstances();
	for (auto& entry : instances)
	{
		// add running state
		entry["running"] = _runningInstances.contains(entry["instance"].toInt());
	}
	return instances;
}

bool AmbilightAppManager::areInstancesReady()
{
	if (_fireStarter <= 0)
		return false;

	return (--_fireStarter == 0);
}

void AmbilightAppManager::startAll(bool disableOnStartup)
{
	auto instanceList = _instanceTable->getAllInstances(true);

	_fireStarter = instanceList.count();

	for (const auto& entry : instanceList)
	{
		startInstance(entry["instance"].toInt(), nullptr, 0, disableOnStartup);
	}
}

void AmbilightAppManager::stopAllonExit()
{
	disconnect(this, nullptr, nullptr, nullptr);
	
	Warning(_log, "Running instances: %i, starting instances: %i"
		, (int)_runningInstances.size()
		, (int)_startingInstances.size());

	_runningInstances.clear();
	_startingInstances.clear();

	while (AmbilightAppInstance::getTotalRunningCount())
	{
		Warning(_log, "Waiting for instances: %i", (int)AmbilightAppInstance::getTotalRunningCount());
		QThread::msleep(25);
	}		
	Info(_log, "All instances are closed now");
}

void AmbilightAppManager::setSmoothing(int time)
{
	for (const auto& instance : _runningInstances)
		QUEUE_CALL_1(instance.get(), setSmoothing, int, time);
}

QJsonObject AmbilightAppManager::getAverageColor(quint8 index)
{
	AmbilightAppInstance* instance = AmbilightAppManager::getAmbilightAppInstance(index).get();
	QJsonObject res;

	SAFE_CALL_0_RET(instance, getAverageColor, QJsonObject, res);

	return res;
}

void AmbilightAppManager::setSignalStateByCEC(bool enable)
{
	for (const auto& instance : _runningInstances)
	{
		QUEUE_CALL_1(instance.get(), setSignalStateByCEC, bool, enable);
	}
}

void AmbilightAppManager::toggleStateAllInstances(bool pause)
{
	for (const auto& instance : _runningInstances)
	{
		emit instance->SignalRequestComponent(ambilightapp::COMP_ALL, pause);
	}
}

void AmbilightAppManager::toggleGrabbersAllInstances(bool pause)
{
	for (const auto& instance : _runningInstances)
	{
		QUEUE_CALL_1(instance.get(), turnGrabbers, bool, pause);
	}
}

void AmbilightAppManager::hibernate(bool wakeUp, ambilightapp::SystemComponent source)
{
	if (source == ambilightapp::SystemComponent::LOCKER || source == ambilightapp::SystemComponent::MONITOR)
	{
		Debug(_log, "OS event: %s", (source == ambilightapp::SystemComponent::LOCKER) ? ((wakeUp) ? "OS unlocked" : "OS locked") : ((wakeUp) ? "Monitor On" : "Monitor Off"));
		bool _hasEffect = false;
		for (const auto& instance : _runningInstances)
		{
			SAFE_CALL_1_RET(instance.get(), hasPriority, bool, _hasEffect, int, Muxer::LOWEST_EFFECT_PRIORITY);
			if (_hasEffect)
			{
				break;
			}
		}

		if (!wakeUp && _hasEffect)
		{
			Warning(_log, "The user has set a background effect, and therefore we will not turn off the LEDs when locking the operating system");
		}

		if (_hasEffect)
		{
			if (wakeUp)
				QTimer::singleShot(3000, this, [this, wakeUp]() { toggleGrabbersAllInstances(wakeUp);  });
			else
				toggleGrabbersAllInstances(wakeUp);
			
			return;
		}
	}

	if (!wakeUp)
	{
		Warning(_log, "The system is going to sleep");
		toggleStateAllInstances(false);
	}
	else
	{
		Warning(_log, "The system is going to wake up");
		QTimer::singleShot(3000, this, [this]() { toggleStateAllInstances(true);  });
	}
}

QString removeDiacritics(const QString& str) {
    QString normalized = str.normalized(QString::NormalizationForm_D);
    QString result;
    for (const QChar &ch : normalized) {
        if (ch.category() != QChar::Mark_NonSpacing) {
            result.append(ch);
        }
    }
    return result;
}

bool AmbilightAppManager::addMusicDevice(const quint8 inst)
{
    Info(_log, "Adding music device for instance %d", inst);

    // Kiểm tra instance có tồn tại không
    if (!_instanceTable->instanceExist(inst)) {
        Error(_log, "Instance %d doesn't exist", inst);
        return false;
    }

    // Lấy tên instance từ instance table
    QString instanceName = _instanceTable->getNamebyIndex(inst);
    QString deviceId = removeDiacritics(instanceName).toLower().replace(" ", "-");

    Info(_log, "Creating device config for: %s (id: %s)", 
         QSTRING_CSTR(instanceName), QSTRING_CSTR(deviceId));

    // Kiểm tra và tạo thư mục .mls
    QString configDir = QDir::homePath() + "/.mls";
    QDir dir(configDir);
    if (!dir.exists()) {
        Info(_log, "Creating config directory: %s", QSTRING_CSTR(configDir));
        dir.mkpath(".");
    }

    // Đọc hoặc tạo file config.json
    QString configPath = configDir + "/config.json";
    QFile file(configPath);
    QJsonObject config;

    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        config = doc.object();
        file.close();
    }

	// Lấy LED count từ instance settings
    auto instance = getAmbilightAppInstance(inst);
    if (!instance) {
        Error(_log, "Cannot get instance %d", inst);
        return false;
    }

	// Lấy LED type từ device settings của instance
   	QString ledType = instance->getSetting(settings::type::DEVICE).object()["ledType"].toString("screen");

    QJsonArray ledsConfig = instance->getSetting(settings::type::LEDS).array();
    int ledCount = ledsConfig.size();
    
    if (ledCount <= 0) {
        Error(_log, "Invalid LED count from instance config: %d", ledCount);
        return false;
    }

    Info(_log, "Found %d LEDs for instance %d", ledCount, inst);

    // Tạo device config
    QJsonObject deviceConfig;
    deviceConfig["baudrate"] = 1000000;
    deviceConfig["center_offset"] = 0;
    deviceConfig["color_order"] = "RGB";
    deviceConfig["com_port"] = "auto";
    deviceConfig["icon_name"] = "mdi:led-strip";
    deviceConfig["led_type"] = ledType;
    deviceConfig["name"] = instanceName;
    deviceConfig["pixel_count"] = ledCount;
    deviceConfig["refresh_rate"] = 62;

    QJsonObject device;
    device["config"] = deviceConfig;
    device["id"] = deviceId;
    device["type"] = "ambilightusb";

    // Thêm device vào config nếu chưa tồn tại
    QJsonArray devices = config["devices"].toArray();
    QJsonArray virtuals = config["virtuals"].toArray();
    bool exists = false;
    
    for (const auto& dev : devices) {
        if (dev.toObject()["id"].toString() == deviceId) {
            exists = true;
            break;
        }
    }

    if (!exists) {
        // Thêm device config
        devices.append(device);
        config["devices"] = devices;

        // Tạo virtual device config
        QJsonObject virtualConfig;
        virtualConfig["auto_generated"] = false;
        
        QJsonObject vConfig;
        vConfig["center_offset"] = 0;
        vConfig["frequency_max"] = 15000;
        vConfig["frequency_min"] = 20;
        vConfig["grouping"] = 1;
        vConfig["icon_name"] = "mdi:led-strip";
        vConfig["mapping"] = "span";
        vConfig["max_brightness"] = 1.0;
        vConfig["name"] = instanceName;
        vConfig["preview_only"] = false;
        vConfig["rows"] = 1;
        vConfig["transition_mode"] = "Add";
        vConfig["transition_time"] = 0.4;
        virtualConfig["config"] = vConfig;

        // Thêm effect config
        QJsonObject effectConfig;
        effectConfig["background_brightness"] = 1.0;
        effectConfig["background_color"] = "#000000";
        effectConfig["blur"] = 2.0;
        effectConfig["brightness"] = 1.0;
        effectConfig["fix_hues"] = true;
        effectConfig["flip"] = false;
        effectConfig["gradient"] = "linear-gradient(90deg, rgb(255, 0, 0) 0%, rgb(255, 120, 0) 14%, rgb(255, 200, 0) 28%, rgb(0, 255, 0) 42%, rgb(0, 199, 140) 56%, rgb(0, 0, 255) 70%, rgb(128, 0, 128) 84%, rgb(255, 0, 178) 98%)";
        effectConfig["gradient_roll"] = 0.0;
        effectConfig["mirror"] = false;
        effectConfig["reactivity"] = 0.4;
        effectConfig["speed"] = 0.35;

        QJsonObject effect;
        effect["config"] = effectConfig;
        effect["type"] = "melt";
        virtualConfig["effect"] = effect;

        // Thêm effects
        QJsonObject meltEffect;
        meltEffect["config"] = effectConfig;
        meltEffect["type"] = "melt";
        
        QJsonObject effects;
        effects["melt"] = meltEffect;
        virtualConfig["effects"] = effects;

        virtualConfig["id"] = deviceId;
        virtualConfig["is_device"] = deviceId;

        // Thêm segments
        QJsonArray segmentArray;
        QJsonArray segment;
        segment.append(deviceId);
        segment.append(0);
        segment.append(deviceConfig["pixel_count"].toInt() - 1);
        segment.append(false);
        segmentArray.append(segment);
        virtualConfig["segments"] = segmentArray;

        virtuals.append(virtualConfig);
        config["virtuals"] = virtuals;

        // Ghi file
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QJsonDocument saveDoc(config);
            file.write(saveDoc.toJson(QJsonDocument::Indented));
            file.close();
            Info(_log, "Device and virtual config added successfully");
            return true;
        }
        Error(_log, "Failed to write config file");
        return false;
    }

    Info(_log, "Device already exists");
    return true;
}

bool AmbilightAppManager::removeMusicDevice(const quint8 inst)
{
    Info(_log, "Removing music device for instance %d", inst);

    // Kiểm tra instance có tồn tại không
    if (!_instanceTable->instanceExist(inst)) {
        Error(_log, "Instance %d doesn't exist", inst);
        return false;
    }

    // Lấy deviceId
    QString instanceName = _instanceTable->getNamebyIndex(inst);
    QString deviceId = removeDiacritics(instanceName).toLower().replace(" ", "-");

    // Đọc file config
    QString configPath = QDir::homePath() + "/.mls/config.json";
    QFile file(configPath);
    
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        Error(_log, "Config file not found or cannot be opened");
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    QJsonObject config = doc.object();

    // Xóa device từ mảng devices
    QJsonArray devices = config["devices"].toArray();
    for (int i = 0; i < devices.size(); i++) {
        if (devices[i].toObject()["id"].toString() == deviceId) {
            devices.removeAt(i);
            break;
        }
    }
    config["devices"] = devices;

    // Xóa virtual device từ mảng virtuals
    QJsonArray virtuals = config["virtuals"].toArray();
    for (int i = 0; i < virtuals.size(); i++) {
        if (virtuals[i].toObject()["id"].toString() == deviceId) {
            virtuals.removeAt(i);
            break;
        }
    }
    config["virtuals"] = virtuals;

    // Ghi lại file config
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument saveDoc(config);
        file.write(saveDoc.toJson(QJsonDocument::Indented));
        file.close();
        Info(_log, "Device and virtual config removed successfully");
        return true;
    }

    Error(_log, "Failed to write config file");
    return false;
}

bool AmbilightAppManager::startInstance(quint8 inst, QObject* caller, int tan, bool disableOnStartup)
{
	if (_instanceTable->instanceExist(inst))
	{
		if (!_runningInstances.contains(inst) && !_startingInstances.contains(inst))
		{
			QThread* ambilightappThread = new QThread();
			ambilightappThread->setObjectName("AmbilightAppThread");

			auto ambilightapp = std::shared_ptr<AmbilightAppInstance>(
				new AmbilightAppInstance(inst,
					_readonlyMode,
					disableOnStartup,
					_instanceTable->getNamebyIndex(inst)),
				[](AmbilightAppInstance* oldInstance) {
					THREAD_REMOVER(QString("Ambilight App instance at index = %1").arg(oldInstance->getInstanceIndex()),
					oldInstance->thread(), oldInstance);
				}
			);

			ambilightapp->moveToThread(ambilightappThread);

			connect(ambilightappThread, &QThread::started, ambilightapp.get(), &AmbilightAppInstance::start);
			connect(ambilightapp.get(), &AmbilightAppInstance::SignalInstanceJustStarted, this, &AmbilightAppManager::handleInstanceJustStarted);
			connect(ambilightapp.get(), &AmbilightAppInstance::SignalInstancePauseChanged, this, &AmbilightAppManager::SignalInstancePauseChanged);
			connect(ambilightapp.get(), &AmbilightAppInstance::SignalInstanceSettingsChanged, this, &AmbilightAppManager::SignalSettingsChanged);
			connect(ambilightapp.get(), &AmbilightAppInstance::SignalRequestComponent, this, &AmbilightAppManager::SignalCompStateChangeRequest);
			connect(this, &AmbilightAppManager::SignalSetNewComponentStateToAllInstances, ambilightapp.get(), &AmbilightAppInstance::setNewComponentState);
			
			_startingInstances.insert(inst, ambilightapp);

			ambilightappThread->start();

			_instanceTable->setLastUse(inst);
			_instanceTable->setEnable(inst, true);			

			if (!_pendingRequests.contains(inst) && caller != nullptr)
			{
				PendingRequests newDef{ caller, tan };
				_pendingRequests[inst] = newDef;
			}
			
			return true;
		}
		Debug(_log, "Can't start Ambilightapp instance index '%d' with name '%s' it's already running or queued for start", inst, QSTRING_CSTR(_instanceTable->getNamebyIndex(inst)));
		return false;
	}
	Debug(_log, "Can't start Ambilightapp instance index '%d' it doesn't exist in DB", inst);
	return false;
}

bool AmbilightAppManager::stopInstance(quint8 instance)
{
	// inst 0 can't be stopped
	if (!isInstAllowed(instance))
		return false;

	if (_instanceTable->instanceExist(instance))
	{
		if (_runningInstances.contains(instance))
		{
			disconnect(this, nullptr, _runningInstances.value(instance).get(), nullptr);

			_instanceTable->setEnable(instance, false);			

			_runningInstances.remove(instance);

			emit SignalInstanceStateChanged(InstanceState::STOP, instance);
			emit SignalInstancesListChanged();

			removeMusicDevice(instance);

			return true;
		}
		Debug(_log, "Can't stop Ambilight App instance index '%d' with name '%s' it's not running'", instance, QSTRING_CSTR(_instanceTable->getNamebyIndex(instance)));
		return false;
	}
	Debug(_log, "Can't stop Ambilight App instance index '%d' it doesn't exist in DB", instance);
	return false;
}

bool AmbilightAppManager::createInstance(const QString& name, bool start)
{
	quint8 inst;
	if (_instanceTable->createInstance(name, inst))
	{
		Info(_log, "New Ambilight App instance created with name '%s'", QSTRING_CSTR(name));

		if (start)
			startInstance(inst);
		else
			emit SignalInstancesListChanged();

		return true;
	}
	return false;
}

bool AmbilightAppManager::deleteInstance(quint8 inst)
{
	// inst 0 can't be deleted
	if (!isInstAllowed(inst))
		return false;

	// stop it if required as blocking and wait
	bool stopped = stopInstance(inst);

	if (_instanceTable->deleteInstance(inst))
	{
		Info(_log, "Ambilightapp instance with index '%d' has been deleted", inst);

		if (!stopped)
			emit SignalInstancesListChanged();

		return true;
	}
	return false;
}

bool AmbilightAppManager::saveName(quint8 inst, const QString& name)
{
	if (_instanceTable->saveName(inst, name))
	{
		emit SignalInstancesListChanged();
		return true;
	}
	return false;
}

void AmbilightAppManager::handleInstanceJustStarted()
{
	AmbilightAppInstance* ambilightapp = qobject_cast<AmbilightAppInstance*>(sender());
	quint8 instance = ambilightapp->getInstanceIndex();	

	if (_startingInstances.contains(instance))
	{
		Info(_log, "Ambilight App instance '%s' has been started", QSTRING_CSTR(_instanceTable->getNamebyIndex(instance)));

		std::shared_ptr<AmbilightAppInstance> runningInstance = _startingInstances.value(instance);
		_runningInstances.insert(instance, runningInstance);
		_startingInstances.remove(instance);

		emit SignalInstanceStateChanged(InstanceState::START, instance, _instanceTable->getNamebyIndex(instance));
		emit SignalInstancesListChanged();

		if (_pendingRequests.contains(instance))
		{
			PendingRequests def = _pendingRequests.take(instance);
			emit SignalStartInstanceResponse(def.caller, def.tan);
			_pendingRequests.remove(instance);
		}
		addMusicDevice(instance);
		// Kết nối signal khi settings thay đổi
        connect(runningInstance.get(), &AmbilightAppInstance::SignalInstanceSettingsChanged, 
                this, [this, instance](settings::type type, const QJsonDocument& config) {
            if (type == settings::type::LEDS) {
                // Cập nhật lại music device config khi LED count thay đổi
                removeMusicDevice(instance);
                addMusicDevice(instance);
            }
        });
	}
	else
		Error(_log, "Could not find instance '%s (index: %i)' in the starting list",
						QSTRING_CSTR(_instanceTable->getNamebyIndex(instance)), instance);
}

const QJsonObject AmbilightAppManager::getBackup()
{
	return _instanceTable->getBackup();
}


QString AmbilightAppManager::restoreBackup(const QJsonObject& message)
{
	QString error("Empty instance table manager");

	if (_instanceTable != nullptr)
	{
		error = _instanceTable->restoreBackup(message);
	}

	return error;
}

void AmbilightAppManager::saveCalibration(QString saveData)
{
	AmbilightAppInstance* instance = AmbilightAppManager::getAmbilightAppInstance(0).get();

	if (instance != nullptr)		
		QUEUE_CALL_1(instance, saveCalibration, QString, saveData)
	else
		Error(_log, "Ambilightapp instance is not running...can't save the calibration data");
}
