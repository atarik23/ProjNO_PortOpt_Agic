#pragma once

#include "PortfolioData.h"

#include <string>

namespace portfolio
{

class StatisticsCalculator
{
public:
    static bool calculate(const ReturnData& data, Statistics& output, std::string& error);
};

} // namespace portfolio

