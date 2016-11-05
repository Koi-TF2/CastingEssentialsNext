#include "Modules.h"
#include "PluginBase/Interfaces.h"
#include "PluginBase/StubPanel.h"

#include <cdll_int.h>

static ModuleManager s_ModuleManager;
ModuleManager& Modules() { return s_ModuleManager; }

class ModuleManager::Panel final : public vgui::StubPanel
{
public:
	void OnTick() override;
};

void ModuleManager::Init()
{
	m_Panel.reset(new Panel());
}

void ModuleManager::UnloadAllModules()
{
	for (auto& iterator : modules)
	{
		iterator.second.reset();
		PluginColorMsg(Color(0, 255, 0, 255), "Module %s unloaded!\n", moduleNames[iterator.first].c_str());
	}

	modules.clear();
	m_Panel.reset();
}

void ModuleManager::Panel::OnTick()
{
	if (Interfaces::GetEngineClient()->IsInGame())
		Modules().TickAllModules(true);
	else
		Modules().TickAllModules(false);
}

void ModuleManager::TickAllModules(bool inGame)
{
	for (auto& pair : modules)
		pair.second->OnTick(inGame);
}