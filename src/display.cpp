#include "display.h"
#include "leaderboard.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sys/ioctl.h>
#include <unistd.h>
#include <chrono>

using namespace std;

// ANSI Colors
const string COLOR_RESET = "\033[0m";
const string COLOR_WHITE = "\033[97m";
const string COLOR_RED   = "\033[91m";
const string COLOR_GREY  = "\033[90m"; // Dim grey for empty slots
const string COLOR_BLUE  = "\033[38;5;33m"; // Vibrant blue for selection & title
const string COLOR_GREEN = "\033[92m"; // Vibrant green for completed foundation border & win message

const int GAME_WIDTH = 68; // 7 columns * 8 + 6 gaps * 2 = 68

// Rounded Box-Drawing Title matching card geometry: "CASCADE"
static const vector<string> BANNER = {
    R"(╭─────╮ ╭─────╮ ╭─────╮ ╭─────╮ ╭─────╮ ╭──────╮ ╭─────╮)",
    R"(│ ╭───╯ │ ╭─╮ │ │ ╭───╯ │ ╭───╯ │ ╭─╮ │ │ ╭──╮ │ │ ╭───╯)",
    R"(│ │     │ ╰─╯ │ │ ╰───╮ │ │     │ ╰─╯ │ │ │  │ │ │ ╰───╮)",
    R"(│ │     │ ╭─╮ │ ╰───╮ │ │ │     │ ╭─╮ │ │ │  │ │ │ ╭───╯)",
    R"(│ ╰───╮ │ │ │ │ ╭───╯ │ │ ╰───╮ │ │ │ │ │ ╰──╯ │ │ ╰───╮)",
    R"(╰─────╯ ╰─╯ ╰─╯ ╰─────╯ ╰─────╯ ╰─╯ ╰─╯ ╰──────╯ ╰─────╯)"
};

static int get_terminal_width() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return w.ws_col;
    }
    return 80; // Fallback standard terminal width
}

static int get_terminal_height() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_row > 0) {
        return w.ws_row;
    }
    return 24; // Fallback standard terminal height
}

static string get_dashes(int label_width) {
    int dashes_count = 6 - label_width;
    string dashes = "";
    for (int i = 0; i < dashes_count; ++i) {
        dashes += "─";
    }
    return dashes;
}

static string get_card_top(const Card& card, bool green_border = false) {
    string border_color = green_border ? COLOR_GREEN : (card.selected ? COLOR_BLUE : COLOR_WHITE);

    if (!card.faceUp) {
        return border_color + "╭──────╮" + COLOR_RESET;
    }

    string rank = card.getValueDisplay();
    string suit = card.getSuitDisplay();
    string label = rank + suit;
    int label_width = (card.value == 10) ? 3 : 2;
    string dashes = get_dashes(label_width);
    string text_color = card.isRed() ? COLOR_RED : COLOR_WHITE;

    return border_color + "╭" + text_color + label + border_color + dashes + "╮" + COLOR_RESET;
}

static string get_card_body(const Card& card, int sub_row, bool green_border = false) {
    string border_color = green_border ? COLOR_GREEN : (card.selected ? COLOR_BLUE : COLOR_WHITE);

    if (sub_row == 0) {
        return get_card_top(card, green_border);
    } else if (sub_row >= 1 && sub_row <= 4) {
        if (!card.faceUp) {
            return border_color + "│//////│" + COLOR_RESET;
        } else {
            return border_color + "│      │" + COLOR_RESET;
        }
    } else if (sub_row == 5) {
        if (!card.faceUp) {
            return border_color + "╰──────╯" + COLOR_RESET;
        } else {
            string rank = card.getValueDisplay();
            string suit = card.getSuitDisplay();
            string label = rank + suit;
            int label_width = (card.value == 10) ? 3 : 2;
            string dashes = get_dashes(label_width);
            string text_color = card.isRed() ? COLOR_RED : COLOR_WHITE;
            return border_color + "╰" + dashes + text_color + label + border_color + "╯" + COLOR_RESET;
        }
    }
    return "        ";
}

static string get_empty_outline(int sub_row) {
    if (sub_row == 0) {
        return COLOR_GREY + "╭──────╮" + COLOR_RESET;
    } else if (sub_row >= 1 && sub_row <= 4) {
        return COLOR_GREY + "│      │" + COLOR_RESET;
    } else if (sub_row == 5) {
        return COLOR_GREY + "╰──────╯" + COLOR_RESET;
    }
    return "        ";
}

static string get_blank_slot() {
    return "        ";
}

struct ControlEntry {
    string key;
    string desc;
};

static vector<string> render_controls_box(const GameState& state, int term_width) {
    if (term_width < 40) return {};

    vector<ControlEntry> items;
    if (state.profile == PROFILE_NUMPAD) {
        items = {
            {"1-7",    "Select / Move Col"},
            {" 0 ",    "Select Waste"},
            {" . ",    "Draw Stock"},
            {" - ",    "Cancel Move"},
            {" + ",    "Expand Stack"},
            {"Enter",  "Shrink Stack"},
            {" * ",    "To Foundation"},
            {" / ",    "Auto Sweep"},
            {" P ",    "Toggle Profile"},
            {"Ctrl+A", "Abort / Reset"},
            {"Ctrl+Q", "Quit Game"}
        };
    } else {
        items = {
            {"1-7",    "Select / Move Col"},
            {" 0 ",    "Select Waste"},
            {" - ",    "Draw Stock"},
            {"Esc",    "Cancel Move"},
            {" W ",    "Expand Stack"},
            {" S ",    "Shrink Stack"},
            {" = ",    "To Foundation"},
            {" + ",    "Auto Sweep"},
            {" P ",    "Toggle Profile"},
            {"Ctrl+A", "Abort / Reset"},
            {"Ctrl+Q", "Quit Game"}
        };
    }

    int n = static_cast<int>(items.size());
    int cols = 3;
    int rows = (n + cols - 1) / cols;

    int cell_width = (term_width - 4) / cols;
    if (cell_width < 16) {
        cols = 2;
        rows = (n + cols - 1) / cols;
        cell_width = (term_width - 4) / cols;
    }
    if (cell_width < 16) return {};

    int inner_width = cell_width * cols;
    int left_pad = max(0, (term_width - inner_width - 2) / 2);
    string pad_str(left_pad, ' ');

    vector<string> lines;
    lines.reserve(rows + 2);

    string top_border = pad_str + COLOR_GREY + "╭";
    for (int i = 0; i < inner_width; ++i) top_border += "─";
    top_border += "╮" + COLOR_RESET;
    lines.push_back(top_border);

    for (int r = 0; r < rows; ++r) {
        string row_str = pad_str + COLOR_GREY + "│" + COLOR_RESET;
        for (int c = 0; c < cols; ++c) {
            int idx = c * rows + r;
            string cell_text = "";
            int cell_plain_len = 0;

            if (idx < n) {
                const auto& item = items[idx];
                string k_str = "[" + item.key + "]";
                string d_str = item.desc;
                cell_plain_len = 1 + static_cast<int>(k_str.length()) + 1 + static_cast<int>(d_str.length());

                if (cell_plain_len > cell_width) {
                    int max_d = cell_width - 1 - static_cast<int>(k_str.length()) - 1;
                    if (max_d > 0 && max_d < static_cast<int>(d_str.length())) {
                        d_str = d_str.substr(0, max_d);
                    } else if (max_d <= 0) {
                        d_str = "";
                    }
                    cell_plain_len = 1 + static_cast<int>(k_str.length()) + (d_str.empty() ? 0 : (1 + static_cast<int>(d_str.length())));
                }

                cell_text = " " + COLOR_WHITE + k_str + COLOR_RESET;
                if (!d_str.empty()) {
                    cell_text += " " + COLOR_GREY + d_str + COLOR_RESET;
                }
            }

            int spaces_needed = max(0, cell_width - cell_plain_len);
            row_str += cell_text + string(spaces_needed, ' ');
        }
        row_str += COLOR_GREY + "│" + COLOR_RESET;
        lines.push_back(row_str);
    }

    string bot_border = pad_str + COLOR_GREY + "╰";
    for (int i = 0; i < inner_width; ++i) bot_border += "─";
    bot_border += "╯" + COLOR_RESET;
    lines.push_back(bot_border);

    return lines;
}

void display_card(const Column& col, size_t card_index) {
    if (card_index < col.cards.size()) {
        const Card& card = col.cards[card_index];
        for (int i = 0; i < 6; ++i) {
            cout << get_card_body(card, i) << "\n";
        }
    }
}

void display_game(const GameState& state) {
    int term_width = get_terminal_width();
    int term_height = get_terminal_height();

    int left_margin = (term_width - GAME_WIDTH) / 2;
    if (left_margin < 0) left_margin = 0;
    string margin_str(left_margin, ' ');

    // Targeted Redraw (Cursor-Home In-Place Overwrite)
    ostringstream out;
    out << "\033[H";

    // 1st line drawn: Vertical spacing at the top
    out << "\033[K\n";

    // Profile marker details (Right side)
    string profile_label = (state.profile == PROFILE_NUMPAD) ? "Numpad" : "Standard";
    string badge_plain = "[ Profile: " + profile_label + " ]";
    string badge_colored = COLOR_GREY + "[ Profile: " + COLOR_WHITE + profile_label + COLOR_GREY + " ]" + COLOR_RESET;
    int badge_len = static_cast<int>(badge_plain.length());
    const int RIGHT_PADDING = 3;

    // --- "Cascade" Rounded Title (Centered) ---
    // 2nd through 7th lines drawn are the BANNER lines
    int banner_len = 56; // 56 columns wide
    int banner_margin = (term_width - banner_len) / 2;
    if (banner_margin < 0) banner_margin = 0;
    string banner_margin_str(banner_margin, ' ');

    // Timer & Move counter box details (Left side)
    const int STATS_WIDTH = 27;
    const int LEFT_PADDING = 3;
    bool show_stats = (banner_margin >= LEFT_PADDING + STATS_WIDTH + 1);
    bool show_leaderboard = (left_margin >= LEFT_PADDING + STATS_WIDTH + 1);

    auto lb_lines = render_leaderboard_box(load_leaderboard());

    int elapsed = state.elapsed_seconds;
    if (state.timer_running) {
        auto now = chrono::steady_clock::now();
        elapsed = static_cast<int>(
            chrono::duration_cast<chrono::seconds>(now - state.start_time).count()
        );
    }
    if (elapsed < 0) elapsed = 0;
    int mins = min(99, elapsed / 60);
    int secs = min(59, elapsed % 60);
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", mins, secs);

    char moves_str[16];
    snprintf(moves_str, sizeof(moves_str), "%-4d", max(0, min(9999, state.move_count)));

    string stats_box[3] = {
        COLOR_GREY + "╭─────────────────────────╮" + COLOR_RESET,
        COLOR_GREY + "│ " + COLOR_GREY + "Time: " + COLOR_WHITE + string(time_str) + "  " + COLOR_GREY + "Moves: " + COLOR_WHITE + string(moves_str) + COLOR_GREY + "│" + COLOR_RESET,
        COLOR_GREY + "╰─────────────────────────╯" + COLOR_RESET
    };

    for (size_t b = 0; b < BANNER.size(); ++b) {
        // Left side: Render stats box aligned at b == 1, 2, 3
        if (show_stats && b >= 1 && b <= 3) {
            out << string(LEFT_PADDING, ' ') << stats_box[b - 1];
            int gap = banner_margin - (LEFT_PADDING + STATS_WIDTH);
            if (gap > 0) {
                out << string(gap, ' ');
            }
        } else if (show_leaderboard && b >= 4 && b <= 5) {
            out << string(LEFT_PADDING, ' ') << lb_lines[b - 4];
            int gap = banner_margin - (LEFT_PADDING + STATS_WIDTH);
            if (gap > 0) {
                out << string(gap, ' ');
            }
        } else {
            out << banner_margin_str;
        }

        // Center: Title Banner
        out << COLOR_BLUE << BANNER[b] << COLOR_RESET;

        // Right side:
        // 4th line drawn (b == 2: 1 top line + 3rd banner line): display profile marker on the right
        if (b == 2) {
            int banner_end = banner_margin + banner_len;
            int badge_pad = term_width - banner_end - badge_len - RIGHT_PADDING;
            if (badge_pad > 0) {
                out << string(badge_pad, ' ') << badge_colored << string(RIGHT_PADDING, ' ');
            }
        }

        // 5th line drawn (b == 3: directly below profile marker): display "Incorrect move!" in red if active
        if (b == 3 && state.show_error) {
            int banner_end = banner_margin + banner_len;
            int badge_pad = term_width - banner_end - badge_len - RIGHT_PADDING;
            const string err_text = "Incorrect move!";
            int err_len = static_cast<int>(err_text.length());
            int offset = (badge_len - err_len) / 2;
            int err_pad = badge_pad + offset;
            if (err_pad > 0) {
                out << string(err_pad, ' ') << COLOR_RED << err_text << COLOR_RESET;
            }
        }

        out << "\033[K\n";
    }

    // Line 1 between banner and game: Blank spacing (or Leaderboard line 2)
    if (show_leaderboard && lb_lines.size() > 2) {
        int gap = left_margin - (LEFT_PADDING + STATS_WIDTH);
        out << string(LEFT_PADDING, ' ') << lb_lines[2] << string(max(0, gap), ' ');
    }
    out << "\033[K\n";

    // Line 2 between banner and game: Display waste card count above top right corner of waste card (or Leaderboard line 3)
    if (show_leaderboard && lb_lines.size() > 3) {
        int gap = left_margin - (LEFT_PADDING + STATS_WIDTH);
        out << string(LEFT_PADDING, ' ') << lb_lines[3] << string(max(0, gap), ' ');
    } else {
        out << margin_str;
    }
    out << string(8, ' ') << "  "; // Slot 0 (Stock) + 2 spaces gap
    if (!state.waste.empty()) {
        string count_str = to_string(state.waste.size());
        int pad = 8 - static_cast<int>(count_str.length());
        if (pad < 0) pad = 0;
        out << string(pad, ' ') << COLOR_WHITE << count_str << COLOR_RESET;
    } else {
        out << string(8, ' ');
    }
    out << "\033[K\n";

    // --- Top section: Stock (Col 1), Waste (Col 2), Space (Col 3), Foundations 0-3 (Cols 4-7) ---
    for (int line = 0; line < 6; ++line) {
        int lb_idx = line + 4;
        if (show_leaderboard && lb_idx < static_cast<int>(lb_lines.size())) {
            int gap = left_margin - (LEFT_PADDING + STATS_WIDTH);
            out << string(LEFT_PADDING, ' ') << lb_lines[lb_idx] << string(max(0, gap), ' ');
        } else {
            out << margin_str;
        }

        // Slot 0 (Column 1): Stock
        if (!state.stock.empty()) {
            out << get_card_body(state.stock.back(), line);
        } else {
            out << get_empty_outline(line);
        }
        out << "  ";

        // Slot 1 (Column 2): Waste
        if (!state.waste.empty()) {
            out << get_card_body(state.waste.back(), line);
        } else {
            out << get_empty_outline(line);
        }
        out << "  ";

        // Slot 2 (Column 3): Empty space
        out << get_blank_slot();
        out << "  ";

        // Slots 3..6 (Columns 4..7): Foundation stacks 0..3
        for (int f = 0; f < 4; ++f) {
            if (!state.foundation_stacks[f].empty()) {
                bool complete = state.foundation_stacks[f].is_complete();
                out << get_card_body(state.foundation_stacks[f].topCard.value(), line, complete);
            } else {
                out << get_empty_outline(line);
            }
            if (f < 3) out << "  ";
        }

        out << "\033[K\n";
    }

    // Two line space between the top stacks and bottom columns
    out << "\033[K\n\033[K\n";

    // --- Bottom section: 7 Tableau Columns or Win / Deadlock Message ---
    int max_lines = 6;
    if (state.won) {
        // Clear column area and display centered win message instead of the columns
        string msg = "You won! Press a to play again or q to quit.";
        int msg_len = static_cast<int>(msg.length());
        int mid_col = max(0, (term_width - msg_len) / 2);

        for (int line = 0; line < 6; ++line) {
            if (line == 2) {
                out << string(mid_col, ' ') << COLOR_GREEN << msg << COLOR_RESET;
            }
            out << "\033[K\n";
        }
    } else if (state.deadlocked) {
        // Clear column area and display centered deadlock message instead of the columns
        string msg = "No moves left! Press a to try again or q to quit.";
        int msg_len = static_cast<int>(msg.length());
        int mid_col = max(0, (term_width - msg_len) / 2);

        for (int line = 0; line < 6; ++line) {
            if (line == 2) {
                out << string(mid_col, ' ') << COLOR_RED << msg << COLOR_RESET;
            }
            out << "\033[K\n";
        }
    } else {
        max_lines = 0;
        for (int c = 0; c < 7; ++c) {
            int h = state.columns[c].cards.empty() ? 6 : (static_cast<int>(state.columns[c].cards.size()) + 5);
            max_lines = max(max_lines, h);
        }

        for (int line = 0; line < max_lines; ++line) {
            out << margin_str;
            for (int c = 0; c < 7; ++c) {
                if (c > 0) out << "  ";

                int n = static_cast<int>(state.columns[c].cards.size());
                if (n == 0) {
                    if (line < 6) {
                        out << get_empty_outline(line);
                    } else {
                        out << get_blank_slot();
                    }
                } else {
                    if (line < n - 1) {
                        out << get_card_top(state.columns[c].cards[line]);
                    } else if (line < n + 5) {
                        int sub_row = line - (n - 1);
                        out << get_card_body(state.columns[c].cards.back(), sub_row);
                    } else {
                        out << get_blank_slot();
                    }
                }
            }
            out << "\033[K\n";
        }
    }

    // --- Pinned Controls Box at bottom of terminal window ---
    int tableau_end_line = 17 + max_lines;
    auto box_lines = render_controls_box(state, term_width);
    int box_height = static_cast<int>(box_lines.size());
    int min_start_row = tableau_end_line + 2; // At least 1 blank line between tableau and controls
    int box_start_row = max(min_start_row, term_height - box_height + 1);

    // Clear lines between tableau end and controls box without wiping entire screen
    for (int r = tableau_end_line + 1; r < box_start_row; ++r) {
        out << "\033[" << r << ";1H\033[K";
    }

    for (size_t i = 0; i < box_lines.size(); ++i) {
        int row = box_start_row + static_cast<int>(i);
        out << "\033[" << row << ";1H" << box_lines[i] << "\033[K";
    }

    // Clear any leftover lines below controls box up to terminal height
    int box_end_row = box_start_row + static_cast<int>(box_lines.size()) - 1;
    for (int r = box_end_row + 1; r <= term_height; ++r) {
        out << "\033[" << r << ";1H\033[K";
    }

    // Single atomic write to stdout
    cout << out.str() << flush;
}

void update_timer_display(const GameState& state) {
    int term_width = get_terminal_width();
    int banner_len = 56;
    int banner_margin = (term_width - banner_len) / 2;
    if (banner_margin < 0) banner_margin = 0;

    const int STATS_WIDTH = 27;
    const int LEFT_PADDING = 3;
    bool show_stats = (banner_margin >= LEFT_PADDING + STATS_WIDTH + 1);
    if (!show_stats) return;

    int elapsed = state.elapsed_seconds;
    if (state.timer_running) {
        auto now = chrono::steady_clock::now();
        elapsed = static_cast<int>(
            chrono::duration_cast<chrono::seconds>(now - state.start_time).count()
        );
    }
    if (elapsed < 0) elapsed = 0;
    int mins = min(99, elapsed / 60);
    int secs = min(59, elapsed % 60);
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", mins, secs);

    char moves_str[16];
    snprintf(moves_str, sizeof(moves_str), "%-4d", max(0, min(9999, state.move_count)));

    string stats_line = COLOR_GREY + "│ " + COLOR_GREY + "Time: " + COLOR_WHITE + string(time_str) + "  " + COLOR_GREY + "Moves: " + COLOR_WHITE + string(moves_str) + COLOR_GREY + "│" + COLOR_RESET;

    // Row 4 is the stats content line (1 top blank line + banner line 0 + banner line 1 top border + banner line 2 stats content)
    ostringstream out;
    out << "\033[4;" << (LEFT_PADDING + 1) << "H" << stats_line;
    cout << out.str() << flush;
}
