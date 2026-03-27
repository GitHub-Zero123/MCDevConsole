#include <windows.h>

#include "MCDevConsole/AppWindow.h"

namespace MCDevConsole {

int Run(HINSTANCE instance, int show_command) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    AppWindow window;
    if (!window.Create(instance, show_command)) {
        return -1;
    }

    return window.MessageLoop();
}

} // namespace MCDevConsole

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    return MCDevConsole::Run(instance, show_command);
}
