#pragma once
#include <memory> // unique_ptr
#include <tuple>

template<class... A>
using unique_tuple_ptr = std::unique_ptr<std::tuple<A...>>;

template<class... A>
auto make_tuple_ptr(A &&... a) -> unique_tuple_ptr<std::remove_cvref_t<A>...> {
    [[gsl::suppress("26409"), gsl::suppress("26402")]] // implied by purpose
    return unique_tuple_ptr<std::remove_cvref_t<A>...>{new std::tuple((A &&) a...)};
}

template<class... A>
auto ulong_ptr_cast(unique_tuple_ptr<A...> &&ptr) noexcept -> ULONG_PTR {
    return reinterpret_cast<ULONG_PTR>(ptr.release());
}

template<class... A>
auto unique_tuple_ptr_cast(ULONG_PTR p) noexcept -> unique_tuple_ptr<A...> {
    [[gsl::suppress("26490")]] // required by bad C APIs
    return unique_tuple_ptr<A...>(reinterpret_cast<std::tuple<A...> *>(p));
}
