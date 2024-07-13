#pragma once

#include <chrono>

class PerfTimer {
    public:
        PerfTimer() {};
        ~PerfTimer() {};

        inline void begin() { beginTime = std::chrono::high_resolution_clock::now(); }
        inline void end() { endTime = std::chrono::high_resolution_clock::now(); }
        double getElapsedTime() const;

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> beginTime{};
        std::chrono::time_point<std::chrono::high_resolution_clock> endTime{};
};
