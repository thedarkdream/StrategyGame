#pragma once

#include <cstdint>

// Singleton that hands out monotonically-increasing entity IDs.
// Call reset() once at the start of each new game so IDs begin from 1.
class IdGenerator {
public:
    static IdGenerator& instance() {
        static IdGenerator s_instance;
        return s_instance;
    }

    // Reset counter — call at the beginning of Game::initialize().
    void reset() { m_next = 1; }

    // Returns the next unique ID and advances the counter.
    uint32_t nextId() { return m_next++; }

private:
    IdGenerator() = default;
    IdGenerator(const IdGenerator&) = delete;
    IdGenerator& operator=(const IdGenerator&) = delete;

    uint32_t m_next = 1;
};
