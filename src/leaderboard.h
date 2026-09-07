#ifndef LEADERBOARD_H
#define LEADERBOARD_H

#include <string>
#include <vector>

using namespace std;

struct LeaderboardRecord {
    string date;
    string time;
    int moves;
};

vector<LeaderboardRecord> load_leaderboard();
void add_leaderboard_entry(int moves);
vector<string> render_leaderboard_box(const vector<LeaderboardRecord>& records);

#endif // LEADERBOARD_H
