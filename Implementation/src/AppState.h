#pragma once

#include "core/CsvReader.h"
#include "core/Statistics.h"
#include "numerics/ActiveSetQPSolver.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <fstream>
#include <locale>

class AppState
{
    portfolio::ReturnData _data;
    portfolio::Statistics _statistics;
    portfolio::PortfolioSolution _solution;
    portfolio::EfficientFrontier _frontier;
    portfolio::LinearSystemBackend _solutionBackend =
        portfolio::LinearSystemBackend::Dense;
    portfolio::LinearSystemBackend _frontierBackend =
        portfolio::LinearSystemBackend::Dense;
    std::string _sourcePath;
    bool _hasData = false;
    bool _hasSolution = false;
    bool _hasFrontier = false;

public:
    bool loadReturns(const std::string& fileName, std::string& error)
    {
        portfolio::ReturnData data;
        portfolio::Statistics statistics;
        if (!portfolio::CsvReader::loadFile(fileName, data, error))
            return false;
        if (!portfolio::StatisticsCalculator::calculate(data, statistics, error))
            return false;

        _data = std::move(data);
        _statistics = std::move(statistics);
        _sourcePath = fileName;
        _hasData = true;
        _solution = {};
        _frontier = {};
        _hasSolution = false;
        _hasFrontier = false;
        return true;
    }

    bool solveTargetReturn(double targetReturn, portfolio::LinearSystemBackend backend, std::string& error)
    {
        error.clear();
        if (!_hasData)
        {
            error = "Load return data before running the optimizer.";
            return false;
        }

        portfolio::ActiveSetQPSolver::Options options;
        options.linearSystemBackend = backend;
        portfolio::ActiveSetQPSolver solver(options);
        portfolio::PortfolioSolution solution =
            solver.solveTargetReturn(_statistics, targetReturn);
        if (!solution.converged)
        {
            error = solution.message;
            return false;
        }

        _solution = std::move(solution);
        _solutionBackend = backend;
        _hasSolution = true;
        return true;
    }

    bool buildEfficientFrontier(std::size_t pointCount, portfolio::LinearSystemBackend backend, std::string& error)
    {
        error.clear();
        if (!_hasData)
        {
            error = "Load return data before constructing the efficient frontier.";
            return false;
        }

        portfolio::ActiveSetQPSolver::Options options;
        options.linearSystemBackend = backend;
        portfolio::ActiveSetQPSolver solver(options);
        portfolio::EfficientFrontier frontier =
            solver.buildEfficientFrontier(_statistics, pointCount);
        if (!frontier.complete)
        {
            error = frontier.message;
            return false;
        }

        _frontier = std::move(frontier);
        _frontierBackend = backend;
        _hasFrontier = true;
        return true;
    }

    bool selectFrontierPoint(std::size_t pointIndex, std::string& error)
    {
        error.clear();

        if (!_hasFrontier || _frontier.points.empty())
        {
            error = "Build the efficient frontier before selecting a landmark.";
            return false;
        }

        if (pointIndex >= _frontier.points.size())
        {
            error = "The selected efficient-frontier point is not available.";
            return false;
        }

        _solution = _frontier.points[pointIndex];
        _solutionBackend = _frontierBackend;
        _hasSolution = true;
        return true;
    }

    bool compareBackends(
        double targetReturn,
        std::string& report,
        std::string& error) const
    {
        report.clear();
        error.clear();

        if (!_hasData)
        {
            error = "Load return data before comparing matrix backends.";
            return false;
        }

        const auto runSolver =
            [this, targetReturn](
                portfolio::LinearSystemBackend backend,
                double& elapsedMilliseconds)
            -> portfolio::PortfolioSolution
            {
                portfolio::ActiveSetQPSolver::Options options;
                options.linearSystemBackend = backend;

                portfolio::ActiveSetQPSolver solver(options);

                const auto start = std::chrono::steady_clock::now();

                portfolio::PortfolioSolution solution =
                    solver.solveTargetReturn(_statistics, targetReturn);

                const auto finish = std::chrono::steady_clock::now();

                elapsedMilliseconds =
                    std::chrono::duration<double, std::milli>(
                        finish - start).count();

                return solution;
            };

        double denseMilliseconds = 0.0;
        double sparseMilliseconds = 0.0;

        const portfolio::PortfolioSolution denseSolution =
            runSolver(
                portfolio::LinearSystemBackend::Dense,
                denseMilliseconds);

        const portfolio::PortfolioSolution sparseSolution =
            runSolver(
                portfolio::LinearSystemBackend::Sparse,
                sparseMilliseconds);

        if (!denseSolution.converged)
        {
            error = "Dense backend failed: " + denseSolution.message;
            return false;
        }

        if (!sparseSolution.converged)
        {
            error = "Sparse backend failed: " + sparseSolution.message;
            return false;
        }

        if (denseSolution.weights.size() != sparseSolution.weights.size())
        {
            error = "Dense and sparse backends returned incompatible result dimensions.";
            return false;
        }

        double maximumWeightDifference = 0.0;

        for (std::size_t asset = 0;
            asset < denseSolution.weights.size();
            ++asset)
        {
            maximumWeightDifference = std::max(
                maximumWeightDifference,
                std::abs(
                    denseSolution.weights[asset] -
                    sparseSolution.weights[asset]));
        }

        const double denseWeightSum = std::accumulate(
            denseSolution.weights.begin(),
            denseSolution.weights.end(),
            0.0);

        const double sparseWeightSum = std::accumulate(
            sparseSolution.weights.begin(),
            sparseSolution.weights.end(),
            0.0);

        const double denseBudgetResidual =
            std::abs(denseWeightSum - 1.0);

        const double sparseBudgetResidual =
            std::abs(sparseWeightSum - 1.0);

        const double denseReturnResidual =
            std::abs(denseSolution.expectedReturn - targetReturn);

        const double sparseReturnResidual =
            std::abs(sparseSolution.expectedReturn - targetReturn);

        const double varianceDifference =
            std::abs(
                denseSolution.variance -
                sparseSolution.variance);

        const double maximumFeasibilityResidual = std::max(
            {
                denseBudgetResidual,
                sparseBudgetResidual,
                denseReturnResidual,
                sparseReturnResidual
            });

        constexpr double comparisonTolerance = 1e-7;

        const bool agreement =
            maximumWeightDifference <= comparisonTolerance &&
            maximumFeasibilityResidual <= comparisonTolerance;

        std::ostringstream output;

        output << std::fixed << std::setprecision(6);
        output << "Dense vs. sparse backend comparison\n";
        output << "Target return: "
            << 100.0 * targetReturn << "%\n\n";

        output << "Dense backend:\n";
        output << "  Iterations: "
            << denseSolution.iterations << "\n";
        output << "  Active constraints: "
            << denseSolution.activeSet.size() << "\n";
        output << "  Single-run time: "
            << std::setprecision(4)
            << denseMilliseconds << " ms\n\n";

        output << std::setprecision(6);
        output << "Sparse backend:\n";
        output << "  Iterations: "
            << sparseSolution.iterations << "\n";
        output << "  Active constraints: "
            << sparseSolution.activeSet.size() << "\n";
        output << "  Single-run time: "
            << std::setprecision(4)
            << sparseMilliseconds << " ms\n\n";

        output << std::scientific << std::setprecision(3);
        output << "Maximum absolute weight difference: "
            << maximumWeightDifference << "\n";
        output << "Variance difference: "
            << varianceDifference << "\n";
        output << "Dense budget residual: "
            << denseBudgetResidual << "\n";
        output << "Dense return residual: "
            << denseReturnResidual << "\n";
        output << "Sparse budget residual: "
            << sparseBudgetResidual << "\n";
        output << "Sparse return residual: "
            << sparseReturnResidual << "\n";

        output << "\nActive sets equal: "
            << (denseSolution.activeSet ==
                sparseSolution.activeSet
                ? "yes"
                : "no");

        output << "\nComparison tolerance: "
            << comparisonTolerance;

        output << "\nResult: "
            << (agreement
                ? "PASS - both natID backends agree."
                : "REVIEW REQUIRED - tolerance exceeded.");

        report = output.str();
        return true;
    }

    bool exportFrontierCsv(
        const std::string& fileName,
        std::string& error) const
    {
        error.clear();

        if (!_hasFrontier || _frontier.points.empty())
        {
            error = "Build the efficient frontier before exporting it.";
            return false;
        }

        if (fileName.empty())
        {
            error = "No export file was selected.";
            return false;
        }

        for (const auto& point : _frontier.points)
        {
            if (point.weights.size() != _data.assetCount())
            {
                error = "Frontier weight dimensions do not match the loaded assets.";
                return false;
            }
        }

        std::ofstream output(fileName, std::ios::out | std::ios::trunc);

        if (!output.is_open())
        {
            error = "The selected export file could not be opened.";
            return false;
        }

        output.imbue(std::locale::classic());

        const auto writeCsvField =
            [](std::ostream& stream, const std::string& value)
            {
                stream << '"';

                for (char character : value)
                {
                    if (character == '"')
                        stream << "\"\"";
                    else
                        stream << character;
                }

                stream << '"';
            };

        output
            << "target_return,"
            << "expected_return,"
            << "risk,"
            << "variance,"
            << "iterations,"
            << "active_constraint_count,"
            << "active_constraints";

        for (const std::string& assetName : _data.assetNames)
        {
            output << ',';
            writeCsvField(output, "weight_" + assetName);
        }

        output << '\n';
        output << std::setprecision(17);

        for (const auto& point : _frontier.points)
        {
            output
                << point.targetReturn << ','
                << point.expectedReturn << ','
                << point.risk << ','
                << point.variance << ','
                << point.iterations << ','
                << point.activeSet.size() << ',';

            std::ostringstream activeConstraints;

            for (std::size_t index = 0;
                index < point.activeSet.size();
                ++index)
            {
                const std::size_t asset = point.activeSet[index];

                if (index > 0)
                    activeConstraints << ';';

                if (asset < _data.assetNames.size())
                    activeConstraints << _data.assetNames[asset];
                else
                    activeConstraints << "asset_" << asset;
            }

            writeCsvField(output, activeConstraints.str());

            for (double weight : point.weights)
                output << ',' << weight;

            output << '\n';
        }

        output.flush();

        if (!output.good())
        {
            error = "An error occurred while writing the frontier CSV file.";
            return false;
        }

        return true;
    }

    [[nodiscard]] bool hasData() const noexcept
    {
        return _hasData;
    }

    [[nodiscard]] const portfolio::ReturnData& data() const noexcept
    {
        return _data;
    }

    [[nodiscard]] const portfolio::Statistics& statistics() const noexcept
    {
        return _statistics;
    }

    [[nodiscard]] const std::string& sourcePath() const noexcept
    {
        return _sourcePath;
    }

    [[nodiscard]] bool hasSolution() const noexcept
    {
        return _hasSolution;
    }

    [[nodiscard]] bool hasFrontier() const noexcept
    {
        return _hasFrontier;
    }

    [[nodiscard]] const portfolio::PortfolioSolution& solution() const noexcept
    {
        return _solution;
    }

    [[nodiscard]] portfolio::LinearSystemBackend solutionBackend() const noexcept
    {
        return _solutionBackend;
    }

    [[nodiscard]] const portfolio::EfficientFrontier& frontier() const noexcept
    {
        return _frontier;
    }

    [[nodiscard]] std::string summary() const
    {
        if (!_hasData)
            return "No return data loaded.";

        std::ostringstream output;
        output << "Loaded " << _data.observationCount() << " observations for "
               << _data.assetCount() << " assets.\n\n";
        output << std::fixed << std::setprecision(4);
        output << "Asset statistics (per observation period):\n";
        for (std::size_t asset = 0; asset < _data.assetCount(); ++asset)
        {
            output << "  " << _data.assetNames[asset]
                   << ": mean = " << 100.0 * _statistics.meanReturns[asset] << "%"
                   << ", volatility = " << 100.0 * _statistics.standardDeviations[asset]
                   << "%\n";
        }

        const auto [minimumReturn, maximumReturn] = std::minmax_element(
            _statistics.meanReturns.begin(),
            _statistics.meanReturns.end());

        output << "\nFeasible long-only target-return range: "
               << 100.0 * *minimumReturn << "% to "
               << 100.0 * *maximumReturn << "%.";
        output << "\nReturns and risk are expressed per observation period.";
        output << "\nThe covariance matrix has been estimated using the unbiased sample estimator (n - 1).";
        return output.str();
    }

    [[nodiscard]] std::string solutionSummary() const
    {
        if (!_hasSolution)
            return "No optimized portfolio is available.";

        std::ostringstream output;
        output << std::fixed << std::setprecision(6);
        output << "Active-set QP converged in " << _solution.iterations << " iterations.\n";
        output << "Target return: " << 100.0 * _solution.targetReturn << "%\n";
        output << "Expected return: " << 100.0 * _solution.expectedReturn << "%\n";
        output << "Risk (standard deviation): " << 100.0 * _solution.risk << "%\n";
        output << "Variance: " << _solution.variance << "\n";

        const double weightSum = std::accumulate(
            _solution.weights.begin(),
            _solution.weights.end(),
            0.0);
        const double achievedReturn = std::inner_product(
            _solution.weights.begin(),
            _solution.weights.end(),
            _statistics.meanReturns.begin(),
            0.0);

        output << std::scientific << std::setprecision(3);
        output << "Budget residual: "
            << std::abs(weightSum - 1.0) << "\n";
        output << "Target-return residual: "
            << std::abs(achievedReturn - _solution.targetReturn) << "\n";
        output << std::fixed << std::setprecision(6);
        output << "Active constraints: "
            << _solution.activeSet.size() << "\n\n";
        output << "Optimal weights:\n";
        for (std::size_t asset = 0; asset < _solution.weights.size(); ++asset)
        {
            output << "  " << _data.assetNames[asset] << ": "
                   << 100.0 * _solution.weights[asset] << "%\n";
        }
        output << "\nActive zero-weight constraints ("
            << _solution.activeSet.size() << "):";

        if (_solution.activeSet.empty())
        {
            output << " none.";
        }
        else
        {
            for (std::size_t asset : _solution.activeSet)
            {
                if (asset < _data.assetNames.size())
                {
                    output << "\n  " << _data.assetNames[asset]
                        << ": w = 0";
                }
            }
        }
        return output.str();
    }

    [[nodiscard]] std::string frontierSummary() const
    {
        if (!_hasFrontier)
            return "No efficient frontier is available.";

        std::ostringstream output;
        output << std::fixed << std::setprecision(6);
        output << "Constructed " << _frontier.points.size()
               << " efficient-frontier portfolios.\n";
        output << "Global minimum-variance return: "
               << 100.0 * _frontier.minimumVariancePortfolio.expectedReturn << "%\n";
        output << "Global minimum-variance risk: "
               << 100.0 * _frontier.minimumVariancePortfolio.risk << "%\n";
        output << "Maximum frontier target: "
               << 100.0 * _frontier.points.back().targetReturn << "%";

        std::size_t activeSetChanges = 0;

        for (std::size_t point = 1;
            point < _frontier.points.size();
            ++point)
        {
            if (_frontier.points[point].activeSet !=
                _frontier.points[point - 1].activeSet)
            {
                ++activeSetChanges;
            }
        }

        output << "\nDetected active-set changes between sampled frontier points: "
            << activeSetChanges << ".";
        output << "\nDistinct active-set regions: "
            << activeSetChanges + 1 << ".";
        return output.str();
    }
};
