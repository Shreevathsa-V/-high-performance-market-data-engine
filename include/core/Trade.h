#pragma once

#include <vector>

#include "TradeInfo.h"

struct Trade
{
    TradeInfo bid;
    TradeInfo ask;
};

using Trades = std::vector<Trade>;