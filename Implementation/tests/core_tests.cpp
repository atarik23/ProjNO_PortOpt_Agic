#include "core/CsvReader.h"
#include "core/Statistics.h"
#include "numerics/ActiveSetQPSolver.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>

namespace
{

bool closeTo(double lhs, double rhs, double tolerance = 1e-12)
{
    return std::abs(lhs - rhs) <= tolerance;
}

int fail(const std::string& message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

double weightedReturn(const std::vector<double>& means,
                      const std::vector<double>& weights)
{
    return std::inner_product(means.begin(), means.end(), weights.begin(), 0.0);
}

bool isLongOnlyFeasible(const portfolio::PortfolioSolution& solution,
                        const std::vector<double>& means,
                        double target,
                        double tolerance = 1e-8)
{
    if (!solution.converged || solution.weights.size() != means.size())
        return false;
    const double weightSum = std::accumulate(
        solution.weights.begin(), solution.weights.end(), 0.0);
    if (std::abs(weightSum - 1.0) > tolerance ||
        std::abs(weightedReturn(means, solution.weights) - target) > tolerance)
    {
        return false;
    }
    return std::all_of(solution.weights.begin(), solution.weights.end(),
                       [tolerance](double weight)
                       {
                           return weight >= -tolerance;
                       });
}

bool sameWeights(const portfolio::PortfolioSolution& lhs,
    const portfolio::PortfolioSolution& rhs,
    double tolerance = 1e-6)
{
    if (lhs.weights.size() != rhs.weights.size())
        return false;

    for (std::size_t asset = 0;
        asset < lhs.weights.size();
        ++asset)
    {
        if (std::abs(lhs.weights[asset] -
            rhs.weights[asset]) > tolerance)
        {
            return false;
        }
    }

    return true;
}

bool satisfiesTargetKkt(const portfolio::Statistics& statistics,
                        const portfolio::PortfolioSolution& solution,
                        double tolerance = 1e-7)
{
    std::vector<std::size_t> freeAssets;
    for (std::size_t asset = 0; asset < solution.weights.size(); ++asset)
    {
        if (solution.weights[asset] > tolerance)
            freeAssets.push_back(asset);
    }
    if (freeAssets.size() < 2)
        return false;

    std::size_t first = freeAssets.front();
    std::size_t second = freeAssets.back();
    for (std::size_t asset : freeAssets)
    {
        if (std::abs(statistics.meanReturns[first] - statistics.meanReturns[asset]) > tolerance)
        {
            second = asset;
            break;
        }
    }
    const double meanDifference =
        statistics.meanReturns[first] - statistics.meanReturns[second];
    if (std::abs(meanDifference) <= tolerance)
        return false;

    std::vector<double> gradient(solution.weights.size(), 0.0);
    for (std::size_t row = 0; row < solution.weights.size(); ++row)
    {
        gradient[row] = std::inner_product(
            statistics.covariance[row].begin(),
            statistics.covariance[row].end(),
            solution.weights.begin(), 0.0);
    }

    const double returnMultiplier =
        -(gradient[first] - gradient[second]) / meanDifference;
    const double budgetMultiplier =
        -gradient[first] - returnMultiplier * statistics.meanReturns[first];

    for (std::size_t asset = 0; asset < solution.weights.size(); ++asset)
    {
        const double reducedGradient = gradient[asset] + budgetMultiplier +
            returnMultiplier * statistics.meanReturns[asset];
        if (solution.weights[asset] > tolerance)
        {
            if (std::abs(reducedGradient) > tolerance)
                return false;
        }
        else if (reducedGradient < -tolerance)
        {
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    std::istringstream csv(
        "Period,Asset A,Asset B,Asset C\n"
        "2026-01,0.01,0.00,-0.01\n"
        "2026-02,0.02,0.01,0.00\n"
        "2026-03,0.00,0.02,0.01\n"
        "2026-04,-0.01,0.01,0.02\n");

    portfolio::ReturnData data;
    std::string error;
    if (!portfolio::CsvReader::load(csv, data, error))
        return fail(error);
    if (data.assetCount() != 3 || data.observationCount() != 4)
        return fail("Unexpected parsed matrix dimensions.");

    portfolio::Statistics statistics;
    if (!portfolio::StatisticsCalculator::calculate(data, statistics, error))
        return fail(error);
    if (!closeTo(statistics.meanReturns[0], 0.005) ||
        !closeTo(statistics.meanReturns[1], 0.010) ||
        !closeTo(statistics.meanReturns[2], 0.005))
    {
        return fail("Incorrect sample means.");
    }
    if (!closeTo(statistics.covariance[0][1], statistics.covariance[1][0]))
        return fail("Covariance matrix is not symmetric.");
    if (statistics.standardDeviations[0] <= 0.0)
        return fail("Expected a positive standard deviation.");

    portfolio::ActiveSetQPSolver solver;

    portfolio::ActiveSetQPSolver::Options sparseOptions;
    sparseOptions.linearSystemBackend =
        portfolio::LinearSystemBackend::Sparse;

    portfolio::ActiveSetQPSolver sparseSolver(sparseOptions);

    portfolio::Statistics twoAssetStatistics;
    twoAssetStatistics.meanReturns = {0.10, 0.04};
    twoAssetStatistics.covariance = {{0.04, 0.0}, {0.0, 0.01}};
    const portfolio::PortfolioSolution twoAssetSolution =
        solver.solveTargetReturn(twoAssetStatistics, 0.07);
    if (!isLongOnlyFeasible(twoAssetSolution, twoAssetStatistics.meanReturns, 0.07) ||
        !closeTo(twoAssetSolution.weights[0], 0.5, 1e-8) ||
        !closeTo(twoAssetSolution.weights[1], 0.5, 1e-8))
    {
        return fail("Two-asset target-return solution is incorrect.");
    }

    portfolio::Statistics diagonalStatistics;
    diagonalStatistics.meanReturns = {0.03, 0.07, 0.12};
    diagonalStatistics.covariance = {
        {1.0, 0.0, 0.0},
        {0.0, 4.0, 0.0},
        {0.0, 0.0, 9.0}};
    const portfolio::PortfolioSolution minimumVariance =
        solver.solveMinimumVariance(diagonalStatistics);
    if (!minimumVariance.converged ||
        !closeTo(minimumVariance.weights[0], 36.0 / 49.0, 1e-7) ||
        !closeTo(minimumVariance.weights[1], 9.0 / 49.0, 1e-7) ||
        !closeTo(minimumVariance.weights[2], 4.0 / 49.0, 1e-7))
    {
        return fail("Global minimum-variance solution is incorrect.");
    }

    portfolio::Statistics activeBoundStatistics;
    activeBoundStatistics.meanReturns = {0.04, 0.08, 0.12};
    activeBoundStatistics.covariance = {
        {0.01, 0.0, 0.02},
        {0.0, 0.02, 0.04},
        {0.02, 0.04, 0.20}};
    const portfolio::PortfolioSolution activeBoundSolution =
        solver.solveTargetReturn(activeBoundStatistics, 0.06);
    if (!isLongOnlyFeasible(activeBoundSolution,
                            activeBoundStatistics.meanReturns, 0.06) ||
        activeBoundSolution.weights[2] > 1e-8 ||
        !satisfiesTargetKkt(activeBoundStatistics, activeBoundSolution))
    {
        return fail("Active-bound solution does not satisfy the KKT conditions.");
    }

    const portfolio::PortfolioSolution infeasible =
        solver.solveTargetReturn(activeBoundStatistics, 0.20);
    if (infeasible.converged)
        return fail("An infeasible target return was accepted.");

    portfolio::Statistics largeStatistics;

    constexpr std::size_t largeAssetCount = 20;
    largeStatistics.meanReturns.resize(largeAssetCount);
    largeStatistics.covariance.assign(
        largeAssetCount,
        std::vector<double>(largeAssetCount, 0.0));

    for (std::size_t asset = 0;
        asset < largeAssetCount;
        ++asset)
    {
        largeStatistics.meanReturns[asset] =
            0.002 + 0.0005 * static_cast<double>(asset);

        largeStatistics.covariance[asset][asset] =
            0.002 + 0.0001 * static_cast<double>(asset);
    }

    constexpr double largeTargetReturn = 0.0065;

    const portfolio::PortfolioSolution denseLargeSolution =
        solver.solveTargetReturn(
            largeStatistics,
            largeTargetReturn);

    const portfolio::PortfolioSolution sparseLargeSolution =
        sparseSolver.solveTargetReturn(
            largeStatistics,
            largeTargetReturn);

    const bool denseFeasible = isLongOnlyFeasible(
        denseLargeSolution,
        largeStatistics.meanReturns,
        largeTargetReturn);

    const bool sparseFeasible = isLongOnlyFeasible(
        sparseLargeSolution,
        largeStatistics.meanReturns,
        largeTargetReturn);

    if (!denseFeasible)
    {
        std::ostringstream details;
        details << "Dense 20-asset optimization failed. "
            << "Converged = " << denseLargeSolution.converged
            << ", iterations = " << denseLargeSolution.iterations
            << ", message = " << denseLargeSolution.message;

        return fail(details.str());
    }

    if (!sparseFeasible)
    {
        std::ostringstream details;
        details << "Sparse 20-asset optimization failed. "
            << "Converged = " << sparseLargeSolution.converged
            << ", iterations = " << sparseLargeSolution.iterations
            << ", message = " << sparseLargeSolution.message;

        return fail(details.str());
    }

    if (!sameWeights(denseLargeSolution, sparseLargeSolution))
    {
        double maximumDifference = 0.0;
        std::size_t maximumDifferenceAsset = 0;

        for (std::size_t asset = 0;
            asset < denseLargeSolution.weights.size();
            ++asset)
        {
            const double difference = std::abs(
                denseLargeSolution.weights[asset] -
                sparseLargeSolution.weights[asset]);

            if (difference > maximumDifference)
            {
                maximumDifference = difference;
                maximumDifferenceAsset = asset;
            }
        }

        std::ostringstream details;
        details << std::scientific << std::setprecision(12)
            << "Dense and sparse solutions are both feasible but differ. "
            << "Maximum weight difference = " << maximumDifference
            << " at asset " << maximumDifferenceAsset
            << ", dense weight = "
            << denseLargeSolution.weights[maximumDifferenceAsset]
            << ", sparse weight = "
            << sparseLargeSolution.weights[maximumDifferenceAsset]
            << ", dense risk = " << denseLargeSolution.risk
            << ", sparse risk = " << sparseLargeSolution.risk
            << ", dense iterations = " << denseLargeSolution.iterations
            << ", sparse iterations = " << sparseLargeSolution.iterations;

        return fail(details.str());
    }

    std::istringstream invalidCsv(
        "Period,A,B\n"
        "2026-01,0.1,not-a-number\n"
        "2026-02,0.2,0.3\n");
    portfolio::ReturnData invalidData;
    if (portfolio::CsvReader::load(invalidCsv, invalidData, error))
        return fail("Malformed numeric data was accepted.");

    {
        constexpr std::size_t correlatedAssetCount = 20;

        portfolio::Statistics correlatedStatistics;
        correlatedStatistics.meanReturns.resize(correlatedAssetCount);
        correlatedStatistics.covariance.assign(
            correlatedAssetCount,
            std::vector<double>(correlatedAssetCount, 0.0));

        std::vector<double> marketLoadings(correlatedAssetCount);
        std::vector<double> styleLoadings(correlatedAssetCount);

        for (std::size_t asset = 0;
            asset < correlatedAssetCount;
            ++asset)
        {
            correlatedStatistics.meanReturns[asset] =
                0.003 + 0.0006 * static_cast<double>(asset);

            marketLoadings[asset] =
                0.012 + 0.0003 * static_cast<double>(asset);

            styleLoadings[asset] =
                0.003 *
                static_cast<double>(
                    static_cast<int>(asset % 5) - 2);
        }

        // Construct Sigma = B * B^T + D.
        //
        // B * B^T introduces correlations between assets, while the
        // strictly positive diagonal D guarantees positive definiteness.
        for (std::size_t row = 0;
            row < correlatedAssetCount;
            ++row)
        {
            for (std::size_t column = 0;
                column < correlatedAssetCount;
                ++column)
            {
                correlatedStatistics.covariance[row][column] =
                    marketLoadings[row] * marketLoadings[column] +
                    styleLoadings[row] * styleLoadings[column];

                if (row == column)
                {
                    correlatedStatistics.covariance[row][column] +=
                        0.0003 +
                        0.00001 * static_cast<double>(row);
                }
            }
        }

        constexpr double correlatedTargetReturn = 0.0093;

        const portfolio::PortfolioSolution denseCorrelatedSolution =
            solver.solveTargetReturn(
                correlatedStatistics,
                correlatedTargetReturn);

        if (!denseCorrelatedSolution.converged)
        {
            return fail(
                std::string(
                    "Dense correlated 20-asset optimization failed: ") +
                denseCorrelatedSolution.message);
        }

        const portfolio::PortfolioSolution sparseCorrelatedSolution =
            sparseSolver.solveTargetReturn(
                correlatedStatistics,
                correlatedTargetReturn);

        if (!sparseCorrelatedSolution.converged)
        {
            return fail(
                std::string(
                    "Sparse correlated 20-asset optimization failed: ") +
                sparseCorrelatedSolution.message);
        }

        if (!isLongOnlyFeasible(
            denseCorrelatedSolution,
            correlatedStatistics.meanReturns,
            correlatedTargetReturn,
            1e-8))
        {
            return fail(
                "Dense correlated 20-asset solution is infeasible.");
        }

        if (!isLongOnlyFeasible(
            sparseCorrelatedSolution,
            correlatedStatistics.meanReturns,
            correlatedTargetReturn,
            1e-8))
        {
            return fail(
                "Sparse correlated 20-asset solution is infeasible.");
        }

        if (!sameWeights(
            denseCorrelatedSolution,
            sparseCorrelatedSolution,
            1e-6))
        {
            return fail(
                "Dense and sparse backends produced different weights "
                "for the correlated 20-asset problem.");
        }

        if (std::abs(
            denseCorrelatedSolution.variance -
            sparseCorrelatedSolution.variance) > 1e-10)
        {
            return fail(
                "Dense and sparse backends produced different variances "
                "for the correlated 20-asset problem.");
        }

        // For this deliberately constructed problem, the optimum is
        // strictly interior, so all initially active bounds should be released.
        if (!denseCorrelatedSolution.activeSet.empty() ||
            !sparseCorrelatedSolution.activeSet.empty())
        {
            return fail(
                "The correlated 20-asset test unexpectedly retained "
                "zero-weight constraints.");
        }
    }

    if (argc > 1)
    {
        portfolio::ReturnData sampleData;
        if (!portfolio::CsvReader::loadFile(argv[1], sampleData, error))
            return fail(error);
        if (sampleData.assetCount() != 5 || sampleData.observationCount() != 24)
            return fail("Unexpected dimensions in the bundled synthetic sample.");

        portfolio::Statistics sampleStatistics;
        if (!portfolio::StatisticsCalculator::calculate(sampleData, sampleStatistics, error))
            return fail(error);

        const portfolio::EfficientFrontier frontier =
            solver.buildEfficientFrontier(sampleStatistics, 25);
        if (!frontier.complete || frontier.points.size() != 25)
            return fail("Synthetic-sample efficient frontier was not completed.");
        for (std::size_t point = 0; point < frontier.points.size(); ++point)
        {
            const auto& solution = frontier.points[point];
            if (!isLongOnlyFeasible(solution, sampleStatistics.meanReturns,
                                    solution.targetReturn, 1e-7))
            {
                return fail("An efficient-frontier point is infeasible.");
            }
            if (point > 0 &&
                solution.targetReturn + 1e-12 < frontier.points[point - 1].targetReturn)
            {
                return fail("Efficient-frontier targets are not monotonic.");
            }
        }
    }

    std::cout << "All portfolio core tests passed.\n";
    return 0;
}
