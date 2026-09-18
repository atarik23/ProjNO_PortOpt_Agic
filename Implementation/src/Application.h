#pragma once

#include "MainWindow.h"

#include <gui/Application.h>

class Application : public gui::Application
{
protected:
    gui::Window* createInitialWindow() override
    {
        return new MainWindow();
    }

public:
    Application(int argc, const char** argv)
        : gui::Application(argc, argv)
    {
    }
};

