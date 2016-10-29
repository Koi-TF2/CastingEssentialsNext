#pragma once

class ConCommand;
class ConVar;
class IConVar;

#include "PluginBase/Modules.h"

#include <memory>

class LocalPlayer : public Module
{
public:
	LocalPlayer();

	static bool CheckDependencies();
	static LocalPlayer* GetModule() { return Modules().GetModule<LocalPlayer>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<LocalPlayer>().c_str(); }
private:
	int GetLocalPlayerIndexOverride();

	int m_GetLocalPlayerIndexHookID;

	class TickPanel;
	std::unique_ptr<TickPanel> m_Panel;

	ConVar *enabled;
	ConVar *player;
	ConCommand *set_current_target;
	ConVar *track_spec_target;
	void SetToCurrentTarget();
	void ToggleEnabled(IConVar *var, const char *pOldValue, float flOldValue);
	void ToggleTrackSpecTarget(IConVar *var, const char *pOldValue, float flOldValue);
};