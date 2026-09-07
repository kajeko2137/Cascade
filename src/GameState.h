#ifndef GAMESTATE_H
#define GAMESTATE_H

#include <vector>
#include <optional>
#include <string>
#include <chrono>

using namespace std;

enum InputProfile {
    PROFILE_STANDARD,
    PROFILE_NUMPAD
};

class Card {
public:
    int value;
    char suit;
    bool selected;
    bool faceUp;

    Card(int value, char suit, bool faceUp = false)
        : value(value), suit(suit), selected(false), faceUp(faceUp) {}

    char getSuit() const {
        return suit;
    }

    string getSuitDisplay() const {
        switch (suit) {
            case 'S': return "♠";
            case 'H': return "♥";
            case 'D': return "♦";
            case 'C': return "♣";
            default:  return "?";
        }
    }

    string getValueDisplay() const {
        switch (value) {
            case 1:  return "A";
            case 10: return "10";
            case 11: return "J";
            case 12: return "Q";
            case 13: return "K";
            default: return to_string(value);
        }
    }

    bool isRed() const {
        return suit == 'H' || suit == 'D';
    }
};

class Column {
public:
    vector<Card> cards;
};

class Foundation {
public:
    optional<Card> topCard;

    bool empty() const {
        return !topCard.has_value();
    }

    bool is_complete() const {
        return topCard.has_value() && topCard->value == 13;
    }
};

struct GameState {
    vector<Card> stock;
    vector<Card> waste;
    Foundation foundation_stacks[4];
    Column columns[7];
    bool has_selection = false;
    bool running = true;
    InputProfile profile = PROFILE_STANDARD;
    bool show_error = false;
    bool won = false;
    bool win_recorded = false;
    bool deadlocked = false;
    int move_count = 0;
    int elapsed_seconds = 0;
    bool timer_running = true;
    chrono::steady_clock::time_point start_time;

    bool is_deadlocked() const;
    bool is_solvable() const;
};

#endif // GAMESTATE_H
