#ifndef LOGIC_H
#define LOGIC_H

#include "GameState.h"

using namespace std;

void trigger_incorrect_move(GameState& state);
void clear_selection(GameState& state);
void select_column_top(GameState& state, int col_idx);
void select_waste(GameState& state);
int get_selected_column(const GameState& state);
void expand_selection(GameState& state);
void shrink_selection(GameState& state);
void draw_from_stock(GameState& state);
bool can_place_on_column(const Card& card, const Column& target_col);
bool move_selection_to_column(GameState& state, int target_col_idx);
bool can_place_on_foundation(const Card& card, const Foundation& foundation);
int find_eligible_foundation(const Card& card, const Foundation foundation_stacks[4]);
bool send_selection_to_foundation(GameState& state);
bool auto_sweep_step(GameState& state);
void auto_sweep(GameState& state);
bool is_foundation_complete(const Foundation& foundation);
bool check_win_condition(GameState& state);
void check_deadlock(GameState& state);
void reset_game(GameState& state);
GameState init_game();

#endif // LOGIC_H
