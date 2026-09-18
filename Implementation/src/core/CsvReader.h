#pragma once

#include "PortfolioData.h"

#include <istream>
#include <string>

namespace portfolio
{

class CsvReader
{
public:
    static bool loadFile(const std::string& fileName, ReturnData& output, std::string& error);
    static bool load(std::istream& input, ReturnData& output, std::string& error);
};

} // namespace portfolio

