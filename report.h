#pragma once

#include "scheduler.h"

#include <string>

class Report {
public:
    static std::string generate(Scheduler& scheduler);

    static bool save(
        const std::string& report,
        const std::string& filename = "csopesy-log.txt"
    );
};
