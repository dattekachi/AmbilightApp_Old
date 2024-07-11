#pragma once

#ifndef PCH_ENABLED
	#include <map>
	#include <QObject>

	#include <utils/Logger.h>
	#include <utils/Components.h>
#endif

class AmbilightAppInstance;

class ComponentController : public QObject
{
	Q_OBJECT

public:
    ComponentController(AmbilightAppInstance* ambilightapp, bool disableOnStartup);
    virtual ~ComponentController();

	int isComponentEnabled(ambilightapp::Components comp) const;
	const std::map<ambilightapp::Components, bool>& getComponentsState() const;

signals:
	void SignalComponentStateChanged(ambilightapp::Components comp, bool state);
	void SignalRequestComponent(ambilightapp::Components component, bool enabled);

public slots:
	void setNewComponentState(ambilightapp::Components comp, bool activated);
	void turnGrabbers(bool activated);

private slots:
	void handleCompStateChangeRequest(ambilightapp::Components comps, bool activated);

private:
	Logger* _log;
	std::map<ambilightapp::Components, bool> _componentStates;
	std::map<ambilightapp::Components, bool> _prevComponentStates;
	std::map<ambilightapp::Components, bool> _prevGrabbers;
	bool _disableOnStartup;
};