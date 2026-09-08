#include "interface.h"
#include "display.h"
#include "logic.h"
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <chrono>

using namespace std;

static struct termios orig_termios;
static bool raw_mode_enabled = false;

static void disable_raw_mode() {
    if (raw_mode_enabled) {
        cout << "\033[?25h" << flush; // Restore cursor visibility
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_enabled = false;
    }
}

static void enable_raw_mode() {
    if (!raw_mode_enabled) {
        tcgetattr(STDIN_FILENO, &orig_termios);
        atexit(disable_raw_mode);

        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON);
        raw.c_iflag &= ~(IXON); // Disable Ctrl+S / Ctrl+Q software flow control
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 10; // 1-second timeout for live timer updates
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        raw_mode_enabled = true;
        cout << "\033[?25l" << flush; // Hide cursor for flicker-free rendering
    }
}

static char read_key() {
    char c = 0;
    if (read(STDIN_FILENO, &c, 1) < 0) {
        return 0;
    }
    return c;
}

bool show_start_screen(GameState& state) {
    enable_raw_mode();

    struct winsize last_ws = {0, 0, 0, 0};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &last_ws);

    display_start_screen();

    while (state.running) {
        char key = read_key();

        if (key == 0) {
            struct winsize curr_ws;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &curr_ws) == 0 &&
                (curr_ws.ws_col != last_ws.ws_col || curr_ws.ws_row != last_ws.ws_row)) {
                last_ws = curr_ws;
                display_start_screen();
            }
            continue;
        }

        if (key == 17) { // Ctrl+Q (ASCII 17): Safe quit
            state.running = false;
            return false;
        }

        tcflush(STDIN_FILENO, TCIFLUSH);
        state.start_time = chrono::steady_clock::now();
        state.elapsed_seconds = 0;
        return true;
    }

    return false;
}

void run_keyboard_listener(GameState& state) {
    enable_raw_mode();

    struct winsize last_ws = {0, 0, 0, 0};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &last_ws);

    while (state.running) {
        char key = read_key();

        if (key == 0) {
            if (state.timer_running) {
                struct winsize curr_ws;
                if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &curr_ws) == 0 &&
                    (curr_ws.ws_col != last_ws.ws_col || curr_ws.ws_row != last_ws.ws_row)) {
                    last_ws = curr_ws;
                    display_game(state);
                } else {
                    update_timer_display(state);
                }
            }
            continue;
        }

        if (state.won || state.deadlocked) {
            if (key == 'a' || key == 'A' || key == 1) {
                reset_game(state);
                display_game(state);
            } else if (key == 'q' || key == 'Q' || key == 17) {
                state.running = false;
            }
            continue;
        }

        switch (key) {
            case 1: // Ctrl+A (ASCII 1): Abort current game and reset board
                reset_game(state);
                break;

            case 17: // Ctrl+Q (ASCII 17): Safe quit
                state.running = false;
                break;

            case 'p':
            case 'P': // Switch between Standard and Numpad profile
                state.profile = (state.profile == PROFILE_STANDARD) ? PROFILE_NUMPAD : PROFILE_STANDARD;
                break;

            case '0':
                select_waste(state);
                break;

            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7': {
                int col_idx = key - '1';
                if (!state.has_selection) {
                    select_column_top(state, col_idx);
                } else {
                    int sel_col = get_selected_column(state);
                    if (sel_col == col_idx) {
                        // Tapped same column again -> deselect / cancel
                        clear_selection(state);
                    } else {
                        // Tapped a different column -> attempt to drop selection
                        if (!move_selection_to_column(state, col_idx)) {
                            // Illegal move -> show subtle error message below profile marker
                            trigger_incorrect_move(state);
                        }
                    }
                }
                break;
            }

            case 27: // Esc: Cancel selection
                clear_selection(state);
                break;

            default: {
                if (state.profile == PROFILE_STANDARD) {
                    switch (key) {
                        case '-':
                            draw_from_stock(state);
                            break;
                        case 'o':
                        case 'O':
                            expand_selection(state);
                            break;
                        case 'l':
                        case 'L':
                            shrink_selection(state);
                            break;
                        case '=':
                            if (!send_selection_to_foundation(state)) {
                                trigger_incorrect_move(state);
                            }
                            break;
                        case '+':
                        case '/':
                            auto_sweep(state);
                            break;
                        default:
                            break;
                    }
                } else { // PROFILE_NUMPAD
                    switch (key) {
                        case '.':
                            draw_from_stock(state);
                            break;
                        case '-':
                            clear_selection(state);
                            break;
                        case '+':
                            expand_selection(state);
                            break;
                        case '\n':
                        case '\r':
                            shrink_selection(state);
                            break;
                        case '*':
                            if (!send_selection_to_foundation(state)) {
                                trigger_incorrect_move(state);
                            }
                            break;
                        case '/':
                            auto_sweep(state);
                            break;
                        default:
                            break;
                    }
                }
                break;
            }
        }

        if (!state.running) {
            break;
        }

        // Reposition and reprint on key press (in-place redraw)
        display_game(state);
    }
}
