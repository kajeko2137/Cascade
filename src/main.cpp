#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include "GameState.h"
#include "logic.h"
#include "display.h"
#include "windowctl.h"
#include "interface.h"

using namespace std;

bool prepare_game_window(int argc, char* argv[]) {
    if (should_spawn_window(argc, argv)) {
        open_in_new_fullscreen_window(argc, argv);
        return false;
    }

    if (getenv("CASCADE_WINDOW") != nullptr) {
        wait_for_fullscreen();
    }

    return true;
}

int main(int argc, char* argv[]) {
    if (!prepare_game_window(argc, argv)) {
        return 0;
    }

    GameState state = init_game();

    // Clear screen and draw the game centered at the final fullscreen dimensions
    cout << "\033[2J\033[H" << flush;
    display_game(state);

    // Hand off execution to the keyboard listener
    run_keyboard_listener(state);

    return 0;
}
