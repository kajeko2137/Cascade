#include "logic.h"
#include "display.h"
#include "leaderboard.h"
#include <iostream>
#include <unistd.h>
#include <algorithm>
#include <random>

using namespace std;

void trigger_incorrect_move(GameState& state) {
    state.show_error = true;
    display_game(state);
    cout << flush;
    usleep(1000000); // 1.0s
    state.show_error = false;
    display_game(state);
    cout << flush;
}

void clear_selection(GameState& state) {
    for (int c = 0; c < 7; ++c) {
        for (auto& card : state.columns[c].cards) {
            card.selected = false;
        }
    }
    for (auto& card : state.waste) {
        card.selected = false;
    }
    for (auto& card : state.stock) {
        card.selected = false;
    }
    state.has_selection = false;
}

void select_column_top(GameState& state, int col_idx) {
    if (col_idx < 0 || col_idx >= 7 || state.columns[col_idx].cards.empty()) {
        trigger_incorrect_move(state);
        return;
    }

    auto& cards = state.columns[col_idx].cards;
    if (cards.empty() || !cards.back().faceUp) {
        trigger_incorrect_move(state);
        return;
    }

    clear_selection(state);
    cards.back().selected = true;
    state.has_selection = true;
}

void select_waste(GameState& state) {
    if (state.waste.empty()) {
        trigger_incorrect_move(state);
        return;
    }

    clear_selection(state);
    state.waste.back().selected = true;
    state.has_selection = true;
}

int get_selected_column(const GameState& state) {
    for (int c = 0; c < 7; ++c) {
        for (const auto& card : state.columns[c].cards) {
            if (card.selected) {
                return c;
            }
        }
    }
    return -1;
}

void expand_selection(GameState& state) {
    int sel_col = get_selected_column(state);
    if (sel_col == -1) {
        trigger_incorrect_move(state);
        return;
    }

    auto& cards = state.columns[sel_col].cards;
    int first_sel = -1;
    for (size_t i = 0; i < cards.size(); ++i) {
        if (cards[i].selected) {
            first_sel = static_cast<int>(i);
            break;
        }
    }

    if (first_sel <= 0) {
        trigger_incorrect_move(state);
        return;
    }

    int prev_idx = first_sel - 1;
    if (!cards[prev_idx].faceUp) {
        trigger_incorrect_move(state);
        return;
    }

    cards[prev_idx].selected = true;
}

void shrink_selection(GameState& state) {
    int sel_col = get_selected_column(state);
    if (sel_col == -1) {
        trigger_incorrect_move(state);
        return;
    }

    auto& cards = state.columns[sel_col].cards;
    int first_sel = -1;
    int sel_count = 0;
    for (size_t i = 0; i < cards.size(); ++i) {
        if (cards[i].selected) {
            if (first_sel == -1) {
                first_sel = static_cast<int>(i);
            }
            sel_count++;
        }
    }

    if (sel_count <= 1 || first_sel == -1) {
        trigger_incorrect_move(state);
        return;
    }

    cards[first_sel].selected = false;
}

void draw_from_stock(GameState& state) {
    clear_selection(state);

    if (!state.stock.empty()) {
        Card card = state.stock.back();
        state.stock.pop_back();
        card.faceUp = true;
        card.selected = false;
        state.waste.push_back(card);
        state.move_count++;
    } else if (!state.waste.empty()) {
        // Recycle waste back into stock
        while (!state.waste.empty()) {
            Card card = state.waste.back();
            state.waste.pop_back();
            card.faceUp = false;
            card.selected = false;
            state.stock.push_back(card);
        }
        state.move_count++;
    } else {
        // Failsafe: No cards in stock or waste to draw or recycle
        trigger_incorrect_move(state);
        return;
    }

    check_deadlock(state);
}

bool can_place_on_column(const Card& card, const Column& target_col) {
    if (target_col.cards.empty()) {
        // Only a King can be placed on an empty tableau column
        return card.value == 13;
    }
    const Card& dest_card = target_col.cards.back();
    if (!dest_card.faceUp) {
        return false;
    }
    // Must be opposite color and descending rank by 1
    return (card.isRed() != dest_card.isRed()) && (card.value == dest_card.value - 1);
}

bool move_selection_to_column(GameState& state, int target_col_idx) {
    if (target_col_idx < 0 || target_col_idx >= 7) return false;

    // Check if selection originates from a column
    int sel_col = get_selected_column(state);
    if (sel_col != -1 && sel_col != target_col_idx) {
        auto& source_cards = state.columns[sel_col].cards;
        int first_sel = -1;
        for (size_t i = 0; i < source_cards.size(); ++i) {
            if (source_cards[i].selected) {
                first_sel = static_cast<int>(i);
                break;
            }
        }

        if (first_sel == -1) return false;

        const Card& moving_base = source_cards[first_sel];
        if (!can_place_on_column(moving_base, state.columns[target_col_idx])) {
            return false;
        }

        // Move cards from source column to target column
        for (size_t i = first_sel; i < source_cards.size(); ++i) {
            Card moved = source_cards[i];
            moved.selected = false;
            state.columns[target_col_idx].cards.push_back(moved);
        }
        source_cards.erase(source_cards.begin() + first_sel, source_cards.end());

        // Reveal the newly exposed top card in source column
        if (!source_cards.empty()) {
            source_cards.back().faceUp = true;
        }

        clear_selection(state);
        state.move_count++;
        check_deadlock(state);
        return true;
    }

    // Check if selection originates from waste
    if (!state.waste.empty() && state.waste.back().selected) {
        const Card& moving_card = state.waste.back();
        if (!can_place_on_column(moving_card, state.columns[target_col_idx])) {
            return false;
        }

        Card moved = moving_card;
        moved.selected = false;
        state.columns[target_col_idx].cards.push_back(moved);
        state.waste.pop_back();

        clear_selection(state);
        state.move_count++;
        check_deadlock(state);
        return true;
    }

    return false;
}

bool can_place_on_foundation(const Card& card, const Foundation& foundation) {
    if (foundation.empty()) {
        return card.value == 1; // Only an Ace can start a foundation stack
    }
    const Card& top = foundation.topCard.value();
    // Same suit and ascending value (+1)
    return (card.suit == top.suit) && (card.value == top.value + 1);
}

int find_eligible_foundation(const Card& card, const Foundation foundation_stacks[4]) {
    // Check non-empty foundations with matching suit first
    for (int i = 0; i < 4; ++i) {
        if (!foundation_stacks[i].empty() && can_place_on_foundation(card, foundation_stacks[i])) {
            return i;
        }
    }
    // If card is an Ace, place on the first empty foundation stack
    if (card.value == 1) {
        for (int i = 0; i < 4; ++i) {
            if (foundation_stacks[i].empty()) {
                return i;
            }
        }
    }
    return -1;
}

bool is_foundation_complete(const Foundation& foundation) {
    return foundation.is_complete();
}

bool check_win_condition(GameState& state) {
    for (int f = 0; f < 4; ++f) {
        if (!state.foundation_stacks[f].is_complete()) {
            state.won = false;
            return false;
        }
    }
    state.won = true;
    state.deadlocked = false;
    state.timer_running = false;
    if (!state.win_recorded) {
        add_leaderboard_entry(state.move_count);
        state.win_recorded = true;
    }
    return true;
}

bool send_selection_to_foundation(GameState& state) {
    // 1. Identify selected card (can only be a single card from column top or waste)
    int sel_col = get_selected_column(state);
    Card moving_card(1, 'S');

    if (sel_col != -1) {
        auto& col_cards = state.columns[sel_col].cards;
        if (col_cards.empty() || !col_cards.back().selected) {
            return false;
        }

        // Only single top card can be moved to foundation
        int sel_count = 0;
        for (const auto& card : col_cards) {
            if (card.selected) sel_count++;
        }
        if (sel_count != 1) {
            return false;
        }

        moving_card = col_cards.back();
    } else if (!state.waste.empty() && state.waste.back().selected) {
        moving_card = state.waste.back();
    } else {
        return false;
    }

    // 2. Find first eligible foundation slot (0..3)
    int target_slot = find_eligible_foundation(moving_card, state.foundation_stacks);
    if (target_slot == -1) {
        return false;
    }

    // 3. Move card to foundation
    Card placed = moving_card;
    placed.selected = false;
    state.foundation_stacks[target_slot].topCard = placed;

    // 4. Remove card from its source
    if (sel_col != -1) {
        auto& col_cards = state.columns[sel_col].cards;
        col_cards.pop_back();
        if (!col_cards.empty()) {
            col_cards.back().faceUp = true;
        }
    } else {
        state.waste.pop_back();
    }

    clear_selection(state);
    state.move_count++;

    // Check if foundations are complete and update win state
    if (!check_win_condition(state)) {
        check_deadlock(state);
    }

    return true;
}

void check_deadlock(GameState& state) {
    if (state.won) {
        state.deadlocked = false;
        return;
    }
    state.deadlocked = state.is_deadlocked();
    if (state.deadlocked) {
        state.timer_running = false;
    }
}

void reset_game(GameState& state) {
    state.stock.clear();
    state.waste.clear();
    for (int f = 0; f < 4; ++f) {
        state.foundation_stacks[f].topCard = nullopt;
    }
    for (int c = 0; c < 7; ++c) {
        state.columns[c].cards.clear();
    }
    state.has_selection = false;
    state.show_error = false;
    state.won = false;
    state.win_recorded = false;
    state.deadlocked = false;
    state.running = true;
    state.move_count = 0;
    state.elapsed_seconds = 0;
    state.timer_running = true;
    state.start_time = chrono::steady_clock::now();

    // Create 52 standard cards
    vector<Card> deck;
    deck.reserve(52);
    const char suits[] = {'S', 'H', 'D', 'C'};
    for (char s : suits) {
        for (int v = 1; v <= 13; ++v) {
            deck.emplace_back(v, s, false);
        }
    }

    // Shuffle the deck
    random_device rd;
    mt19937 g(rd());
    shuffle(deck.begin(), deck.end(), g);

    // Deal cards into the 7 columns (column i gets i + 1 cards)
    int deck_index = 0;
    for (int col = 0; col < 7; ++col) {
        for (int i = 0; i <= col; ++i) {
            Card card = deck[deck_index++];
            if (i == col) {
                card.faceUp = true;
            }
            state.columns[col].cards.push_back(card);
        }
    }

    // The remaining 24 cards go to the stock (all face-down)
    while (deck_index < 52) {
        state.stock.push_back(deck[deck_index++]);
    }

    check_deadlock(state);
}

GameState init_game() {
    GameState state;
    reset_game(state);
    return state;
}

bool auto_sweep_step(GameState& state) {
    // 1. Check all 7 tableau columns
    for (int c = 0; c < 7; ++c) {
        auto& col_cards = state.columns[c].cards;
        if (!col_cards.empty() && col_cards.back().faceUp) {
            const Card& card = col_cards.back();
            int target_slot = find_eligible_foundation(card, state.foundation_stacks);
            if (target_slot != -1) {
                Card placed = card;
                placed.selected = false;
                state.foundation_stacks[target_slot].topCard = placed;
                col_cards.pop_back();
                if (!col_cards.empty()) {
                    col_cards.back().faceUp = true;
                }
                state.move_count++;
                if (!check_win_condition(state)) {
                    check_deadlock(state);
                }
                return true;
            }
        }
    }

    // 2. Check waste pile
    if (!state.waste.empty() && state.waste.back().faceUp) {
        const Card& card = state.waste.back();
        int target_slot = find_eligible_foundation(card, state.foundation_stacks);
        if (target_slot != -1) {
            Card placed = card;
            placed.selected = false;
            state.foundation_stacks[target_slot].topCard = placed;
            state.waste.pop_back();
            state.move_count++;
            if (!check_win_condition(state)) {
                check_deadlock(state);
            }
            return true;
        }
    }

    return false;
}

void auto_sweep(GameState& state) {
    clear_selection(state);

    bool any_moved = false;
    while (!state.won && auto_sweep_step(state)) {
        any_moved = true;
        display_game(state);
        cout << flush;
        usleep(500000); // 0.5s delay between each move
    }

    if (!any_moved) {
        trigger_incorrect_move(state);
    }
}
