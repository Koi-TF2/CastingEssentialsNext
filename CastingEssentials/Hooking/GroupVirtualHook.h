#pragma once
#include "GroupClassHook.h"

#include <type_traits>

namespace Hooking
{
template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class OriginalFnType, class DetourFnType,
         class MemFnType, class FunctionalType, class Type, class RetVal, class... Args>
class BaseGroupVirtualHook : public BaseGroupClassHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType,
                                                       FunctionalType, Type, RetVal, Args...>
{
public:
    using BaseGroupVirtualHookType = BaseGroupVirtualHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType,
                                                          MemFnType, FunctionalType, Type, RetVal, Args...>;
    using SelfType = BaseGroupVirtualHookType;
    using BaseType = BaseGroupClassHook<FuncEnumType, hookID, vaArgs, OriginalFnType, DetourFnType, FunctionalType,
                                        Type, RetVal, Args...>;
    typedef MemFnType MemFnType;

    BaseGroupVirtualHook(Type* instance, MemFnType fn, DetourFnType detour = nullptr)
        : BaseType((typename BaseType::ConstructorParam1*)instance, (typename BaseType::ConstructorParam2*)detour)
    {
        m_MemberFunction = fn;
        Assert(m_MemberFunction);
    }

    virtual HookType GetType() const override { return HookType::Virtual; }
    virtual int GetUniqueHookID() const override { return (int)hookID; }

protected:
    MemFnType m_MemberFunction;

private:
    void InitHook() override
    {
        Assert(GetType() == HookType::Virtual);

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
                this->m_BaseHook =
                    CreateVTableSwapHook(this->m_Instance, this->m_DetourFunction, VTableOffset(m_MemberFunction));
                this->m_BaseHook->Hook();
            }
        }
    }

    BaseGroupVirtualHook() = delete;
    BaseGroupVirtualHook(const SelfType& other) = delete;
};

template<class FuncEnumType, FuncEnumType hookID, bool vaArgs, class Type, class RetVal, class... Args>
class GroupVirtualHook;

// Non variable-arguments version
template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
class GroupVirtualHook<FuncEnumType, hookID, false, Type, RetVal, Args...>
    : public BaseGroupVirtualHook<FuncEnumType, hookID, false, Internal::LocalFnPtr<Type, RetVal, Args...>,
                                  Internal::LocalDetourFnPtr<Type, RetVal, Args...>,
                                  Internal::MemberFnPtr<Type, RetVal, Args...>,
                                  Internal::GlobalFunctionalType<RetVal, Args...>, Type, RetVal, Args...>
{
    auto GetSelf() const { return this; }

public:
    using SelfType = GroupVirtualHook<FuncEnumType, hookID, false, Type, RetVal, Args...>;
    using BaseType = BaseGroupVirtualHook<FuncEnumType, hookID, false, Internal::LocalFnPtr<Type, RetVal, Args...>,
                                          Internal::LocalDetourFnPtr<Type, RetVal, Args...>,
                                          Internal::MemberFnPtr<Type, RetVal, Args...>,
                                          Internal::GlobalFunctionalType<RetVal, Args...>, Type, RetVal, Args...>;

    GroupVirtualHook(Type* instance, typename BaseType::MemFnType fn, typename BaseType::DetourFnType detour = nullptr)
        : BaseType(instance, fn, detour)
    {
    }
    GroupVirtualHook(Type* instance, RetVal (Type::*fn)(Args...) const,
                     typename BaseType::DetourFnType detour = nullptr)
        : BaseType(instance, reinterpret_cast<typename BaseType::MemFnType>(fn), detour)
    {
    }

private:
    GroupVirtualHook() = delete;
    GroupVirtualHook(const SelfType& other) = delete;

    typename BaseType::DetourFnType DefaultDetourFn() override { return this->LocalDetourFn(); }
};

// Variable arguments version
template<class FuncEnumType, FuncEnumType hookID, class Type, class RetVal, class... Args>
class GroupVirtualHook<FuncEnumType, hookID, true, Type, RetVal, Args...>
    : public BaseGroupVirtualHook<FuncEnumType, hookID, true, Internal::LocalVaArgsFnPtr<Type, RetVal, Args...>,
                                  Internal::LocalVaArgsFnPtr<Type, RetVal, Args...>,
                                  Internal::MemFnVaArgsPtr<Type, RetVal, Args...>,
                                  Internal::GlobalFunctionalType<RetVal, Args...>, Type, RetVal, Args...>
{
public:
    using SelfType = GroupVirtualHook<FuncEnumType, hookID, true, Type, RetVal, Args...>;
    using BaseType = BaseGroupVirtualHook<FuncEnumType, hookID, true, Internal::LocalVaArgsFnPtr<Type, RetVal, Args...>,
                                          Internal::LocalVaArgsFnPtr<Type, RetVal, Args...>,
                                          Internal::MemFnVaArgsPtr<Type, RetVal, Args...>,
                                          Internal::GlobalFunctionalType<RetVal, Args...>, Type, RetVal, Args...>;

    GroupVirtualHook(Type* instance, typename BaseType::MemFnType fn, typename BaseType::DetourFnType detour = nullptr)
        : BaseType(instance, fn, detour)
    {
    }
    GroupVirtualHook(Type* instance, RetVal (Type::*fn)(Args..., ...) const,
                     typename BaseType::DetourFnType detour = nullptr)
        : SelfType(instance, reinterpret_cast<typename BaseType::MemFnType>(fn), detour)
    {
    }

private:
    GroupVirtualHook() = delete;
    GroupVirtualHook(const SelfType& other) = delete;

    typename BaseType::DetourFnType DefaultDetourFn() override { return this->LocalVaArgsDetourFn(); }
};
}