#include "text/unique_strings.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using ninfer::text::UniqueStringState;
using Json = nlohmann::ordered_json;
namespace {
void require(bool value, const char* expr, int line) {
    if (!value) throw std::runtime_error("line " + std::to_string(line) + ": " + expr);
}
#define REQUIRE(expr) require(static_cast<bool>(expr), #expr, __LINE__)
void bytes(UniqueStringState& state, std::string_view value) {
    for (char c : value) REQUIRE(state.accept(std::string_view(&c, 1)));
}
}
int main() {
    const std::string schema = R"({"type":"string","minLength":1,"maxLength":3})";
    {
        UniqueStringState state(schema);
        REQUIRE(state.has_constraints());
        REQUIRE(!state.accepts(R"("")"));
        REQUIRE(state.accept("\""));
        auto info = state.mask_state(); REQUIRE(info.fast_plain && info.bounded && info.max_remaining == 3);
        auto a = UniqueStringState::InspectToken("abc");
        REQUIRE(!state.needs_check(a)); REQUIRE(state.accepts("abc", a));
        REQUIRE(!state.accepts("abcd", UniqueStringState::InspectToken("abcd")));
        REQUIRE(state.accept("abc"));
        REQUIRE(state.mask_state().max_remaining == 0);
        REQUIRE(!state.accepts("d", UniqueStringState::InspectToken("d")));
        REQUIRE(state.accept("\""));
    }
    for (const auto& value : std::vector<std::string>{R"(\n)", R"(\u0061)", R"(\uD83D\uDE00)", "\xf0\x9f\x98\x80", "\xe4\xb8\xad"}) {
        UniqueStringState state(R"({"type":"string","minLength":1,"maxLength":1})");
        REQUIRE(state.accept("\""));
        bytes(state, value);
        REQUIRE(state.mask_state().reserved_scalars == 1);
        REQUIRE(!state.accepts("x"));
        REQUIRE(state.accept("\""));
    }
    {
        UniqueStringState state(R"({"type":"string","minLength":1,"maxLength":1})");
        REQUIRE(state.accept("\"\xf0"));
        REQUIRE(state.mask_state().fast_plain);
        REQUIRE(state.mask_state().reserved_scalars == 1);
        REQUIRE(!state.accepts("x"));
        REQUIRE(state.accepts("\x9f\x98\x80", UniqueStringState::InspectToken("\x9f\x98\x80")));
        bytes(state, "\x9f\x98\x80");
        REQUIRE(!state.accepts("\xe4\xb8\xad"));
        REQUIRE(state.accept("\""));
    }
    {
        UniqueStringState state(R"({"type":"string","minLength":1,"maxLength":1})");
        REQUIRE(state.accept(R"("\uD83D)"));
        REQUIRE(!state.mask_state().fast_plain);
        REQUIRE(state.mask_state().reserved_scalars == 1);
        REQUIRE(!state.accepts(R"(\uDE00x)"));
        REQUIRE(state.accept(R"(\uDE00")"));
    }
    {
        UniqueStringState state(R"({"type":"string","maxLength":0})");
        REQUIRE(state.accepts(R"("")")); REQUIRE(!state.accepts(R"("a")"));
        UniqueStringState off(R"({"type":"string","minLength":0})"); REQUIRE(!off.has_constraints());
    }
    {
        const std::string object = R"({"type":"object","properties":{"a":{"type":"string","minLength":2,"maxLength":3},"b":{"type":"array","items":{"type":"string","maxLength":1}}},"additionalProperties":false})";
        UniqueStringState state(object);
        REQUIRE(!state.accepts(R"({"a":"x","b":[]})"));
        REQUIRE(state.accept(R"({"a":"ab","b":["x",""]})"));
        UniqueStringState bad(object); REQUIRE(!bad.accepts(R"({"a":"abcd","b":[]})"));
        UniqueStringState array_bad(object); REQUIRE(!array_bad.accepts(R"({"a":"ab","b":["xy"]})"));
    }
    {
        const std::string refs = R"({"$defs":{"S":{"type":"string","minLength":1,"maxLength":1}},"type":"array","prefixItems":[{"$ref":"#/$defs/S"},{"type":"string","maxLength":2}],"items":false})";
        UniqueStringState state(refs); REQUIRE(state.accept(R"(["中","ab"])"));
        UniqueStringState bad(refs); REQUIRE(!bad.accepts(R"(["ab","ab"])"));
        auto projected = Json::parse(UniqueStringState::GrammarProjection(refs));
        REQUIRE(!projected["$defs"]["S"].contains("maxLength"));
    }
    {
        const std::string nullable = R"({"anyOf":[{"type":"null"},{"type":"string","minLength":2,"maxLength":3}]})";
        auto p = Json::parse(UniqueStringState::GrammarProjection(nullable));
        REQUIRE(!p["anyOf"][1].contains("maxLength"));
        UniqueStringState a(nullable); REQUIRE(a.accept("null"));
        UniqueStringState b(nullable); REQUIRE(b.accept(R"("ab")"));
        UniqueStringState c(nullable); REQUIRE(!c.accepts(R"("a")"));
    }
    {
        const std::string union_schema = R"({"type":"object","properties":{"safe":{"type":"string","maxLength":3},"mixed":{"anyOf":[{"type":"string","maxLength":1},{"type":"string","minLength":4,"maxLength":5}]}}})";
        auto p = Json::parse(UniqueStringState::GrammarProjection(union_schema));
        REQUIRE(!p["properties"]["safe"].contains("maxLength"));
        REQUIRE(p["properties"]["mixed"]["anyOf"][0].contains("maxLength"));
        UniqueStringState state(union_schema); REQUIRE(!state.accepts(R"({"safe":"ok","mixed":"ab"})"));
        REQUIRE(state.accept(R"({"safe":"ok","mixed":"abcd"})"));
    }
    {
        const std::string shared = R"({"$defs":{"S":{"type":"string","maxLength":2}},"type":"object","properties":{"safe":{"type":"string","maxLength":3},"mixed":{"anyOf":[{"$ref":"#/$defs/S"},{"type":"string","pattern":"^a+$"}]}}})";
        auto p = Json::parse(UniqueStringState::GrammarProjection(shared));
        REQUIRE(p["$defs"]["S"].contains("maxLength"));
        REQUIRE(!p["properties"]["safe"].contains("maxLength"));
    }
    {
        const std::string literal = R"({"type":"object","properties":{"minLength":{"type":"string","minLength":1,"maxLength":2},"data":{"const":{"minLength":20,"maxLength":30}}},"default":{"minLength":7}})";
        auto p = Json::parse(UniqueStringState::GrammarProjection(literal));
        REQUIRE(p["properties"].contains("minLength"));
        REQUIRE(!p["properties"]["minLength"].contains("maxLength"));
        REQUIRE(p["properties"]["data"]["const"]["maxLength"] == 30);
        REQUIRE(p["default"]["minLength"] == 7);
        UniqueStringState state(literal);
        bytes(state, R"({"minLength": "a", "data": {"minLength": 20, "maxLength": 30}})");
    }
    {
        const std::string literal_names = R"({"type":"object","properties":{"minLength":{"type":"string","minLength":1,"maxLength":180},"metadata":{"const":{"maxLength":180,"minLength":1}}},"required":["minLength","metadata"],"additionalProperties":false})";
        UniqueStringState state(literal_names);
        bytes(state, R"({"minLength": "a", "metadata": {"maxLength": 180, "minLength": 1}})");
    }
    {
        UniqueStringState state(schema, "END"); REQUIRE(state.accept("thought E"));
        REQUIRE(state.mask_state().reasoning && state.mask_state().reasoning_tail_size == 1);
        REQUIRE(!state.accepts(R"(ND"abcd")")); REQUIRE(state.accept(R"(ND"a)"));
        auto fork = state; REQUIRE(fork.accept(R"(bc")"));
        REQUIRE(!state.accepts(R"(bcd")")); REQUIRE(state.accept(R"(b")"));
        UniqueStringState tool(schema, "END"); REQUIRE(tool.accept(R"(END<tool>free</tool>)"));
        REQUIRE(tool.mask_state().skip_semantics);
    }
    for (auto count : {127,128,129,179,180,181}) {
        UniqueStringState state(R"({"type":"string","minLength":1,"maxLength":180})");
        const std::string value = "\"" + std::string(count, 'a') + "\"";
        REQUIRE(state.accepts(value) == (count <= 180));
    }
    std::cout << "Original-schema scalar length, projection, UTF/escapes, forks and boundaries PASS\n";
}
