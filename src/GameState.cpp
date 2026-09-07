#include "GameState.h"
#include <queue>
#include <unordered_set>
#include <string>

using namespace std;

bool GameState::is_solvable() const {
    return !is_deadlocked();
}

bool GameState::is_deadlocked() const {
    if (won) return false;

    // Check if foundations are already complete
    bool all_foundations_complete = true;
    for (int f = 0; f < 4; ++f) {
        if (!foundation_stacks[f].is_complete()) {
            all_foundations_complete = false;
            break;
        }
    }
    if (all_foundations_complete) return false;

    // Helper: can a given card move to any foundation stack?
    auto can_to_foundation = [&](const Card& card) -> bool {
        for (int f = 0; f < 4; ++f) {
            if (foundation_stacks[f].empty()) {
                if (card.value == 1) return true;
            } else {
                const Card& top = foundation_stacks[f].topCard.value();
                if (card.suit == top.suit && card.value == top.value + 1) return true;
            }
        }
        return false;
    };

    // 1. Can any card currently in waste or stock move to a foundation?
    for (const auto& c : waste) {
        if (can_to_foundation(c)) return false;
    }
    for (const auto& c : stock) {
        if (can_to_foundation(c)) return false;
    }

    // Collect all cards in stock + waste pool
    vector<Card> pool;
    pool.reserve(waste.size() + stock.size());
    pool.insert(pool.end(), waste.begin(), waste.end());
    pool.insert(pool.end(), stock.begin(), stock.end());

    // 2. Setup initial tableau representation
    struct BFSState {
        vector<pair<int, char>> face_up[7];
        int hidden_count[7];
    };

    BFSState initial;
    for (int c = 0; c < 7; ++c) {
        initial.hidden_count[c] = 0;
        for (const auto& card : columns[c].cards) {
            if (!card.faceUp) {
                initial.hidden_count[c]++;
            } else {
                initial.face_up[c].push_back({card.value, card.suit});
            }
        }
    }

    // Helper to check progress conditions on any BFS state:
    // A: Any tableau column top card can move to foundation?
    // B: Any card from stock/waste pool can be placed on a tableau column?
    auto check_state_progress = [&](const BFSState& s) -> bool {
        for (int c = 0; c < 7; ++c) {
            if (s.face_up[c].empty()) {
                // Empty column: check if pool has any King
                for (const auto& pc : pool) {
                    if (pc.value == 13) return true;
                }
            } else {
                const auto& top_p = s.face_up[c].back();
                Card top_card(top_p.first, top_p.second, true);
                // Can column top move to foundation?
                if (can_to_foundation(top_card)) return true;

                // Can any pool card play on this column?
                bool top_red = top_card.isRed();
                for (const auto& pc : pool) {
                    if (pc.isRed() != top_red && pc.value == top_p.first - 1) {
                        return true;
                    }
                }
            }
        }
        return false;
    };

    if (check_state_progress(initial)) {
        return false;
    }

    // BFS Search over reachable face-up column rearrangements
    auto encode = [](const BFSState& s) -> string {
        string k;
        k.reserve(96);
        for (int c = 0; c < 7; ++c) {
            if (s.face_up[c].empty()) {
                k += "-|";
            } else {
                for (const auto& card : s.face_up[c]) {
                    k += to_string(card.first);
                    k += card.second;
                }
                k += "|";
            }
        }
        return k;
    };

    queue<BFSState> q;
    unordered_set<string> visited;

    q.push(initial);
    visited.insert(encode(initial));

    const size_t MAX_STATES = 3000;

    while (!q.empty()) {
        if (visited.size() >= MAX_STATES) {
            return false; // High mobility, not deadlocked
        }

        BFSState curr = q.front();
        q.pop();

        for (int src = 0; src < 7; ++src) {
            if (curr.face_up[src].empty()) continue;

            int n_cards = static_cast<int>(curr.face_up[src].size());
            for (int i = 0; i < n_cards; ++i) {
                const auto& base = curr.face_up[src][i];
                bool base_red = (base.second == 'H' || base.second == 'D');

                for (int dst = 0; dst < 7; ++dst) {
                    if (dst == src) continue;

                    bool can_move = false;
                    if (curr.face_up[dst].empty()) {
                        if (base.first == 13) {
                            if (i == 0 && curr.hidden_count[src] == 0) {
                                can_move = false; // Pointless empty-to-empty King move
                            } else {
                                can_move = true;
                            }
                        }
                    } else {
                        const auto& dst_top = curr.face_up[dst].back();
                        bool dst_red = (dst_top.second == 'H' || dst_top.second == 'D');
                        if (base_red != dst_red && base.first == dst_top.first - 1) {
                            can_move = true;
                        }
                    }

                    if (!can_move) continue;

                    BFSState next_s = curr;
                    for (int k = i; k < n_cards; ++k) {
                        next_s.face_up[dst].push_back(curr.face_up[src][k]);
                    }
                    next_s.face_up[src].erase(next_s.face_up[src].begin() + i, next_s.face_up[src].end());

                    // Progress C: Uncovered a hidden card
                    if (next_s.face_up[src].empty() && next_s.hidden_count[src] > 0) {
                        return false;
                    }

                    // Progress A & B
                    if (check_state_progress(next_s)) {
                        return false;
                    }

                    string key = encode(next_s);
                    if (visited.insert(key).second) {
                        q.push(next_s);
                    }
                }
            }
        }
    }

    return true; // No progress possible in any reachable state
}
