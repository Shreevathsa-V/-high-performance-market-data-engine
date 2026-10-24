#pragma once

#include <vector>

#include "ExecutionReport.h"
#include "Trade.h"

struct OrderResult
{
    Trades trades;
    std::vector<ExecutionReport> reports;
};