#pragma once

#include "PluginBase/Modules.h"

#include <convar.h>
#include <igameevents.h>
#include <shareddefs.h>

#include <vector>

class C_BaseCombatCharacter;
class CAccountPanel;
class CDamageAccountPanel;
class C_TFPlayer;
class IHandleEntity;
using trace_t = class CGameTrace;

class HitEvents final : public Module<HitEvents>, IGameEventListener2
{
public:
	HitEvents();
	virtual ~HitEvents();

	static bool CheckDependencies() { return true; }

protected:
	void FireGameEvent(IGameEvent* event) override;

	void LevelInit() override;
	void LevelShutdown() override;

private:
	std::vector<IGameEvent*> m_EventsToIgnore;

	void AddEventListener();

	void UpdateEnabledState();
	void Enable();
	void Disable();

	void DisplayDamageFeedbackOverride(CDamageAccountPanel* pThis, C_TFPlayer* pAttacker, C_BaseCombatCharacter* pVictim, int iDamageAmount, int iHealth, bool unknown);

	bool m_OverrideUTILTraceline;
	void UTILTracelineOverride(const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, const IHandleEntity* ignore, int collisionGroup, trace_t* ptr);

	bool DamageAccountPanelShouldDrawOverride(CDamageAccountPanel* pThis);

	int m_DisplayDamageFeedbackHook;
	int m_UTILTracelineHook;
	int m_DamageAccountPanelShouldDrawHook;

	CAccountPanel* m_LastDamageAccount;

	ConVar ce_hitevents_enabled;
	ConVar ce_hitevents_dmgnumbers_los;
	ConVar ce_hitevents_debug;

	IGameEvent* TriggerPlayerHurt(int playerEntIndex, int damage);
};