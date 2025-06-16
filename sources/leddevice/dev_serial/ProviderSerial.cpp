#include <AmbilightappConfig.h>

#ifdef ENABLE_BONJOUR
	#include <bonjour/DiscoveryWrapper.h>	
#endif

// LedDevice includes
#include <leddevice/LedDevice.h>
#include "ProviderSerial.h"

// qt includes
#include <QSerialPortInfo>
#include <QEventLoop>
#include <QThread>
#include <QSerialPort>
#include <QSettings>
#include <QMutex>
#include <QMutexLocker>

#include <utils/InternalClock.h>
#include <utils/GlobalSignals.h>

#include "EspTools.h"

// Constants
constexpr std::chrono::milliseconds WRITE_TIMEOUT{ 1000 };	// device write timeout in ms
constexpr std::chrono::milliseconds OPEN_TIMEOUT{ 5000 };		// device open timeout in ms
const int MAX_WRITE_TIMEOUTS = 5;	// Maximum number of allowed timeouts
const int NUM_POWEROFF_WRITE_BLACK = 3;	// Number of write "BLACK" during powering off

QMap<QString, QString> ProviderSerial::s_lastSuccessPorts;
QMutex ProviderSerial::s_portAccessMutex;
QSettings ProviderSerial::s_settings("HKEY_CURRENT_USER\\Software\\AmbilightApp", QSettings::NativeFormat);

ProviderSerial::ProviderSerial(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
	, _serialPort(nullptr)
	, _baudRate_Hz(1000000)
	, _isAutoDeviceName(false)
	, _delayAfterConnect_ms(1000)
	, _frameDropCounter(0)
	, _espHandshake(false)
	, _forceSerialDetection(false)
	, _ledType("screen")
{
	// Đọc thông tin cổng đã lưu khi khởi tạo instance đầu tiên
	static bool isFirstInstance = true;
	if (isFirstInstance)
	{
		QMutexLocker locker(&s_portAccessMutex);
		s_settings.beginGroup("Instances");
		QStringList instances = s_settings.childGroups();
		for (const QString& instance : instances)
		{
			s_settings.beginGroup(instance);
			if (s_settings.contains("Port"))
			{
				s_lastSuccessPorts[instance] = s_settings.value("Port").toString();
			}
			s_settings.endGroup();
		}
		s_settings.endGroup();
		isFirstInstance = false;
	}
}

bool ProviderSerial::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	if (_serialPort == nullptr)
		_serialPort = new QSerialPort(this);

	if (LedDevice::init(deviceConfig))
	{

		Debug(_log, "DeviceType   : %s", QSTRING_CSTR(this->getActiveDeviceType()));
		Debug(_log, "LedCount     : %d", this->getLedCount());
		Debug(_log, "RefreshTime  : %d", this->getRefreshTime());

		_deviceName = deviceConfig["output"].toString("auto");

		// If device name was given as unix /dev/ system-location, get port name
		if (_deviceName.startsWith(QLatin1String("/dev/")))
			_deviceName = _deviceName.mid(5);

		_isAutoDeviceName = _deviceName.toLower() == "auto";
		_baudRate_Hz = deviceConfig["rate"].toInt(1000000);
		_delayAfterConnect_ms = deviceConfig["delayAfterConnect"].toInt(1000);
		_espHandshake = deviceConfig["espHandshake"].toBool(false);
		_maxRetry = _devConfig["maxRetry"].toInt(10);
		_forceSerialDetection = deviceConfig["forceSerialDetection"].toBool(false);
		_ledType = deviceConfig["ledType"].toString("screen");

		_instanceKey = QString("Instance_%1").arg(this->getInstanceIndex());

		Debug(_log, "Device name   : %s", QSTRING_CSTR(_deviceName));
		Debug(_log, "Auto selection: %d", _isAutoDeviceName);
		Debug(_log, "Baud rate     : %d", _baudRate_Hz);
		Debug(_log, "ESP handshake : %s", (_espHandshake) ? "ON" : "OFF");
		Debug(_log, "Force ESP/Pico Detection : %s", (_forceSerialDetection) ? "ON" : "OFF");
		Debug(_log, "Delayed open  : %d", _delayAfterConnect_ms);
		Debug(_log, "Retry limit   : %d", _maxRetry);

		if (_defaultInterval > 0)
			Warning(_log, "The refresh timer is enabled ('Refresh time' > 0) and may limit the performance of the LED driver. Ignore this error if you set it on purpose for some reason (but you almost never need it).");

		isInitOK = true;
	}
	return isInitOK;
}

ProviderSerial::~ProviderSerial()
{
	if (_serialPort != nullptr && _serialPort->isOpen())
		_serialPort->close();

	delete _serialPort;

	_serialPort = nullptr;
}

int ProviderSerial::open()
{
	int retval = -1;

	if (_serialPort == nullptr)
		return retval;

	_isDeviceReady = false;

	// open device physically
	if (tryOpen(_delayAfterConnect_ms))
	{
		// Everything is OK, device is ready
		_isDeviceReady = true;
		retval = 0;
	}
	else
	{
		setupRetry(2500);
	}
	return retval;
}

bool ProviderSerial::waitForExitStats()
{
	if (_serialPort->isOpen())
	{
		if (_serialPort->bytesAvailable() > 32)
		{
			auto check = _serialPort->peek(256);
			if (check.lastIndexOf('\n') > 1)
			{
				auto incoming = QString(_serialPort->readAll());

				Info(_log, "Received goodbye: '%s' (%i)", QSTRING_CSTR(incoming), incoming.length());
				return true;
			}
		}		
	}

	return false;
}

int ProviderSerial::close()
{
	int retval = 0;

	_isDeviceReady = false;

	if (_serialPort != nullptr && _serialPort->isOpen())
	{
		if (_serialPort->flush())
		{
			Debug(_log, "Flush was successful");
		}


		// if (_espHandshake)
		// {
		QTimer::singleShot(200, this, [this]() { if (_serialPort->isOpen()) EspTools::goingSleep(_serialPort); });
		EspTools::goingSleep(_serialPort);

		for (int i = 0; i < 6 && _serialPort->isOpen(); i++)
		{
			if (_serialPort->waitForReadyRead(100) && waitForExitStats())
				break;
		}
		// }

		_serialPort->close();

		Debug(_log, "Serial port is closed: %s", QSTRING_CSTR(_deviceName));

	}
	return retval;
}

bool ProviderSerial::powerOff()
{
	// Simulate power-off by writing a final "Black" to have a defined outcome
	bool rc = false;
	if (writeBlack(NUM_POWEROFF_WRITE_BLACK) >= 0)
	{
		rc = true;
	}
	return rc;
}

bool ProviderSerial::tryOpen(int delayAfterConnect_ms)
{
	if (_deviceName.isEmpty() || _serialPort->portName().isEmpty() || _isAutoDeviceName)
	{
		if (!_serialPort->isOpen())
		{
			if (_isAutoDeviceName)
			{
				_deviceName = discoverFirst();
				if (_deviceName.isEmpty())
				{
					this->setInError(QString("No serial device found automatically!"));
					return false;
				}
			}
		}

		_serialPort->setPortName(_deviceName);
	}

	if (!_serialPort->isOpen())
	{
		Info(_log, "Opening UART: %s", QSTRING_CSTR(_deviceName));

		_frameDropCounter = 0;

		_serialPort->setBaudRate(_baudRate_Hz);

		Debug(_log, "_serialPort.open(QIODevice::ReadWrite): %s, Baud rate [%d]bps", QSTRING_CSTR(_deviceName), _baudRate_Hz);

		QSerialPortInfo serialPortInfo(_deviceName);

		QJsonObject portInfo;
		Debug(_log, "portName:          %s", QSTRING_CSTR(serialPortInfo.portName()));
		Debug(_log, "systemLocation:    %s", QSTRING_CSTR(serialPortInfo.systemLocation()));
		Debug(_log, "description:       %s", QSTRING_CSTR(serialPortInfo.description()));
		Debug(_log, "manufacturer:      %s", QSTRING_CSTR(serialPortInfo.manufacturer()));
		Debug(_log, "productIdentifier: %s", QSTRING_CSTR(QString("0x%1").arg(serialPortInfo.productIdentifier(), 0, 16)));
		Debug(_log, "vendorIdentifier:  %s", QSTRING_CSTR(QString("0x%1").arg(serialPortInfo.vendorIdentifier(), 0, 16)));
		Debug(_log, "serialNumber:      %s", QSTRING_CSTR(serialPortInfo.serialNumber()));

		if (!serialPortInfo.isNull())
		{
			if (!_serialPort->isOpen() && !_serialPort->open(QIODevice::ReadWrite))
			{
				this->setInError(_serialPort->errorString());
				return false;
			}

			disconnect(_serialPort, &QSerialPort::readyRead, nullptr, nullptr);

			if (_espHandshake)
			{
				EspTools::initializeEsp(_serialPort, serialPortInfo, _log, _forceSerialDetection);
			}

			// Lưu cổng kết nối thành công vào Registry nếu khác với cổng đã lưu
			QString savedPort = s_lastSuccessPorts.value(_instanceKey);
			if (savedPort != _deviceName)
			{
				s_lastSuccessPorts[_instanceKey] = _deviceName;
				QMutexLocker locker(&s_portAccessMutex);
				s_settings.beginGroup("Instances");
				s_settings.beginGroup(_instanceKey);
				s_settings.setValue("Port", _deviceName);
				s_settings.endGroup();
				s_settings.endGroup();
				s_settings.sync();
				Info(_log, "Saved new successful port connection: %s for instance %s", 
					QSTRING_CSTR(_deviceName), QSTRING_CSTR(_instanceKey));
				
				// Phát signal khi cổng thay đổi thông qua GlobalSignals
				emit GlobalSignals::getInstance()->SignalPortChanged(_instanceKey, _deviceName);
			}
		}
		else
		{
			QString errortext = QString("Invalid serial device name: [%1]!").arg(_deviceName);
			this->setInError(errortext);
			return false;
		}
	}

	if (delayAfterConnect_ms > 0)
	{
		Debug(_log, "delayAfterConnect for %d ms - start", delayAfterConnect_ms);

		// Wait delayAfterConnect_ms before allowing write
		QEventLoop loop;
		QTimer::singleShot(delayAfterConnect_ms, &loop, &QEventLoop::quit);
		loop.exec();

		Debug(_log, "delayAfterConnect for %d ms - finished", delayAfterConnect_ms);
	}

	return _serialPort->isOpen();
}

void ProviderSerial::setInError(const QString& errorMsg)
{
	_serialPort->clearError();
	this->close();

	LedDevice::setInError(errorMsg);
}

int ProviderSerial::writeBytes(const qint64 size, const uint8_t* data)
{
	int rc = 0;
	if (!_serialPort->isOpen())
	{
		Debug(_log, "!_serialPort.isOpen()");

		if (!tryOpen(OPEN_TIMEOUT.count()))
		{
			return -1;
		}
	}
	qint64 bytesWritten = _serialPort->write(reinterpret_cast<const char*>(data), size);
	if (bytesWritten == -1 || bytesWritten != size)
	{
		this->setInError(QString("Serial port error: %1").arg(_serialPort->errorString()));
		rc = -1;
	}
	else
	{
		if (!_serialPort->waitForBytesWritten(WRITE_TIMEOUT.count()))
		{
			if (_serialPort->error() == QSerialPort::TimeoutError)
			{
				Debug(_log, "Timeout after %dms: %d frames already dropped", WRITE_TIMEOUT, _frameDropCounter);

				++_frameDropCounter;

				// Check,if number of timeouts in a given time frame is greater than defined
				// TODO: ProviderRs232::writeBytes - Add time frame to check for timeouts that devices does not close after absolute number of timeouts
				if (_frameDropCounter > MAX_WRITE_TIMEOUTS)
				{
					this->setInError(QString("Timeout writing data to %1").arg(_deviceName));
					rc = -1;
				}
				else
				{
					//give it another try
					_serialPort->clearError();
				}
			}
			else
			{
				this->setInError(QString("Rs232 SerialPortError: %1").arg(_serialPort->errorString()));
				rc = -1;
			}
		}
	}

	if (_maxRetry > 0 && rc == -1 && !_signalTerminate)
	{
		QTimer::singleShot(2000, this, [this]() { if (!_signalTerminate) enable(); });
	}

	return rc;
}

QString ProviderSerial::discoverFirst()
{   
    // Thử kết nối với cổng đã biết trước
    if (s_lastSuccessPorts.contains(_instanceKey))
    {
        QString lastPort = s_lastSuccessPorts[_instanceKey];
        for (const auto& port : QSerialPortInfo::availablePorts())
        {
            if (port.portName() == lastPort)
            {
                quint16 vendor = port.vendorIdentifier();
                quint16 prodId = port.productIdentifier();
                bool isCH340 = (vendor == 0x1a86 && prodId == 0x7523);
                
                if (isCH340)
                {
                    Info(_log, "Trying last successful port: %s", QSTRING_CSTR(lastPort));
                    if (isAmbilightDevice(lastPort))
                    {
                        Info(_log, "Reconnected to last port: %s", QSTRING_CSTR(lastPort));
                        return lastPort;
                    }
                    // Xóa port không còn phù hợp
                    s_lastSuccessPorts.remove(_instanceKey);
                    QMutexLocker locker(&s_portAccessMutex);
                    s_settings.beginGroup("Instances");
                    s_settings.beginGroup(_instanceKey);
                    s_settings.remove("Port");
                    s_settings.endGroup();
                    s_settings.endGroup();
                    s_settings.sync();
                    break;
                }
            }
        }
    }
    
    // Tìm cổng mới nếu không có cổng đã biết
    static int nextPortIndex = 0;
    QList<QSerialPortInfo> ch340Ports;
    
    for (const auto& port : QSerialPortInfo::availablePorts())
    {
        if (!port.isNull())
        {
            quint16 vendor = port.vendorIdentifier();
            quint16 prodId = port.productIdentifier();
            bool isCH340 = (vendor == 0x1a86 && prodId == 0x7523) || (vendor == 0x403 && prodId == 0x6001);
            
            if (isCH340)
            {
                ch340Ports.append(port);
            }
        }
    }
    
    if (ch340Ports.isEmpty())
    {
        Debug(_log, "No devices found");
        nextPortIndex = 0;
        return "";
    }

    int startIndex = nextPortIndex++ % ch340Ports.size();
	static int fallbackPortIndex = 0;
    if (!_espHandshake)
	{
		for (int i = 0; i < ch340Ports.size(); i++)
		{
			int currentIndex = (startIndex + i) % ch340Ports.size();
			const auto& port = ch340Ports[currentIndex];
			
			QString infoMessage = QString("%1 (%2 => %3)")
				.arg(port.description())
				.arg(port.systemLocation())
				.arg(port.portName());

			if (isAmbilightDevice(port.portName())) return port.portName();
		}
	}
	else
	{
		if (!ch340Ports.isEmpty())
		{        
			int currentIndex = fallbackPortIndex % ch340Ports.size();
			const auto& port = ch340Ports[currentIndex];
			
			// Nếu KHÔNG phản hồi thì mới tiếp tục với cổng này
			if (!hasSerialResponse(port.portName()))
			{   
				// Chuẩn bị index cho lần gọi tiếp theo
				fallbackPortIndex = (currentIndex + 1) % ch340Ports.size();
				return port.portName();
			}
			
			// Thử cổng tiếp theo nếu cổng hiện tại có phản hồi
			fallbackPortIndex = (currentIndex + 1) % ch340Ports.size();
		}
	}
    
    return "";
}

bool ProviderSerial::isAmbilightDevice(const QString& portName)
{
	QMutexLocker locker(&s_portAccessMutex);

	QSerialPort testPort;
	testPort.setPortName(portName);

	QString searchType = _ledType;
    if (_ledType == "dualscreen")
        searchType = "screen";
	
	Debug(_log, "Checking port: %s for LED type: %s", QSTRING_CSTR(portName), QSTRING_CSTR(searchType));
	
	if (testPort.open(QIODevice::ReadWrite))
	{
		testPort.setBaudRate(_baudRate_Hz);
		testPort.clear();
		testPort.write("T");
		testPort.flush();
		
		auto start = InternalClock::now();
		while (InternalClock::now() - start < 1000)
		{
			testPort.waitForReadyRead(100);
			if (testPort.bytesAvailable() > 0)
			{
				auto incoming = testPort.readAll();
				for (int i = 0; i < incoming.length(); i++)
					if (!(incoming[i] == '\n' ||
						(incoming[i] >= ' ' && incoming[i] <= 'Z') ||
						(incoming[i] >= 'a' && incoming[i] <= 'z')))
					{
						incoming.replace(incoming[i], '*');
					}
				
				QString result = QString(incoming).remove('*').replace('\n', ' ').trimmed();
				
				if (result.contains(searchType, Qt::CaseInsensitive))
				{
					Debug(_log, "Found matching LED device (%s) on port: %s", QSTRING_CSTR(searchType), QSTRING_CSTR(portName));
					testPort.close();
					return true;
				}
			}
		}

		Debug(_log, "No matching LED device (%s) found on port: %s", QSTRING_CSTR(searchType), QSTRING_CSTR(portName));
		testPort.close();
		return false;
	}
	
	Debug(_log, "Failed to open port: %s", QSTRING_CSTR(testPort.errorString()));
	return false;
}

bool ProviderSerial::hasSerialResponse(const QString& portName)
{
    QMutexLocker locker(&s_portAccessMutex);

    QSerialPort testPort;
    testPort.setPortName(portName);
    
    Debug(_log, "Checking port response: %s", QSTRING_CSTR(portName));
    
    if (testPort.open(QIODevice::ReadWrite))
    {
        testPort.setBaudRate(_baudRate_Hz);
        
        testPort.write("T");
        testPort.flush();
        
        auto start = InternalClock::now();
        while (InternalClock::now() - start < 1000)
        {
            testPort.waitForReadyRead(100);
            if (testPort.bytesAvailable() > 0)
            {
                Debug(_log, "Got response from port: %s", QSTRING_CSTR(portName));
                testPort.close();
                return true;
            }
        }

        Debug(_log, "No response from port: %s", QSTRING_CSTR(portName));
        testPort.close();
        return false;
    }
    
    Debug(_log, "Failed to open port: %s", QSTRING_CSTR(testPort.errorString()));
    return false;
}

QJsonObject ProviderSerial::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;
	QJsonArray deviceList;

	deviceList.push_back(QJsonObject{
			{"value", "auto"},
			{ "name", "Auto"} });

	for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts())
	{
		quint16 vendor = info.vendorIdentifier();
		quint16 prodId = info.productIdentifier();
		
		// Lọc chỉ hiển thị CH340 và FTDI
		bool isCH340 = (vendor == 0x1a86 && prodId == 0x7523);
		bool isFTDI = (vendor == 0x403 && prodId == 0x6001);
		
		if (isCH340 || isFTDI)
		{
			deviceList.push_back(QJsonObject{
				{ "value", info.portName() },
				{ "name", QString("%1 (%2)").arg(info.portName()).arg(info.description()) }
			});
		}

#ifdef ENABLE_BONJOUR
		// quint16 vendor = info.vendorIdentifier();
		// quint16 prodId = info.productIdentifier();
		DiscoveryRecord newRecord;

		if ((vendor == 0x303a) && (prodId == 0x80c2))
		{
			newRecord.type = DiscoveryRecord::Service::ESP32_S2;
		}
		else if ((vendor == 0x2e8a) && (prodId == 0xa))
		{
			newRecord.type = DiscoveryRecord::Service::Pico;
		}
		else if ((vendor == 0x303a) ||
			((vendor == 0x10c4) && (prodId == 0xea60)) ||
			((vendor == 0x1A86) && (prodId == 0x7523 || prodId == 0x55d4)) ||
            ((vendor == 0x403) && (prodId == 0x6001)))
		{
			newRecord.type = DiscoveryRecord::Service::ESP;
		}

		if (newRecord.type != DiscoveryRecord::Service::Unknown)
		{			
			newRecord.hostName = info.description();
			newRecord.address = info.portName();
			newRecord.isExists = true;
			emit GlobalSignals::getInstance()->SignalDiscoveryEvent(newRecord);
		}
#endif
	}


	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);
	devicesDiscovered.insert("devices", deviceList);

#ifndef ENABLE_BONJOUR
	Debug(_log, "Serial devices discovered: [%s]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());
#endif

	return devicesDiscovered;
}
