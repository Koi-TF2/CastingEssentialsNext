#pragma once
#include "GroupClassHook.h"

namespace Hooking
{
	template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class OriginalFnType, class DetourFnType, class MemFnType, class Type, class RetVal, class... Args>
	class BaseGroupVirtualHook : public BaseGroupClassHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, Type, RetVal, Args...>
	{
	public:
		typedef BaseGroupVirtualHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, MemFnType, Type, RetVal, Args...> BaseGroupVirtualHookType;
		typedef BaseGroupVirtualHookType SelfType;
		typedef BaseGroupClassHookType BaseType;
		typedef MemFnType MemFnType;

		BaseGroupVirtualHook(Type* instance, MemFnType fn, DetourFnType detour = nullptr) : BaseType((ConstructorParam1*)instance, (ConstructorParam2*)detour)
		{
			m_MemberFunction = fn;
			Assert(m_MemberFunction);
		}

		virtual HookType GetType() const override { return HookType::Virtual; }

	protected:
		MemFnType m_MemberFunction;

	private:
		void InitHook() override
		{
			Assert(GetType() == HookType::Virtual);

			if (!m_DetourFunction)
				m_DetourFunction = (DetourFnType)DefaultDetourFn();

			Assert(m_Instance);
			Assert(m_MemberFunction);
			Assert(m_DetourFunction);

			if (!m_BaseHook)
			{
				std::lock_guard<std::recursive_mutex> lock(m_BaseHookMutex);
				if (!m_BaseHook)
				{
					m_BaseHook = CreateVTableSwapHook(m_Instance, m_DetourFunction, VTableOffset(m_MemberFunction));
					m_BaseHook->Hook();
				}
			}
		}

		BaseGroupVirtualHook() = delete;
		BaseGroupVirtualHook(const SelfType& other) = delete;
	};

	template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args> class GroupVirtualHook;

	// Non variable-arguments version
	template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
	class GroupVirtualHook<FuncEnumType, hookID, false, Type, RetVal, Args...> :
		public BaseGroupVirtualHook<FuncEnumType, hookID, false, Internal::LocalFnPtr<Type, RetVal, Args...>, Internal::LocalDetourFnPtr<Type, RetVal, Args...>, Internal::MemberFnPtr<Type, RetVal, Args...>, Type, RetVal, Args...>
	{
	public:
		typedef BaseGroupVirtualHookType BaseType;

		GroupVirtualHook(Type* instance, MemFnType fn, DetourFnType detour = nullptr) : BaseType(instance, fn, detour) { }
		GroupVirtualHook(Type* instance, RetVal(Type::*fn)(Args...) const, DetourFnType detour = nullptr) :
			BaseType(instance, reinterpret_cast<MemFnType>(fn), detour)
		{ }

	private:
		GroupVirtualHook() = delete;
		GroupVirtualHook(const SelfType& other) = delete;

		DetourFnType DefaultDetourFn() override { return Internal::LocalDetourFn<SelfType, Type, RetVal, Args...>(this); }
	};

	// Variable arguments version
	template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
	class GroupVirtualHook<FuncEnumType, hookID, true, Type, RetVal, Args...> :
		public BaseGroupVirtualHook<FuncEnumType, hookID, true, Internal::LocalVaArgsFnPtr<Type, RetVal, Args...>, Internal::LocalVaArgsFnPtr<Type, RetVal, Args...>, Internal::MemFnVaArgsPtr<Type, RetVal, Args...>, Type, RetVal, Args...>
	{
	public:
		typedef GroupVirtualHook<FuncEnumType, hookID, true, Type, RetVal, Args...> SelfType;
		typedef BaseGroupVirtualHookType BaseType;

		GroupVirtualHook(Type* instance, MemFnType fn, DetourFnType detour = nullptr) : BaseType(instance, fn, detour) { }
		GroupVirtualHook(Type* instance, RetVal(Type::*fn)(Args..., ...) const, DetourFnType detour = nullptr) :
			SelfType(instance, reinterpret_cast<MemFnType>(fn), detour)	{ }

	private:
		GroupVirtualHook() = delete;
		GroupVirtualHook(const SelfType& other) = delete;

		DetourFnType DefaultDetourFn() override { return Internal::LocalVaArgsDetourFn<SelfType, Type, RetVal, Args...>(this); }
	};
}