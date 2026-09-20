#include "../../superscalar/core/CacheLine.hpp"

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>

#include <array>
#include <cassert>
#include <iostream>

namespace {

using Cache = cpu::InstructionCacheLine<32, 32, 16, 4>;

struct CacheHarness {
    logic::Wire enable{logic::LogicState::HIGH};
    logic::Bus<32> address0;
    logic::Bus<32> address1;
    logic::Bus<32> instruction0;
    logic::Bus<32> instruction1;
    logic::Wire hit0{logic::LogicState::LOW};
    logic::Wire hit1{logic::LogicState::LOW};
    Cache cache{enable, address0, address1, instruction0, instruction1, hit0, hit1};
};

void test_address_splits_into_tag_index_offset() {
    std::cout << "[Test 1] Address splits into tag / index / offset...\n";

    static_assert(Cache::InstructionBytes == 4);
    static_assert(Cache::OffsetBits == 4);
    static_assert(Cache::IndexBits == 4);
    static_assert(Cache::TagBits == 24);

    // 0x00A12CC8:
    //   offset bits [3:0]  = 0x8 -> instruction word 2
    //   index  bits [7:4]  = 0xC -> line 12
    //   tag    bits [31:8] = 0x00A12C
    constexpr std::size_t address = 0x00A12CC8;

    assert(Cache::get_instruction_offset(address) == 2);
    assert(Cache::get_line_index(address) == 0xC);
    assert(Cache::get_tag(address) == 0x00A12C);

    std::cout << "  [PASS] tag/index/offset decode\n";
}

void test_miss_returns_empty() {
    std::cout << "[Test 2] Lookup MISS returns empty data...\n";

    CacheHarness h;
    h.address0.write_value(0x00000100);
    h.address1.write_value(0x00000204);
    h.cache.evaluate();

    assert(h.hit0.read() == logic::LogicState::LOW);
    assert(h.hit1.read() == logic::LogicState::LOW);
    assert(h.instruction0.read_value() == 0);
    assert(h.instruction1.read_value() == 0);

    std::cout << "  [PASS] cold lookup is a miss with empty buses\n";
}

void test_hit_returns_data() {
    std::cout << "[Test 3] Lookup HIT returns cached instruction...\n";

    CacheHarness h;
    constexpr std::size_t address = 0x00000104;
    h.cache.install_line(address, {0x11111111, 0x22222222, 0x33333333, 0x44444444});

    h.address0.write_value(address);
    h.cache.evaluate();

    assert(h.hit0.read() == logic::LogicState::HIGH);
    assert(h.instruction0.read_value() == 0x22222222);

    std::cout << "  [PASS] matching tag/index returns data\n";
}

void test_offset_selects_instruction_in_line() {
    std::cout << "[Test 4] Offset selects the word inside a hit line...\n";

    CacheHarness h;
    constexpr std::size_t line_base = 0x00000300;
    h.cache.install_line(line_base, {0xAAAA0000, 0xAAAA0001, 0xAAAA0002, 0xAAAA0003});

    h.address0.write_value(line_base + 0x0);
    h.address1.write_value(line_base + 0xC);
    h.cache.evaluate();

    assert(h.hit0.read() == logic::LogicState::HIGH);
    assert(h.hit1.read() == logic::LogicState::HIGH);
    assert(h.instruction0.read_value() == 0xAAAA0000);
    assert(h.instruction1.read_value() == 0xAAAA0003);

    std::cout << "  [PASS] same line, different offsets\n";
}

void test_tag_mismatch_is_miss() {
    std::cout << "[Test 5] Same index, different tag is a MISS...\n";

    CacheHarness h;
    constexpr std::size_t installed = 0x00000100; // index 1, tag 0
    constexpr std::size_t alias = 0x00001100;     // index 1, tag 0x11

    assert(Cache::get_line_index(installed) == Cache::get_line_index(alias));
    assert(Cache::get_tag(installed) != Cache::get_tag(alias));

    h.cache.install_line(installed, {0xCAFE0000, 0xCAFE0001, 0xCAFE0002, 0xCAFE0003});
    h.address0.write_value(alias);
    h.cache.evaluate();

    assert(h.hit0.read() == logic::LogicState::LOW);
    assert(h.instruction0.read_value() == 0);

    std::cout << "  [PASS] tag mismatch leaves the data bus empty\n";
}

void test_independent_ports_hit_and_miss() {
    std::cout << "[Test 6] Dual-port lookup can HIT and MISS together...\n";

    CacheHarness h;
    constexpr std::size_t hit_address = 0x00000408;
    h.cache.install_line(hit_address, {0x10, 0x20, 0x30, 0x40});

    h.address0.write_value(hit_address);
    h.address1.write_value(0x00000500);
    h.cache.evaluate();

    assert(h.hit0.read() == logic::LogicState::HIGH);
    assert(h.instruction0.read_value() == 0x30);
    assert(h.hit1.read() == logic::LogicState::LOW);
    assert(h.instruction1.read_value() == 0);

    std::cout << "  [PASS] port 0 hit / port 1 miss\n";
}

void test_disabled_cache_is_empty() {
    std::cout << "[Test 7] Disabled cache reports miss and empty data...\n";

    CacheHarness h;
    constexpr std::size_t address = 0x00000600;
    h.cache.install_line(address, {1, 2, 3, 4});

    h.enable.write(logic::LogicState::LOW);
    h.address0.write_value(address);
    h.address1.write_value(address);
    h.cache.evaluate();

    assert(h.hit0.read() == logic::LogicState::LOW);
    assert(h.hit1.read() == logic::LogicState::LOW);
    assert(h.instruction0.read_value() == 0);
    assert(h.instruction1.read_value() == 0);

    std::cout << "  [PASS] enable LOW suppresses lookup\n";
}

void test_invalidate_turns_hit_into_miss() {
    std::cout << "[Test 8] Invalidate makes a previous HIT empty...\n";

    CacheHarness h;
    constexpr std::size_t address = 0x00000704;
    h.cache.install_line(address, {9, 8, 7, 6});

    h.address0.write_value(address);
    h.cache.evaluate();
    assert(h.hit0.read() == logic::LogicState::HIGH);
    assert(h.instruction0.read_value() == 8);

    h.cache.invalidate();
    h.cache.evaluate();
    assert(h.hit0.read() == logic::LogicState::LOW);
    assert(h.instruction0.read_value() == 0);

    std::cout << "  [PASS] invalidate clears valid lines\n";
}

} // namespace

int main() {
    std::cout << "--- Testing InstructionCacheLine ---\n";

    test_address_splits_into_tag_index_offset();
    test_miss_returns_empty();
    test_hit_returns_data();
    test_offset_selects_instruction_in_line();
    test_tag_mismatch_is_miss();
    test_independent_ports_hit_and_miss();
    test_disabled_cache_is_empty();
    test_invalidate_turns_hit_into_miss();

    std::cout << "[PASS] InstructionCacheLine unit tests successful\n";
    return 0;
}
