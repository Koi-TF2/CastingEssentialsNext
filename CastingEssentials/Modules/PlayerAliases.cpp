#include "PlayerAliases.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "PluginBase/TFDefinitions.h"
#include <cdll_int.h>
#include <steam/steam_api.h>
#include <toolframework/ienginetool.h>
#include <vprof.h>

MODULE_REGISTER(PlayerAliases);

PlayerAliases::PlayerAliases()
    : ce_playeraliases_enabled(
          "ce_playeraliases_enabled", "0", FCVAR_NONE, "Enables player aliases.",
          [](IConVar* var, const char*, float) { GetModule()->ToggleEnabled(static_cast<ConVar*>(var)); }),

      ce_playeraliases_format_mode("ce_playeraliases_format_mode", "0", FCVAR_NONE,
                                   "0 = apply format to all players, 1 = apply format to aliased players only"),
      ce_playeraliases_format_blu("ce_playeraliases_format_blu", "%alias%", FCVAR_NONE, "Name format for BLU players."),
      ce_playeraliases_format_red("ce_playeraliases_format_red", "%alias%", FCVAR_NONE, "Name format for RED players."),
      ce_playeraliases_format_swap(
          "ce_playeraliases_format_swap", []() { GetModule()->SwapTeamFormats(); },
          "Swaps the values of ce_playeraliases_format_red and ce_playeraliases_format_blu."),

      ce_playeraliases_list(
          "ce_playeraliases_list", []() { GetModule()->PrintPlayerAliases(); },
          "Prints all player aliases to console."),
      ce_playeraliases_add(
          "ce_playeraliases_add", [](const CCommand& args) { GetModule()->AddPlayerAlias(args); },
          "Adds a new player alias."),
      ce_playeraliases_remove(
          "ce_playeraliases_remove", [](const CCommand& args) { GetModule()->RemovePlayerAlias(args); },
          "Removes an existing player alias."),

      m_GetPlayerInfoHook(
          std::bind(&PlayerAliases::GetPlayerInfoOverride, this, std::placeholders::_1, std::placeholders::_2))
{
}

bool PlayerAliases::CheckDependencies()
{
    bool ready = true;

    if (!Interfaces::GetEngineClient())
    {
        PluginWarning("Required interface IVEngineClient for module %s not available!\n", GetModuleName());
        ready = false;
    }

    if (!Interfaces::AreSteamLibrariesAvailable())
    {
        PluginWarning("Required Steam libraries for module %s not available!\n", GetModuleName());
        ready = false;
    }

    if (!Player::CheckDependencies())
    {
        PluginWarning("Required player helper class for module %s not available!\n", GetModuleName());
        ready = false;
    }

    if (!GetHooks()->GetHook<HookFunc::IVEngineClient_GetPlayerInfo>())
    {
        PluginWarning("Required hook IVEngineClient::GetPlayerInfo for module %s not available!\n", GetModuleName());
        ready = false;
    }

    return ready;
}

bool PlayerAliases::GetPlayerInfoOverride(int ent_num, player_info_s* pinfo)
{
    VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_CE);
    if (ent_num < 1 || ent_num > Interfaces::GetEngineTool()->GetMaxClients())
        return false;

    Player* player = Player::GetPlayer(ent_num, __FUNCSIG__);
    if (!player)
        return false;

    static EUniverse universe = k_EUniverseInvalid;
    if (universe == k_EUniverseInvalid)
    {
        if (Interfaces::GetSteamAPIContext()->SteamUtils())
            universe = Interfaces::GetSteamAPIContext()->SteamUtils()->GetConnectedUniverse();
        else
        {
            PluginWarning("Steam libraries not available - assuming public universe for user Steam IDs!\n");
            universe = k_EUniversePublic;
        }
    }

    bool result = m_GetPlayerInfoHook.GetOriginal()(ent_num, pinfo);

    CSteamID playerSteamID(pinfo->friendsID, 1, universe, k_EAccountTypeIndividual);
    const char* alias = GetAlias(playerSteamID);

    if (auto mode = ce_playeraliases_format_mode.GetInt(); mode == 0 || (mode == 1 && alias))
    {
        if (!alias)
            alias = pinfo->name;

        std::string gameName;
        switch (player->GetTeam())
        {
            case TFTeam::Red:
                gameName = ce_playeraliases_format_red.GetString();
                break;

            case TFTeam::Blue:
                gameName = ce_playeraliases_format_blu.GetString();
                break;

            default:
                gameName = "%alias%";
                break;
        }

        FindAndReplaceInString(gameName, "%alias%", alias);
        V_strcpy_safe(pinfo->name, gameName.c_str());

        GetHooks()->SetState<HookFunc::IVEngineClient_GetPlayerInfo>(Hooking::HookAction::SUPERCEDE);
        return result;
    }

    return true;
}

const char* PlayerAliases::GetAlias(const CSteamID& player) const
{
    if (!player.IsValid())
        return nullptr;

    auto found = m_CustomAliases.find(player);
    if (found != m_CustomAliases.end())
        return found->second.c_str();

    return nullptr;
}

void PlayerAliases::SwapTeamFormats()
{
    std::string red(ce_playeraliases_format_red.GetString());
    ce_playeraliases_format_red.SetValue(ce_playeraliases_format_blu.GetString());
    ce_playeraliases_format_blu.SetValue(red.c_str());
}

void PlayerAliases::PrintPlayerAliases()
{
    Msg("%i player aliases:\n", m_CustomAliases.size());

    for (auto alias : m_CustomAliases)
        Msg("    %s = %s\n", RenderSteamID(alias.first).c_str(), alias.second.c_str());
}

void PlayerAliases::AddPlayerAlias(const CCommand& brokenCommand)
{
    CCommand command;
    if (!ReparseForSteamIDs(brokenCommand, command))
        return;

    if (command.ArgC() != 3)
    {
        Warning("Usage: %s <steam id> <name>\n", ce_playeraliases_add.GetName());
        return;
    }

    const CSteamID id(command.Arg(1));
    if (!id.IsValid())
    {
        Warning("Failed to parse steamid \"%s\"!\n", command.Arg(1));
        return;
    }

    // Check for existing
    {
        const auto& found = m_CustomAliases.find(id);
        if (found != m_CustomAliases.end())
            m_CustomAliases.erase(found);
    }

    {
        const std::string& name = command.Arg(2);
        m_CustomAliases.insert(std::make_pair(id, name));
    }

    return;
}

void PlayerAliases::RemovePlayerAlias(const CCommand& brokenCommand)
{
    CCommand command;
    if (!ReparseForSteamIDs(brokenCommand, command))
        return;

    if (command.ArgC() != 2)
        goto Usage;

    const CSteamID id(command.Arg(1));
    if (!id.IsValid())
    {
        Warning("Failed to parse steamid \"%s\"!\n", command.Arg(1));
        goto Usage;
    }

    // Find and remove existing
    {
        const auto& found = m_CustomAliases.find(id);
        if (found != m_CustomAliases.end())
            m_CustomAliases.erase(found);
        else
        {
            Warning("Unable to find existing alias for steamid \"%s\"!\n", command.Arg(1));
            return;
        }
    }

    return;
Usage:
    Warning("Usage: %s <steam id>\n", ce_playeraliases_remove.GetName());
}

void PlayerAliases::FindAndReplaceInString(std::string& str, const std::string_view& find,
                                           const std::string_view& replace)
{
    if (find.empty())
        return;

    size_t start_pos = 0;

    while ((start_pos = str.find(find, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, find.length(), replace);
        start_pos += replace.length();
    }
}

void PlayerAliases::ToggleEnabled(const ConVar* var) { m_GetPlayerInfoHook.SetEnabled(var->GetBool()); }