#pragma once

#include "../core/PortfolioData.h"
#include "MatrixBackend.h"

#include <cstddef>
#include <vector>

namespace portfolio
{

    class ActiveSetQPSolver
    {
    public:
        struct Options
        {
            std::size_t maxIterations = 500;
            double primalTolerance = 1e-9;
            double directionTolerance = 1e-10;
            double multiplierTolerance = 1e-9;
            double relativeRegularization = 1e-9;

            LinearSystemBackend linearSystemBackend =
                LinearSystemBackend::Dense;
        };

        ActiveSetQPSolver();
        explicit ActiveSetQPSolver(Options options);

        [[nodiscard]] PortfolioSolution solveMinimumVariance(
            const Statistics& statistics) const;

        [[nodiscard]] PortfolioSolution solveTargetReturn(
            const Statistics& statistics,
            double targetReturn) const;

        [[nodiscard]] EfficientFrontier buildEfficientFrontier(
            const Statistics& statistics,
            std::size_t pointCount) const;

    private:
        using Matrix = std::vector<std::vector<double>>;

        Options _options;

        [[nodiscard]] PortfolioSolution solveWithEqualities(
            const Statistics& statistics,
            const Matrix& equalityMatrix,
            const std::vector<double>& equalityValues,
            const std::vector<double>& initialWeights,
            double targetReturn) const;
    };

} // namespace portfolio