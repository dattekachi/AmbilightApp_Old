#ifndef PCH_ENABLED
	#include <QVariant>
#endif

#include <AmbilightappConfig.h>
#include <api/CallbackAPI.h>
#include <base/AmbilightAppInstance.h>
#include <base/GrabberWrapper.h>
#include <base/AmbilightAppManager.h>
#include <base/ComponentController.h>
#include <base/Muxer.h>
#include <base/ImageToLedManager.h>
#include <flatbufserver/FlatBufferServer.h>
#include <utils/ColorSys.h>
#include <utils/PerformanceCounters.h>

using namespace ambilightapp;

CallbackAPI::CallbackAPI(Logger* log, bool localConnection, QObject* parent)
	: BaseAPI(log, localConnection, parent)
{
	_lutCalibrator = nullptr;
	_availableCommands << "components-update" << "performance-update" << "sessions-update" << "priorities-update" << "imageToLedMapping-update" << "grabberstate-update" << "lut-calibration-update"
		<< "adjustment-update" << "leds-colors" << "live-video" << "videomodehdr-update" << "settings-update" << "leds-update" << "instance-update" << "token-update" << "benchmark-update";
}

void CallbackAPI::addSubscriptions()
{
	// get current subs
	QStringList currSubs(_subscribedCommands);

	// stop subs
	removeSubscriptions();

	// re-apply subs
	for (const auto& entry : currSubs)
	{
		subscribeFor(entry);
	}
}

bool CallbackAPI::subscribeFor(const QString& type, bool unsubscribe)
{
	if (!_availableCommands.contains(type))
		return false;

	GrabberWrapper* grabberWrapper = (_videoGrabber != nullptr) ? _videoGrabber->grabberWrapper() : nullptr;

	if (unsubscribe)
		_subscribedCommands.removeAll(type);
	else
		_subscribedCommands << type;

	if (type == "components-update" && _ambilightapp != nullptr)
	{
		if (unsubscribe)
			disconnect(_ambilightapp.get(), &AmbilightAppInstance::SignalComponentStateChanged, this, &CallbackAPI::componentStateHandler);
		else
			connect(_ambilightapp.get(), &AmbilightAppInstance::SignalComponentStateChanged, this, &CallbackAPI::componentStateHandler, Qt::UniqueConnection);
	}

	if (type == "performance-update")
	{
		if (unsubscribe)
			disconnect(_performanceCounters.get(), &PerformanceCounters::SignalPerformanceStatisticsUpdated, this, &CallbackAPI::performanceUpdateHandler);
		else
		{
			connect(_performanceCounters.get(), &PerformanceCounters::SignalPerformanceStatisticsUpdated, this, &CallbackAPI::performanceUpdateHandler, Qt::UniqueConnection);
			QUEUE_CALL_0(_performanceCounters.get(), triggerBroadcast);
		}
	}

	if (type == "lut-calibration-update")
	{
		if (unsubscribe)
		{
			if (_lutCalibrator != nullptr)
				disconnect(_lutCalibrator.get(), &LutCalibrator::SignalLutCalibrationUpdated, this, &CallbackAPI::lutCalibrationUpdateHandler);
			_lutCalibrator = nullptr;
		}
		else
		{
			_lutCalibrator = std::unique_ptr<LutCalibrator>(new LutCalibrator());
			connect(_lutCalibrator.get(), &LutCalibrator::SignalLutCalibrationUpdated, this, &CallbackAPI::lutCalibrationUpdateHandler, Qt::UniqueConnection);
		}
	}

	if (type == "sessions-update" && _discoveryWrapper != nullptr)
	{
#ifdef ENABLE_BONJOUR
		if (unsubscribe)
			disconnect(_discoveryWrapper.get(), &DiscoveryWrapper::SignalDiscoveryFoundService, this, &CallbackAPI::signalDiscoveryFoundServiceHandler);
		else
			connect(_discoveryWrapper.get(), &DiscoveryWrapper::SignalDiscoveryFoundService, this, &CallbackAPI::signalDiscoveryFoundServiceHandler, Qt::UniqueConnection);
#endif
	}

	if (type == "priorities-update" && _ambilightapp != nullptr)
	{
		if (unsubscribe)
			disconnect(_ambilightapp.get(), &AmbilightAppInstance::SignalPrioritiesChanged, this, &CallbackAPI::priorityUpdateHandler);
		else
			connect(_ambilightapp.get(), &AmbilightAppInstance::SignalPrioritiesChanged, this, &CallbackAPI::priorityUpdateHandler, Qt::UniqueConnection);
	}

	if (type == "imageToLedMapping-update" && _ambilightapp != nullptr)
	{
		if (unsubscribe)
			disconnect(_ambilightapp.get(), &AmbilightAppInstance::SignalImageToLedsMappingChanged, this, &CallbackAPI::imageToLedsMappingChangeHandler);
		else
			connect(_ambilightapp.get(), &AmbilightAppInstance::SignalImageToLedsMappingChanged, this, &CallbackAPI::imageToLedsMappingChangeHandler, Qt::UniqueConnection);
	}

	if (type == "adjustment-update" && _ambilightapp != nullptr)
	{
		if (unsubscribe)
			disconnect(_ambilightapp.get(), &AmbilightAppInstance::SignalAdjustmentUpdated, this, &CallbackAPI::signalAdjustmentUpdatedHandler);
		else
			connect(_ambilightapp.get(), &AmbilightAppInstance::SignalAdjustmentUpdated, this, &CallbackAPI::signalAdjustmentUpdatedHandler, Qt::UniqueConnection);
	}

	if (type == "grabberstate-update" && grabberWrapper != nullptr)
	{
		if (unsubscribe)
			disconnect(grabberWrapper, &GrabberWrapper::SignalVideoStreamChanged, this, &CallbackAPI::videoStreamChangedHandler);
		else
			connect(grabberWrapper, &GrabberWrapper::SignalVideoStreamChanged, this, &CallbackAPI::videoStreamChangedHandler, Qt::UniqueConnection);
	}

	if (type == "videomodehdr-update")
	{
		if (unsubscribe)
			disconnect(_instanceManager.get(), &AmbilightAppManager::SignalSetNewComponentStateToAllInstances, this, &CallbackAPI::videoModeHdrChangeHandler);
		else
			connect(_instanceManager.get(), &AmbilightAppManager::SignalSetNewComponentStateToAllInstances, this, &CallbackAPI::videoModeHdrChangeHandler, Qt::UniqueConnection);
	}

	if (type == "settings-update" && _ambilightapp != nullptr)
	{
		if (unsubscribe)
			disconnect(_ambilightapp.get(), &AmbilightAppInstance::SignalInstanceSettingsChanged, this, &CallbackAPI::settingsChangeHandler);
		else
			connect(_ambilightapp.get(), &AmbilightAppInstance::SignalInstanceSettingsChanged, this, &CallbackAPI::settingsChangeHandler, Qt::UniqueConnection);
	}

	if (type == "leds-update" && _ambilightapp != nullptr)
	{
		if (unsubscribe)
			disconnect(_ambilightapp.get(), &AmbilightAppInstance::SignalInstanceSettingsChanged, this, &CallbackAPI::ledsConfigChangeHandler);
		else
			connect(_ambilightapp.get(), &AmbilightAppInstance::SignalInstanceSettingsChanged, this, &CallbackAPI::ledsConfigChangeHandler, Qt::UniqueConnection);
	}

	if (type == "leds-colors" && _ambilightapp != nullptr)
	{
		if (unsubscribe)
			disconnect(_ambilightapp.get(), &AmbilightAppInstance::SignalRawColorsChanged, this, &CallbackAPI::handleIncomingColors);
		else
			connect(_ambilightapp.get(), &AmbilightAppInstance::SignalRawColorsChanged, this, &CallbackAPI::handleIncomingColors, Qt::UniqueConnection);
	}

	if (type == "live-video" && _ambilightapp != nullptr)
	{
		if (unsubscribe)
		{
			_liveImage = Image<ColorRgb>();
			disconnect(_ambilightapp.get(), &AmbilightAppInstance::SignalInstanceImageUpdated, this, &CallbackAPI::handlerInstanceImageUpdated);
		}
		else
			connect(_ambilightapp.get(), &AmbilightAppInstance::SignalInstanceImageUpdated, this, &CallbackAPI::handlerInstanceImageUpdated, Qt::UniqueConnection);
	}

	if (type == "instance-update")
	{
		if (unsubscribe)
			disconnect(_instanceManager.get(), &AmbilightAppManager::SignalInstancesListChanged, this, &CallbackAPI::instancesListChangedHandler);
		else
			connect(_instanceManager.get(), &AmbilightAppManager::SignalInstancesListChanged, this, &CallbackAPI::instancesListChangedHandler, Qt::UniqueConnection);
	}

	if (type == "token-update")
	{
		if (unsubscribe)
			disconnect(_accessManager.get(), &AccessManager::SignalTokenUpdated, this, &CallbackAPI::tokenChangeHandler);
		else
			connect(_accessManager.get(), &AccessManager::SignalTokenUpdated, this, &CallbackAPI::tokenChangeHandler, Qt::UniqueConnection);
	}

	if (type == "benchmark-update" && grabberWrapper != nullptr)
	{
		if (unsubscribe)
			disconnect(grabberWrapper, &GrabberWrapper::SignalBenchmarkUpdate, this, &CallbackAPI::signalBenchmarkUpdateHandler);
		else
			connect(grabberWrapper, &GrabberWrapper::SignalBenchmarkUpdate, this, &CallbackAPI::signalBenchmarkUpdateHandler, Qt::UniqueConnection);
	}

	return true;
}

void CallbackAPI::subscribe(QJsonArray subsArr)
{
	// catch the all keyword and build a list of all cmds
	if (subsArr.contains("all"))
	{
		subsArr = QJsonArray();
		for (const auto& entry : _availableCommands)
		{
			subsArr.append(entry);
		}
	}

	for (const QJsonValueRef entry : subsArr)
	{
		// config callbacks just if auth is set
		if ((entry == "settings-update" || entry == "token-update" || entry == "imagestream-start") &&
			!BaseAPI::isAdminAuthorized())
			continue;
		// silent failure if a subscribe type is not found
		subscribeFor(entry.toString());
	}
}

void CallbackAPI::removeSubscriptions()
{
	if (_ambilightapp != nullptr)
	{
		QStringList toRemove(_subscribedCommands);

		for (const auto& entry : toRemove)
		{
			subscribeFor(entry, true);
		}
	}
}

void CallbackAPI::doCallback(const QString& cmd, const QVariant& data)
{
	QJsonObject obj;
	obj["command"] = cmd;

	if (data.userType() == QMetaType::QJsonArray)
		obj["data"] = data.toJsonArray();
	else
		obj["data"] = data.toJsonObject();

	emit SignalCallbackToClient(obj);
}

void CallbackAPI::componentStateHandler(ambilightapp::Components comp, bool state)
{
	QJsonObject data;
	data["name"] = componentToIdString(comp);
	data["enabled"] = state;

	doCallback("components-update", QVariant(data));
}

#ifdef ENABLE_BONJOUR
	void CallbackAPI::signalDiscoveryFoundServiceHandler(DiscoveryRecord::Service type, QList<DiscoveryRecord> records)
	{
		QJsonObject retValue;
		QJsonArray data;

		for (const auto& session : records)
		{
			QJsonObject item;
			item["name"] = session.getName();
			item["host"] = session.hostName;
			item["address"] = session.address;
			item["port"] = session.port;
			data.append(item);
		}

		retValue["service"] = DiscoveryRecord::getName(type);
		retValue["items"] = data;
		doCallback("sessions-update", QVariant(retValue));
	}
#endif

void CallbackAPI::priorityUpdateHandler()
{
	QJsonObject info;
	QJsonArray priorities;

	if (_ambilightapp == nullptr)
		return;

	BLOCK_CALL_2(_ambilightapp.get(), putJsonInfo, QJsonObject&, info, bool, false);

	doCallback("priorities-update", QVariant(info));
}

void CallbackAPI::imageToLedsMappingChangeHandler(int mappingType)
{
	QJsonObject data;
	data["imageToLedMappingType"] = ImageToLedManager::mappingTypeToStr(mappingType);

	doCallback("imageToLedMapping-update", QVariant(data));
}

void CallbackAPI::signalAdjustmentUpdatedHandler(const QJsonArray& newConfig)
{
	doCallback("adjustment-update", QVariant(newConfig));
}

void CallbackAPI::videoModeHdrChangeHandler(ambilightapp::Components component, bool enable)
{
	if (component == ambilightapp::Components::COMP_HDR)
	{
		QJsonObject data;
		data["videomodehdr"] = enable;
		doCallback("videomodehdr-update", QVariant(data));
	}
}

void CallbackAPI::videoStreamChangedHandler(QString device, QString videoMode)
{
	QJsonObject data;
	data["device"] = device;
	data["videoMode"] = videoMode;
	doCallback("grabberstate-update", QVariant(data));
}

void CallbackAPI::settingsChangeHandler(settings::type type, const QJsonDocument& data)
{
	QJsonObject dat;
	if (data.isObject())
		dat[typeToString(type)] = data.object();
	else
		dat[typeToString(type)] = data.array();

	doCallback("settings-update", QVariant(dat));
}

void CallbackAPI::ledsConfigChangeHandler(settings::type type, const QJsonDocument& data)
{
	if (type == settings::type::LEDS)
	{
		QJsonObject dat;
		dat[typeToString(type)] = data.array();
		doCallback("leds-update", QVariant(dat));
	}
}

void CallbackAPI::instancesListChangedHandler()
{
	QJsonArray arr;
	QVector<QVariantMap> entries;

	SAFE_CALL_0_RET(_instanceManager.get(), getInstanceData, QVector<QVariantMap>, entries);

	for (const auto& entry : entries)
	{
		QJsonObject obj;
		obj.insert("friendly_name", entry["friendly_name"].toString());
		obj.insert("instance", entry["instance"].toInt());
		//obj.insert("last_use", entry["last_use"].toString());
		obj.insert("running", entry["running"].toBool());
		arr.append(obj);
	}
	doCallback("instance-update", QVariant(arr));
}

void CallbackAPI::tokenChangeHandler(const QVector<AccessManager::AuthDefinition>& def)
{
	QJsonArray arr;
	for (const auto& entry : def)
	{
		QJsonObject sub;
		sub["comment"] = entry.comment;
		sub["id"] = entry.id;
		sub["last_use"] = entry.lastUse;
		arr.push_back(sub);
	}
	doCallback("token-update", QVariant(arr));
}

void CallbackAPI::signalBenchmarkUpdateHandler(int status, QString message)
{
	QJsonObject dat;
	dat["status"] = status;
	dat["message"] = message;
	doCallback("benchmark-update", QVariant(dat));
}

void CallbackAPI::lutCalibrationUpdateHandler(const QJsonObject& data)
{
	doCallback("lut-calibration-update", QVariant(data));
}


void CallbackAPI::performanceUpdateHandler(const QJsonObject& data)
{
	doCallback("performance-update", QVariant(data));
}

