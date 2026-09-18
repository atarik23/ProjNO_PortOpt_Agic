#include "ActiveSetQPSolver.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <string>
#include <utility>

namespace portfolio
{
namespace
{

using Matrix = std::vector<std::vector<double>>;

bool isFinite(double value)
{
    return std::isfinite(value);
}

bool validateStatistics(const Statistics& statistics, std::string& error)
{
    const std::size_t assetCount = statistics.meanReturns.size();
    if (assetCount < 2)
    {
        error = "Optimization requires at least two assets.";
        return false;
    }
    if (statistics.covariance.size() != assetCount)
    {
        error = "Covariance matrix dimensions do not match the mean-return vector.";
        return false;
    }

    for (std::size_t row = 0; row < assetCount; ++row)
    {
        if (!isFinite(statistics.meanReturns[row]) ||
            statistics.covariance[row].size() != assetCount)
        {
            error = "Optimization inputs contain invalid dimensions or non-finite values.";
            return false;
        }
        for (double value : statistics.covariance[row])
        {
            if (!isFinite(value))
            {
                error = "Covariance matrix contains a non-finite value.";
                return false;
            }
        }
    }
    return true;
}

double dot(const std::vector<double>& lhs, const std::vector<double>& rhs)
{
    return std::inner_product(lhs.begin(), lhs.end(), rhs.begin(), 0.0);
}

std::vector<double> multiply(const Matrix& matrix, const std::vector<double>& vector)
{
    std::vector<double> result(matrix.size(), 0.0);
    for (std::size_t row = 0; row < matrix.size(); ++row)
        result[row] = dot(matrix[row], vector);
    return result;
}

double infinityNorm(const std::vector<double>& values)
{
    double norm = 0.0;
    for (double value : values)
        norm = std::max(norm, std::abs(value));
    return norm;
}

Matrix regularizedCovariance(const Matrix& covariance, double relativeRegularization)
{
    const std::size_t dimension = covariance.size();
    Matrix result(dimension, std::vector<double>(dimension, 0.0));
    double diagonalScale = 0.0;

    for (std::size_t row = 0; row < dimension; ++row)
    {
        diagonalScale += std::abs(covariance[row][row]);
        for (std::size_t column = 0; column < dimension; ++column)
        {
            result[row][column] =
                0.5 * (covariance[row][column] + covariance[column][row]);
        }
    }

    diagonalScale /= static_cast<double>(dimension);
    const double ridge = std::max(1e-12,
        relativeRegularization * std::max(1e-12, diagonalScale));
    for (std::size_t index = 0; index < dimension; ++index)
        result[index][index] += ridge;
    return result;
}

void populateMetrics(PortfolioSolution& solution, const Statistics& statistics)
{
    solution.expectedReturn = dot(statistics.meanReturns, solution.weights);
    const std::vector<double> covarianceTimesWeights =
        multiply(statistics.covariance, solution.weights);
    solution.variance = std::max(0.0, dot(solution.weights, covarianceTimesWeights));
    solution.risk = std::sqrt(solution.variance);
}

} // namespace

ActiveSetQPSolver::ActiveSetQPSolver() = default;

ActiveSetQPSolver::ActiveSetQPSolver(Options options)
    : _options(std::move(options))
{
}

PortfolioSolution ActiveSetQPSolver::solveWithEqualities(
    const Statistics& statistics,
    const Matrix& equalityMatrix,
    const std::vector<double>& equalityValues,
    const std::vector<double>& initialWeights,
    double targetReturn) const
{
    PortfolioSolution result;
    result.targetReturn = targetReturn;
    result.weights = initialWeights;

    const std::size_t assetCount = statistics.meanReturns.size();
    const std::size_t equalityCount = equalityMatrix.size();
    if (initialWeights.size() != assetCount || equalityValues.size() != equalityCount)
    {
        result.message = "Internal optimization dimensions are inconsistent.";
        return result;
    }
    for (const auto& row : equalityMatrix)
    {
        if (row.size() != assetCount)
        {
            result.message = "Internal equality-constraint dimensions are inconsistent.";
            return result;
        }
    }
    for (std::size_t constraint = 0; constraint < equalityCount; ++constraint)
    {
        if (std::abs(dot(equalityMatrix[constraint], initialWeights) -
                     equalityValues[constraint]) > 100.0 * _options.primalTolerance)
        {
            result.message = "The active-set starting point is not equality-feasible.";
            return result;
        }
    }

    const Matrix hessian = regularizedCovariance(
        statistics.covariance, _options.relativeRegularization);
    std::vector<bool> active(assetCount, false);
    for (std::size_t asset = 0; asset < assetCount; ++asset)
    {
        if (result.weights[asset] <= _options.primalTolerance)
        {
            result.weights[asset] = 0.0;
            active[asset] = true;
        }
    }

    for (std::size_t iteration = 1; iteration <= _options.maxIterations; ++iteration)
    {
        result.iterations = iteration;
        std::vector<std::size_t> activeIndices;
        for (std::size_t asset = 0; asset < assetCount; ++asset)
        {
            if (active[asset])
                activeIndices.push_back(asset);
        }

        const std::size_t kktDimension =
            assetCount + equalityCount + activeIndices.size();
        Matrix kkt(kktDimension, std::vector<double>(kktDimension, 0.0));
        std::vector<double> rhs(kktDimension, 0.0);
        const std::vector<double> gradient = multiply(hessian, result.weights);

        for (std::size_t row = 0; row < assetCount; ++row)
        {
            rhs[row] = -gradient[row];
            for (std::size_t column = 0; column < assetCount; ++column)
                kkt[row][column] = hessian[row][column];
        }

        for (std::size_t constraint = 0; constraint < equalityCount; ++constraint)
        {
            const std::size_t kktIndex = assetCount + constraint;
            for (std::size_t asset = 0; asset < assetCount; ++asset)
            {
                kkt[asset][kktIndex] = equalityMatrix[constraint][asset];
                kkt[kktIndex][asset] = equalityMatrix[constraint][asset];
            }
        }

        for (std::size_t constraint = 0; constraint < activeIndices.size(); ++constraint)
        {
            const std::size_t kktIndex = assetCount + equalityCount + constraint;
            const std::size_t asset = activeIndices[constraint];
            kkt[asset][kktIndex] = 1.0;
            kkt[kktIndex][asset] = 1.0;
        }

        std::vector<double> kktSolution;
        std::string linearSystemError;

        if (!MatrixBackend::solveSymmetricSystem(
            kkt,
            rhs,
            kktSolution,
            _options.linearSystemBackend,
            linearSystemError))
        {
            result.message =
                "The active-set KKT system could not be solved with " +
                std::string(MatrixBackend::name(
                    _options.linearSystemBackend)) +
                ": " + linearSystemError;

            populateMetrics(result, statistics);
            return result;
        }

        std::vector<double> direction(
            kktSolution.begin(), kktSolution.begin() + static_cast<std::ptrdiff_t>(assetCount));
        if (infinityNorm(direction) <= _options.directionTolerance)
        {
            std::size_t constraintToRemove = activeIndices.size();
            double largestInvalidMultiplier = _options.multiplierTolerance;
            for (std::size_t constraint = 0; constraint < activeIndices.size(); ++constraint)
            {
                const double equalityFormMultiplier =
                    kktSolution[assetCount + equalityCount + constraint];
                if (equalityFormMultiplier > largestInvalidMultiplier)
                {
                    largestInvalidMultiplier = equalityFormMultiplier;
                    constraintToRemove = constraint;
                }
            }

            if (constraintToRemove < activeIndices.size())
            {
                active[activeIndices[constraintToRemove]] = false;
                continue;
            }

            result.converged = true;
            result.message = "Optimal long-only portfolio found.";
            break;
        }

        double stepLength = 1.0;
        std::size_t blockingAsset = assetCount;
        for (std::size_t asset = 0; asset < assetCount; ++asset)
        {
            if (!active[asset] && direction[asset] < -_options.directionTolerance)
            {
                const double candidate = -result.weights[asset] / direction[asset];
                if (candidate < stepLength)
                {
                    stepLength = std::max(0.0, candidate);
                    blockingAsset = asset;
                }
            }
        }

        for (std::size_t asset = 0; asset < assetCount; ++asset)
        {
            result.weights[asset] += stepLength * direction[asset];
            if (std::abs(result.weights[asset]) <= 10.0 * _options.primalTolerance)
                result.weights[asset] = 0.0;
        }
        if (blockingAsset < assetCount &&
            stepLength < 1.0 - _options.primalTolerance)
        {
            result.weights[blockingAsset] = 0.0;
            active[blockingAsset] = true;
        }
    }

    if (!result.converged && result.message.empty())
        result.message = "The active-set iteration limit was reached.";

    if (result.converged)
    {
        for (std::size_t constraint = 0; constraint < equalityCount; ++constraint)
        {
            if (std::abs(dot(equalityMatrix[constraint], result.weights) -
                         equalityValues[constraint]) >
                100.0 * _options.primalTolerance)
            {
                result.converged = false;
                result.message = "The final portfolio failed the equality-feasibility check.";
                break;
            }
        }
        if (result.converged &&
            *std::min_element(result.weights.begin(), result.weights.end()) <
                -100.0 * _options.primalTolerance)
        {
            result.converged = false;
            result.message = "The final portfolio failed the non-negativity check.";
        }
    }

    result.activeSet.clear();
    for (std::size_t asset = 0; asset < assetCount; ++asset)
    {
        if (result.weights[asset] <= 10.0 * _options.primalTolerance)
            result.activeSet.push_back(asset);
    }
    populateMetrics(result, statistics);
    return result;
}

PortfolioSolution ActiveSetQPSolver::solveMinimumVariance(
    const Statistics& statistics) const
{
    PortfolioSolution result;
    std::string error;
    if (!validateStatistics(statistics, error))
    {
        result.message = error;
        return result;
    }

    const std::size_t assetCount = statistics.meanReturns.size();
    Matrix equalityMatrix(1, std::vector<double>(assetCount, 1.0));
    std::vector<double> equalityValues{1.0};
    std::vector<double> initialWeights(
        assetCount, 1.0 / static_cast<double>(assetCount));

    result = solveWithEqualities(statistics, equalityMatrix, equalityValues,
                                 initialWeights, 0.0);
    result.targetReturn = result.expectedReturn;
    if (result.converged)
        result.message = "Global minimum-variance long-only portfolio found.";
    return result;
}

PortfolioSolution ActiveSetQPSolver::solveTargetReturn(
    const Statistics& statistics, double targetReturn) const
{
    PortfolioSolution result;
    result.targetReturn = targetReturn;
    std::string error;
    if (!validateStatistics(statistics, error))
    {
        result.message = error;
        return result;
    }
    if (!isFinite(targetReturn))
    {
        result.message = "Target return must be finite.";
        return result;
    }

    const auto [minimumIterator, maximumIterator] = std::minmax_element(
        statistics.meanReturns.begin(), statistics.meanReturns.end());
    const double minimumReturn = *minimumIterator;
    const double maximumReturn = *maximumIterator;
    const double returnRange = maximumReturn - minimumReturn;
    const double returnTolerance = std::max(
        _options.primalTolerance, 1e-9 * std::max(1.0, std::abs(returnRange)));

    if (targetReturn < minimumReturn - returnTolerance ||
        targetReturn > maximumReturn + returnTolerance)
    {
        result.message = "Target return is outside the feasible long-only range.";
        return result;
    }

    if (returnRange <= returnTolerance)
    {
        result = solveMinimumVariance(statistics);
        result.targetReturn = targetReturn;
        if (result.converged)
            result.message = "All assets have the same expected return; the minimum-variance portfolio was selected.";
        return result;
    }

    const bool atLowerBoundary = std::abs(targetReturn - minimumReturn) <= returnTolerance;
    const bool atUpperBoundary = std::abs(targetReturn - maximumReturn) <= returnTolerance;
    if (atLowerBoundary || atUpperBoundary)
    {
        const double boundaryReturn = atLowerBoundary ? minimumReturn : maximumReturn;
        std::vector<std::size_t> eligibleAssets;
        for (std::size_t asset = 0; asset < statistics.meanReturns.size(); ++asset)
        {
            if (std::abs(statistics.meanReturns[asset] - boundaryReturn) <= returnTolerance)
                eligibleAssets.push_back(asset);
        }

        Statistics reduced;
        reduced.meanReturns.assign(eligibleAssets.size(), boundaryReturn);
        reduced.covariance.assign(eligibleAssets.size(),
                                  std::vector<double>(eligibleAssets.size(), 0.0));
        for (std::size_t row = 0; row < eligibleAssets.size(); ++row)
        {
            for (std::size_t column = 0; column < eligibleAssets.size(); ++column)
            {
                reduced.covariance[row][column] =
                    statistics.covariance[eligibleAssets[row]][eligibleAssets[column]];
            }
        }

        PortfolioSolution reducedSolution;
        if (eligibleAssets.size() == 1)
        {
            reducedSolution.converged = true;
            reducedSolution.weights = {1.0};
            reducedSolution.iterations = 0;
        }
        else
        {
            reducedSolution = solveMinimumVariance(reduced);
        }

        result = reducedSolution;
        result.targetReturn = targetReturn;
        result.weights.assign(statistics.meanReturns.size(), 0.0);
        for (std::size_t index = 0; index < eligibleAssets.size(); ++index)
            result.weights[eligibleAssets[index]] = reducedSolution.weights[index];
        result.activeSet.clear();
        for (std::size_t asset = 0; asset < result.weights.size(); ++asset)
        {
            if (result.weights[asset] <= 10.0 * _options.primalTolerance)
                result.activeSet.push_back(asset);
        }
        populateMetrics(result, statistics);
        if (result.converged)
            result.message = "Boundary-return minimum-variance portfolio found.";
        return result;
    }

    std::size_t lowerAsset = statistics.meanReturns.size();
    std::size_t upperAsset = statistics.meanReturns.size();
    double lowerReturn = -std::numeric_limits<double>::infinity();
    double upperReturn = std::numeric_limits<double>::infinity();
    for (std::size_t asset = 0; asset < statistics.meanReturns.size(); ++asset)
    {
        const double mean = statistics.meanReturns[asset];
        if (mean < targetReturn - returnTolerance && mean > lowerReturn)
        {
            lowerReturn = mean;
            lowerAsset = asset;
        }
        if (mean > targetReturn + returnTolerance && mean < upperReturn)
        {
            upperReturn = mean;
            upperAsset = asset;
        }
    }
    if (lowerAsset == statistics.meanReturns.size() ||
        upperAsset == statistics.meanReturns.size())
    {
        result.message = "Could not construct a feasible starting portfolio.";
        return result;
    }

    std::vector<double> initialWeights(statistics.meanReturns.size(), 0.0);
    initialWeights[lowerAsset] =
        (upperReturn - targetReturn) / (upperReturn - lowerReturn);
    initialWeights[upperAsset] = 1.0 - initialWeights[lowerAsset];

    Matrix equalityMatrix;
    equalityMatrix.emplace_back(statistics.meanReturns.size(), 1.0);
    equalityMatrix.push_back(statistics.meanReturns);
    std::vector<double> equalityValues{1.0, targetReturn};
    return solveWithEqualities(statistics, equalityMatrix, equalityValues,
                               initialWeights, targetReturn);
}

EfficientFrontier ActiveSetQPSolver::buildEfficientFrontier(
    const Statistics& statistics, std::size_t pointCount) const
{
    EfficientFrontier frontier;
    if (pointCount < 2)
    {
        frontier.message = "The efficient frontier requires at least two points.";
        return frontier;
    }
    if (pointCount > 500)
    {
        frontier.message = "The efficient frontier is limited to 500 points.";
        return frontier;
    }

    frontier.minimumVariancePortfolio = solveMinimumVariance(statistics);
    if (!frontier.minimumVariancePortfolio.converged)
    {
        frontier.message = frontier.minimumVariancePortfolio.message;
        return frontier;
    }

    const double startReturn = frontier.minimumVariancePortfolio.expectedReturn;
    const double maximumReturn = *std::max_element(
        statistics.meanReturns.begin(), statistics.meanReturns.end());
    if (maximumReturn - startReturn <= _options.primalTolerance)
    {
        frontier.points.push_back(frontier.minimumVariancePortfolio);
        frontier.complete = true;
        frontier.message = "The global minimum-variance portfolio already has the maximum feasible return.";
        return frontier;
    }

    frontier.points.reserve(pointCount);
    PortfolioSolution firstPoint = frontier.minimumVariancePortfolio;
    firstPoint.targetReturn = firstPoint.expectedReturn;
    frontier.points.push_back(std::move(firstPoint));

    for (std::size_t point = 1; point < pointCount; ++point)
    {
        const double fraction = static_cast<double>(point) /
                                static_cast<double>(pointCount - 1);
        const double target = startReturn + fraction * (maximumReturn - startReturn);
        PortfolioSolution solution = solveTargetReturn(statistics, target);
        if (!solution.converged)
        {
            frontier.message = "Efficient-frontier construction failed: " + solution.message;
            return frontier;
        }
        frontier.points.push_back(std::move(solution));
    }

    frontier.complete = true;
    frontier.message = "Efficient frontier constructed successfully.";
    return frontier;
}

} // namespace portfolio
