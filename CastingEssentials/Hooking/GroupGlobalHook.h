#pragma once
#include "BaseGroupHook.h"

#include <utility>

namespace Hooking
{
template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class OriginalFnType, class DetourFnType,
         class FunctionalType, class RetVal, class... Args>
class BaseGroupGlobalHook : public BaseGroupHook<FuncEnumType, hookID, FunctionalType, RetVal, Args...>
{
    static_assert(!vaArgs || (sizeof...(Args) >= 1),
                  "Must have at least 1 concrete argument defined for a variable-argument function");

public:
    using BaseGroupGlobalHookType = BaseGroupGlobalHook;
    using SelfType = BaseGroupGlobalHookType;
    using BaseType = BaseGroupHook<FuncEnumType, hookID, FunctionalType, RetVal, Args...>;
    typedef OriginalFnType OriginalFnType;
    typedef DetourFnType DetourFnType;

    BaseGroupGlobalHook(OriginalFnType fn, DetourFnType detour = nullptr)
    {
        m_OriginalFunction = fn;
        // Assert(m_OriginalFunction); we pass in null for GroupGlobalVirtualHook >.>

        m_DetourFunction = detour;
    }

    virtual HookType GetType() const override { return HookType::Global; }
    virtual int GetUniqueHookID() const override { return (int)hookID; }

protected:
    static SelfType* This() { return assert_cast<SelfType*>(BaseType::BaseThis()); }

    static Internal::GlobalDetourFnPtr<RetVal, Args...> GlobalDetourFn()
    {
        return &BaseType::template HookFunctionsInvoker<RetVal>::Invoke;
    }

    DetourFnType m_DetourFunction;

    virtual DetourFnType DefaultDetourFn() = 0;

    virtual void InitHook() override
    {
        Assert(GetType() == HookType::Global || GetType() == HookType::Class || GetType() == HookType::GlobalClass);

        if (!m_DetourFunction)
            m_DetourFunction = (DetourFnType)DefaultDetourFn();

        Assert(m_OriginalFunction);
        Assert(m_DetourFunction);

        if (!this->m_BaseHook)
        {
            std::lock_guard<std::recursive_mutex> lock(this->m_BaseHookMutex);
            if (!this->m_BaseHook)
            {
                this->m_BaseHook = CreateDetour(m_OriginalFunction, m_DetourFunction);
                this->m_BaseHook->Hook();
            }
        }
    }

    template<size_t... Is>
    FunctionalType GetOriginalImpl(std::index_sequence<Is...>)
    {
        Assert(GetType() == HookType::Global);

        // Make sure we're initialized so we don't have any nasty race conditions
        InitHook();

        if (this->m_BaseHook)
        {
            OriginalFnType originalFnPtr = (OriginalFnType)(this->m_BaseHook->GetOriginalFunction());
            return std::bind(originalFnPtr, (std::_Ph<(int)(Is + 1)>{})...);
        }
        else
        {
            Assert(!"Should never get here... hook should be initialized so we don't have a potential race condition!");
            return nullptr;
        }
    }

    struct ConstructorParam1;
    struct ConstructorParam2;
    BaseGroupGlobalHook(ConstructorParam1* detour)
    {
        m_OriginalFunction = nullptr;
        m_DetourFunction = (DetourFnType)detour;
    }

private:
    OriginalFnType m_OriginalFunction;

    BaseGroupGlobalHook() = delete;
    BaseGroupGlobalHook(const SelfType& other) = delete;
};

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class RetVal, class... Args>
class GroupGlobalHook;

// Non variable-arguments version
template<class FuncEnumType, FuncEnumType hookID, class RetVal, class... Args>
class GroupGlobalHook<FuncEnumType, hookID, false, RetVal, Args...> final
    : public BaseGroupGlobalHook<FuncEnumType, hookID, false, Internal::GlobalDetourFnPtr<RetVal, Args...>,
                                 Internal::GlobalDetourFnPtr<RetVal, Args...>,
                                 Internal::GlobalFunctionalType<RetVal, Args...>, RetVal, Args...>
{
public:
    typedef GroupGlobalHook<FuncEnumType, hookID, false, RetVal, Args...> GroupGlobalHookType;
    typedef GroupGlobalHookType SelfType;
    using BaseType = BaseGroupGlobalHook<FuncEnumType, hookID, false, Internal::GlobalDetourFnPtr<RetVal, Args...>,
                                         Internal::GlobalDetourFnPtr<RetVal, Args...>,
                                         Internal::GlobalFunctionalType<RetVal, Args...>, RetVal, Args...>;

    GroupGlobalHook(typename BaseType::OriginalFnType fn, typename BaseType::DetourFnType detour = nullptr)
        : BaseType(fn, detour)
    {
    }

    virtual typename BaseType::Functional GetOriginal() override
    {
        return this->GetOriginalImpl(std::index_sequence_for<Args...>{});
    }

private:
    typename BaseType::DetourFnType DefaultDetourFn() override { return this->GlobalDetourFn(); }

    GroupGlobalHook() = delete;
    GroupGlobalHook(const SelfType& other) = delete;
};

#if 0
	// Variable arguments version
	template<class FuncEnumType, FuncEnumType hookID, class RetVal, class... Args>
	class GroupGlobalHook<FuncEnumType, hookID, true, RetVal, Args...> final :
		public BaseGroupGlobalHook<FuncEnumType, hookID, true, Internal::GlobalVaArgsFnPtr<RetVal, Args...>, Internal::GlobalVaArgsFnPtr<RetVal, Args...>, Internal::GlobalFunctionalType<RetVal, Args...>, RetVal, Args...>
	{
	public:
		typedef GroupGlobalHook<FuncEnumType, hookID, true, RetVal, Args...> SelfType;
		typedef SelfType GroupGlobalHookType;
		typedef BaseGroupGlobalHookType BaseType;

		GroupGlobalHook(OriginalFnType fn, DetourFnType detour = nullptr) : BaseType(fn, detour) { }

		virtual Functional GetOriginal() override { return GetOriginalImpl(std::index_sequence_for<Args...>{}); }

	private:
		GroupGlobalHook() = delete;
		GroupGlobalHook(const SelfType& other) = delete;
	};
#endif
}