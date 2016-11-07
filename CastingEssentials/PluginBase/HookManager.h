#pragma once
#include "Hooking/GroupGlobalHook.h"
#include "Hooking/GroupClassHook.h"
#include "Hooking/GroupVirtualHook.h"
#include "PluginBase/Modules.h"

#include <memory>
#include <vector>

class C_HLTVCamera;
class QAngle;
class ICvar;
class C_BaseEntity;
struct model_t;
class IVEngineClient;
struct player_info_s;
typedef player_info_s player_info_t;
class IGameEventManager2;
class IGameEvent;
class IClientEngineTools;
class Vector;

class HookManager final
{
	enum class Func
	{
		ICvar_ConsoleColorPrintf,
		ICvar_ConsoleDPrintf,
		ICvar_ConsolePrintf,

		IClientEngineTools_InToolMode,
		IClientEngineTools_IsThirdPersonCamera,
		IClientEngineTools_SetupEngineView,

		IVEngineClient_GetPlayerInfo,

		IGameEventManager2_FireEventClientSide,

		C_HLTVCamera_SetCameraAngle,
		C_HLTVCamera_SetMode,
		C_HLTVCamera_SetPrimaryTarget,

		Global_GetLocalPlayerIndex,

		Count,
	};

	enum ShimType;
	template<class HookType, class... Args> class HookShim final :
		public Hooking::BaseGroupHook<ShimType, (ShimType)HookType::HOOK_ID, typename HookType::RetVal, Args...>
	{
	public:
		~HookShim()
		{
			if (m_InnerHook)
				DetachHook();

			m_HooksTable.clear();
		}

		Functional GetOriginal() override { return m_InnerHook->GetOriginal(); }
		Hooking::HookType GetType() const override { return m_InnerHook->GetType(); }

		int AddHook(const Functional& newHook) override
		{
			const auto retVal = BaseGroupHookType::AddHook(newHook);
			AddInnerHook(retVal, newHook);
			return retVal;
		}
		bool RemoveHook(int hookID, const char* funcName) override
		{
			bool retVal = BaseGroupHookType::RemoveHook(hookID, funcName);
			RemoveInnerHook(hookID, funcName);
			return retVal;
		}

		void SetState(Hooking::HookAction action) override { Assert(m_InnerHook); m_InnerHook->SetState(action); }

	private:
		friend class HookManager;

		void InitHook() override { }

		typedef HookType Inner;
		void AttachHook(const std::shared_ptr<HookType>& innerHook)
		{
			Assert(!m_InnerHook);

			std::lock_guard<decltype(m_Mutex)> lock(m_Mutex);
			m_InnerHook = innerHook;

			std::lock_guard<decltype(m_HooksTableMutex)> hooksTableLock(m_HooksTableMutex);
			for (auto hooks : m_HooksTable)
				AddInnerHook(hooks.first, hooks.second);
		}
		void DetachHook()
		{
			Assert(m_InnerHook);

			std::lock_guard<decltype(m_Mutex)> lock(m_Mutex);
			for (auto hookID : m_ActiveHooks)
				m_InnerHook->RemoveHook(hookID.second, __FUNCSIG__);

			m_ActiveHooks.clear();
			m_InnerHook.reset();
		}

		void AddInnerHook(uint64 fakeHookID, const Functional& newHook)
		{
			std::lock_guard<decltype(m_Mutex)> lock(m_Mutex);
			if (m_InnerHook)
			{
				const auto current = m_InnerHook->AddHook(newHook);
				m_ActiveHooks.insert(std::make_pair(fakeHookID, current));
			}
		}
		void RemoveInnerHook(uint64 hookID, const char* funcName)
		{
			std::lock_guard<decltype(m_Mutex)> lock(m_Mutex);
			if (m_InnerHook)
			{
				const auto& link = m_ActiveHooks.find(hookID);
				if (link != m_ActiveHooks.end())
				{
					m_InnerHook->RemoveHook(link->second, funcName);
					m_ActiveHooks.erase(link);
				}
			}
		}

		std::recursive_mutex m_Mutex;

		std::shared_ptr<HookType> m_InnerHook;
		std::map<uint64, uint64> m_ActiveHooks;
	};

	template<Func fn, bool vaArgs, class Type, class RetVal, class... Args> using VirtualHook =
		HookShim<Hooking::GroupVirtualHook<Func, fn, vaArgs, Type, RetVal, Args...>, Args...>;
	template<Func fn, bool vaArgs, class Type, class RetVal, class... Args> using ClassHook =
		HookShim<Hooking::GroupClassHook<Func, fn, vaArgs, Type, RetVal, Args...>, Args...>;
	template<Func fn, bool vaArgs, class RetVal, class... Args> using GlobalHook =
		HookShim<Hooking::GroupGlobalHook<Func, fn, vaArgs, RetVal, Args...>, Args...>;

	typedef void(__thiscall *RawSetCameraAngleFn)(C_HLTVCamera*, const QAngle&);
	typedef void(__thiscall *RawSetModeFn)(C_HLTVCamera*, int);
	typedef void(__thiscall *RawSetPrimaryTargetFn)(C_HLTVCamera*, int);
	typedef int(*RawGetLocalPlayerIndexFn)();

	static RawSetCameraAngleFn GetRawFunc_C_HLTVCamera_SetCameraAngle();
	static RawSetModeFn GetRawFunc_C_HLTVCamera_SetMode();
	static RawSetPrimaryTargetFn GetRawFunc_C_HLTVCamera_SetPrimaryTarget();
	static RawGetLocalPlayerIndexFn GetRawFunc_Global_GetLocalPlayerIndex();

public:
	HookManager();

	static bool Load();
	static bool Unload();

	typedef VirtualHook<Func::ICvar_ConsoleColorPrintf, true, ICvar, void, const Color&, const char*> ICvar_ConsoleColorPrintf;
	typedef VirtualHook<Func::ICvar_ConsoleDPrintf, true, ICvar, void, const char*> ICvar_ConsoleDPrintf;
	typedef VirtualHook<Func::ICvar_ConsolePrintf, true, ICvar, void, const char*> ICvar_ConsolePrintf;

	typedef VirtualHook<Func::IClientEngineTools_InToolMode, false, IClientEngineTools, bool> IClientEngineTools_InToolMode;
	typedef VirtualHook<Func::IClientEngineTools_IsThirdPersonCamera, false, IClientEngineTools, bool> IClientEngineTools_IsThirdPersonCamera;
	typedef VirtualHook<Func::IClientEngineTools_SetupEngineView, false, IClientEngineTools, bool, Vector&, QAngle&, float&> IClientEngineTools_SetupEngineView;

	typedef VirtualHook<Func::IVEngineClient_GetPlayerInfo, false, IVEngineClient, bool, int, player_info_t*> IVEngineClient_GetPlayerInfo;

	typedef VirtualHook<Func::IGameEventManager2_FireEventClientSide, false, IGameEventManager2, bool, IGameEvent*> IGameEventManager2_FireEventClientSide;

	typedef ClassHook<Func::C_HLTVCamera_SetCameraAngle, false, C_HLTVCamera, void, const QAngle&> C_HLTVCamera_SetCameraAngle;
	typedef ClassHook<Func::C_HLTVCamera_SetMode, false, C_HLTVCamera, void, int> C_HLTVCamera_SetMode;
	typedef ClassHook<Func::C_HLTVCamera_SetPrimaryTarget, false, C_HLTVCamera, void, int> C_HLTVCamera_SetPrimaryTarget;

	typedef GlobalHook<Func::Global_GetLocalPlayerIndex, false, int> Global_GetLocalPlayerIndex;

	template<class Hook> typename Hook::Functional GetFunc() { static_assert(false, "Invalid hook type"); }
	template<> C_HLTVCamera_SetCameraAngle::Functional GetFunc<C_HLTVCamera_SetCameraAngle>();
	template<> C_HLTVCamera_SetMode::Functional GetFunc<C_HLTVCamera_SetMode>();
	template<> C_HLTVCamera_SetPrimaryTarget::Functional GetFunc<C_HLTVCamera_SetPrimaryTarget>();
	template<> Global_GetLocalPlayerIndex::Functional GetFunc<Global_GetLocalPlayerIndex>();

	template<class Hook> Hook* GetHook() { static_assert(false, "Invalid hook type"); }
	template<> ICvar_ConsoleColorPrintf* GetHook<ICvar_ConsoleColorPrintf>() { return &m_Hook_ICvar_ConsoleColorPrintf; }
	template<> ICvar_ConsoleDPrintf* GetHook<ICvar_ConsoleDPrintf>() { return &m_Hook_ICvar_ConsoleDPrintf; }
	template<> ICvar_ConsolePrintf* GetHook<ICvar_ConsolePrintf>() { return &m_Hook_ICvar_ConsolePrintf; }
	template<> IClientEngineTools_InToolMode* GetHook<IClientEngineTools_InToolMode>() { return &m_Hook_IClientEngineTools_InToolMode; }
	template<> IClientEngineTools_IsThirdPersonCamera* GetHook<IClientEngineTools_IsThirdPersonCamera>() { return &m_Hook_IClientEngineTools_IsThirdPersonCamera; }
	template<> IClientEngineTools_SetupEngineView* GetHook<IClientEngineTools_SetupEngineView>() { return &m_Hook_IClientEngineTools_SetupEngineView; }
	template<> IVEngineClient_GetPlayerInfo* GetHook<IVEngineClient_GetPlayerInfo>() { return &m_Hook_IVEngineClient_GetPlayerInfo; }
	template<> IGameEventManager2_FireEventClientSide* GetHook<IGameEventManager2_FireEventClientSide>() { return &m_Hook_IGameEventManager2_FireEventClientSide; }
	template<> C_HLTVCamera_SetCameraAngle* GetHook<C_HLTVCamera_SetCameraAngle>() { return &m_Hook_C_HLTVCamera_SetCameraAngle; }
	template<> C_HLTVCamera_SetMode* GetHook<C_HLTVCamera_SetMode>() { return &m_Hook_C_HLTVCamera_SetMode; }
	template<> C_HLTVCamera_SetPrimaryTarget* GetHook<C_HLTVCamera_SetPrimaryTarget>() { return &m_Hook_C_HLTVCamera_SetPrimaryTarget; }
	template<> Global_GetLocalPlayerIndex* GetHook<Global_GetLocalPlayerIndex>() { return &m_Hook_Global_GetLocalPlayerIndex; }

	template<class Hook> int AddHook(const typename Hook::Functional& hook)
	{
		auto hkPtr = GetHook<Hook>();
		if (!hkPtr)
			return 0;

		return hkPtr->AddHook(hook);
	}
	template<class Hook> bool RemoveHook(int hookID, const char* funcName)
	{
		auto hkPtr = GetHook<Hook>();
		if (!hkPtr)
			return false;

		return hkPtr->RemoveHook(hookID, funcName);
	}
	template<class Hook> typename Hook::Functional GetOriginal()
	{
		auto hkPtr = GetHook<Hook>();
		if (!hkPtr)
			return nullptr;

		return hkPtr->GetOriginal();
	}
	template<class Hook> void SetState(Hooking::HookAction state)
	{
		auto hkPtr = GetHook<Hook>();
		if (!hkPtr)
			return;

		hkPtr->SetState(state);
	}

private:
	ICvar_ConsoleColorPrintf m_Hook_ICvar_ConsoleColorPrintf;
	ICvar_ConsoleDPrintf m_Hook_ICvar_ConsoleDPrintf;
	ICvar_ConsolePrintf m_Hook_ICvar_ConsolePrintf;

	IClientEngineTools_InToolMode m_Hook_IClientEngineTools_InToolMode;
	IClientEngineTools_IsThirdPersonCamera m_Hook_IClientEngineTools_IsThirdPersonCamera;
	IClientEngineTools_SetupEngineView m_Hook_IClientEngineTools_SetupEngineView;

	IVEngineClient_GetPlayerInfo m_Hook_IVEngineClient_GetPlayerInfo;

	IGameEventManager2_FireEventClientSide m_Hook_IGameEventManager2_FireEventClientSide;

	C_HLTVCamera_SetCameraAngle m_Hook_C_HLTVCamera_SetCameraAngle;
	C_HLTVCamera_SetMode m_Hook_C_HLTVCamera_SetMode;
	C_HLTVCamera_SetPrimaryTarget m_Hook_C_HLTVCamera_SetPrimaryTarget;

	Global_GetLocalPlayerIndex m_Hook_Global_GetLocalPlayerIndex;

	void IngameStateChanged(bool inGame);
	class Panel;
	std::unique_ptr<Panel> m_Panel;

	// Passthrough from Interfaces::GetHLTVCamera() so we don't have to #include "Interfaces.h" yet
	static C_HLTVCamera* GetHLTVCamera();
};

using ICvar_ConsoleColorPrintf = HookManager::ICvar_ConsoleColorPrintf;
using ICvar_ConsoleDPrintf = HookManager::ICvar_ConsoleDPrintf;
using ICvar_ConsolePrintf = HookManager::ICvar_ConsolePrintf;

using IClientEngineTools_InToolMode = HookManager::IClientEngineTools_InToolMode;
using IClientEngineTools_IsThirdPersonCamera = HookManager::IClientEngineTools_IsThirdPersonCamera;
using IClientEngineTools_SetupEngineView = HookManager::IClientEngineTools_SetupEngineView;

using IVEngineClient_GetPlayerInfo = HookManager::IVEngineClient_GetPlayerInfo;

using IGameEventManager2_FireEventClientSide = HookManager::IGameEventManager2_FireEventClientSide;

using C_HLTVCamera_SetCameraAngle = HookManager::C_HLTVCamera_SetCameraAngle;
using C_HLTVCamera_SetMode = HookManager::C_HLTVCamera_SetMode;
using C_HLTVCamera_SetPrimaryTarget = HookManager::C_HLTVCamera_SetPrimaryTarget;

using Global_GetLocalPlayerIndex = HookManager::Global_GetLocalPlayerIndex;

extern void* SignatureScan(const char* moduleName, const char* signature, const char* mask);
extern HookManager* GetHooks();

template<> C_HLTVCamera_SetCameraAngle::Functional HookManager::GetFunc<C_HLTVCamera_SetCameraAngle>()
{
	return std::bind(
		[](RawSetCameraAngleFn func, C_HLTVCamera* pThis, const QAngle& ang) { func(pThis, ang); },
		GetRawFunc_C_HLTVCamera_SetCameraAngle(), GetHLTVCamera(), std::placeholders::_1);
}
template<> HookManager::C_HLTVCamera_SetMode::Functional HookManager::GetFunc<HookManager::C_HLTVCamera_SetMode>()
{
	return std::bind(
		[](RawSetModeFn func, C_HLTVCamera* pThis, int mode) { func(pThis, mode); },
		GetRawFunc_C_HLTVCamera_SetMode(), GetHLTVCamera(), std::placeholders::_1);
}
template<> C_HLTVCamera_SetPrimaryTarget::Functional HookManager::GetFunc<C_HLTVCamera_SetPrimaryTarget>()
{
	return std::bind(
		[](RawSetPrimaryTargetFn func, C_HLTVCamera* pThis, int target) { func(pThis, target); },
		GetRawFunc_C_HLTVCamera_SetPrimaryTarget(), GetHLTVCamera(), std::placeholders::_1);
}

template<> Global_GetLocalPlayerIndex::Functional HookManager::GetFunc<Global_GetLocalPlayerIndex>()
{
	return std::bind(GetRawFunc_Global_GetLocalPlayerIndex());
}