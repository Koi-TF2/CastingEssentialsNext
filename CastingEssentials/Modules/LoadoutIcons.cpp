#include "LoadoutIcons.h"
#include "ItemSchema.h"
#include "Modules/HUDHacking.h"
#include "PluginBase/Entities.h"
#include "PluginBase/HookManager.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"

#include <client/c_basecombatweapon.h>
#include <client/iclientmode.h>
#include <vgui/IVGui.h>
#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/ImagePanel.h"
#include <vprof.h>

LoadoutIcons::LoadoutIcons() :
	ce_loadout_enabled("ce_loadout_enabled", "0", FCVAR_NONE, "Enable weapon icons inside player panels in the specgui."),
	ce_loadout_filter_active_red("ce_loadout_filter_active_red", "255 255 255 255", FCVAR_NONE, "drawcolor_override for red team's active loadout items."),
	ce_loadout_filter_active_blu("ce_loadout_filter_active_blu", "255 255 255 255", FCVAR_NONE, "drawcolor_override for blu team's active loadout items."),
	ce_loadout_filter_inactive_red("ce_loadout_filter_inactive_red", "255 255 255 255", FCVAR_NONE, "drawcolor_override for red team's inactive loadout items."),
	ce_loadout_filter_inactive_blu("ce_loadout_filter_inactive_blu", "255 255 255 255", FCVAR_NONE, "drawcolor_override for blu team's inactive loadout items.")
{

	memset(m_Weapons, 0, sizeof(m_Weapons));
	memset(m_ActiveWeaponIndices, 0, sizeof(m_ActiveWeaponIndices));
}

bool LoadoutIcons::CheckDependencies()
{
	bool ready = true;

	if (!g_pVGuiPanel)
	{
		PluginWarning("Required interface vgui::IPanel for module %s not available!\n", GetModuleName());
		ready = false;
	}

	if (!Interfaces::GetClientMode())
	{
		PluginWarning("Required interface IClientMode for module %s not available!\n", GetModuleName());
		ready = false;
	}

	try
	{
		HookManager::GetRawFunc<HookFunc::vgui_EditablePanel_GetDialogVariables>();
		HookManager::GetRawFunc<HookFunc::vgui_ImagePanel_SetImage>();
	}
	catch (bad_pointer ex)
	{
		PluginWarning("No signature match found for %s, required for module %s!\n", ex.what(), GetModuleName());
		ready = false;
	}

	return ready;
}

void LoadoutIcons::OnTick(bool ingame)
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	if (ingame && ce_loadout_enabled.GetBool())
	{
		GatherWeapons();
		DrawIcons();
	}
}

int LoadoutIcons::GetWeaponDefinitionIndex(IClientNetworkable* networkable)
{
	if (!networkable)
		return -1;

	auto definitionIndex = Entities::GetEntityProp<int>(networkable, "m_iItemDefinitionIndex");
	Assert(definitionIndex);
	if (!definitionIndex)
		return -1;

	return *definitionIndex;
}

void LoadoutIcons::GatherWeapons()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	for (Player* player : Player::Iterable())
	{
		const auto playerIndex = player->entindex() - 1;

		auto activeWeapon = Entities::GetEntityProp<EHANDLE>(player->GetEntity(), "m_hActiveWeapon");
		m_Weapons[playerIndex][IDX_ACTIVE] = activeWeapon ? ItemSchema::GetModule()->GetBaseItemID(GetWeaponDefinitionIndex(activeWeapon->Get())) : -1;

		for (int weaponIndex = 0; weaponIndex < 5; weaponIndex++)
		{
			C_BaseCombatWeapon* weapon = player->GetWeapon(weaponIndex);
			if (!weapon)
				continue;

			auto currentID = ItemSchema::GetModule()->GetBaseItemID(GetWeaponDefinitionIndex(weapon));
			if (currentID == m_Weapons[playerIndex][IDX_ACTIVE])
				m_ActiveWeaponIndices[playerIndex] = weaponIndex;

			m_Weapons[playerIndex][weaponIndex] = currentID;
		}
	}
}

void LoadoutIcons::DrawIcons()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
	vgui::VPANEL specguiPanel = HUDHacking::GetSpecGUI();
	if (!specguiPanel)
		return;

	const auto specguiChildCount = g_pVGuiPanel->GetChildCount(specguiPanel);
	for (int playerPanelIndex = 0; playerPanelIndex < specguiChildCount; playerPanelIndex++)
	{
		vgui::VPANEL playerVPanel = g_pVGuiPanel->GetChild(specguiPanel, playerPanelIndex);
		const char* playerPanelName = g_pVGuiPanel->GetName(playerVPanel);
		if (!g_pVGuiPanel->IsVisible(playerVPanel) || strncmp(playerPanelName, "playerpanel", 11))	// Names are like "playerpanel13"
			continue;

		vgui::EditablePanel* player = assert_cast<vgui::EditablePanel*>(g_pVGuiPanel->GetPanel(playerVPanel, "ClientDLL"));

		PlayerPanelUpdateIcons(player);
	}
}

void LoadoutIcons::PlayerPanelUpdateIcons(vgui::EditablePanel* playerPanel)
{
	const int playerIndex = GetPlayerIndex(playerPanel);
	if (playerIndex < 0 || playerIndex >= MAX_PLAYERS)
		return;

	TFTeam team = TFTeam::Unassigned;
	{
		auto player = Player::GetPlayer(playerIndex + 1, __FUNCSIG__);
		if (player)
			team = player->GetTeam();

		if (team != TFTeam::Red && team != TFTeam::Blue)
			return;
	}

	const int teamIndex = (int)team - (int)TFTeam::Red;

	// Force get the panel even though we're in a different module
	const auto playerVPANEL = playerPanel->GetVPanel();
	for (int i = 0; i < g_pVGuiPanel->GetChildCount(playerVPANEL); i++)
	{
		auto childVPANEL = g_pVGuiPanel->GetChild(playerVPANEL, i);
		auto childPanelName = g_pVGuiPanel->GetName(childVPANEL);


		for (uint_fast8_t iconIndex = 0; iconIndex < ITEM_COUNT; iconIndex++)
		{
			if (strcmp(childPanelName, LOADOUT_ICONS[iconIndex][teamIndex]))
				continue;

			auto iconPanel = g_pVGuiPanel->GetPanel(childVPANEL, "ClientDLL");

			const auto weaponIndex = m_Weapons[playerIndex][iconIndex];

			if (weaponIndex < 0)
			{
				iconPanel->SetVisible(false);
			}
			else
			{
				char materialBuffer[32];
				sprintf_s(materialBuffer, "loadout_icons/%i_%s", weaponIndex, TF_TEAM_NAMES[(int)team]);

				// Dumb, evil, unsafe hacks
				auto hackImgPanel = reinterpret_cast<vgui::ImagePanel*>(iconPanel);

				HookManager::GetRawFunc<HookFunc::vgui_ImagePanel_SetImage>()(hackImgPanel, materialBuffer);

				//Color* m_FillColor = (Color*)(((DWORD*)hackImgPanel) + 94);
				Color* m_DrawColor = (Color*)(((DWORD*)hackImgPanel) + 95);

				if (iconIndex == IDX_ACTIVE || m_ActiveWeaponIndices[playerIndex] == iconIndex)
				{
					*m_DrawColor = ColorFromConVar(team == TFTeam::Red ? ce_loadout_filter_active_red : ce_loadout_filter_active_blu);
				}
				else
				{
					*m_DrawColor = ColorFromConVar(team == TFTeam::Red ? ce_loadout_filter_inactive_red : ce_loadout_filter_inactive_blu);
				}

				iconPanel->SetVisible(true);
			}

			break;
		}
	}
}

int LoadoutIcons::GetPlayerIndex(vgui::EditablePanel* playerPanel)
{
	auto player = HUDHacking::GetPlayerFromPanel(playerPanel);
	return (player ? player->entindex() : 0) - 1;
}
