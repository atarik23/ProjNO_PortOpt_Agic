#include "Application.h"

#include <gui/WinMain.h>

int main(int argc, const char* argv[])
{
    Application application(argc, argv);
    application.init("EN");
    return application.run();
}

