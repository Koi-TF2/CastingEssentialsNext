#pragma once
#include "GroupManualClassHook.h"

namespace Hooking
{
template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class OriginalFnType, class DetourFnType,
         class MemFnType, class FunctionalType, class Type, class RetVal, class... Args>
class BaseGroupGlobalVirtualHook : public BaseGroupManualClassHook<FuncEnumType, hookID, vaArgs, OriginalFnType,
                                                                   DetourFnType, FunctionalType, Type, RetVal, Args...>
{
public:
    using BaseGroupGlobalVirtualHookType = BaseGroupGlobalVirtualHook;
    using SelfType = BaseGroupGlobalVirtualHookType;
    using BaseType = BaseGroupManualClassHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType,
                                              FunctionalType, Type, RetVal, Args...>;
    typedef MemFnType MemFnType;

    BaseGroupGlobalVirtualHook(Type* instance, MemFnType memFn, DetourFnType detour = nullptr)
        : BaseType(nullptr, detour)
    {
        m_Instance = instance;
        m_MemberFunction = memFn;
        Assert(m_MemberFunction);
    }

    virtual HookType GetType() const override { return HookType::VirtualGlobal; }
    virtual int GetUniqueHookID() const override { return (int)hookID; }

protected:
    Type* m_Instance;
    MemFnType m_MemberFunction;

private:
    void InitHook() override
    {
        Assert(GetType() == HookType::VirtualGlobal);

        if (!this->m_DetourFunction)
            this->m_DetourFunction = (DetourFnType)this->DefaultDetourFn();

        Assert(m_Instance);
        Assert(m_MemberFunction);
        Assert(m_DetourFunction);

        if (!this->m_BaseHook)
        {
            std::lock_guard<std::recursive_mutex> lock(this->m_BaseHookMutex);
            if (!this->m_BaseHook)
            {
                // Only actual difference between this and non-global virtual hook (CreateVFuncSwapHook vs
                // CreateVTableSwapHook). SMH
                this->m_BaseHook =
                    CreateVFuncSwapHook(m_Instance, this->m_DetourFunction, VTableOffset(m_MemberFunction));
                this->m_BaseHook->Hook();
            }
        }
    }

    BaseGroupGlobalVirtualHook() = delete;
    BaseGroupGlobalVirtualHook(const SelfType& other) = delete;
};

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
class GroupGlobalVirtualHook;

// Non variable-arguments version
template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
class GroupGlobalVirtualHook<FuncEnumType, hookID, false, Type, RetVal, Args...>
    : public BaseGroupGlobalVirtualHook<FuncEnumType, hookID, false, Internal::LocalFnPtr<Type, RetVal, Args...>,
                                        Internal::LocalDetourFnPtr<Type, RetVal, Args...>,
                                        Internal::MemberFnPtr<Type, RetVal, Args...>,
                                        Internal::LocalFunctionalType<Type, RetVal, Args...>, Type, RetVal, Args...>
{
public:
    using SelfType = GroupGlobalVirtualHook;
    using BaseType =
        BaseGroupGlobalVirtualHook<FuncEnumType, hookID, false, Internal::LocalFnPtr<Type, RetVal, Args...>,
                                   Internal::LocalDetourFnPtr<Type, RetVal, Args...>,
                                   Internal::MemberFnPtr<Type, RetVal, Args...>,
                                   Internal::LocalFunctionalType<Type, RetVal, Args...>, Type, RetVal, Args...>;
    using MemFnType = typename BaseType::MemFnType;
    using DetourFnType = typename BaseType::DetourFnType;

    GroupGlobalVirtualHook(Type* instance, MemFnType fn, DetourFnType detour = nullptr) : BaseType(instance, fn, detour)
    {
    }
    GroupGlobalVirtualHook(Type* instance, RetVal (Type::*fn)(Args...) const, DetourFnType detour = nullptr)
        : BaseType(instance, reinterpret_cast<MemFnType>(fn), detour)
    {
    }

private:
    GroupGlobalVirtualHook() = delete;
    GroupGlobalVirtualHook(const SelfType& other) = delete;

    DetourFnType DefaultDetourFn() override { return this->SharedLocalDetourFn(); }
};

#if 0
	// Variable arguments version
	template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
	class GroupGlobalVirtualHook<FuncEnumType, hookID, true, Type, RetVal, Args...> :
		public BaseGroupGlobalVirtualHook<FuncEnumType, hookID, true, Internal::LocalVaArgsFnPtr<Type, RetVal, Args...>, Internal::LocalVaArgsFnPtr<Type, RetVal, Args...>, Internal::MemFnVaArgsPtr<Type, RetVal, Args...>, Type, RetVal, Args...>
	{
	public:
		typedef GroupGlobalVirtualHook<FuncEnumType, hookID, true, Type, RetVal, Args...> SelfType;
		typedef BaseGroupGlobalVirtualHookType BaseType;

		GroupGlobalVirtualHook(Type* instance, MemFnType fn, DetourFnType detour = nullptr) : BaseType(instance, fn, detour) { }
		GroupGlobalVirtualHook(Type* instance, RetVal(Type::*fn)(Args..., ...) const, DetourFnType detour = nullptr) :
			SelfType(instance, reinterpret_cast<MemFnType>(fn), detour) { }

	private:
		GroupGlobalVirtualHook() = delete;
		GroupGlobalVirtualHook(const SelfType& other) = delete;

		DetourFnType DefaultDetourFn() override { return Internal::LocalVaArgsDetourFn<SelfType, Type, RetVal, Args...>(this); }
	};
#endif
}