#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <QStringList>
	#include <QSize>
	#include <QJsonObject>
	#include <QJsonValue>
	#include <QJsonArray>
	#include <QMap>
	#include <QTime>
	#include <QThread>

	#include <list>
	#include <memory>
	#include <atomic>

	#include <utils/ColorRgb.h>
	#include <utils/Image.h>
	#include <utils/settings.h>
	#include <utils/Components.h>
#endif

#include <base/LedString.h>
#include <effectengine/EffectDefinition.h>
#include <effectengine/ActiveEffectDefinition.h>
#include <leddevice/LedDevice.h>

class Muxer;
class ComponentController;
class ImageToLedManager;
class NetworkForwarder;
class Smoothing;
class EffectEngine;
class LedCalibration;
class ColorAdjustment;
class InstanceConfig;
class VideoControl;
class SystemControl;
class BoblightServer;
class RawUdpServer;
class LedDeviceWrapper;
class Logger;
class AmbilightAppManager;
class SoundCapture;
class GrabberHelper;


class AmbilightAppInstance : public QObject
{
	Q_OBJECT

public:
	AmbilightAppInstance(quint8 instance, bool readonlyMode, bool disableOnstartup, QString name);
	~AmbilightAppInstance();
	
	quint8 getInstanceIndex() const { return _instIndex; }
	QSize getLedGridSize() const { return _ledGridSize; }
	bool getScanParameters(size_t led, double& hscanBegin, double& hscanEnd, double& vscanBegin, double& vscanEnd) const;
	unsigned updateSmoothingConfig(unsigned id, int settlingTime_ms = 200, double ledUpdateFrequency_hz = 25.0, bool directMode = false);

	static void signalTerminateTriggered();
	static bool isTerminated();
	static int getTotalRunningCount();

public slots:
	bool clear(int priority, bool forceClearAll = false);
	QJsonObject getAverageColor();
	bool hasPriority(int priority);
	ambilightapp::Components getComponentForPriority(int priority);
	ambilightapp::Components getCurrentPriorityActiveComponent();
	int getCurrentPriority() const;
	std::list<EffectDefinition> getEffects() const;
	int getLedCount() const;
	bool getReadOnlyMode() const { return _readOnlyMode; };
	QJsonDocument getSetting(settings::type type) const;
	int hasLedClock();
	void identifyLed(const QJsonObject& params);
	int isComponentEnabled(ambilightapp::Components comp) const;
	void putJsonConfig(QJsonObject& info) const;
	void putJsonInfo(QJsonObject& info, bool full);
	void registerInput(int priority, ambilightapp::Components component, const QString& origin = "System", const QString& owner = "", unsigned smooth_cfg = 0);
	void saveCalibration(QString saveData);
	bool saveSettings(const QJsonObject& config, bool correct = false);
	void setSignalStateByCEC(bool enable);
	bool setInputLeds(int priority, const std::vector<ColorRgb>& ledColors, int timeout_ms = -1, bool clearEffect = true);
	void setInputImage(int priority, const Image<ColorRgb>& image, int64_t timeout_ms = -1, bool clearEffect = true);
	void signalSetGlobalImageHandler(int priority, const Image<ColorRgb>& image, int timeout_ms, ambilightapp::Components origin, QString clientDescription);
	void setColor(int priority, const std::vector<ColorRgb>& ledColors, int timeout_ms = -1, const QString& origin = "System", bool clearEffects = true);
	void signalSetGlobalColorHandler(int priority, const std::vector<ColorRgb>& ledColor, int timeout_ms, ambilightapp::Components origin, QString clientDescription);
	bool setInputInactive(quint8 priority);
	void setLedMappingType(int mappingType);
	void setNewComponentState(ambilightapp::Components component, bool state);
	void setSetting(settings::type type, QString config);
	void setSourceAutoSelect(bool state);
	void setSmoothing(int time);
	bool setVisiblePriority(int priority);
	bool sourceAutoSelectEnabled() const;
	void start();
	void turnGrabbers(bool active);
	void update();
	void updateAdjustments(const QJsonObject& config);
	void updateResult(std::vector<ColorRgb> _ledBuffer);
	
	int setEffect(const QString& effectName, int priority, int timeout = -1, const QString& origin = "System");

signals:
	void SignalComponentStateChanged(ambilightapp::Components comp, bool state);
	void SignalPrioritiesChanged();
	void SignalVisiblePriorityChanged(quint8 priority);
	void SignalVisibleComponentChanged(ambilightapp::Components comp);
	void SignalInstancePauseChanged(int instance, bool isEnabled);
	void SignalRequestComponent(ambilightapp::Components component, bool enabled);
	void SignalImageToLedsMappingChanged(int mappingType);
	void SignalInstanceImageUpdated(const Image<ColorRgb>& ret);
	void SignalForwardJsonMessage(QJsonObject);
	void SignalInstanceSettingsChanged(settings::type type, const QJsonDocument& data);
	void SignalAdjustmentUpdated(const QJsonArray& newConfig);
	void SignalUpdateLeds(const std::vector<ColorRgb>& ledValues);
	void SignalSmoothingClockTick();
	void SignalSmoothingRestarted(int suggestedInterval);
	void SignalRawColorsChanged(const std::vector<ColorRgb>& ledValues);
	void SignalInstanceJustStarted();

private slots:
	void handleVisibleComponentChanged(ambilightapp::Components comp);
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);
	void handlePriorityChangedLedDevice(const quint8& priority);

private:
	const quint8	_instIndex;
	QTime			_bootEffect;
	LedString		_ledString;

	std::unique_ptr<InstanceConfig> _instanceConfig;
	std::unique_ptr<ComponentController> _componentController;
	std::unique_ptr<ImageToLedManager> _imageProcessor;
	std::unique_ptr<Muxer> _muxer;
	std::unique_ptr<LedCalibration> _ledColorCalibration;
	std::unique_ptr<LedDeviceWrapper> _ledDeviceWrapper;
	std::unique_ptr<Smoothing> _smoothing;
	std::unique_ptr<EffectEngine> _effectEngine;
	std::unique_ptr<VideoControl> _videoControl;
	std::unique_ptr<SystemControl> _systemControl;
	std::unique_ptr<BoblightServer> _boblightServer;
	std::unique_ptr<RawUdpServer> _rawUdpServer;

	Logger*				_log;
	int					_hwLedCount;
	QSize				_ledGridSize;

	std::vector<ColorRgb>	_currentLedColors;
	QString					_name;

	bool					_readOnlyMode;
	bool					_disableOnStartup;

	static std::atomic<bool>	_signalTerminate;
	static std::atomic<int>		_totalRunningCount;
	
	struct {
		qint64		token = 0;
		qint64		statBegin = 0;
		uint32_t	total = 0;
	} _computeStats;
};
