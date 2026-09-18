#pragma once

#include "../AppState.h"

#include <gui/Canvas.h>
#include <gui/DrawableString.h>
#include <gui/Shape.h>

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <string>

class PortfolioChartCanvas : public gui::Canvas
{
protected:
    static constexpr double plotLeft = 92.0;
    static constexpr double plotRight = 950.0;
    static constexpr double defaultPlotTop = 100.0;
    static constexpr double plotBottom = 500.0;

    td::String _xLabel;
    td::String _yLabel;
    td::String _emptyMessage;

    bool getModelSize(gui::Size& modelSize) const override
    {
        modelSize.width = 1000;
        modelSize.height = 600;
        return true;
    }

    static void drawText(const std::string& text, double x, double y,
                         gui::Font::ID font = gui::Font::ID::SystemNormal,
                         td::ColorID color = td::ColorID::SysText)
    {
        gui::DrawableString drawable(text.c_str());
        drawable.draw({x, y}, font, color);
    }

    static void drawTextInRect(const std::string& text,
                               double left, double top,
                               double right, double bottom,
                               gui::Font::ID font = gui::Font::ID::SystemNormal,
                               td::ColorID color = td::ColorID::SysText)
    {
        gui::DrawableString::draw(
            td::String(text.c_str()),
            {left, top, right, bottom},
            font,
            color,
            td::TextAlignment::Left,
            td::VAlignment::Center,
            td::TextEllipsize::End);
    }

    static std::string percentage(double value, int decimals = 2)
    {
        std::ostringstream output;
        output << std::fixed << std::setprecision(decimals) << 100.0 * value << '%';
        return output.str();
    }

    static void drawLineLegend(double x, double y, td::ColorID color,
                               const std::string& label,
                               td::LinePattern pattern = td::LinePattern::Solid,
                               float width = 3.0F,
                               double entryWidth = 180.0)
    {
        gui::Shape::drawLine({x, y + 7.0}, {x + 24.0, y + 7.0},
                             color, width, pattern);
        drawTextInRect(label, x + 31.0, y - 3.0,
                       x + entryWidth, y + 18.0,
                       gui::Font::ID::SystemSmaller,
                       td::ColorID::SysText);
    }

    static void drawSquareLegend(double x, double y, td::ColorID color,
                                 const std::string& label,
                                 double entryWidth = 180.0)
    {
        gui::Shape::drawRect({x + 7.0, y + 3.0, x + 17.0, y + 13.0}, color);
        drawTextInRect(label, x + 31.0, y - 3.0,
                       x + entryWidth, y + 18.0,
                       gui::Font::ID::SystemSmaller,
                       td::ColorID::SysText);
    }

    static void drawCrossMarker(double x, double y, td::ColorID color,
                                double halfSize = 7.0, float width = 2.5F)
    {
        gui::Shape::drawLine({x - halfSize, y}, {x + halfSize, y},
                             color, width);
        gui::Shape::drawLine({x, y - halfSize}, {x, y + halfSize},
                             color, width);
        gui::Shape::drawRect({x - 2.0, y - 2.0, x + 2.0, y + 2.0}, color);
    }

    static void drawCrossLegend(double x, double y, td::ColorID color,
                                const std::string& label,
                                double entryWidth = 180.0)
    {
        drawCrossMarker(x + 12.0, y + 7.0, color, 6.0, 2.0F);
        drawTextInRect(label, x + 31.0, y - 3.0,
                       x + entryWidth, y + 18.0,
                       gui::Font::ID::SystemSmaller,
                       td::ColorID::SysText);
    }

    void drawAxes(double plotTop = defaultPlotTop) const
    {
        gui::Shape::drawRect({18.0, 12.0, 982.0, plotTop - 18.0},
                             td::ColorID::SysBackAlt2);
        gui::Shape::drawRect({plotLeft, plotTop, plotRight, plotBottom},
                             td::ColorID::SysBackAlt1);
        gui::Shape::drawRect({plotLeft, plotTop, plotRight, plotBottom},
                             td::ColorID::SysText, 1.0F);

        gui::DrawableString::draw(
            _xLabel,
            {plotLeft, plotBottom + 31.0, plotRight, plotBottom + 67.0},
            gui::Font::ID::SystemBold,
            td::ColorID::SysText,
            td::TextAlignment::Center,
            td::VAlignment::Center);
        gui::DrawableString::draw(
            _yLabel,
            {30.0, 18.0, 390.0, 52.0},
            gui::Font::ID::SystemBold,
            td::ColorID::SysText,
            td::TextAlignment::Left,
            td::VAlignment::Center);
    }

    void drawEmptyMessage(double plotTop = defaultPlotTop) const
    {
        gui::DrawableString::draw(
            _emptyMessage,
            {plotLeft + 20.0, plotTop, plotRight - 20.0, plotBottom},
            gui::Font::ID::SystemNormal,
            td::ColorID::DarkGray,
            td::TextAlignment::Center,
            td::VAlignment::Center);
    }

public:
    PortfolioChartCanvas(const td::String& xLabel, const td::String& yLabel,
                         const td::String& emptyMessage)
        : _xLabel(xLabel)
        , _yLabel(yLabel)
        , _emptyMessage(emptyMessage)
    {
        setClipsToBounds();
    }

    void refresh()
    {
        reDraw();
    }
};

class EfficientFrontierCanvas : public PortfolioChartCanvas
{
    const AppState& _state;

    void onDraw(const gui::Rect&) override
    {
        constexpr double plotTop = defaultPlotTop;
        drawAxes(plotTop);
        if (!_state.hasFrontier() || _state.frontier().points.empty())
        {
            drawEmptyMessage(plotTop);
            return;
        }

        const auto& points = _state.frontier().points;
        double minimumRisk = points.front().risk;
        double maximumRisk = points.front().risk;
        double minimumReturn = points.front().expectedReturn;
        double maximumReturn = points.front().expectedReturn;
        for (const auto& point : points)
        {
            minimumRisk = std::min(minimumRisk, point.risk);
            maximumRisk = std::max(maximumRisk, point.risk);
            minimumReturn = std::min(minimumReturn, point.expectedReturn);
            maximumReturn = std::max(maximumReturn, point.expectedReturn);
        }
        for (std::size_t asset = 0; asset < _state.statistics().meanReturns.size(); ++asset)
        {
            minimumRisk = std::min(minimumRisk, _state.statistics().standardDeviations[asset]);
            maximumRisk = std::max(maximumRisk, _state.statistics().standardDeviations[asset]);
            minimumReturn = std::min(minimumReturn, _state.statistics().meanReturns[asset]);
            maximumReturn = std::max(maximumReturn, _state.statistics().meanReturns[asset]);
        }

        const bool showSelectedPortfolio =
            _state.hasSolution() && _state.solution().converged;
        if (showSelectedPortfolio)
        {
            minimumRisk = std::min(minimumRisk, _state.solution().risk);
            maximumRisk = std::max(maximumRisk, _state.solution().risk);
            minimumReturn = std::min(minimumReturn, _state.solution().expectedReturn);
            maximumReturn = std::max(maximumReturn, _state.solution().expectedReturn);
        }

        const double riskPadding = std::max(1e-6, 0.08 * (maximumRisk - minimumRisk));
        const double returnPadding = std::max(1e-6, 0.08 * (maximumReturn - minimumReturn));
        minimumRisk = std::max(0.0, minimumRisk - riskPadding);
        maximumRisk += riskPadding;
        minimumReturn -= returnPadding;
        maximumReturn += returnPadding;

        const auto mapX = [minimumRisk, maximumRisk](double risk)
        {
            return plotLeft + (risk - minimumRisk) / (maximumRisk - minimumRisk) *
                                  (plotRight - plotLeft);
        };
        const auto mapY = [minimumReturn, maximumReturn](double expectedReturn)
        {
            return plotBottom - (expectedReturn - minimumReturn) /
                                    (maximumReturn - minimumReturn) *
                                    (plotBottom - plotTop);
        };

        constexpr int tickIntervals = 5;
        for (int tick = 0; tick <= tickIntervals; ++tick)
        {
            const double fraction = static_cast<double>(tick) / tickIntervals;
            const double x = plotLeft + fraction * (plotRight - plotLeft);
            const double y = plotBottom - fraction * (plotBottom - plotTop);
            if (tick > 0 && tick < tickIntervals)
            {
                gui::Shape::drawLine({x, plotTop}, {x, plotBottom},
                                     td::ColorID::LightGray, 0.5F);
                gui::Shape::drawLine({plotLeft, y}, {plotRight, y},
                                     td::ColorID::LightGray, 0.5F);
            }
            drawText(percentage(minimumRisk + fraction * (maximumRisk - minimumRisk), 1),
                     x - 22.0, plotBottom + 8.0, gui::Font::ID::SystemSmaller,
                     td::ColorID::DarkGray);
            drawText(percentage(minimumReturn + fraction * (maximumReturn - minimumReturn), 2),
                     24.0, y - 7.0, gui::Font::ID::SystemSmaller,
                     td::ColorID::DarkGray);
        }

        for (std::size_t point = 1; point < points.size(); ++point)
        {
            gui::Shape::drawLine(
                {mapX(points[point - 1].risk), mapY(points[point - 1].expectedReturn)},
                {mapX(points[point].risk), mapY(points[point].expectedReturn)},
                td::ColorID::RoyalBlue, 3.0F);
        }
        const std::size_t markerStep = std::max<std::size_t>(1, points.size() / 30);
        for (std::size_t point = 0; point < points.size(); ++point)
        {
            if (point % markerStep != 0 && point + 1 != points.size())
                continue;

            const double x = mapX(points[point].risk);
            const double y = mapY(points[point].expectedReturn);
            gui::Shape::drawRect({x - 2.25, y - 2.25, x + 2.25, y + 2.25},
                                 td::ColorID::RoyalBlue);
        }

        for (std::size_t asset = 0; asset < _state.data().assetCount(); ++asset)
        {
            const double x = mapX(_state.statistics().standardDeviations[asset]);
            const double y = mapY(_state.statistics().meanReturns[asset]);
            gui::Shape::drawRect({x - 4.0, y - 4.0, x + 4.0, y + 4.0},
                                 td::ColorID::DarkOrange);

            const std::string& assetName = _state.data().assetNames[asset];
            gui::Size labelSize;
            gui::DrawableString::measure(assetName.c_str(),
                                         gui::Font::ID::SystemSmaller,
                                         labelSize);
            double labelX = x + 7.0;
            if (labelX + labelSize.width > plotRight - 4.0)
                labelX = x - labelSize.width - 7.0;

            labelX = std::clamp(labelX,
                                plotLeft + 4.0,
                                std::max(plotLeft + 4.0,
                                         plotRight - labelSize.width - 4.0));
            const double labelY = std::clamp(y - 8.0,
                                             plotTop + 3.0,
                                             plotBottom - labelSize.height - 3.0);
            drawText(assetName, labelX, labelY,
                     gui::Font::ID::SystemSmaller,
                     td::ColorID::SysText);
        }

        if (showSelectedPortfolio)
        {
            drawCrossMarker(mapX(_state.solution().risk),
                            mapY(_state.solution().expectedReturn),
                            td::ColorID::Green, 8.0, 3.0F);
        }

        drawLineLegend(405.0, 29.0, td::ColorID::RoyalBlue,
                       tr("frontierLegend").c_str());
        drawSquareLegend(590.0, 29.0, td::ColorID::DarkOrange,
                         tr("individualAssetsLegend").c_str());
        if (showSelectedPortfolio)
        {
            drawCrossLegend(785.0, 29.0, td::ColorID::Green,
                            tr("selectedPortfolioLegend").c_str());
        }
    }

public:
    explicit EfficientFrontierCanvas(const AppState& state)
        : PortfolioChartCanvas(tr("riskAxis"), tr("returnAxis"),
                               tr("frontierPlaceholder"))
        , _state(state)
    {
    }
};

class PortfolioWeightsCanvas : public PortfolioChartCanvas
{
    const AppState& _state;

    static td::ColorID seriesColor(std::size_t index)
    {
        constexpr std::array<td::ColorID, 10> colors{
            td::ColorID::RoyalBlue, td::ColorID::Crimson,
            td::ColorID::SeaGreen, td::ColorID::DarkOrange,
            td::ColorID::Purple, td::ColorID::DarkCyan,
            td::ColorID::Brown, td::ColorID::Magenta,
            td::ColorID::DodgerBlue,
            td::ColorID::Olive};
        return colors[index % colors.size()];
    }

    void onDraw(const gui::Rect&) override
    {
        const std::size_t assetCount = _state.data().assetCount();
        const std::size_t legendRows = std::max<std::size_t>(
            1, (assetCount + 4) / 5);
        const double plotTop = defaultPlotTop
            + 18.0 * static_cast<double>(legendRows - 1);

        drawAxes(plotTop);
        if (!_state.hasFrontier() || _state.frontier().points.empty())
        {
            drawEmptyMessage(plotTop);
            return;
        }

        const auto& points = _state.frontier().points;
        const double minimumTarget = points.front().targetReturn;
        const double maximumTarget = points.back().targetReturn;
        const double targetRange = std::max(1e-12, maximumTarget - minimumTarget);
        const auto mapX = [minimumTarget, targetRange](double target)
        {
            return plotLeft + (target - minimumTarget) / targetRange *
                                  (plotRight - plotLeft);
        };
        const auto mapY = [plotTop](double weight)
        {
            return plotBottom - std::clamp(weight, 0.0, 1.0) *
                                    (plotBottom - plotTop);
        };

        constexpr int tickIntervals = 5;
        for (int tick = 0; tick <= tickIntervals; ++tick)
        {
            const double fraction = static_cast<double>(tick) / tickIntervals;
            const double x = plotLeft + fraction * (plotRight - plotLeft);
            const double y = plotBottom - fraction * (plotBottom - plotTop);
            if (tick > 0 && tick < tickIntervals)
            {
                gui::Shape::drawLine({x, plotTop}, {x, plotBottom},
                                     td::ColorID::LightGray, 0.5F);
                gui::Shape::drawLine({plotLeft, y}, {plotRight, y},
                                     td::ColorID::LightGray, 0.5F);
            }
            drawText(percentage(minimumTarget + fraction * targetRange, 2),
                     x - 22.0, plotBottom + 8.0, gui::Font::ID::SystemSmaller,
                     td::ColorID::DarkGray);
            drawText(percentage(fraction, 0), 42.0, y - 7.0,
                     gui::Font::ID::SystemSmaller, td::ColorID::DarkGray);
        }

        std::size_t activeSetChangeCount = 0;

        for (std::size_t point = 1; point < points.size(); ++point)
        {
            if (points[point].activeSet != points[point - 1].activeSet)
            {
                const double x = mapX(points[point].targetReturn);

                gui::Shape::drawLine(
                    { x, plotTop },
                    { x, plotBottom },
                    td::ColorID::DarkGray,
                    1.0F,
                    td::LinePattern::Dash);

                gui::Shape::drawRect(
                    { x - 2.5, plotBottom - 5.0,
                     x + 2.5, plotBottom },
                    td::ColorID::DarkGray);

                ++activeSetChangeCount;
            }
        }

        if (activeSetChangeCount > 0)
        {
            drawLineLegend(750.0, 17.0, td::ColorID::DarkGray,
                           tr("activeSetChangeLegend").c_str(),
                           td::LinePattern::Dash, 1.0F, 215.0);
        }

        constexpr std::size_t legendColumns = 5;
        const double legendColumnWidth =
            (plotRight - plotLeft) / static_cast<double>(legendColumns);
        for (std::size_t asset = 0; asset < assetCount; ++asset)
        {
            const td::ColorID color = seriesColor(asset);
            for (std::size_t point = 1; point < points.size(); ++point)
            {
                gui::Shape::drawLine(
                    {mapX(points[point - 1].targetReturn),
                     mapY(points[point - 1].weights[asset])},
                    {mapX(points[point].targetReturn),
                     mapY(points[point].weights[asset])},
                    color, 2.5F);
            }

            const double legendX = plotLeft + legendColumnWidth *
                static_cast<double>(asset % legendColumns);
            const double legendY = 47.0 + 18.0 *
                static_cast<double>(asset / legendColumns);
            drawLineLegend(legendX, legendY, color,
                           _state.data().assetNames[asset],
                           td::LinePattern::Solid, 3.0F,
                           legendColumnWidth - 7.0);
        }
    }

public:
    explicit PortfolioWeightsCanvas(const AppState& state)
        : PortfolioChartCanvas(tr("targetReturnAxis"), tr("weightAxis"),
                               tr("weightsPlaceholder"))
        , _state(state)
    {
    }
};


class PortfolioAllocationCanvas : public PortfolioChartCanvas
{
    const AppState& _state;

    void onDraw(const gui::Rect&) override
    {
        constexpr double contentTop = 88.0;
        drawAxes(contentTop);

        if (!_state.hasSolution() || !_state.solution().converged)
        {
            drawEmptyMessage(contentTop);
            return;
        }

        const auto& solution = _state.solution();
        const auto& names = _state.data().assetNames;
        const std::size_t assetCount =
            std::min(names.size(), solution.weights.size());

        if (assetCount == 0)
        {
            drawEmptyMessage(contentTop);
            return;
        }

        const double availableHeight = plotBottom - contentTop - 18.0;
        const double rowHeight = availableHeight /
            static_cast<double>(assetCount);
        const double barLeft = 315.0;
        const double barRight = 850.0;
        const double valueLeft = 862.0;

        for (std::size_t asset = 0; asset < assetCount; ++asset)
        {
            const double weight =
                std::clamp(solution.weights[asset], 0.0, 1.0);
            const double centerY =
                contentTop + (static_cast<double>(asset) + 0.5) * rowHeight;
            const double halfBarHeight =
                std::clamp(0.28 * rowHeight, 5.0, 14.0);

            drawTextInRect(names[asset],
                           plotLeft + 12.0,
                           centerY - 15.0,
                           barLeft - 18.0,
                           centerY + 15.0,
                           gui::Font::ID::SystemNormal,
                           td::ColorID::SysText);

            gui::Shape::drawRect(
                {barLeft, centerY - halfBarHeight,
                 barRight, centerY + halfBarHeight},
                td::ColorID::SysBackAlt2);

            if (weight > 0.0)
            {
                gui::Shape::drawRect(
                    {barLeft, centerY - halfBarHeight,
                     barLeft + weight * (barRight - barLeft),
                     centerY + halfBarHeight},
                    td::ColorID::RoyalBlue);
            }

            drawTextInRect(percentage(weight, 2),
                           valueLeft,
                           centerY - 15.0,
                           plotRight - 8.0,
                           centerY + 15.0,
                           gui::Font::ID::SystemNormal,
                           td::ColorID::SysText);
        }

        drawTextInRect(tr("allocationHint").c_str(),
                       405.0, 22.0, 960.0, 54.0,
                       gui::Font::ID::SystemSmaller,
                       td::ColorID::DarkGray);
    }

public:
    explicit PortfolioAllocationCanvas(const AppState& state)
        : PortfolioChartCanvas(tr("allocationAxis"),
                               tr("allocationTitle"),
                               tr("allocationPlaceholder"))
        , _state(state)
    {
    }
};
