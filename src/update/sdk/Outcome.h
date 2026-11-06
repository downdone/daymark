#pragma once
#include "Update.h"
#include <variant>

namespace daymark::update {
struct UpdateError {
    std::string key;
    int exitCode = 41;
    std::int64_t retryAtUtcSeconds = 0;
    std::uint32_t platformError = 0;
};
template <class T> using Outcome = std::variant<T, UpdateError>;
struct Done {};

// The native engine uses RAII exceptions internally. Embedders can keep that
// implementation detail behind this standard C++ value boundary, without Qt.
template <class Operation>
auto invoke(Operation &&operation) -> Outcome<std::invoke_result_t<Operation>> {
    using Value = std::invoke_result_t<Operation>;
    static_assert(!std::is_void_v<Value>, "Return Done for commands without a value");
    try {
        return std::forward<Operation>(operation)();
    } catch (const Failure &error) {
        return UpdateError{error.key, error.exitCode, error.retryAtUtcSeconds, error.platformError};
    } catch (const std::exception &) {
        return UpdateError{"update.internal_error", 41, 0};
    }
}
} // namespace daymark::update
