#include "cache.h"
#include "champsim.h"
#include <deque>
#include <cstdint>

#define EXTRACT_PAGE_ID(addr)  ((addr) >> LOG2_PAGE_SIZE)
#define EXTRACT_BLOCK_ID(addr)  (((addr) >> LOG2_BLOCK_SIZE) & 0x3f)

#define BLOCK_ID_MIN 0
#define BLOCK_ID_MAX ((PAGE_SIZE / BLOCK_SIZE) - 1)

#define BUFFER_LENGTH 5

uint64_t prev_page_id[NUM_CPUS];

uint64_t prepare_fetch_address(uint64_t page_id, uint64_t block_id) {
    return (page_id << LOG2_PAGE_SIZE) + (block_id << LOG2_BLOCK_SIZE);
}

bool is_on_same_page(uint32_t cpu_id, uint64_t curr_page_id) {
    uint64_t tmp_prev_page_id = prev_page_id[cpu_id];
    prev_page_id[cpu_id] = curr_page_id;
    return curr_page_id == tmp_prev_page_id;
}

class HistoryBuffer {
    std::deque<uint32_t> buffer;
    size_t k = BUFFER_LENGTH;

public:
    HistoryBuffer() : k(BUFFER_LENGTH) {}

    int32_t size() const {
        return buffer.size();
    }

    void append(uint32_t blk_id) {
        if (is_full()) {
            buffer.pop_front();
        }
        buffer.push_back(blk_id);
    }

    void reset() {
        buffer.clear();
        k = BUFFER_LENGTH;
    }

    bool is_full() const {
        return buffer.size() == k;
    }

    bool has_forward_movement() const {
        if (!is_full()) {
            return false;
        }

        int forward_movements = 0;
        for (size_t i = 0; i < buffer.size() - 1; i++) {
            if (buffer[i] < buffer[i + 1]) {
                forward_movements++;
            }
        }

        return forward_movements >= (BUFFER_LENGTH - 1) / 2 + 1;
    }
};

HistoryBuffer hist_buff[NUM_CPUS];

void CACHE::prefetcher_initialize() {
    for (unsigned int i = 0; i < NUM_CPUS; i++) {
        prev_page_id[i] = -1;
        hist_buff[i] = HistoryBuffer(); 
    }
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in) {
    uint64_t page_id = EXTRACT_PAGE_ID(addr);
    uint32_t block_id = EXTRACT_BLOCK_ID(addr);
    int32_t prefetch_block_id = static_cast<int32_t>(block_id);
    uint64_t prefetch_addr;

    if (!is_on_same_page(cpu, page_id)) {
        hist_buff[cpu].reset();
        return metadata_in;
    }

    hist_buff[cpu].append(block_id);

    if (!hist_buff[cpu].has_forward_movement()) {
        prefetch_block_id++;
    } else {
        prefetch_block_id--;
    }

    if (prefetch_block_id >= static_cast<int32_t>(BLOCK_ID_MIN) && prefetch_block_id <= static_cast<int32_t>(BLOCK_ID_MAX)) {
        prefetch_addr = prepare_fetch_address(page_id, prefetch_block_id);
        prefetch_line(ip, addr, prefetch_addr, true, 0); 
    }

    return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in) {
    return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {}
