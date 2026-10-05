#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace ninfer::text {
// Request-owned incremental string length and uniqueness. The immutable schema
// plan is shared; a copy owns independent parser, branch and seen-value state.
class UniqueStringState {
public:
    struct TokenFeatures {
        std::uint32_t raw_scalar_starts = 0;
        bool quote_or_escape = false;
        bool control = false;
    };
    struct StringMaskState {
        bool skip_semantics = false;
        bool fast_plain = false;
        std::uint64_t reserved_scalars = 0;
        bool bounded = false;
        std::uint64_t max_remaining = 0;
        bool reasoning = false;
        std::string_view reasoning_close;
        std::size_t reasoning_tail_size = 0;
    };
    // Token metadata is immutable and computed once for the model vocabulary.
    static TokenFeatures InspectToken(std::string_view decoded_token) noexcept;
    // Only the compiler projection loses these keys; the request and this
    // semantic plan retain the constraints. Complex correlated unions keep the
    // original grammar constraints when projection cannot be proved safe.
    static std::string GrammarProjection(const std::string& normalized_schema);
    static void AnalyzeSchema(const std::string& normalized_schema);
    explicit UniqueStringState(const std::string& normalized_schema,
                               const std::string& reasoning_close = {});
    ~UniqueStringState();
    UniqueStringState(const UniqueStringState&);
    UniqueStringState& operator=(const UniqueStringState&);
    UniqueStringState(UniqueStringState&&) noexcept;
    UniqueStringState& operator=(UniqueStringState&&) noexcept;
    [[nodiscard]] bool accepts(std::string_view decoded_token) const;
    [[nodiscard]] bool accepts(std::string_view decoded_token, const TokenFeatures&) const;
    // False never changes the committed state.
    bool accept(std::string_view decoded_token);
    [[nodiscard]] bool has_constraints() const noexcept;
    [[nodiscard]] bool needs_check(std::string_view decoded_token) const noexcept;
    [[nodiscard]] bool needs_check(const TokenFeatures&) const noexcept;
    [[nodiscard]] StringMaskState mask_state() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ninfer::text
