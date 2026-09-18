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
    static constexpr double plotLeft = 90.0;
    static constexpr double plotRight = 940.0;
    static constexpr double plotTop = 60.0;
    static constexpr double plotBottom = 500.0;

    gui::DrawableString _xLabel;
    gui::DrawableString _yLabel;
    gui::DrawableString _emptyMessage;

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

    static std::string percentage(double value, int decimals = 2)
    {
        std::ostringstream output;
        output << std::fixed << std::setprecision(decimals) << 100.0 * value << '%';
        return output.str();
    }

    void drawAxes() const
    {
        gui::Shape::drawLine({plotLeft, plotBottom}, {plotRight, plotBottom},
                             td::ColorID::SysText, 2.0F);
        gui::Shape::drawLine({plotLeft, plotBottom}, {plotLeft, plotTop},
                             td::ColorID::SysText, 2.0F);
        _xLabel.draw({405, 545}, gui::Font::ID::SystemBold, td::ColorID::SysText);
        _yLabel.draw({20, 25}, gui::Font::ID::SystemBold, td::ColorID::SysText);
    }

    void drawEmptyMessage() const
    {
        _emptyMessage.draw({285, 270}, gui::Font::ID::SystemNormal,
                           td::ColorID::DarkGray);
    }

public:
    PortfolioChartCanvas(const td::String& xLabel, const td::String& yLabel,
                         const td::String& emptyMessage)
        : _xLabel(xLabel)
        , _yLabel(yLabel)
        , _emptyMessage(emptyMessage)
    {
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
        drawAxes();
        if (!_state.hasFrontier() || _state.frontier().points.empty())
        {
            drawEmptyMessage();
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

        for (int tick = 0; tick <= 4; ++tick)
        {
            const double fraction = static_cast<double>(tick) / 4.0;
            const double x = plotLeft + fraction * (plotRight - plotLeft);
            const double y = plotBottom - fraction * (plotBottom - plotTop);
            gui::Shape::drawLine({x, plotTop}, {x, plotBottom},
                                 td::ColorID::LightGray, 0.5F);
            gui::Shape::drawLine({plotLeft, y}, {plotRight, y},
                                 td::ColorID::LightGray, 0.5F);
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
                td::ColorID::Blue, 2.5F);
        }
        for (const auto& point : points)
        {
            const double x = mapX(point.risk);
            const double y = mapY(point.expectedReturn);
            gui::Shape::drawRect({x - 2.5, y - 2.5, x + 2.5, y + 2.5},
                                 td::ColorID::Blue);
        }

        for (std::size_t asset = 0; asset < _state.data().assetCount(); ++asset)
        {
            const double x = mapX(_state.statistics().standardDeviations[asset]);
            const double y = mapY(_state.statistics().meanReturns[asset]);
            gui::Shape::drawRect({x - 4.0, y - 4.0, x + 4.0, y + 4.0},
                                 td::ColorID::DarkOrange);
            drawText(_state.data().assetNames[asset], x + 6.0, y - 8.0,
                     gui::Font::ID::SystemSmaller, td::ColorID::DarkOrange);
        }

        drawText("Efficient frontier", 690, 22, gui::Font::ID::SystemSmaller,
                 td::ColorID::Blue);
        drawText("Individual assets", 820, 22, gui::Font::ID::SystemSmaller,
                 td::ColorID::DarkOrange);
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
            td::ColorID::Blue, td::ColorID::Red, td::ColorID::Green,
            td::ColorID::DarkOrange, td::ColorID::Magenta,
            td::ColorID::DarkCyan, td::ColorID::Purple,
            td::ColorID::Brown, td::ColorID::RoyalBlue,
            td::ColorID::Olive};
        return colors[index % colors.size()];
    }

    void onDraw(const gui::Rect&) override
    {
        drawAxes();
        if (!_state.hasFrontier() || _state.frontier().points.empty())
        {
            drawEmptyMessage();
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
        const auto mapY = [](double weight)
        {
            return plotBottom - std::clamp(weight, 0.0, 1.0) *
                                    (plotBottom - plotTop);
        };

        for (int tick = 0; tick <= 4; ++tick)
        {
            const double fraction = static_cast<double>(tick) / 4.0;
            const double x = plotLeft + fraction * (plotRight - plotLeft);
            const double y = plotBottom - fraction * (plotBottom - plotTop);
            gui::Shape::drawLine({x, plotTop}, {x, plotBottom},
                                 td::ColorID::LightGray, 0.5F);
            gui::Shape::drawLine({plotLeft, y}, {plotRight, y},
                                 td::ColorID::LightGray, 0.5F);
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
                    1.0F);

                gui::Shape::drawRect(
                    { x - 2.5, plotBottom - 5.0,
                     x + 2.5, plotBottom },
                    td::ColorID::DarkGray);

                ++activeSetChangeCount;
            }
        }

        if (activeSetChangeCount > 0)
        {
            gui::Shape::drawLine(
                { 730.0, 543.0 },
                { 730.0, 561.0 },
                td::ColorID::DarkGray,
                1.0F);

            drawText(
                "Active-set change",
                739.0,
                546.0,
                gui::Font::ID::SystemSmaller,
                td::ColorID::DarkGray);
        }

        const std::size_t assetCount = _state.data().assetCount();
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
                    color, 2.0F);
            }

            const double legendX = 100.0 + 170.0 * static_cast<double>(asset % 5);
            const double legendY = 18.0 + 18.0 * static_cast<double>(asset / 5);
            gui::Shape::drawLine({legendX, legendY + 6.0},
                                 {legendX + 18.0, legendY + 6.0}, color, 3.0F);
            drawText(_state.data().assetNames[asset], legendX + 24.0, legendY,
                     gui::Font::ID::SystemSmaller, color);
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
