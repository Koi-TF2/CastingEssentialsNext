#include "Player.h"
#include "Interfaces.h"
#include "Entities.h"
#include "TFDefinitions.h"
#include "HookManager.h"
#include "Misc/HLTVCameraHack.h"
#include "Modules/ItemSchema.h"
#include <cdll_int.h>
#include <icliententity.h>
#include <steam/steam_api.h>
#include <client/c_baseentity.h>
#include <toolframework/ienginetool.h>
#include "TFPlayerResource.h"
#include <client/hltvcamera.h>
#include <client/c_basecombatweapon.h>

#undef min

bool Player::s_ClassRetrievalAvailable = false;
bool Player::s_ComparisonAvailable = false;
bool Player::s_ConditionsRetrievalAvailable = false;
bool Player::s_NameRetrievalAvailable = false;
bool Player::s_SteamIDRetrievalAvailable = false;
bool Player::s_UserIDRetrievalAvailable = false;

std::unique_ptr<Player> Player::s_Players[ABSOLUTE_PLAYER_LIMIT];

int Player::s_UserInfoChangedCallbackHook;

Player::Player(CHandle<IClientEntity> handle, int userID) : m_PlayerEntity(handle), m_UserID(userID)
{
	Assert(dynamic_cast<C_BaseEntity*>(handle.Get()));
	m_CachedPlayerEntity = nullptr;
}

void Player::Load()
{
	if (!s_UserInfoChangedCallbackHook)
	{
		s_UserInfoChangedCallbackHook = GetHooks()->AddHook<HookFunc::Global_UserInfoChangedCallback>(std::bind(&Player::UserInfoChangedCallbackOverride, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
	}
}

void Player::Unload()
{
	if (s_UserInfoChangedCallbackHook && GetHooks()->RemoveHook<HookFunc::Global_UserInfoChangedCallback>(s_UserInfoChangedCallbackHook, __FUNCSIG__))
		s_UserInfoChangedCallbackHook = 0;

	for (size_t i = 0; i < arraysize(s_Players); i++)
		s_Players[i].reset();
}

bool Player::CheckDependencies()
{
	bool ready = true;

	if (!Interfaces::GetClientEntityList())
	{
		PluginWarning("Required interface IClientEntityList for player helper class not available!\n");
		ready = false;
	}

	if (!Interfaces::GetEngineTool())
	{
		PluginWarning("Required interface IEngineTool for player helper class not available!\n");
		ready = false;
	}

	s_ClassRetrievalAvailable = true;
	s_ComparisonAvailable = true;
	s_ConditionsRetrievalAvailable = true;
	s_NameRetrievalAvailable = true;
	s_SteamIDRetrievalAvailable = true;
	s_UserIDRetrievalAvailable = true;

	if (!Interfaces::GetEngineClient())
	{
		PluginWarning("Interface IVEngineClient for player helper class not available (required for retrieving certain info)!\n");

		s_NameRetrievalAvailable = false;
		s_SteamIDRetrievalAvailable = false;
		s_UserIDRetrievalAvailable = false;
	}

	if (!Interfaces::GetHLTVCamera())
	{
		PluginWarning("Interface C_HLTVCamera for player helper class not available (required for retrieving spectated player)!\n");
		ready = false;
	}

	if (!Interfaces::AreSteamLibrariesAvailable())
		PluginWarning("Steam libraries for player helper class not available (required for accuracy in retrieving Steam IDs)!\n");

	if (Entities::RetrieveClassPropOffset("CTFPlayer", "m_nPlayerCond") < 0)
	{
		PluginWarning("Required property m_nPlayerCond for CTFPlayer for player helper class not available!\n");
		s_ConditionsRetrievalAvailable = false;
	}

	if (Entities::RetrieveClassPropOffset("CTFPlayer", "_condition_bits") < 0)
	{
		PluginWarning("Required property _condition_bits for CTFPlayer for player helper class not available!\n");
		s_ConditionsRetrievalAvailable = false;
	}

	if (Entities::RetrieveClassPropOffset("CTFPlayer", "m_nPlayerCondEx") < 0)
	{
		PluginWarning("Required property m_nPlayerCondEx for CTFPlayer for player helper class not available!\n");
		s_ConditionsRetrievalAvailable = false;
	}

	if (Entities::RetrieveClassPropOffset("CTFPlayer", "m_nPlayerCondEx2") < 0)
	{
		PluginWarning("Required property m_nPlayerCondEx2 for CTFPlayer for player helper class not available!\n");
		s_ConditionsRetrievalAvailable = false;
	}

	if (Entities::RetrieveClassPropOffset("CTFPlayer", "m_nPlayerCondEx3") < 0)
	{
		PluginWarning("Required property m_nPlayerCondEx3 for CTFPlayer for player helper class not available!\n");
		s_ConditionsRetrievalAvailable = false;
	}

	if (Entities::RetrieveClassPropOffset("CTFPlayer", "m_iClass") < 0)
	{
		PluginWarning("Required property m_iClass for CTFPlayer for player helper class not available!\n");
		s_ClassRetrievalAvailable = false;
		s_ComparisonAvailable = false;
	}

	return ready;
}

CSteamID Player::GetSteamID() const
{
	if (IsValid())
	{
		player_info_t playerInfo;

		if (Interfaces::GetEngineClient()->GetPlayerInfo(GetEntity()->entindex(), &playerInfo))
		{
			if (playerInfo.friendsID)
			{
				static EUniverse universe = k_EUniverseInvalid;

				if (universe == k_EUniverseInvalid)
				{
					if (Interfaces::GetSteamAPIContext()->SteamUtils())
						universe = Interfaces::GetSteamAPIContext()->SteamUtils()->GetConnectedUniverse();
					else
					{
						// let's just assume that it's public - what are the chances that there's a Valve employee testing this on another universe without Steam?
						PluginWarning("Steam libraries not available - assuming public universe for user Steam IDs!\n");
						universe = k_EUniversePublic;
					}
				}

				return CSteamID(playerInfo.friendsID, 1, universe, k_EAccountTypeIndividual);
			}
		}
	}

	return CSteamID();
}

C_BaseEntity * Player::GetBaseEntity() const
{
	auto entity = GetEntity();
	return entity ? entity->GetBaseEntity() : nullptr;
}

C_BaseAnimating * Player::GetBaseAnimating() const
{
	auto entity = GetBaseEntity();
	return entity ? entity->GetBaseAnimating() : nullptr;
}

C_BasePlayer* Player::GetBasePlayer() const
{
	return dynamic_cast<C_BasePlayer*>(GetEntity());
}

bool Player::CheckCondition(TFCond condition) const
{
	if (IsValid())
	{
		CheckCache();

		if (condition < 32)
		{
			if (!m_CachedCondBits[0] || !m_CachedCondBits[1])
			{
				m_CachedCondBits[0] = Entities::GetEntityProp<uint32_t>(GetEntity(), { "m_nPlayerCond" });
				m_CachedCondBits[1] = Entities::GetEntityProp<uint32_t>(GetEntity(), { "_condition_bits" });
			}

			return (*m_CachedCondBits[0] | *m_CachedCondBits[1]) & (1 << condition);
		}
		else if (condition < 64)
		{
			if (!m_CachedCondBits[2])
				m_CachedCondBits[2] = Entities::GetEntityProp<uint32_t>(GetEntity(), { "m_nPlayerCondEx" });

			return *m_CachedCondBits[2] & (1 << (condition - 32));
		}
		else if (condition < 96)
		{
			if (!m_CachedCondBits[3])
				m_CachedCondBits[3] = Entities::GetEntityProp<uint32_t>(GetEntity(), { "m_nPlayerCondEx" });

			return *m_CachedCondBits[3] & (1 << (condition - 64));
		}
		else if (condition < 128)
		{
			if (!m_CachedCondBits[4])
				m_CachedCondBits[4] = Entities::GetEntityProp<uint32_t>(GetEntity(), { "m_nPlayerCondEx3" });

			return *m_CachedCondBits[4] & (1 << (condition - 96));
		}
	}

	return false;
}

TFTeam Player::GetTeam() const
{
	if (IsValid())
	{
		if (CheckCache() || !m_CachedTeam)
		{
			IClientEntity* clientEnt = GetEntity();
			if (!clientEnt)
				return TFTeam::Unassigned;

			C_BaseEntity* entity = clientEnt->GetBaseEntity();
			if (!entity)
				return TFTeam::Unassigned;

			m_CachedTeam = Entities::GetEntityProp<TFTeam>(entity, { "m_iTeamNum" });
		}

		if (m_CachedTeam)
			return *m_CachedTeam;
	}

	return TFTeam::Unassigned;
}

int Player::GetUserID() const
{
	if (IsValid())
	{
		player_info_t playerInfo;
		if (Interfaces::GetEngineClient()->GetPlayerInfo(GetEntity()->entindex(), &playerInfo))
			return playerInfo.userID;
	}

	return 0;
}

const char* Player::GetName() const
{
	if (IsValid())
		return GetPlayerInfo().name;

	return nullptr;
}

const char* Player::GetName(int entIndex)
{
	Player* player = GetPlayer(entIndex, __FUNCSIG__);
	if (player)
		return player->GetName();

	return nullptr;
}

TFClassType Player::GetClass() const
{
	if (IsValid())
	{
		if (CheckCache() || !m_CachedClass)
			m_CachedClass = Entities::GetEntityProp<TFClassType>(GetEntity(), { "m_iClass" });

		if (m_CachedClass)
			return *m_CachedClass;
	}

	return TFClassType::Unknown;
}

float Player::GetLastHurtTime() const
{
	return Interfaces::GetEngineTool()->ClientTime() - m_LastHurtTime;
}

void Player::ResetLastHurtTime()
{
	m_LastHurtTime = Interfaces::GetEngineTool()->ClientTime();
}

void Player::UpdateLastHurtTime()
{
	const auto tick = Interfaces::GetEngineTool()->ClientTick();
	if (tick == m_LastHurtUpdateTick)
		return;

	auto health = GetHealth();

	// Update last hurt time
	if (health < m_LastHurtHealth)
		m_LastHurtTime = Interfaces::GetEngineTool()->ClientTime();

	m_LastHurtHealth = health;
	m_LastHurtUpdateTick = tick;
}

int Player::GetHealth() const
{
	if (IsValid())
	{
		if (CheckCache() || !m_CachedHealth)
			m_CachedHealth = Entities::GetEntityProp<int>(GetEntity(), "m_iHealth");

		Assert(m_CachedHealth);
		if (m_CachedHealth)
			return *m_CachedHealth;
	}

	Assert(!"Called " __FUNCTION__ "() on an invalid player!");
	return 0;
}

int Player::GetMaxHealth() const
{
	if (IsValid())
	{
		auto playerResource = TFPlayerResource::GetPlayerResource();
		if (playerResource)
			return playerResource->GetMaxHealth(entindex());

		Assert(!"Attempted to GetMaxHealth on a player, but unable to GetPlayerResource!");
	}
	else
	{
		Assert(!"Called " __FUNCTION__ "() on an invalid player!");
	}

	return 1;	// So we avoid dividing by zero somewhere
}

int Player::GetMaxOverheal() const
{
	return (int(GetMaxHealth() * 1.5f) / 5) * 5;
}

bool Player::IsValid() const
{
	if (!IsValidIndex(entindex()))
		return false;

	if (!GetEntity())
		return false;

	return true;
}

bool Player::CheckCache() const
{
	Assert(IsValid());

	void* current = m_PlayerEntity.Get();
	if (current != m_CachedPlayerEntity)
	{
		m_CachedPlayerEntity = current;

		m_CachedTeam = nullptr;
		m_CachedClass = nullptr;
		m_CachedHealth = nullptr;
		m_CachedMaxHealth = nullptr;
		m_CachedObserverMode = nullptr;
		m_CachedObserverTarget = nullptr;
		m_CachedActiveWeapon = nullptr;
		m_CachedCondBits.fill(nullptr);

		m_CachedPlayerInfoLastUpdateFrame = 0;

		for (auto& wpn : m_CachedWeapons)
			wpn = nullptr;

		return true;
	}

	return false;
}

void Player::UserInfoChangedCallbackOverride(void*, INetworkStringTable* stringTable, int stringNumber, const char* newString, const void* newData)
{
	// If there's any changes, force a recreation of the Player instance
	Assert(stringNumber >= 0 && stringNumber < std::size(s_Players));
	s_Players[stringNumber].reset();
}

Player::Iterator Player::end()
{
	return Player::Iterator(Interfaces::GetEngineTool()->GetMaxClients() + 1);
}

Player::Iterator::Iterator()
{
	// Find the first valid player
	for (int i = 1; i <= Interfaces::GetEngineTool()->GetMaxClients(); i++)
	{
		auto player = GetPlayer(i);
		if (!player || !player->IsValid())
			continue;

		m_Index = i;
		Assert((*(*this))->GetEntity());
		return;
	}

	m_Index = Interfaces::GetEngineTool()->GetMaxClients() + 1;
}

Player::Iterator& Player::Iterator::operator++()
{
	// Find the next valid player
	for (int i = m_Index + 1; i <= Interfaces::GetEngineTool()->GetMaxClients(); i++)
	{
		auto player = GetPlayer(i);
		if (!player || !player->IsValid())
			continue;

		m_Index = i;

		Assert((*(*this))->GetEntity());
		return *this;
	}

	m_Index = Interfaces::GetEngineTool()->GetMaxClients() + 1;
	return *this;
}

bool Player::IsValidIndex(int entIndex)
{
	const auto maxclients = Interfaces::GetEngineTool()->GetMaxClients();
	if (entIndex < 1 || entIndex > maxclients)
		return false;

	return true;
}

Player* Player::GetLocalPlayer()
{
	const auto localPlayerIndex = Interfaces::GetEngineClient()->GetLocalPlayer();
	if (!IsValidIndex(localPlayerIndex))
		return nullptr;

	return GetPlayer(localPlayerIndex, __FUNCSIG__);
}

Player* Player::GetPlayer(int entIndex, const char* functionName)
{
	if (!IsValidIndex(entIndex))
	{
		if (!functionName)
			functionName = "<UNKNOWN>";

		PluginWarning("Out of range playerEntIndex %i in %s\n", entIndex, functionName);
		return nullptr;
	}

	Assert((entIndex - 1) >= 0 && (entIndex - 1) < MAX_PLAYERS);

	Player* p = s_Players[entIndex - 1].get();
	if (!p || !p->IsValid())
	{
		IClientEntity* playerEntity = Interfaces::GetClientEntityList()->GetClientEntity(entIndex);
		if (!playerEntity)
			return nullptr;

		player_info_t info;
		if (!GetHooks()->GetOriginal<HookFunc::IVEngineClient_GetPlayerInfo>()(entIndex, &info))
			return nullptr;

		s_Players[entIndex - 1] = std::unique_ptr<Player>(p = new Player(playerEntity, info.userID));

		// Check again
		if (!p || !p->IsValid())
			return nullptr;
	}

	return p;
}

Player* Player::GetPlayerFromUserID(int userID)
{
	for (Player* player : Player::Iterable())
	{
		if (player->GetUserID() == userID)
			return player;
	}

	return nullptr;
}

Player* Player::GetPlayerFromName(const char* exactName)
{
	for (Player* player : Player::Iterable())
	{
		if (!strcmp(player->GetName(), exactName))
			return player;
	}

	return nullptr;
}

Player* Player::AsPlayer(IClientEntity* entity)
{
	if (!entity)
		return nullptr;

	const int entIndex = entity->entindex();
	if (entIndex >= 1 && entIndex <= Interfaces::GetEngineTool()->GetMaxClients())
		return GetPlayer(entIndex);

	return nullptr;
}

bool Player::IsAlive() const
{
	if (IsValid())
	{
		auto playerResource = TFPlayerResource::GetPlayerResource();
		if (playerResource)
			return playerResource->IsAlive(m_PlayerEntity.GetEntryIndex());

		Assert(!"Attempted to call IsAlive on a player, but unable to GetPlayerResource!");
	}
	else
	{
		Assert(!"Called " __FUNCTION__ "() on an invalid player!");
	}

	return false;
}

int Player::entindex() const
{
	if (m_PlayerEntity.IsValid())
		return m_PlayerEntity.GetEntryIndex();
	else
		return -1;
}

const player_info_t& Player::GetPlayerInfo() const
{
	static player_info_t s_InvalidPlayerInfo = []()
	{
		player_info_t retVal;
		strcpy_s(retVal.name, "INVALID");
		retVal.userID = -1;
		strcpy_s(retVal.guid, "[U:0:0]");
		retVal.friendsID = 0;
		strcpy_s(retVal.friendsName, "INVALID");
		retVal.fakeplayer = true;
		retVal.ishltv = false;

		for (auto& crc : retVal.customFiles)
			crc = 0;

		retVal.filesDownloaded = 0;

		return retVal;
	}();

	const auto framecount = Interfaces::GetEngineTool()->HostFrameCount();
	if (m_CachedPlayerInfoLastUpdateFrame == framecount)
		return m_CachedPlayerInfo;

	if (Interfaces::GetEngineClient()->GetPlayerInfo(entindex(), &m_CachedPlayerInfo))
	{
		m_CachedPlayerInfoLastUpdateFrame = framecount;
		return m_CachedPlayerInfo;
	}

	return s_InvalidPlayerInfo;
}

C_BaseCombatWeapon* Player::GetMedigun(TFMedigun* medigunType) const
{
	if (!IsValid())
		goto Failed;

	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		C_BaseCombatWeapon* weapon = GetWeapon(i);
		if (!weapon || !Entities::CheckEntityBaseclass(weapon, "WeaponMedigun"))
			continue;

		if (medigunType)
		{
			const auto itemdefIndex = Entities::GetEntityProp<int>(weapon, "m_iItemDefinitionIndex");
			if (!itemdefIndex)
				continue;

			const auto baseID = ItemSchema::GetModule()->GetBaseItemID(*itemdefIndex);

			switch (baseID)
			{
				case 29:	*medigunType = TFMedigun::MediGun; break;
				case 35:	*medigunType = TFMedigun::Kritzkrieg; break;
				case 411:	*medigunType = TFMedigun::QuickFix; break;
				case 998:	*medigunType = TFMedigun::Vaccinator; break;
				default:	*medigunType = TFMedigun::Unknown; break;
			}
		}

		return weapon;
	}

Failed:
	if (medigunType)
		*medigunType = TFMedigun::Unknown;

	return nullptr;
}

Vector Player::GetAbsOrigin() const
{
	if (!IsValid())
		return vec3_origin;

	IClientEntity* const clientEntity = GetEntity();
	if (!clientEntity)
		return vec3_origin;

	return clientEntity->GetAbsOrigin();
}

QAngle Player::GetAbsAngles() const
{
	if (!IsValid())
		return vec3_angle;

	IClientEntity* const clientEntity = GetEntity();
	if (!clientEntity)
		return vec3_angle;

	return clientEntity->GetAbsAngles();
}

Vector Player::GetEyePosition() const
{
	if (!IsValid())
		return vec3_origin;

	static const Vector VIEW_OFFSETS[] =
	{
		Vector(0, 0, 72),		// TF_CLASS_UNDEFINED

		Vector(0, 0, 65),		// TF_CLASS_SCOUT,			// TF_FIRST_NORMAL_CLASS
		Vector(0, 0, 75),		// TF_CLASS_SNIPER,
		Vector(0, 0, 68),		// TF_CLASS_SOLDIER,
		Vector(0, 0, 68),		// TF_CLASS_DEMOMAN,
		Vector(0, 0, 75),		// TF_CLASS_MEDIC,
		Vector(0, 0, 75),		// TF_CLASS_HEAVYWEAPONS,
		Vector(0, 0, 68),		// TF_CLASS_PYRO,
		Vector(0, 0, 75),		// TF_CLASS_SPY,
		Vector(0, 0, 68),		// TF_CLASS_ENGINEER,		// TF_LAST_NORMAL_CLASS
	};

	return GetAbsOrigin() + VIEW_OFFSETS[(int)GetClass()];
}

QAngle Player::GetEyeAngles() const
{
	if (!IsValid())
		return vec3_angle;

	IClientEntity* const clientEntity = GetEntity();
	if (!clientEntity)
		return vec3_angle;

	C_BaseEntity* const baseEntity = clientEntity->GetBaseEntity();
	if (!baseEntity)
		return vec3_angle;

	return baseEntity->EyeAngles();
}

ObserverMode Player::GetObserverMode() const
{
	if (IsValid())
	{
		if (CheckCache() || !m_CachedObserverMode)
			m_CachedObserverMode = Entities::GetEntityProp<ObserverMode>(GetEntity(), { "m_iObserverMode" });

		if (m_CachedObserverMode)
			return *m_CachedObserverMode;
	}

	return OBS_MODE_NONE;
}

C_BaseEntity *Player::GetObserverTarget() const
{
	if (IsValid())
	{
		if (CheckCache() || !m_CachedObserverTarget)
			m_CachedObserverTarget = Entities::GetEntityProp<EHANDLE>(GetEntity(), { "m_hObserverTarget" });

		if (m_CachedObserverTarget)
			return m_CachedObserverTarget->Get();
	}

	return GetEntity() ? GetEntity()->GetBaseEntity() : nullptr;
}

C_BaseCombatWeapon *Player::GetWeapon(int i) const
{
	if (i < 0 || i >= MAX_WEAPONS)
	{
		PluginWarning("Out of range index %i in %s\n", i, __FUNCTION__);
		return nullptr;
	}

	if (IsValid())
	{
		if (CheckCache() || !m_CachedWeapons[i])
		{
			char buffer[32];
			Entities::PropIndex(buffer, "m_hMyWeapons", i);
			m_CachedWeapons[i] = Entities::GetEntityProp<CHandle<C_BaseCombatWeapon>>(GetEntity(), buffer);
		}

		if (m_CachedWeapons[i])
			return m_CachedWeapons[i]->Get();
	}

	return nullptr;
}

C_BaseCombatWeapon* Player::GetActiveWeapon() const
{
	if (IsValid())
	{
		if (CheckCache() || !m_CachedActiveWeapon)
			m_CachedActiveWeapon = Entities::GetEntityProp<CHandle<C_BaseCombatWeapon>>(GetEntity(), "m_hActiveWeapon");

		if (m_CachedActiveWeapon)
			return *m_CachedActiveWeapon;
	}

	return nullptr;
}
