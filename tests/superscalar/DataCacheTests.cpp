#include <iostream>
#include <cassert>
#include <cstdint>

#include "../../superscalar/core/memory/data/DataMemory.hpp"
#include "../../superscalar/core/memory/data/DataCache.hpp"

using namespace cpu;

void test_datacache_read_refill_and_hits()
{
    std::cout << "[Test 1] Testing DataCache read refill and hits..." << std::endl;

    SuperScalarDataMemory<1024> memory;
    memory.write_word(0, 0x11111111);
    memory.write_word(1, 0x22222222);
    memory.write_word(2, 0x33333333);
    memory.write_word(3, 0x44444444);

    DataCache<4, 4, 1024> cache(memory);

    // Initial read is a compulsory miss; refills entire 4-word line
    auto val0 = cache.read(0);
    assert(val0.has_value() && *val0 == 0x11111111);
    assert(cache.stats().read_misses == 1);
    assert(cache.stats().read_hits == 0);

    // Subsequent reads within the same line must hit
    auto val1 = cache.read(4);
    assert(val1.has_value() && *val1 == 0x22222222);
    auto val2 = cache.read(8);
    assert(val2.has_value() && *val2 == 0x33333333);
    auto val3 = cache.read(12);
    assert(val3.has_value() && *val3 == 0x44444444);

    assert(cache.stats().read_hits == 3);
    assert(cache.stats().read_misses == 1);
    assert(cache.stats().hit_rate() == 75.0);

    std::cout << "  [PASS] Read refill and hits verified (Hit rate: "
              << cache.stats().hit_rate() << "%)." << std::endl;
}

void test_datacache_writeback_and_eviction()
{
    std::cout << "[Test 2] Testing DataCache write-allocate and dirty eviction..." << std::endl;

    SuperScalarDataMemory<1024> memory;
    DataCache<2, 4, 1024> cache(memory); // 2 lines, 4 words per line = 8 words total

    // Write to byte address 0 (word 0, line 0)
    bool w0 = cache.write(0, 0xCAFEBABE);
    assert(w0);
    assert(cache.stats().write_misses == 1);

    // Read back immediately from cache (hit)
    auto r0 = cache.read(0);
    assert(r0.has_value() && *r0 == 0xCAFEBABE);
    assert(cache.stats().read_hits == 1);

    // Backing memory is not yet updated (write-back policy)
    assert(memory.read_word(0) == 0);

    // Access address that maps to the SAME line index (index 0) with a different tag
    // line_index = (word_addr / 4) % 2.
    // word_addr 8: (8 / 4) % 2 = 2 % 2 = 0. Same line index 0, new tag!
    bool w_conflict = cache.write(8 * 4, 0xDEADBEEF);
    assert(w_conflict);
    assert(cache.stats().evictions == 1);
    assert(cache.stats().writebacks == 1);

    // The evicted dirty line must have written back to word 0 in memory!
    assert(memory.read_word(0) == 0xCAFEBABE);

    std::cout << "  [PASS] Writeback on eviction verified." << std::endl;
}

void test_datacache_flush()
{
    std::cout << "[Test 3] Testing DataCache flush()..." << std::endl;

    SuperScalarDataMemory<1024> memory;
    DataCache<4, 4, 1024> cache(memory);

    cache.write(16, 0x12345678); // word 4
    cache.write(32, 0x87654321); // word 8

    // Memory does not have dirty words yet
    assert(memory.read_word(4) == 0);
    assert(memory.read_word(8) == 0);

    // Flush cache
    cache.flush();

    // Memory now contains both committed words
    assert(memory.read_word(4) == 0x12345678);
    assert(memory.read_word(8) == 0x87654321);

    std::cout << "  [PASS] Flush verified." << std::endl;
}

int main()
{
    std::cout << "========================================================" << std::endl;
    std::cout << "Starting DataCache Unit Tests" << std::endl;
    std::cout << "========================================================" << std::endl;

    test_datacache_read_refill_and_hits();
    test_datacache_writeback_and_eviction();
    test_datacache_flush();

    std::cout << "========================================================" << std::endl;
    std::cout << "ALL DataCache Unit Tests PASSED!" << std::endl;
    std::cout << "========================================================" << std::endl;

    return 0;
}
