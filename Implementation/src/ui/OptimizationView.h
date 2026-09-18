#pragma once

#include "../AppState.h"
#include "../numerics/MatrixBackend.h"
#include "ChartCanvas.h"

#include <gui/Alert.h>
#include <gui/Button.h>
#include <gui/ComboBox.h>
#include <gui/GridComposer.h>
#include <gui/GridLayout.h>
#include <gui/HorizontalLayout.h>
#include <gui/Label.h>
#include <gui/NumericEdit.h>
#include <gui/Slider.h>
#include <gui/TextEdit.h>
#include <gui/VerticalLayout.h>
#include <gui/View.h>
#include <gui/FileDialog.h>

#include <algorithm>
#include <cstdio>
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
    gui::Label _targetSliderLabel;
    gui::Slider _targetSlider;

    gui::Label _frontierPointsLabel;
    gui::NumericEdit _frontierPoints;

    gui::Button _optimizeTarget;
    gui::Button _buildFrontier;
    gui::Button _exportFrontier;
    gui::Button _compareBackends;

    gui::Label _landmarksLabel;
    gui::Button _minimumRisk;
    gui::Button _balanced;
    gui::Button _highestReturn;

    gui::Label _selectedPortfolioHeading;
    gui::TextEdit _selectedPortfolioSummary;

    gui::Label _numericalDetailsHeading;

    gui::TextEdit _status;
    gui::HorizontalLayout _primaryButtonLayout;
    gui::HorizontalLayout _landmarkButtonLayout;
    gui::HorizontalLayout _verificationButtonLayout;
    gui::GridLayout _controlLayout;
    gui::VerticalLayout _resultLayout;
    gui::HorizontalLayout _layout;

    std::function<void()> _onResultsChanged;

    [[nodiscard]] portfolio::LinearSystemBackend selectedBackend() const noexcept
    {
        return _backend.getSelectedIndex() == 1
            ? portfolio::LinearSystemBackend::Sparse
            : portfolio::LinearSystemBackend::Dense;
    }

    void updateSelectedPortfolioDisplay()
    {
        if (!_state.hasSolution() || !_state.solution().converged)
        {
            _selectedPortfolioSummary.setText(tr("noSelectedPortfolio"));
            return;
        }

        const auto& solution = _state.solution();
        const auto& assetNames = _state.data().assetNames;

        std::string summary;
        char line[128];

        std::snprintf(
            line, sizeof(line),
            "Expected return: %.4f%%\n"
            "Risk: %.4f%%\n"
            "Variance: %.6f\n\n"
            "Allocation:\n",
            100.0 * solution.expectedReturn,
            100.0 * solution.risk,
            solution.variance);

        summary += line;

        const std::size_t assetCount =
            std::min(assetNames.size(), solution.weights.size());

        for (std::size_t asset = 0; asset < assetCount; ++asset)
        {
            std::snprintf(
                line, sizeof(line),
                "%s: %.2f%%\n",
                assetNames[asset].c_str(),
                100.0 * solution.weights[asset]);

            summary += line;
        }

        _selectedPortfolioSummary.setText(td::String(summary.c_str()));
    }

    void showCurrentSolutionDetails()
    {
        const auto& solution = _state.solution();
        const auto& assetNames = _state.data().assetNames;

        std::string details;
        char line[256];

        std::snprintf(
            line,
            sizeof(line),
            "Backend: %s\n"
            "Active-set iterations: %zu\n"
            "Active constraints: %zu",
            portfolio::MatrixBackend::name(_state.solutionBackend()),
            solution.iterations,
            solution.activeSet.size());

        details += line;

        if (!solution.activeSet.empty())
        {
            details += "\nZero-weight constraints: ";

            for (std::size_t i = 0; i < solution.activeSet.size(); ++i)
            {
                const std::size_t asset = solution.activeSet[i];

                if (asset < assetNames.size())
                {
                    if (i > 0)
                        details += ", ";

                    details += assetNames[asset];
                }
            }
        }

        _status.setText(td::String(details.c_str()));
        updateSelectedPortfolioDisplay();

        if (_onResultsChanged)
            _onResultsChanged();
    }

    void selectLandmark(std::size_t pointIndex)
    {
        std::string error;
        if (!_state.selectFrontierPoint(pointIndex, error))
        {
            _status.setText(td::String(error.c_str()));
            gui::Alert::show(
                tr("landmarkError"),
                td::String(error.c_str()));
            return;
        }

        const double targetReturn = _state.solution().targetReturn;
        _targetReturn.setValue(targetReturn);
        _targetSlider.setValue(targetReturn, false);
        showCurrentSolutionDetails();
    }

public:
    explicit OptimizationView(AppState& state)
        : _state(state)
        , _backendLabel(tr("matrixBackend"))
        , _targetLabel(tr("targetReturn"))
        , _targetReturn(td::real8)
        , _targetSliderLabel(tr("targetSlider"))
        , _targetSlider(gui::DataCtrl::Orientation::Horizontal, true)
        , _frontierPointsLabel(tr("frontierPoints"))
        , _frontierPoints(td::int4)
        , _optimizeTarget(tr("optimizeTarget"))
        , _buildFrontier(tr("buildFrontier"))
        , _exportFrontier(tr("exportFrontier"))
        , _compareBackends(tr("compareBackends"))
        , _landmarksLabel(tr("landmarks"), gui::Font::ID::SystemBold)
        , _minimumRisk(tr("minimumRisk"))
        , _balanced(tr("balanced"))
        , _highestReturn(tr("highestReturn"))
        , _selectedPortfolioHeading(
            tr("selectedPortfolioHeading"),
            gui::Font::ID::SystemBold)
        , _numericalDetailsHeading(
            tr("numericalDetails"),
            gui::Font::ID::SystemBold)
        , _primaryButtonLayout(4)
        , _landmarkButtonLayout(4)
        , _verificationButtonLayout(2)
        , _controlLayout(10, 2)
        , _resultLayout(3)
        , _layout(2)
    {
        _backend.addItem(tr("denseBackend"));
        _backend.addItem(tr("sparseBackend"));
        _backend.selectIndex(0);

        _targetReturn.setValue(0.01);
        _targetReturn.setMinValue(-1.0);
        _targetReturn.setMaxValue(1.0);
        _targetReturn.setNumberOfDigitsAfterDecimalPoint(6);
        _targetSlider.setRange(-1.0, 1.0, 101);
        _targetSlider.setValue(0.01, false);

        _frontierPoints.setValue(static_cast<td::INT4>(25));
        _frontierPoints.setMinValue(2.0);
        _frontierPoints.setMaxValue(500.0);

        _status.setAsReadOnly();
        _status.setText(tr("optimizationReady"));
        _selectedPortfolioSummary.setAsReadOnly();
        _selectedPortfolioSummary.setText(tr("noSelectedPortfolio"));
        _selectedPortfolioSummary.setSizeLimits(
            0, gui::Control::Limit::None,
            260, gui::Control::Limit::Fixed);

        _optimizeTarget.setType(gui::Button::Type::Constructive);

        _backend.setToolTip(tr("backendTooltip"));
        _targetReturn.setToolTip(tr("targetReturnTooltip"));
        _targetSlider.setToolTip(tr("targetSliderTooltip"));
        _frontierPoints.setToolTip(tr("frontierPointsTooltip"));
        _optimizeTarget.setToolTip(tr("optimizeTargetTooltip"));
        _buildFrontier.setToolTip(tr("buildFrontierTooltip"));
        _exportFrontier.setToolTip(tr("exportFrontierTooltip"));
        _compareBackends.setToolTip(tr("compareBackendsTooltip"));
        _minimumRisk.setToolTip(tr("minimumRiskTooltip"));
        _balanced.setToolTip(tr("balancedTooltip"));
        _highestReturn.setToolTip(tr("highestReturnTooltip"));

        _backend.disable();
        _targetReturn.disable();
        _targetSlider.disable();
        _frontierPoints.disable();
        _optimizeTarget.disable();
        _buildFrontier.disable();
        _exportFrontier.disable();
        _compareBackends.disable();
        _minimumRisk.disable();
        _balanced.disable();
        _highestReturn.disable();

        _primaryButtonLayout << _optimizeTarget
            << _buildFrontier
            << _exportFrontier;
        _primaryButtonLayout.appendSpacer();

        _landmarkButtonLayout << _minimumRisk
            << _balanced
            << _highestReturn;
        _landmarkButtonLayout.appendSpacer();

        _verificationButtonLayout << _compareBackends;
        _verificationButtonLayout.appendSpacer();

        gui::GridComposer composer(_controlLayout);

        // Portfolio exploration
        composer.appendRow(_targetLabel) << _targetReturn;
        composer.appendRow(_targetSliderLabel) << _targetSlider;
        composer.appendRow(_primaryButtonLayout, 0);
        composer.appendRow(_landmarksLabel, 0);
        composer.appendRow(_landmarkButtonLayout, 0);

        // Numerical optimization
        composer.appendRow(_numericalDetailsHeading, 0);
        composer.appendRow(_backendLabel) << _backend;
        composer.appendRow(_frontierPointsLabel) << _frontierPoints;
        composer.appendRow(_verificationButtonLayout, 0);
        composer.appendRow(_status, 0);

        _resultLayout << _selectedPortfolioHeading
            << _selectedPortfolioSummary;
        _resultLayout.appendSpacer();

        _layout << _controlLayout << _resultLayout;
        setLayout(&_layout);

        _targetSlider.onChangedValue([this]()
        {
            _targetReturn.setValue(_targetSlider.getValue());
        });

        _optimizeTarget.onClick([this]()
            {
                double targetReturn = 0.0;
                _targetReturn.getValue(targetReturn);
                _targetSlider.setValue(targetReturn, false);

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

                showCurrentSolutionDetails();
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
                _exportFrontier.disable(false);
                _minimumRisk.disable(false);
                _balanced.disable(false);
                _highestReturn.disable(false);

                if (_onResultsChanged)
                    _onResultsChanged();
            });

        _minimumRisk.onClick([this]()
        {
            selectLandmark(0);
        });

        _balanced.onClick([this]()
        {
            const auto& points = _state.frontier().points;
            if (!points.empty())
                selectLandmark(points.size() / 2);
        });

        _highestReturn.onClick([this]()
        {
            const auto& points = _state.frontier().points;
            if (!points.empty())
                selectLandmark(points.size() - 1);
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
    }

    void setOnResultsChanged(std::function<void()> callback)
    {
        _onResultsChanged = std::move(callback);
    }

    void onDataLoaded()
    {
        if (!_state.hasData() || _state.statistics().meanReturns.empty())
            return;

        const auto [minimumIterator, maximumIterator] = std::minmax_element(
            _state.statistics().meanReturns.begin(),
            _state.statistics().meanReturns.end());

        const double minimumReturn = *minimumIterator;
        const double maximumReturn = *maximumIterator;
        constexpr double preferredTarget = 0.01;
        const double initialTarget =
            preferredTarget >= minimumReturn &&
            preferredTarget <= maximumReturn
            ? preferredTarget
            : 0.5 * (minimumReturn + maximumReturn);

        _targetReturn.setMinValue(minimumReturn);
        _targetReturn.setMaxValue(maximumReturn);
        _targetReturn.setValue(initialTarget);
        _targetSlider.setRange(minimumReturn, maximumReturn, 101);
        _targetSlider.setValue(initialTarget, false);

        _backend.disable(false);
        _targetReturn.disable(false);
        _targetSlider.disable(false);
        _frontierPoints.disable(false);
        _optimizeTarget.disable(false);
        _buildFrontier.disable(false);
        _compareBackends.disable(false);
        _exportFrontier.disable();
        _minimumRisk.disable();
        _balanced.disable();
        _highestReturn.disable();
        updateSelectedPortfolioDisplay();

        td::String message;
        message.format(
            tr("optimizationDataReady").c_str(),
            100.0 * minimumReturn,
            100.0 * maximumReturn);
        _status.setText(message);
    }
};
