#include "sim_allocation_tracker.hpp"
#include <cstdio>
#include <cstring>

namespace sim::allocation {

Tracker& Tracker::instance() {
    static Tracker instance;
    return instance;
}

void Tracker::track_allocation(void* ptr, size_t size, const char* type_name) {
    if (!_enabled || !ptr) return;
    
    // Simple hash of type name to index into stats array
    size_t hash = 0;
    const char* p = type_name;
    while (*p) {
        hash = hash * 31 + static_cast<size_t>(*p);
        p++;
    }
    size_t idx = hash % MAX_TRACKED_TYPES;
    
    TypeStats& stats = _type_stats[idx];
    
    uint64_t old_current = stats.current_bytes.fetch_add(size);
    uint64_t new_current = old_current + size;
    
    // Update peak
    uint64_t old_peak = stats.peak_bytes.load();
    while (new_current > old_peak) {
        if (stats.peak_bytes.compare_exchange_weak(old_peak, new_current)) {
            break;
        }
    }
    
    stats.allocation_count.fetch_add(1);
    stats.total_bytes.fetch_add(size);
}

void Tracker::track_deallocation(void* ptr, size_t size, const char* type_name) {
    if (!_enabled || !ptr) return;
    
    // Simple hash of type name to index into stats array
    size_t hash = 0;
    const char* p = type_name;
    while (*p) {
        hash = hash * 31 + static_cast<size_t>(*p);
        p++;
    }
    size_t idx = hash % MAX_TRACKED_TYPES;
    
    TypeStats& stats = _type_stats[idx];
    
    stats.deallocation_count.fetch_add(1);
    stats.current_bytes.fetch_sub(size);
}

void Tracker::reset() {
    for (size_t i = 0; i < MAX_TRACKED_TYPES; ++i) {
        _type_stats[i].allocation_count.store(0);
        _type_stats[i].total_bytes.store(0);
        _type_stats[i].deallocation_count.store(0);
        _type_stats[i].peak_bytes.store(0);
        _type_stats[i].current_bytes.store(0);
    }
}

void Tracker::report_to_stderr() {
    std::fprintf(stderr, "=== Allocation Tracker Report ===\n");
    
    uint64_t total_allocs = 0;
    uint64_t total_bytes = 0;
    uint64_t total_peak = 0;
    
    for (size_t i = 0; i < MAX_TRACKED_TYPES; ++i) {
        uint64_t allocs = _type_stats[i].allocation_count.load();
        uint64_t bytes = _type_stats[i].total_bytes.load();
        uint64_t peak = _type_stats[i].peak_bytes.load();
        uint64_t current = _type_stats[i].current_bytes.load();
        
        if (allocs > 0) {
            std::fprintf(stderr, "Type %zu: %llu allocations, %llu total bytes, %llu peak bytes, %llu current bytes\n",
                         i, (unsigned long long)allocs, (unsigned long long)bytes,
                         (unsigned long long)peak, (unsigned long long)current);
            total_allocs += allocs;
            total_bytes += bytes;
            total_peak = (total_peak > peak) ? total_peak : peak;
        }
    }
    
    std::fprintf(stderr, "Total: %llu allocations, %llu total bytes, %llu peak bytes\n",
                 (unsigned long long)total_allocs, (unsigned long long)total_bytes,
                 (unsigned long long)total_peak);
    std::fprintf(stderr, "=== End Report ===\n");
}

} // namespace sim::allocation
