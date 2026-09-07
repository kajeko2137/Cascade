#include "leaderboard.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <algorithm>

using namespace std;

// ANSI Colors matching display.cpp
static const string COLOR_RESET = "\033[0m";
static const string COLOR_WHITE = "\033[97m";
static const string COLOR_GREY  = "\033[90m";
static const string COLOR_BLUE  = "\033[38;5;33m";
static const string COLOR_GREEN = "\033[92m";

const string LEADERBOARD_FILE = "leaderboard.txt";

vector<LeaderboardRecord> load_leaderboard() {
    vector<LeaderboardRecord> records;
    ifstream in(LEADERBOARD_FILE);
    if (!in.is_open()) {
        // Failsafe: Create the file if it does not exist yet
        ofstream create_file(LEADERBOARD_FILE);
        return records;
    }

    string line;
    while (getline(in, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        string date, time_val, sep, mov_label;
        int moves = 0;
        if (ss >> date >> time_val >> sep >> mov_label >> moves) {
            records.push_back({date, time_val, moves});
        }
    }
    in.close();

    // Ensure sorted ascending by moves
    stable_sort(records.begin(), records.end(), [](const LeaderboardRecord& a, const LeaderboardRecord& b) {
        return a.moves < b.moves;
    });

    return records;
}

void add_leaderboard_entry(int moves) {
    auto records = load_leaderboard();

    // Get current local date and time
    time_t now = time(nullptr);
    tm tm_val;
    localtime_r(&now, &tm_val);

    char date_buf[32];
    strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &tm_val);

    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_val);

    LeaderboardRecord new_rec = { string(date_buf), string(time_buf), moves };

    // Insert below entries with lower move count and above entries with higher move count
    auto it = records.begin();
    while (it != records.end() && it->moves <= moves) {
        ++it;
    }
    records.insert(it, new_rec);

    // Save back to leaderboard.txt (ofstream creates the file if missing)
    ofstream out(LEADERBOARD_FILE);
    if (out.is_open()) {
        for (const auto& r : records) {
            out << r.date << " " << r.time << " | Moves: " << r.moves << "\n";
        }
        out.close();
    }
}

vector<string> render_leaderboard_box(const vector<LeaderboardRecord>& records) {
    vector<string> lines;
    lines.reserve(8);

    // Line 0: Top border (27 visual width: 1 + 6 + 1 + 11 + 1 + 6 + 1 = 27)
    lines.push_back(COLOR_GREY + "╭────── " + COLOR_BLUE + "LEADERBOARD" + COLOR_GREY + " ──────╮" + COLOR_RESET);

    // Line 1: Header line (27 visual width: 2 + 23 + 2 = 27)
    lines.push_back(COLOR_GREY + "│ " + COLOR_GREY + "#  Date      Time   Mov" + COLOR_GREY + " │" + COLOR_RESET);

    // Lines 2..6: Top 5 entries
    const int MAX_DISPLAY = 5;
    if (records.empty()) {
        lines.push_back(COLOR_GREY + "│ " + COLOR_WHITE + "   No records yet      " + COLOR_GREY + " │" + COLOR_RESET);
        for (int i = 1; i < MAX_DISPLAY; ++i) {
            lines.push_back(COLOR_GREY + "│" + string(25, ' ') + "│" + COLOR_RESET);
        }
    } else {
        for (int i = 0; i < MAX_DISPLAY; ++i) {
            if (i < static_cast<int>(records.size())) {
                const auto& r = records[i];
                string rank = to_string(i + 1) + ".";
                string time_short = (r.time.length() >= 5) ? r.time.substr(0, 5) : r.time;

                char buf[32];
                snprintf(buf, sizeof(buf), "%-2s %-10s %-5s%4d",
                         rank.c_str(),
                         r.date.c_str(),
                         time_short.c_str(),
                         r.moves);

                string color = (i == 0) ? COLOR_GREEN : COLOR_WHITE;
                lines.push_back(COLOR_GREY + "│ " + color + string(buf) + COLOR_GREY + " │" + COLOR_RESET);
            } else {
                lines.push_back(COLOR_GREY + "│" + string(25, ' ') + "│" + COLOR_RESET);
            }
        }
    }

    // Line 7: Bottom border (27 visual width: 1 + 25 + 1 = 27)
    lines.push_back(COLOR_GREY + "╰─────────────────────────╯" + COLOR_RESET);

    return lines;
}
