#include "Statistics.h"

#include <algorithm>
#include <cmath>

namespace portfolio
{

bool StatisticsCalculator::calculate(const ReturnData& data, Statistics& output, std::string& error)
{
    output = {};
    error.clear();

    const std::size_t observationCount = data.observationCount();
    const std::size_t assetCount = data.assetCount();
    if (observationCount < 2 || assetCount < 2)
    {
        error = "Statistics require at least two assets and two observations.";
        return false;
    }

    for (const auto& observation : data.observations)
    {
        if (observation.size() != assetCount)
        {
            error = "Return matrix rows do not have a consistent number of assets.";
            return false;
        }
    }

    output.meanReturns.assign(assetCount, 0.0);
    output.standardDeviations.assign(assetCount, 0.0);
    output.covariance.assign(assetCount, std::vector<double>(assetCount, 0.0));

    for (const auto& observation : data.observations)
    {
        for (std::size_t asset = 0; asset < assetCount; ++asset)
            output.meanReturns[asset] += observation[asset];
    }
    for (double& mean : output.meanReturns)
        mean /= static_cast<double>(observationCount);

    const double denominator = static_cast<double>(observationCount - 1);
    for (std::size_t row = 0; row < assetCount; ++row)
    {
        for (std::size_t column = row; column < assetCount; ++column)
        {
            double covariance = 0.0;
            for (const auto& observation : data.observations)
            {
                covariance += (observation[row] - output.meanReturns[row]) *
                              (observation[column] - output.meanReturns[column]);
            }
            covariance /= denominator;
            output.covariance[row][column] = covariance;
            output.covariance[column][row] = covariance;
        }
        output.standardDeviations[row] =
            std::sqrt(std::max(0.0, output.covariance[row][row]));
    }

    return true;
}

} // namespace portfolio

