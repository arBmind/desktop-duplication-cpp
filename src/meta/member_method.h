#pragma once
#include <tuple>

template<class T>
constexpr auto is_member_method = false;

template<class Ret, class C, class... Args>
constexpr auto is_member_method<Ret (C::*)(Args...)> = true;

template<class T>
concept MemberMethod = is_member_method<T>;

template<class Sig, class T>
constexpr auto is_member_method_sig = false;

template<class Ret, class C, class... Args>
constexpr auto is_member_method_sig<Ret(Args...), Ret (C::*)(Args...)> = true;

template<class Sig, class T>
concept MemberMedthodSig = is_member_method_sig<Sig, T>;

template<class Ret, class C, class... Args>
auto memberMethodClass(Ret (C::*)(Args...)) -> C;

template<auto M>
using MemberMethodClass = decltype(memberMethodClass(M));

template<class Ret, class C, class... Args>
auto memberMethodArgsTuple(Ret (C::*)(Args...)) -> std::tuple<std::remove_cvref_t<Args>...>;

template<auto M>
using MemberMethodArgsTuple = decltype(memberMethodArgsTuple(M));

template<class Ret, class C, class... Args>
auto memberMethodClassArgsTuple(Ret (C::*)(Args...))
    -> std::tuple<C *, std::remove_cvref_t<Args>...>;

template<auto M>
using MemberMethodClassArgsTuple = decltype(memberMethodClassArgsTuple(M));

// #include <type_traits>

// struct TC {
//     constexpr int memberMethod(int x) { return x + 5; }
// };

// static_assert(std::is_same_v<MemberMethodClass<&TC::memberMethod>, TC>);

// template<auto M>
// constexpr auto memberCall(TC p, int x) {
//     return (p.*M)(x);
// }

// static_assert(memberCall<&TC::memberMethod>(TC{}, 3) == 8);
