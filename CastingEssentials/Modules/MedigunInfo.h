#pragma once

#include "PluginBase/Modules.h"

enum class TFMedigun;
enum class TFResistType;
enum class TFTeam;
class ConCommand;
class ConVar;
class IConVar;

namespace vgui
{
	typedef unsigned int VPANEL;
	class EditablePanel;
}

class MedigunInfo : public Module<MedigunInfo>
{
public:
	MedigunInfo();

	static bool CheckDependencies();

private:
	struct Data
	{
		bool m_Alive;
		float m_Charge;
		TFMedigun m_Type;
		bool m_Popped;
		TFResistType m_ResistType;
		TFTeam m_Team;
	};

	void CollectMedigunData();
	std::map<int, Data> m_MedigunPanelData;

	class MainPanel;
	std::unique_ptr<MainPanel> m_MainPanel;
	class MedigunPanel;

	void OnTick(bool inGame) override;

	void UpdateEmbeddedPanels();
	void UpdateEmbeddedPanel(vgui::EditablePanel* playerPanel);

	ConVar* ce_mediguninfo_separate_enabled;
	ConCommand* ce_mediguninfo_separate_reload;

	ConVar* ce_mediguninfo_embedded_enabled;
	ConVar* ce_mediguninfo_embedded_medigun_text;
	ConVar* ce_mediguninfo_embedded_kritzkrieg_text;
	ConVar* ce_mediguninfo_embedded_quickfix_text;
	ConVar* ce_mediguninfo_embedded_vaccinator_text;
	ConVar* ce_mediguninfo_embedded_dead_text;

	void ReloadSettings();

	static constexpr const char* EMBEDDED_ICON_RED = "MedigunIconRed";
	static constexpr const char* EMBEDDED_ICON_BLUE = "MedigunIconBlue";
	static constexpr const char* EMBEDDED_PROGRESS_RED = "MedigunChargeRed";
	static constexpr const char* EMBEDDED_PROGRESS_BLUE = "MedigunChargeBlue";
};