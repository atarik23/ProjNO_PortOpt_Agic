#pragma once

#include "../AppState.h"
#include "ChartCanvas.h"
#include "DataView.h"
#include "OptimizationView.h"

#include <gui/StandardTabView.h>

class MainView : public gui::StandardTabView
{
    AppState _state;
    DataView _dataView;
    OptimizationView _optimizationView;
    EfficientFrontierCanvas _frontierView;
    PortfolioWeightsCanvas _weightsView;

public:
    MainView()
        : _dataView(_state)
        , _optimizationView(_state)
        , _frontierView(_state)
        , _weightsView(_state)
    {
        _optimizationView.setOnResultsChanged([this]()
        {
            _frontierView.refresh();
            _weightsView.refresh();
        });

        addView(&_dataView, tr("dataTab"));
        addView(&_optimizationView, tr("optimizationTab"));
        addView(&_frontierView, tr("frontierTab"));
        addView(&_weightsView, tr("weightsTab"));
        setCurrentViewPos(0);
    }
};
