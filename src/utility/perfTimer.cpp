#include "perfTimer.h"

double PerfTimer::getElapsedTime() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - beginTime
    ).count();
}
