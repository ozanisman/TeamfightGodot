#ifndef SIM_ALLOCATION_TRACKER_HPP
#define SIM_ALLOCATION_TRACKER_HPP

#include <cstdint>
#include <cstddef>
#include <atomic>

namespace sim::allocation {

// Allocation statistics for a specific type
struct TypeStats {
    std::atomic<uint64_t> allocation_count{0};
    std::atomic<uint64_t> total_bytes{0};
    std::atomic<uint64_t> deallocation_count{0};
    std::atomic<uint64_t> peak_bytes{0};
    std::atomic<uint64_t> current_bytes{0};
};

// Global allocation tracker (only active in debug builds)
class Tracker {
public:
    static Tracker& instance();
    
    void track_allocation(void* ptr, size_t size, const char* type_name);
    void track_deallocation(void* ptr, size_t size, const char* type_name);
    
    void reset();
    void report_to_stderr();
    
    bool is_enabled() const { return _enabled; }
    void set_enabled(bool enabled) { _enabled = enabled; }
    
private:
    Tracker() = default;
    ~Tracker() = default;
    Tracker(const Tracker&) = delete;
    Tracker& operator=(const Tracker&) = delete;
    
    bool _enabled = false;
    
    // Type-specific stats (indexed by hash of type name for simplicity)
    static constexpr size_t MAX_TRACKED_TYPES = 64;
    TypeStats _type_stats[MAX_TRACKED_TYPES];
};

// RAII helper to enable/disable tracking
class ScopedEnable {
public:
    explicit ScopedEnable(bool enable) : _prev_enabled(Tracker::instance().is_enabled()) {
        Tracker::instance().set_enabled(enable);
    }
    ~ScopedEnable() {
        Tracker::instance().set_enabled(_prev_enabled);
    }
private:
    bool _prev_enabled;
};

} // namespace sim::allocation

// Debug build macro to track allocations
#ifdef DEBUG_BUILD
#define SIM_TRACK_ALLOC(ptr, size, type) \
    do { \
        if (sim::allocation::Tracker::instance().is_enabled()) { \
            sim::allocation::Tracker::instance().track_allocation(ptr, size, type); \
        } \
    } while(0)

#define SIM_TRACK_FREE(ptr, size, type) \
    do { \
        if (sim::allocation::Tracker::instance().is_enabled()) { \
            sim::allocation::Tracker::instance().track_deallocation(ptr, size, type); \
        } \
    } while(0)
#else
// Release build: no-op macros
#define SIM_TRACK_ALLOC(ptr, size, type) ((void)0)
#define SIM_TRACK_FREE(ptr, size, type) ((void)0)
#endif

#endif // SIM_ALLOCATION_TRACKER_HPP
