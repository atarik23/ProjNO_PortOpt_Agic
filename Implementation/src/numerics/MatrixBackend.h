#pragma once

#include <string>
#include <vector>

namespace portfolio
{

    enum class LinearSystemBackend
    {
        Dense,
        Sparse
    };

    class MatrixBackend
    {
    public:
        using Matrix = std::vector<std::vector<double>>;

        static bool solveSymmetricSystem(
            const Matrix& matrix,
            const std::vector<double>& rightHandSide,
            std::vector<double>& solution,
            LinearSystemBackend backend,
            std::string& error);

        [[nodiscard]] static const char* name(
            LinearSystemBackend backend) noexcept;

        static bool verify(
            double& firstValue,
            double& secondValue,
            std::string& error);
    };

} // namespace portfolio