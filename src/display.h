#ifndef DISPLAY_H
#define DISPLAY_H

#include "GameState.h"
#include <cstddef>
#include <vector>

using namespace std;

void display_card(const Column& col, size_t card_index);
void display_game(const GameState& state);
void update_timer_display(const GameState& state);

#endif // DISPLAY_H
