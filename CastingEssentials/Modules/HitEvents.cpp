#include "HitEvents.h"
#include "Modules/CameraState.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFPlayerResource.h"

#include <client/c_baseentity.h>
#include <client/c_baseplayer.h>
#include <client/cdll_client_int.h>
#include <igameevents.h>
#include <toolframework/ienginetool.h>
#include <vgui_controls/EditablePanel.h>

HitEvents::HitEvents() :
	ce_hitevents_enabled("ce_hitevents_enabled", "0", FCVAR_NONE, "Enables hitsounds and damage numbers in STVs.",
		[](IConVar* var, const char* oldValue, float fOldValue) { GetModule()->UpdateEnabledState(); }),
	ce_hitevents_dmgnumbers_los("ce_hitevents_dmgnumbers_los", "1", FCVAR_NONE, "Should we require LOS to the target before showing a damage number? For a \"normal\" TF2 experience, this would be set to 1.", [](IConVar*, const char*, float) { GetModule()->UpdateEnabledState(); }),
	ce_hitevents_debug("ce_hitevents_debug", "0")
{
	m_OverrideUTILTraceline = false;

	m_DisplayDamageFeedbackHook = 0;
	m_UTILTracelineHook = 0;
	m_DamageAccountPanelShouldDrawHook = 0;

	m_LastDamageAccount = nullptr;
}

HitEvents::~HitEvents()
{
	ce_hitevents_enabled.SetValue(false);
}

IGameEvent* HitEvents::TriggerPlayerHurt(int playerEntIndex, int damage)
{
	IGameEvent* hurtEvent = gameeventmanager->CreateEvent("player_hurt");
	if (hurtEvent)
	{
		Player* player = Player::GetPlayer(playerEntIndex, __FUNCTION__);
		Player* localPlayer = Player::GetLocalPlayer();
		hurtEvent->SetInt("userid", player->GetUserID());
		hurtEvent->SetInt("attacker", localPlayer->GetUserID());
		//hurtEvent->SetInt("attacker", player->GetUserID());
		//hurtEvent->SetInt("userid", localPlayer->GetUserID());
		hurtEvent->SetInt("health", player->GetHealth());
		hurtEvent->SetInt("damageamount", damage);

		hurtEvent->SetInt("custom", 0);
		hurtEvent->SetBool("showdisguisedcrit", false);
		hurtEvent->SetBool("crit", false);
		hurtEvent->SetBool("minicrit", false);
		hurtEvent->SetBool("allseecrit", false);
		hurtEvent->SetInt("weaponid", 10);
		hurtEvent->SetInt("bonuseffect", 0);
	}

	return hurtEvent;
}

#pragma pack( 1 )

class CAccountPanel : public vgui::EditablePanel
{
public:
	virtual ~CAccountPanel() = default;

	struct Event
	{
		int m_HealthDelta;			// 0

		bool m_BigFont;				// 4

		uint16_t : 16;
		uint8_t : 8;

		int deltaType;				// 8

		float m_EndTime;			// 12

		int m_StartX;				// 16
		int m_EndX;					// 20
		int m_StartY;				// 24
		int m_EndY;					// 28

		int _unknown32;				// 32

		bool _unknown36;			// 36

		uint16_t : 16;
		uint8_t : 8;
		float m_BatchingWindow;		// 40
		int _unknown44;				// 44

		Color m_TextColor;			// 48

		bool _unknown52;			// 52

		uint8_t : 8;

		wchar_t _unknown54[9];		// 54
	};

private:
	static constexpr auto EVENT_SIZE = sizeof(Event);

	static_assert(EVENT_SIZE == 72);

	static constexpr auto test = offsetof(Event, m_EndTime);
	static_assert(offsetof(Event, m_BigFont) == 4);
	static_assert(offsetof(Event, deltaType) == 8);
	static_assert(offsetof(Event, m_EndTime) == 12);
	static_assert(offsetof(Event, _unknown32) == 32);
	static_assert(offsetof(Event, _unknown36) == 36);
	static_assert(offsetof(Event, m_BatchingWindow) == 40);
	static_assert(offsetof(Event, m_TextColor) == 48);
	static_assert(offsetof(Event, _unknown52) == 52);
	static_assert(offsetof(Event, _unknown54) == 54);

public:

	uint32_t : 32;
	CUtlVector<Event> m_Events;
	//Event* m_Events;

	//uint64_t : 64;
	//int m_EventCount;					// 412
	//CUtlVector<Event> m_Events;

	PADDING(8);
	float m_flDeltaItemStartPos;		// 428
	uint32_t : 32;
	float m_flDeltaItemEndPos;			// 436

	uint32_t : 32;
	float m_flDeltaItemX;				// 444
	uint32_t : 32;
	float m_flDeltaItemXEndPos;			// 452

	uint32_t : 32;
	float m_flBGImageX;					// 460
	uint32_t : 32;
	float m_flBGImageY;					// 468
	uint32_t : 32;
	float m_flBGImageWide;				// 476
	uint32_t : 32;
	float m_flBGImageTall;				// 484

	uint8_t : 8;
	Color m_DeltaPositiveColor;			// 489
	uint8_t : 8;
	Color m_DeltaNegativeColor;			// 494
	uint8_t : 8;
	Color m_DeltaEventColor;			// 499
	uint8_t : 8;
	Color m_DeltaRedRobotScoreColor;	// 504
	uint8_t : 8;
	Color m_DeltaBlueRobotScoreColor;	// 509

	uint16_t : 16;
	uint8_t : 8;

	float m_flDeltaLifetime;			// 516

	uint32_t : 32;
	vgui::HFont m_hDeltaItemFont;		// 524
	uint32_t : 32;
	vgui::HFont m_hDeltaItemFontBig;	// 532
};

static constexpr auto test = offsetof(CAccountPanel, m_flDeltaItemStartPos);
static_assert(offsetof(CAccountPanel, m_Events) == 400);
//static_assert(offsetof(CAccountPanel, m_EventCount) == 412);

static_assert(offsetof(CAccountPanel, m_flDeltaItemStartPos) == 428);
static_assert(offsetof(CAccountPanel, m_flDeltaItemEndPos) == 436);

static_assert(offsetof(CAccountPanel, m_flDeltaItemX) == 444);
static_assert(offsetof(CAccountPanel, m_flDeltaItemXEndPos) == 452);

static_assert(offsetof(CAccountPanel, m_flBGImageX) == 460);
static_assert(offsetof(CAccountPanel, m_flBGImageY) == 468);
static_assert(offsetof(CAccountPanel, m_flBGImageWide) == 476);
static_assert(offsetof(CAccountPanel, m_flBGImageTall) == 484);

static_assert(offsetof(CAccountPanel, m_DeltaPositiveColor) == 489);
static_assert(offsetof(CAccountPanel, m_DeltaNegativeColor) == 494);
static_assert(offsetof(CAccountPanel, m_DeltaEventColor) == 499);
static_assert(offsetof(CAccountPanel, m_DeltaRedRobotScoreColor) == 504);
static_assert(offsetof(CAccountPanel, m_DeltaBlueRobotScoreColor) == 509);

static_assert(offsetof(CAccountPanel, m_flDeltaLifetime) == 516);

static_assert(offsetof(CAccountPanel, m_hDeltaItemFont) == 524);
static_assert(offsetof(CAccountPanel, m_hDeltaItemFontBig) == 532);

struct PaddingStruct
{
	uint64_t : 64;
	uint64_t : 64;
	uint64_t : 64;
	uint64_t : 64;
	uint64_t : 64;
	uint32_t : 32;
};

class CDamageAccountPanel : public CAccountPanel
{
public:
	virtual ~CDamageAccountPanel() = default;

	int garbo;
};

static_assert(offsetof(CDamageAccountPanel, garbo) > offsetof(CAccountPanel, m_hDeltaItemFontBig));

void HitEvents::FireGameEvent(IGameEvent* event)
{
	if (stricmp(event->GetName(), "player_hurt"))
		return;

	// Don't die to infinite recursion
	if (auto found = std::find(m_EventsToIgnore.begin(), m_EventsToIgnore.end(), event); found != m_EventsToIgnore.end())
	{
		m_EventsToIgnore.erase(found);
		return;
	}

	if (event->GetInt("userid") == event->GetInt("attacker"))
		return;	// Early-out self damage

	Player* localPlayer = Player::GetLocalPlayer();
	if (!localPlayer)
		return;

	auto specTarget = Player::AsPlayer(CameraState::GetLocalObserverTarget());
	if (!specTarget || specTarget->GetUserID() != event->GetInt("attacker"))
		return;

	/*{
		auto newEvent = TriggerPlayerHurt(specTarget->entindex(), 123);
		m_EventsToIgnore.push_back(newEvent);
		gameeventmanager->FireEventClientSide(newEvent);
		return;
	}*/

	if (ce_hitevents_debug.GetBool())
	{
		std::stringstream msg;
		msg << std::boolalpha;

		auto attackedPlayer = Player::GetPlayerFromUserID(event->GetInt("userid"));
		auto attackingPlayer = Player::GetPlayerFromUserID(event->GetInt("attacker"));

		msg << "Received " << event->GetName() << " game event:\n"
			<< "\tuserid:            " << event->GetInt("userid") << " (" << (attackedPlayer ? attackedPlayer->GetName() : "<???>") << ")\n"
			<< "\tattacker:          " << event->GetInt("attacker") << " (" << (attackingPlayer ? attackingPlayer->GetName() : "<???>") << ")\n"
			<< "\tdamageamount:      " << event->GetInt("damageamount") << '\n';

		if (ce_hitevents_debug.GetInt() > 1)
		{
			msg << "\thealth:            " << event->GetInt("health") << '\n'
				<< "\tcustom:            " << event->GetInt("custom") << '\n'
				<< "\tshowdisguisedcrit: " << event->GetBool("showdisguisedcrit") << '\n'
				<< "\tcrit:              " << event->GetBool("crit") << '\n'
				<< "\tminicrit:          " << event->GetBool("minicrit") << '\n'
				<< "\tallseecrit:        " << event->GetBool("allseecrit") << '\n'
				<< "\tweaponid:          " << event->GetInt("weaponid") << '\n'
				<< "\tbonuseffect:       " << event->GetInt("bonuseffect") << '\n';
		}

		PluginMsg("%s", msg.str().c_str());
	}

	// Clone event
	IGameEvent* newEvent = gameeventmanager->CreateEvent("player_hurt");
	newEvent->SetInt("userid", event->GetInt("userid"));
	newEvent->SetInt("health", event->GetInt("health"));
	newEvent->SetInt("attacker", event->GetInt("attacker"));
	newEvent->SetInt("damageamount", event->GetInt("damageamount"));

	newEvent->SetInt("custom", event->GetInt("custom"));
	newEvent->SetBool("showdisguisedcrit", event->GetBool("showdisguisedcrit"));
	newEvent->SetBool("crit", event->GetBool("crit"));
	newEvent->SetBool("minicrit", event->GetBool("minicrit"));
	newEvent->SetBool("allseecrit", event->GetBool("allseecrit"));
	newEvent->SetInt("weaponid", event->GetInt("weaponid"));
	newEvent->SetInt("bonuseffect", event->GetInt("bonuseffect"));

	// Override attacker
	newEvent->SetInt("attacker", localPlayer->GetUserID());
	/*Player* localPlayer = Player::GetLocalPlayer();
	if (localPlayer)
	{
		auto attacker = Player::AsPlayer(localPlayer->GetObserverTarget());
		if (attacker && attacker->GetUserID() == newEvent->GetInt("attacker"))
			newEvent->SetInt("attacker", localPlayer->GetUserID());
	}*/

	if (ce_hitevents_debug.GetInt())
	{
		auto attackerUserID = newEvent->GetInt("attacker");
		PluginMsg("Rebroadcasting with attacker = %i (%s)\n", attackerUserID, Player::GetPlayerFromUserID(attackerUserID)->GetName());
	}

	m_EventsToIgnore.push_back(newEvent);

	//VariablePusher<C_BasePlayer*> localPlayerPusher(Interfaces::GetLocalPlayer(), );
	gameeventmanager->FireEventClientSide(newEvent);
}

void HitEvents::AddEventListener()
{
	if (!gameeventmanager->FindListener(this, "player_hurt"))
		gameeventmanager->AddListener(this, "player_hurt", false);
}

void HitEvents::LevelInit()
{
	UpdateEnabledState();
}

void HitEvents::LevelShutdown()
{
	UpdateEnabledState();
}

void HitEvents::Enable()
{
	if (!gameeventmanager->FindListener(this, "player_hurt"))
		gameeventmanager->AddListener(this, "player_hurt", false);

	if (!m_DisplayDamageFeedbackHook)
	{
		m_DisplayDamageFeedbackHook = GetHooks()->AddHook<HookFunc::CDamageAccountPanel_DisplayDamageFeedback>(std::bind(&HitEvents::DisplayDamageFeedbackOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
	}
	if (!m_UTILTracelineHook)
	{
		m_UTILTracelineHook = GetHooks()->AddHook<HookFunc::Global_UTIL_TraceLine>(std::bind(&HitEvents::UTILTracelineOverride, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
	}
	if (!m_DamageAccountPanelShouldDrawHook)
	{
		m_DamageAccountPanelShouldDrawHook = GetHooks()->AddHook<HookFunc::CDamageAccountPanel_ShouldDraw>(std::bind(&HitEvents::DamageAccountPanelShouldDrawOverride, this, std::placeholders::_1));
	}
}

void HitEvents::Disable()
{
	if (m_DisplayDamageFeedbackHook && GetHooks()->RemoveHook<HookFunc::CDamageAccountPanel_DisplayDamageFeedback>(m_DisplayDamageFeedbackHook, __FUNCSIG__))
		m_DisplayDamageFeedbackHook = 0;

	if (m_UTILTracelineHook && GetHooks()->RemoveHook<HookFunc::Global_UTIL_TraceLine>(m_UTILTracelineHook, __FUNCSIG__))
		m_UTILTracelineHook = 0;

	if (m_DamageAccountPanelShouldDrawHook && GetHooks()->RemoveHook<HookFunc::CDamageAccountPanel_ShouldDraw>(m_DamageAccountPanelShouldDrawHook, __FUNCSIG__))
		m_DamageAccountPanelShouldDrawHook = 0;

	gameeventmanager->RemoveListener(this);
}

void HitEvents::UpdateEnabledState()
{
	if (ce_hitevents_enabled.GetBool() && IsInGame())
	{
		Enable();
	}
	else
	{
		Disable();
	}
}

void HitEvents::DisplayDamageFeedbackOverride(CDamageAccountPanel* pThis, C_TFPlayer* pAttacker, C_BaseCombatCharacter* pVictim, int iDamageAmount, int iHealth, bool bigFont)
{
	//CDamageAccountPanel* questionable = (CDamageAccountPanel*)((std::byte*)pThis + 44);

	auto localPlayer = C_BasePlayer::GetLocalPlayer();

	Assert(localPlayer->IsPlayer());

	auto localEntindex = localPlayer->entindex();

	auto otherLocalAttacker = (C_BaseEntity*)pAttacker;
	auto otherLocalEntindex = otherLocalAttacker ? otherLocalAttacker->entindex() : -1;
	if (localEntindex != otherLocalEntindex)
		return;

	const auto lifeStatePusher = CreateVariablePusher<char>(localPlayer->m_lifeState, LIFE_ALIVE);
	const auto tracelineOverridePusher = CreateVariablePusher(m_OverrideUTILTraceline, true);

	auto DisplayDamageFeedback = GetHooks()->GetOriginal<HookFunc::CDamageAccountPanel_DisplayDamageFeedback>();
	DisplayDamageFeedback(pThis, (C_TFPlayer*)localPlayer, pVictim, iDamageAmount, iHealth, bigFont);
	GetHooks()->SetState<HookFunc::CDamageAccountPanel_DisplayDamageFeedback>(Hooking::HookAction::SUPERCEDE);
	Assert(true);
}

void HitEvents::UTILTracelineOverride(const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, const IHandleEntity* ignore, int collisionGroup, trace_t* ptr)
{
	if (m_OverrideUTILTraceline && ce_hitevents_dmgnumbers_los.GetBool())
	{
		ptr->fraction = 1.0;
		GetHooks()->SetState<HookFunc::Global_UTIL_TraceLine>(Hooking::HookAction::SUPERCEDE);
	}
}

bool HitEvents::DamageAccountPanelShouldDrawOverride(CDamageAccountPanel* pThis)
{
	GetHooks()->SetState<HookFunc::CDamageAccountPanel_ShouldDraw>(Hooking::HookAction::SUPERCEDE);

	// Not too sure why this offset is required, maybe i'm just an idiot
	CDamageAccountPanel* questionable = (CDamageAccountPanel*)((std::byte*)pThis + 44);

	if (ce_hitevents_debug.GetBool())
		engine->Con_NPrintf(5, "account panel entries: %i", questionable->m_Events.Count());

	return questionable->m_Events.Count() > 0;
}