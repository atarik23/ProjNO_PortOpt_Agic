#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace portfolio
{

struct ReturnData
{
    std::vector<std::string> assetNames;
    std::vector<std::string> observationLabels;
    std::vector<std::vector<double>> observations;

    [[nodiscard]] std::size_t assetCount() const noexcept
    {
        return assetNames.size();
    }

    [[nodiscard]] std::size_t observationCount() const noexcept
    {
        return observations.size();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return assetNames.empty() || observations.empty();
    }
};

struct Statistics
{
    std::vector<double> meanReturns;
    std::vector<double> standardDeviations;
    std::vector<std::vector<double>> covariance;
};

struct PortfolioSolution
{
    bool converged = false;
    double targetReturn = 0.0;
    double expectedReturn = 0.0;
    double variance = 0.0;
    double risk = 0.0;
    std::vector<double> weights;
    std::vector<std::size_t> activeSet;
    std::size_t iterations = 0;
    std::string message;
};

struct EfficientFrontier
{
    bool complete = false;
    PortfolioSolution minimumVariancePortfolio;
    std::vector<PortfolioSolution> points;
    std::string message;
};

} // namespace portfolio
