#include "CsvReader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace portfolio
{
namespace
{

std::string trim(std::string value)
{
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::vector<std::string> parseLine(const std::string& line, bool& ok)
{
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;

    for (std::size_t i = 0; i < line.size(); ++i)
    {
        const char ch = line[i];
        if (ch == '"')
        {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"')
            {
                field.push_back('"');
                ++i;
            }
            else
            {
                quoted = !quoted;
            }
        }
        else if (ch == ',' && !quoted)
        {
            fields.push_back(trim(field));
            field.clear();
        }
        else
        {
            field.push_back(ch);
        }
    }

    ok = !quoted;
    fields.push_back(trim(field));
    return fields;
}

bool isObservationHeader(const std::string& header)
{
    const std::string normalized = lower(trim(header));
    return normalized == "date" || normalized == "period" || normalized == "time" ||
           normalized == "timestamp" || normalized == "observation";
}

bool parseNumber(const std::string& text, double& value)
{
    try
    {
        std::size_t parsed = 0;
        value = std::stod(text, &parsed);
        return parsed == text.size() && std::isfinite(value);
    }
    catch (const std::exception&)
    {
        return false;
    }
}

} // namespace

bool CsvReader::loadFile(const std::string& fileName, ReturnData& output, std::string& error)
{
    std::ifstream input{std::filesystem::path(fileName)};
    if (!input)
    {
        error = "Cannot open CSV file: " + fileName;
        return false;
    }
    return load(input, output, error);
}

bool CsvReader::load(std::istream& input, ReturnData& output, std::string& error)
{
    output = {};
    error.clear();

    std::string line;
    if (!std::getline(input, line))
    {
        error = "The CSV file is empty.";
        return false;
    }

    if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF)
    {
        line.erase(0, 3);
    }

    bool validQuotes = true;
    auto header = parseLine(line, validQuotes);
    if (!validQuotes || header.empty())
    {
        error = "Invalid CSV header.";
        return false;
    }

    const bool hasObservationLabels = isObservationHeader(header.front());
    const std::size_t firstAssetColumn = hasObservationLabels ? 1U : 0U;
    if (header.size() - firstAssetColumn < 2)
    {
        error = "At least two asset columns are required.";
        return false;
    }

    std::set<std::string> uniqueNames;
    for (std::size_t column = firstAssetColumn; column < header.size(); ++column)
    {
        const std::string name = trim(header[column]);
        if (name.empty())
        {
            error = "Asset names in the CSV header cannot be empty.";
            return false;
        }
        if (!uniqueNames.insert(name).second)
        {
            error = "Duplicate asset name in CSV header: " + name;
            return false;
        }
        output.assetNames.push_back(name);
    }

    std::size_t lineNumber = 1;
    while (std::getline(input, line))
    {
        ++lineNumber;
        if (trim(line).empty())
            continue;

        auto fields = parseLine(line, validQuotes);
        if (!validQuotes)
        {
            error = "Unclosed quoted field at CSV line " + std::to_string(lineNumber) + ".";
            return false;
        }
        if (fields.size() != header.size())
        {
            error = "CSV line " + std::to_string(lineNumber) + " has " +
                    std::to_string(fields.size()) + " columns; expected " +
                    std::to_string(header.size()) + ".";
            return false;
        }

        if (hasObservationLabels)
            output.observationLabels.push_back(fields.front());

        std::vector<double> observation;
        observation.reserve(output.assetNames.size());
        for (std::size_t column = firstAssetColumn; column < fields.size(); ++column)
        {
            double value = 0.0;
            if (fields[column].empty() || !parseNumber(fields[column], value))
            {
                error = "Invalid numeric return at CSV line " + std::to_string(lineNumber) +
                        ", column " + std::to_string(column + 1) + ".";
                return false;
            }
            observation.push_back(value);
        }
        output.observations.push_back(std::move(observation));
    }

    if (output.observationCount() < 2)
    {
        error = "At least two return observations are required.";
        output = {};
        return false;
    }

    return true;
}

} // namespace portfolio
