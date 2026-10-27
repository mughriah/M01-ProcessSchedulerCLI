#pragma once

#include "Scheduler.h"

#include <string>

class Report {
public:
    static std::string generate(Scheduler& scheduler);

    static bool save(
        Scheduler& scheduler,
        const std::string& filename = "csopesy-log.txt"
    );
};
