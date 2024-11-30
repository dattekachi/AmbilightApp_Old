#pragma once

#ifndef PCH_ENABLED
	#include <QMap>
	#include <QStringList>

	#include <utils/ColorRgb.h>
	#include <utils/Logger.h>
	#include <utils/settings.h>
	#include <utils/Components.h>
#endif

#include <effectengine/EffectDefinition.h>

class AmbilightAppInstance;
class InstanceTable;
class SoundCapture;
class GrabberHelper;

namespace ambilightapp
{
	enum class InstanceState {
		START,
		STOP
	};
};

using namespace ambilightapp;

class AmbilightAppManager : public QObject
{
	Q_OBJECT

public:
	~AmbilightAppManager();

	struct PendingRequests
	{
		QObject* caller;
		int     tan;
	};

	QString getRootPath();
	bool areInstancesReady();

public slots:
	void setSmoothing(int time);

	void setSignalStateByCEC(bool enable);

	const QJsonObject getBackup();

	QString restoreBackup(const QJsonObject& message);

	bool IsInstanceRunning(quint8 inst) const { return _runningInstances.contains(inst); }

	std::shared_ptr<AmbilightAppInstance> getAmbilightAppInstance(quint8 instance = 0);

	QVector<QVariantMap> getInstanceData() const;

	bool startInstance(quint8 inst, QObject* caller = nullptr, int tan = 0, bool disableOnStartup = false);

	bool stopInstance(quint8 inst);

	QJsonObject getAverageColor(quint8 index);

	void toggleStateAllInstances(bool pause = false);

	void toggleGrabbersAllInstances(bool pause = false);

	void hibernate(bool wakeUp, ambilightapp::SystemComponent source);

	bool createInstance(const QString& name, bool start = false);

	bool deleteInstance(quint8 inst);

	bool saveName(quint8 inst, const QString& name);

	void saveCalibration(QString saveData);

	void handleInstanceStateChange(InstanceState state, quint8 instance, const QString& name);

	std::vector<QString> getInstances();

	void setInstanceColor(int instance, int priority, ColorRgb ledColors, int timeout_ms);
	void setInstanceEffect(int instance, QString effectName, int priority);
	void setInstanceBrightness(int instance, int brightness);
	void setInstanceComponentState(int instance, ambilightapp::Components component, bool enable);
	void clearInstancePriority(int instance, int priority);
	std::list<EffectDefinition> getEffects();

signals:
	void SignalInstanceStateChanged(InstanceState state, quint8 instance, const QString& name = QString());

	void SignalInstancesListChanged();

	void SignalStartInstanceResponse(QObject* caller, const int& tan);

	void SignalSettingsChanged(settings::type type, const QJsonDocument& data);

	void SignalCompStateChangeRequest(ambilightapp::Components component, bool enable);

	void SignalSetNewComponentStateToAllInstances(ambilightapp::Components component, bool enable);

	void SignalInstancePauseChanged(int instance, bool isEnabled);

private slots:
	void handleInstanceJustStarted();

private:
	friend class AmbilightAppDaemon;

	AmbilightAppManager(const QString& rootPath, bool readonlyMode);

	void startAll(bool disableOnStartup);

	void stopAllonExit();

	bool isInstAllowed(quint8 inst) const { return (inst > 0); }

private:
	
	Logger*			_log;
	std::unique_ptr<InstanceTable> _instanceTable;
	const QString	_rootPath;
	QMap<quint8, std::shared_ptr<AmbilightAppInstance>> _runningInstances;
	QMap<quint8, std::shared_ptr<AmbilightAppInstance>>	_startingInstances;

	bool	_readonlyMode;
	int		_fireStarter;

	QMap<quint8, PendingRequests> _pendingRequests;
};
