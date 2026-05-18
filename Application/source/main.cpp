#include "Application.hpp"

// Force a single FS session regardless of how the app is launched.
// libnx defines this as a weak symbol defaulting to 3; overriding it here
// to 1 ensures the forwarder-style limit is always applied, whether the app
// is opened via the forwarder or launched directly from the homebrew menu.
u32 __nx_fs_num_sessions = 1;

int main() {
    Main::Application * app = new Main::Application();
    app->run();
    delete app;
    return 0;
}