#include <iostream>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <functional>
#include <string>
#include <iomanip>  


// VIRTUAL GARBAGE COLLECTOR (VGC) STATE DEFINITIONS

/**
 * VGC State Machine - 3-bit encoding for object lifecycle management
 *
 * Each state represents a distinct phase in the object's lifecycle,
 * guiding the collector's decisions about memory management.
 */
enum class VGC_State : uint8_t {
    IDLE      = 0b000,  // Object is sleeping, waiting for activation
    ACTIVE    = 0b001,  // Object is in active use, keep alive
    PROMOTE   = 0b010,  // Candidate for moving to a higher-priority zone
    DEMOTE    = 0b011,  // Candidate for moving to a lower-priority zone
    PERSIST   = 0b100,  // Long-lived object, maintain regardless of zone
    DEFERRED  = 0b101,  // Collection deferred to next cycle
    MARKED    = 0b110,  // Flagged for potential deletion
    EXPIRED   = 0b111   // Ready for immediate reclamation
};

/**
 * Memory Zone Classification
 *
 * Objects are assigned to zones based on their expected lifetime
 * and access patterns, enabling differentiated collection strategies.
 */
enum class MemoryZone : uint8_t {
    RED   = 0b001,  // Short-lived, high-turnover objects
    GREEN = 0b010,  // Medium-lived, regular objects
    BLUE  = 0b100   // Long-lived, persistent objects
};

/**
 * Represents an object tracked by the VGC system
 *
 * Each object carries metadata that informs the collector's decisions,
 * balancing immediate needs with long-term memory health.
 */
struct VGC_Object {
    uint32_t id;           // Unique identifier for the object
    uint8_t  zone_mask;    // Memory zone assignment (R/G/B mask)
    uint8_t  state;        // Current lifecycle state (3-bit encoding)

    // Helper function for human-readable state display
    std::string state_name() const {
        switch (static_cast<VGC_State>(state)) {
            case VGC_State::IDLE:     return "IDLE";
            case VGC_State::ACTIVE:   return "ACTIVE";
            case VGC_State::PROMOTE:  return "PROMOTE";
            case VGC_State::DEMOTE:   return "DEMOTE";
            case VGC_State::PERSIST:  return "PERSIST";
            case VGC_State::DEFERRED: return "DEFERRED";
            case VGC_State::MARKED:   return "MARKED";
            case VGC_State::EXPIRED:  return "EXPIRED";
            default:                  return "UNKNOWN";
        }
    }

    // Helper function for human-readable zone display
    std::string zone_name() const {
        switch (zone_mask) {
            case static_cast<uint8_t>(MemoryZone::RED):   return "RED";
            case static_cast<uint8_t>(MemoryZone::GREEN): return "GREEN";
            case static_cast<uint8_t>(MemoryZone::BLUE):  return "BLUE";
            default: return "MIXED_ZONE";
        }
    }
};

/**
 * Virtual Garbage Collector Manager
 * Implements a zone-aware collection strategy using a logic gate approach
 * to determine object liveness based on state, zone, and pending operations.
 */
class VGC_Manager {
private:
    // Storage for managed objects, keyed by object ID
    std::unordered_map<uint32_t, VGC_Object> heap;

    // Collection of callbacks to execute before sweeping
    std::vector<std::function<void()>> pre_sweep_callbacks;

    // Collection of callbacks to execute after sweeping
    std::vector<std::function<void(size_t)>> post_sweep_callbacks;

public:
    /**
     * Allocates a new object into the managed heap
     * @param id     Unique identifier for the object
     * @param zone   Memory zone assignment (Red/Green/Blue)
     * @param initial_state Starting lifecycle state (defaults to ACTIVE)
     */
    void allocate(uint32_t id, MemoryZone zone, VGC_State initial_state = VGC_State::ACTIVE) {
        if (heap.count(id)) {
            std::cerr << "Warning: Object " << id << " already exists. Overwriting.\n";
        }

        VGC_Object new_object = {
            id,
            static_cast<uint8_t>(zone),
            static_cast<uint8_t>(initial_state)
        };

        heap[id] = new_object;
        std::cout << "Allocated object " << id
                  << " in " << new_object.zone_name()
                  << " zone with state: " << new_object.state_name() << "\n";
    }

    /**
     * Core liveness evaluation logic
     * Implements the VGC decision equation: O = (S & Z) | (S & P)
     * Where: S = current state, Z = zone mask, P = pending operations mask
     * This forms the heart of the collector's decision-making process,
     * determining whether an object should survive the current collection cycle.
     */
    bool evaluate_liveness(const VGC_Object& obj, uint8_t pending_mask = 0) const {
        uint8_t current_state = obj.state;
        uint8_t zone_mask = obj.zone_mask;
        uint8_t pending_ops = pending_mask;

        // Special case: EXPIRED objects must always be collected
        if (static_cast<VGC_State>(current_state) == VGC_State::EXPIRED) {
            return false;
        }

        // Special case: PERSIST objects survive regardless of other factors
        if (static_cast<VGC_State>(current_state) == VGC_State::PERSIST) {
            return true;
        }

        // Apply the core VGC liveness equation
        uint8_t liveness_score = (current_state & zone_mask) | (current_state & pending_ops);

        // Objects with any liveness indication survive
        return liveness_score > 0 ||
               static_cast<VGC_State>(current_state) == VGC_State::ACTIVE;
    }

    /**
     * Register a callback to execute before each sweep operation
     */
    void register_pre_sweep_callback(std::function<void()> callback) {
        pre_sweep_callbacks.push_back(callback);
    }

    /**
     * Register a callback to execute after each sweep operation
     * Callback receives the number of objects reclaimed
     */
    void register_post_sweep_callback(std::function<void(size_t)> callback) {
        post_sweep_callbacks.push_back(callback);
    }

    /**
     * Perform a garbage collection sweep
     * Identifies unreachable objects using the liveness evaluation logic
     * and reclaims their memory, maintaining heap health.
     **/
    void sweep(uint8_t pending_mask = 0) {
        std::cout << "\n--- Initiating VGC Collection Cycle ---\n";

        // Execute pre-sweep hooks (e.g., pause application threads)
        for (const auto& callback : pre_sweep_callbacks) {
            callback();
        }

        // Identify objects that are no longer alive
        std::vector<uint32_t> objects_to_reclaim;
        for (const auto& [id, obj] : heap) {
            if (!evaluate_liveness(obj, pending_mask)) {
                objects_to_reclaim.push_back(id);
            }
        }

        // Report and reclaim identified objects
        if (!objects_to_reclaim.empty()) {
            std::cout << "Reclaiming " << objects_to_reclaim.size()
                      << " object(s):\n";

            for (uint32_t id : objects_to_reclaim) {
                const auto& obj = heap[id];
                std::cout << "  • Object " << id
                          << " (Zone: " << obj.zone_name()
                          << ", State: " << obj.state_name() << ")\n";
                heap.erase(id);
            }
        } else {
            std::cout << "No objects eligible for reclamation.\n";
        }

        // Execute post-sweep hooks (e.g., resume application threads, update stats)
        for (const auto& callback : post_sweep_callbacks) {
            callback(objects_to_reclaim.size());
        }

        std::cout << "--- Collection Cycle Complete ---\n";
    }

    /**
     * Transition an object to a new lifecycle state
     * @param id        Object identifier
     * @param new_state Target state for the transition
     **/
    void transition_state(uint32_t id, VGC_State new_state) {
        auto it = heap.find(id);
        if (it != heap.end()) {
            VGC_State old_state = static_cast<VGC_State>(it->second.state);
            it->second.state = static_cast<uint8_t>(new_state);

            std::cout << "Object " << id
                      << " transitioned: "
                      << it->second.state_name()
                      << " (was: ";

            // Create temporary object to get old state name
            VGC_Object temp = it->second;
            temp.state = static_cast<uint8_t>(old_state);
            std::cout << temp.state_name() << ")\n";
        } else {
            std::cerr << "Error: Cannot transition state. Object "
                      << id << " not found.\n";
        }
    }

    /**
     * Display current heap status in a human-readable format
     */
    void display_status() const {
        std::cout << "\n=== VGC Heap Status ===\n";
        std::cout << "Total Managed Objects: " << heap.size() << "\n";

        if (heap.empty()) {
            std::cout << "Heap is empty.\n";
            return;
        }

        // Display summary by zone
        std::cout << "\nZone Distribution:\n";
        size_t red_count = 0, green_count = 0, blue_count = 0;

        for (const auto& [id, obj] : heap) {
            if (obj.zone_mask == static_cast<uint8_t>(MemoryZone::RED)) red_count++;
            else if (obj.zone_mask == static_cast<uint8_t>(MemoryZone::GREEN)) green_count++;
            else if (obj.zone_mask == static_cast<uint8_t>(MemoryZone::BLUE)) blue_count++;
        }

        std::cout << "  RED Zone (Short-lived):   " << red_count << " objects\n";
        std::cout << "  GREEN Zone (Medium-lived): " << green_count << " objects\n";
        std::cout << "  BLUE Zone (Long-lived):    " << blue_count << " objects\n";

        // Display detailed object listing
        std::cout << "\nDetailed Object List:\n";
        std::cout << "ID     | Zone   | State      | Alive?\n";
        std::cout << "-------|--------|------------|--------\n";

        for (const auto& [id, obj] : heap) {
            bool is_alive = evaluate_liveness(obj);
            std::cout << std::setw(6) << id << " | "
                      << std::setw(6) << obj.zone_name() << " | "
                      << std::setw(10) << obj.state_name() << " | "
                      << (is_alive ? "YES" : "NO") << "\n";
        }
        std::cout << "======================================\n";
    }

    /**
     * Get the current number of managed objects
     */
    size_t object_count() const {
        return heap.size();
    }
};


// DEMONSTRATION OF THE VGC SYSTEM


int main() {
    VGC_Manager collector;

    // Register some callbacks to demonstrate extensibility
    collector.register_pre_sweep_callback([]() {
        std::cout << "[Pre-sweep] Pausing application threads...\n";
    });

    collector.register_post_sweep_callback([](size_t reclaimed_count) {
        std::cout << "[Post-sweep] Resuming application threads. "
                  << reclaimed_count << " objects reclaimed.\n";
    });

    std::cout << "=== Virtual Garbage Collector Demonstration ===\n\n";

    // Phase 1: Object Allocation with Zone Assignment
    std::cout << "Phase 1: Allocating Objects into Zones\n";
    collector.allocate(101, MemoryZone::GREEN, VGC_State::ACTIVE);
    collector.allocate(102, MemoryZone::RED, VGC_State::MARKED);
    collector.allocate(103, MemoryZone::BLUE, VGC_State::PERSIST);
    collector.allocate(104, MemoryZone::GREEN, VGC_State::IDLE);
    collector.allocate(105, MemoryZone::RED, VGC_State::ACTIVE);

    collector.display_status();

    // Phase 2: Simulating Application Behavior with State Transitions
    std::cout << "\n\nPhase 2: Simulating Application Runtime Behavior\n";
    collector.transition_state(102, VGC_State::EXPIRED);  // Marked -> Expired
    collector.transition_state(104, VGC_State::ACTIVE);   // Idle -> Active
    collector.transition_state(105, VGC_State::DEMOTE);   // Active -> Demote candidate

    collector.display_status();

    // Phase 3: Garbage Collection Sweep
    std::cout << "\n\nPhase 3: Executing Garbage Collection\n";

    // First sweep with no pending operations
    collector.sweep();
    collector.display_status();

    // Simulate pending operations (e.g., references from recently allocated objects)
    std::cout << "\n\nPhase 4: Simulating Pending Operations\n";
    collector.allocate(106, MemoryZone::GREEN, VGC_State::ACTIVE);
    collector.allocate(107, MemoryZone::RED, VGC_State::ACTIVE);

    // Create a pending mask that might affect liveness decisions
    uint8_t pending_operations_mask = 0b011;  // Example pending operations

    std::cout << "\nExecuting sweep with pending operations mask: "
              << static_cast<int>(pending_operations_mask) << "\n";
    collector.sweep(pending_operations_mask);

    collector.display_status();

    std::cout << "\n=== Demonstration Complete ===\n";
    std::cout << "Final heap contains " << collector.object_count()
              << " managed objects.\n";

    return 0;
}
