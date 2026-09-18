#include "MatrixBackend.h"

#include <dense/Matrix.h>
#include <sparse/ISolver.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace portfolio
{
    namespace
    {

        bool validateSystem(const MatrixBackend::Matrix& matrix,
            const std::vector<double>& rightHandSide,
            std::string& error)
        {
            if (matrix.empty() || rightHandSide.size() != matrix.size())
            {
                error = "Linear-system dimensions are inconsistent.";
                return false;
            }

            for (const auto& row : matrix)
            {
                if (row.size() != matrix.size())
                {
                    error = "The coefficient matrix must be square.";
                    return false;
                }
            }

            return true;
        }

        bool solveDense(const MatrixBackend::Matrix& matrix,
            const std::vector<double>& rightHandSide,
            std::vector<double>& solution,
            std::string& error)
        {
            const auto dimension = static_cast<td::INT4>(matrix.size());

            dense::DblMatrix coefficients(dimension, dimension);
            auto coefficientValues = coefficients.getManipulator();

            for (td::INT4 row = 0; row < dimension; ++row)
            {
                for (td::INT4 column = 0; column < dimension; ++column)
                {
                    coefficientValues(row, column) =
                        matrix[static_cast<std::size_t>(row)]
                        [static_cast<std::size_t>(column)];
                }
            }

            dense::DblMatrix rhs(dimension, 1);
            auto rhsValues = rhs.getColumnManipulator();

            for (td::INT4 row = 0; row < dimension; ++row)
                rhsValues(row) = rightHandSide[static_cast<std::size_t>(row)];

            if (!coefficients.solve(rhs))
            {
                error = "dense::DblMatrix could not solve the system.";
                return false;
            }

            auto result = rhs.getColumnManipulator();
            solution.resize(matrix.size());

            for (td::INT4 row = 0; row < dimension; ++row)
                solution[static_cast<std::size_t>(row)] = result(row);

            error.clear();
            return true;
        }

        bool solveSparse(const MatrixBackend::Matrix& matrix,
            const std::vector<double>& rightHandSide,
            std::vector<double>& solution,
            std::string& error)
        {
            int nonZeroCount = 0;

            for (std::size_t row = 0; row < matrix.size(); ++row)
            {
                for (std::size_t column = 0;
                    column < matrix.size();
                    ++column)
                {
                    if (matrix[row][column] != 0.0)
                        ++nonZeroCount;
                }
            }

            sparse::DblSolverReleaser solverOwner(
                sparse::createDblSolver(
                    static_cast<int>(matrix.size()),
                    std::max(1, nonZeroCount),
                    sparse::Symmetry::NonSymmetric,
                    sparse::SolverType::LU,
                    sparse::Pivoting::MarkowitzMultiPass,
                    sparse::Ordering::Own));

            sparse::DblSolver* solver = solverOwner.ptr();

            if (solver == nullptr)
            {
                error = "Could not create sparse::DblSolver.";
                return false;
            }

            for (std::size_t row = 0; row < matrix.size(); ++row)
            {
                for (std::size_t column = 0;
                    column < matrix.size();
                    ++column)
                {
                    const double value = matrix[row][column];

                    if (value != 0.0)
                    {
                        solver->addTriple(
                            static_cast<td::INT4>(row),
                            static_cast<td::INT4>(column),
                            value);
                    }
                }

                solver->setRHS(
                    static_cast<td::INT4>(row),
                    rightHandSide[row]);
            }

            if (!solver->factorize())
            {
                std::ostringstream message;
                message << "sparse::DblSolver factorization failed";

                if (const char* details = solver->getLastError();
                    details != nullptr && *details != '\0')
                {
                    message << ": " << details;
                }

                error = message.str();
                return false;
            }

            if (!solver->solve())
            {
                std::ostringstream message;
                message << "sparse::DblSolver solve failed";

                if (const char* details = solver->getLastError();
                    details != nullptr && *details != '\0')
                {
                    message << ": " << details;
                }

                error = message.str();
                return false;
            }

            solution.resize(matrix.size());

            for (std::size_t row = 0; row < matrix.size(); ++row)
            {
                solution[row] =
                    solver->x(static_cast<td::INT4>(row));
            }

            error.clear();
            return true;
        }

    } // namespace

    bool MatrixBackend::solveSymmetricSystem(
        const Matrix& matrix,
        const std::vector<double>& rightHandSide,
        std::vector<double>& solution,
        LinearSystemBackend backend,
        std::string& error)
    {
        solution.clear();

        if (!validateSystem(matrix, rightHandSide, error))
            return false;

        if (backend == LinearSystemBackend::Sparse)
        {
            return solveSparse(
                matrix, rightHandSide, solution, error);
        }

        return solveDense(
            matrix, rightHandSide, solution, error);
    }

    const char* MatrixBackend::name(
        LinearSystemBackend backend) noexcept
    {
        return backend == LinearSystemBackend::Sparse
            ? "natID sparse::DblSolver (LU)"
            : "natID dense::DblMatrix";
    }

    bool MatrixBackend::verify(
        double& firstValue,
        double& secondValue,
        std::string& error)
    {
        const Matrix matrix{
            {2.0, 1.0},
            {1.0, 3.0}
        };

        const std::vector<double> rhs{ 5.0, 7.0 };

        std::vector<double> denseSolution;
        std::vector<double> sparseSolution;

        if (!solveSymmetricSystem(
            matrix,
            rhs,
            denseSolution,
            LinearSystemBackend::Dense,
            error) ||
            !solveSymmetricSystem(
                matrix,
                rhs,
                sparseSolution,
                LinearSystemBackend::Sparse,
                error))
        {
            return false;
        }

        if (denseSolution.size() != 2 ||
            sparseSolution.size() != 2 ||
            std::abs(denseSolution[0] - sparseSolution[0]) > 1e-10 ||
            std::abs(denseSolution[1] - sparseSolution[1]) > 1e-10)
        {
            error =
                "Dense and sparse natID solvers returned different results.";
            return false;
        }

        firstValue = denseSolution[0];
        secondValue = denseSolution[1];

        error.clear();
        return true;
    }

} // namespace portfolio