#pragma once

#include <leddevice/LedDevice.h>
#include <QSettings>

class QSerialPort;

class ProviderSerial : public LedDevice
{
	Q_OBJECT

public:
	ProviderSerial(const QJsonObject& deviceConfig);
	~ProviderSerial() override;

signals:
	void portChanged(const QString& instanceKey, const QString& newPort);

protected:

	bool init(const QJsonObject& deviceConfig) override;
	int open() override;
	int close() override;
	bool powerOff() override;
	QString discoverFirst() override;
	bool isAmbilightDevice(const QString& portName);
	bool hasSerialResponse(const QString& portName);
	QJsonObject discover(const QJsonObject& params) override;
	int writeBytes(const qint64 size, const uint8_t* data);

	QString _deviceName;	
	QSerialPort* _serialPort;
	qint32 _baudRate_Hz;

protected slots:
	void setInError(const QString& errorMsg) override;

public slots:
	bool waitForExitStats();

private:
	static QMutex s_portAccessMutex;
	static QMap<QString, QString> s_lastSuccessPorts;
	static QSettings s_settings;
	bool tryOpen(int delayAfterConnect_ms);
	bool _isAutoDeviceName;
	int _delayAfterConnect_ms;
	int _frameDropCounter;
	bool _espHandshake;
	bool _forceSerialDetection;
	QString _ledType;
	QString _instanceKey;
};
