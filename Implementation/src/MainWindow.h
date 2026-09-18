#pragma once

#include "ui/MainView.h"

#include <gui/Window.h>

class MainWindow : public gui::Window
{
    MainView _mainView;

public:
    MainWindow()
        : gui::Window(gui::Size(1100, 700))
    {
        setTitle(tr("appTitle"));
        setCentralView(&_mainView);
    }
};

