#pragma once
#include "PluginBase/Modules.h"

#include <convar.h>

#include <unordered_set>

class ConsoleTools final : public Module<ConsoleTools>
{
public:
	ConsoleTools();
	virtual ~ConsoleTools();

	static bool CheckDependencies();

private:
	void ConsoleColorPrintfHook(const Color& color, const char* msg);
	void ConsoleDPrintfHook(const char* msg);
	void ConsolePrintfHook(const char* msg);

	bool CheckFilters(const std::string& msg) const;

	int m_ConsoleColorPrintfHook;
	int m_ConsoleDPrintfHook;
	int m_ConsolePrintfHook;
	std::unordered_set<std::string> m_Filters;

	ConVar ce_consoletools_filter_enabled;
	ConCommand ce_consoletools_filter_add;
	ConCommand ce_consoletools_filter_remove;
	ConCommand ce_consoletools_filter_list;

	ConCommand ce_consoletools_flags_add;
	ConCommand ce_consoletools_flags_remove;

	void AddFilter(const CCommand& command);
	void RemoveFilter(const CCommand& command);
	void ListFilters(const CCommand& command);
	void ToggleFilterEnabled(IConVar* var, const char* oldValue, float fOldValue);

	void AddFlags(const CCommand& command);
	void RemoveFlags(const CCommand& command);

	void DisableHooks();

	class PauseFilter;
	int m_FilterPaused;

	static int ParseFlags(const CCommand& command);
};