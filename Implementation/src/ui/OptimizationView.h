#pragma once

#include "../AppState.h"
#include "../numerics/MatrixBackend.h"

#include <gui/Alert.h>
#include <gui/Button.h>
#include <gui/ComboBox.h>
#include <gui/GridComposer.h>
#include <gui/GridLayout.h>
#include <gui/HorizontalLayout.h>
#include <gui/Label.h>
#include <gui/NumericEdit.h>
#include <gui/TextEdit.h>
#include <gui/View.h>
#include <gui/FileDialog.h>

#include <functional>
#include <string>
#include <utility>

class OptimizationView : public gui::View
{
    AppState& _state;

    gui::Label _backendLabel;
    gui::ComboBox _backend;

    gui::Label _targetLabel;
    gui::NumericEdit _targetReturn;

    gui::Label _frontierPointsLabel;
    gui::NumericEdit _frontierPoints;

    gui::Button _optimizeTarget;
    gui::Button _buildFrontier;
    gui::Button _exportFrontier;
    gui::Button _compareBackends;
    gui::Button _verifyBackend;

    gui::TextEdit _status;
    gui::HorizontalLayout _buttonLayout;
    gui::GridLayout _layout;

    std::function<void()> _onResultsChanged;

    [[nodiscard]] portfolio::LinearSystemBackend selectedBackend() const noexcept
    {
        return _backend.getSelectedIndex() == 1
            ? portfolio::LinearSystemBackend::Sparse
            : portfolio::LinearSystemBackend::Dense;
    }

public:
    explicit OptimizationView(AppState& state)
        : _state(state)
        , _backendLabel(tr("matrixBackend"))
        , _targetLabel(tr("targetReturn"))
        , _targetReturn(td::real8)
        , _frontierPointsLabel(tr("frontierPoints"))
        , _frontierPoints(td::int4)
        , _optimizeTarget(tr("optimizeTarget"))
        , _buildFrontier(tr("buildFrontier"))
        , _exportFrontier(tr("exportFrontier"))
        , _compareBackends(tr("compareBackends"))
        , _verifyBackend(tr("verifyBackend"))
        , _buttonLayout(6)
        , _layout(5, 2)
    {
        _backend.addItem(tr("denseBackend"));
        _backend.addItem(tr("sparseBackend"));
        _backend.selectIndex(0);

        _targetReturn.setValue(0.01);
        _targetReturn.setMinValue(-1.0);
        _targetReturn.setMaxValue(1.0);
        _targetReturn.setNumberOfDigitsAfterDecimalPoint(6);

        _frontierPoints.setValue(static_cast<td::INT4>(25));
        _frontierPoints.setMinValue(2.0);
        _frontierPoints.setMaxValue(500.0);

        _status.setAsReadOnly();
        _status.setText(tr("optimizationReady"));

        _optimizeTarget.setType(gui::Button::Type::Constructive);

        _buttonLayout << _optimizeTarget
            << _buildFrontier
            << _exportFrontier
            << _compareBackends
            << _verifyBackend;
        _buttonLayout.appendSpacer();

        gui::GridComposer composer(_layout);
        composer.appendRow(_backendLabel) << _backend;
        composer.appendRow(_targetLabel) << _targetReturn;
        composer.appendRow(_frontierPointsLabel) << _frontierPoints;
        composer.appendRow(_buttonLayout, 0);
        composer.appendRow(_status, 0);

        setLayout(&_layout);

        _optimizeTarget.onClick([this]()
            {
                double targetReturn = 0.0;
                _targetReturn.getValue(targetReturn);

                const auto backend = selectedBackend();
                std::string error;

                if (!_state.solveTargetReturn(targetReturn, backend, error))
                {
                    _status.setText(td::String(error.c_str()));
                    gui::Alert::show(
                        tr("optimizationError"),
                        td::String(error.c_str()));
                    return;
                }

                const std::string summary =
                    std::string("Backend: ") +
                    portfolio::MatrixBackend::name(backend) +
                    "\n" +
                    _state.solutionSummary();

                _status.setText(td::String(summary.c_str()));

                if (_onResultsChanged)
                    _onResultsChanged();
            });

        _buildFrontier.onClick([this]()
            {
                td::INT4 pointCount = 0;
                _frontierPoints.getValue(pointCount);

                const auto backend = selectedBackend();
                std::string error;

                if (!_state.buildEfficientFrontier(
                    static_cast<std::size_t>(pointCount),
                    backend,
                    error))
                {
                    _status.setText(td::String(error.c_str()));
                    gui::Alert::show(
                        tr("frontierError"),
                        td::String(error.c_str()));
                    return;
                }

                const std::string summary =
                    std::string("Backend: ") +
                    portfolio::MatrixBackend::name(backend) +
                    "\n" +
                    _state.frontierSummary();

                _status.setText(td::String(summary.c_str()));

                if (_onResultsChanged)
                    _onResultsChanged();
            });

        _exportFrontier.onClick([this]()
            {
                if (!_state.hasFrontier())
                {
                    const std::string error =
                        "Build the efficient frontier before exporting it.";

                    _status.setText(td::String(error.c_str()));
                    gui::Alert::show(
                        tr("exportError"),
                        td::String(error.c_str()));
                    return;
                }

                constexpr td::UINT4 dialogID = 1002;

                gui::SaveFileDialog::show(
                    this,
                    tr("saveFrontier"),
                    { {tr("csvFiles"), "*.csv"} },
                    dialogID,
                    [this](gui::FileDialog* dialog)
                    {
                        if (dialog->getStatus() !=
                            gui::FileDialog::Status::OK)
                        {
                            return;
                        }

                        const td::String fileName =
                            dialog->getFileName();

                        std::string error;

                        if (!_state.exportFrontierCsv(
                            fileName.c_str(),
                            error))
                        {
                            _status.setText(td::String(error.c_str()));
                            gui::Alert::show(
                                tr("exportError"),
                                td::String(error.c_str()));
                            return;
                        }

                        td::String message;
                        message.format(
                            "%s\n%s",
                            tr("exportSuccess").c_str(),
                            fileName.c_str());

                        _status.setText(message);
                    },
                    "portfolio_frontier.csv");
            });

        _compareBackends.onClick([this]()
            {
                double targetReturn = 0.0;
                _targetReturn.getValue(targetReturn);

                std::string report;
                std::string error;

                if (!_state.compareBackends(
                    targetReturn,
                    report,
                    error))
                {
                    _status.setText(td::String(error.c_str()));
                    gui::Alert::show(
                        tr("comparisonError"),
                        td::String(error.c_str()));
                    return;
                }

                _status.setText(td::String(report.c_str()));
            });

        _verifyBackend.onClick([this]()
            {
                double first = 0.0;
                double second = 0.0;
                std::string error;

                if (!portfolio::MatrixBackend::verify(first, second, error))
                {
                    _status.setText(td::String(error.c_str()));
                    gui::Alert::show(
                        tr("backendError"),
                        td::String(error.c_str()));
                    return;
                }

                td::String message;
                message.format(
                    "%s\nx = %.6f, y = %.6f",
                    tr("backendSuccess").c_str(),
                    first,
                    second);

                _status.setText(message);
            });
    }

    void setOnResultsChanged(std::function<void()> callback)
    {
        _onResultsChanged = std::move(callback);
    }
};