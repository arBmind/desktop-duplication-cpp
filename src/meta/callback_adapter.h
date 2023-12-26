#pragma once
#include <bit>
#include <memory>
#include <type_traits>

template<class Parameter = void*>
class UniqueCallbackAdapter final {
    static_assert(sizeof(Parameter) >= sizeof(void*));
    static_assert(alignof(Parameter) >= alignof(void*));
    using ParameterFunction = void(Parameter);
    using Deleter = ParameterFunction*;
    using Invoker = ParameterFunction*;
    static void noopParameterFunction(Parameter){}

    Parameter m_param = nullptr;
    Deleter m_deleter = &UniqueCallbackAdapter::noopParameterFunction;
    Invoker m_invoker = &UniqueCallbackAdapter::noopParameterFunction;

    template<class Functor>
    struct Helper {
        static constexpr auto is_small = (sizeof(Functor) <= sizeof(Parameter));

        static auto param(Functor&& functor) -> Parameter {
            if constexpr (is_small) {
                auto result = Parameter{};
                new (&result) Functor(std::forward<Functor>(functor));
                return result;
            }
            else return std::bit_cast<Parameter>(std::make_unique<Functor>(std::forward<Functor>(functor)).release());
        }
        static void deleter(Parameter param) {
            if constexpr (is_small) {
                auto& functor = *std::launder(std::bit_cast<Functor*>(&param));
                functor.~Functor();
            }
            else {
                std::unique_ptr<Functor>(std::bit_cast<Functor*>(param));
            }
        }
        static void invoker(Parameter param) {
            if constexpr (is_small) {
                auto& functor = *std::launder(std::bit_cast<Functor*>(&param));
                functor();
                functor.~Functor();
            }
            else {
                auto functor_ptr = std::unique_ptr<Functor>(std::bit_cast<Functor*>(param));
                (*functor_ptr)();
            }
        }
    };

public:
    UniqueCallbackAdapter() = default;
    template<class Functor>
    explicit UniqueCallbackAdapter(Functor&& functor)
        : m_param{Helper<Functor>::param(std::forward<Functor>(functor))}
        , m_deleter{&Helper<Functor>::deleter}
        , m_invoker{&Helper<Functor>::invoker} {}
    UniqueCallbackAdapter(const UniqueCallbackAdapter&) = delete;
    UniqueCallbackAdapter& operator=(const UniqueCallbackAdapter&) = delete;
    UniqueCallbackAdapter(UniqueCallbackAdapter&&) = delete;
    UniqueCallbackAdapter& operator=(UniqueCallbackAdapter&&) = delete;
    ~UniqueCallbackAdapter() { m_deleter(m_param); }

    auto c_parameter() const -> Parameter { return m_param; }
    auto c_callback() const -> Invoker { return m_invoker; }

    void release() { m_deleter = &UniqueCallbackAdapter::noopParameterFunction; }
};
