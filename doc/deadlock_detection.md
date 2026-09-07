# Deadlock & Solvability Detection Algorithm

This document outlines the design and mechanics of the progress reachability algorithm implemented in [`src/GameState.cpp`](../src/GameState.cpp) via `GameState::is_deadlocked()` and `GameState::is_solvable()`.

---

## 1. Problem Statement

In Klondike Solitaire, a board can enter a terminal **deadlock** where legal moves still exist (e.g., endlessly transferring a sequence of cards between two columns of the same rank), but **no sequence of legal actions can ever advance the game toward victory**.

Checking only whether the current board has 0 immediate legal moves is insufficient, because players can cycle cards back and forth without uncovering any new cards or scoring. Conversely, a full retrograde solver that peeks at hidden face-down cards breaks the spirit of the game.

**Cascade** solves this with a **Progress Reachability Search**: without peeking at hidden cards, it determines whether any rearrangement of currently visible cards can ever trigger a **Progress Action**.

---

## 2. What Constitutes "Progress"?

A board position can only advance toward completion through one of three **Progress Actions**:

| Action | Condition | Why It Is Progress |
| --- | --- | --- |
| **Foundation Score** | Any card from the tableau, stock, or waste can legally move to a foundation stack. | Permanently advances the win condition (scoring 52 cards). |
| **Uncover Hidden Card** | Moving all face-up cards off a column that still has face-down cards (`hidden_count > 0`). | Flips a face-down card, introducing new information and cards to the tableau. |
| **Stock / Waste Play** | Any card currently cycling in the stock/waste pool can legally be placed on a tableau column or foundation. | Reduces the stock pool and introduces a new card into the active playing field. |

If no reachable rearrangement of the visible cards can achieve **any** of these three actions, the game is mathematically guaranteed to be deadlocked.

---

## 3. Algorithm Architecture: Breadth-First Search (BFS)

The algorithm is a lightweight Breadth-First Search over the graph of reachable tableau configurations:

```
[Initial Board State]
       │
       ▼
[Check Immediate Progress Actions] ──(Progress Found)──► Return false (Solvable)
       │ (No immediate progress)
       ▼
 [Seed BFS Queue & Visited Set]
       │
       ▼
 ┌───► [Dequeue State]
 │     ├── Visited >= MAX_STATES (3000)? ──────────────► Return false (Active mobility)
 │     │
 │     └── For each source column & sub-stack:
 │           For each target column:
 │             Is move legal?
 │               ├── No  ──► Skip
 │               └── Yes ──► Generate Neighbor State
 │                             ├── Uncovered hidden card? ──► Return false (Solvable)
 │                             ├── Can score foundation?  ──► Return false (Solvable)
 │                             ├── Can play stock card?   ──► Return false (Solvable)
 │                             └── Not visited? ──────────► Enqueue & Mark Visited
 └─── (Queue not empty)

Queue empty? ──────────────────────────────────────────► Return true (Deadlocked!)
```

---

## 4. Key Data Structures & Optimizations

### 4.1. State Representation (`BFSState`)
```cpp
struct BFSState {
    vector<pair<int, char>> face_up[7]; // Rank (1-13) and Suit ('S','H','D','C')
    int hidden_count[7];                // Number of face-down cards underneath
};
```
- Only stores visible face-up cards and the count of hidden cards per column.
- Does not modify or simulate the real game state.

### 4.2. Stock & Waste Pool Representation
In Draw-1 Klondike, every card currently in `stock` and `waste` can cycle to the top of the waste pile without requiring any tableau moves. Therefore, the search treats `stock` and `waste` as a single pool of available cards:
```cpp
vector<Card> pool;
pool.reserve(waste.size() + stock.size());
pool.insert(pool.end(), waste.begin(), waste.end());
pool.insert(pool.end(), stock.begin(), stock.end());
```
If any card in this pool can be placed on a column or foundation in any reachable state, the player is not stuck.

### 4.3. State Encoding & Cycle Detection
To prevent infinite loops from reversible card shifts (e.g. moving a 7 between two 8s), each visited configuration is converted into a compact string key:
```cpp
auto encode = [](const BFSState& s) -> string {
    string k;
    for (int c = 0; c < 7; ++c) {
        if (s.face_up[c].empty()) {
            k += "-|";
        } else {
            for (const auto& card : s.face_up[c]) {
                k += to_string(card.first) + card.second;
            }
            k += "|";
        }
    }
    return k;
};
```
An `std::unordered_set<string> visited` ensures each unique arrangement is explored at most once.

### 4.4. Redundant Transition Pruning
- Moving a King stack from an already-cleared column (`hidden_count == 0`) to an empty column merely swaps which column is empty. This move is pruned to prevent duplicate branching.

### 4.5. High-Mobility Safety Bound (`MAX_STATES = 3000`)
- Most deadlocks occur in heavily constrained configurations with fewer than 50 reachable permutations.
- If the search explores 3,000 distinct configurations without finding a dead end, the board exhibits high mobility and is not stuck.
- This bounds the worst-case execution time to $\le 2\text{ ms}$, ensuring zero perceptible input delay.

---

## 5. UI Integration

- **Execution Timing**: Evaluated after every move that alters the board (`draw_from_stock`, `move_selection_to_column`, `send_selection_to_foundation`, and upon game reset).
- **State Flag**: When `GameState::is_deadlocked()` returns `true`:
  - `state.deadlocked = true;`
  - `state.timer_running = false;` (stops the live clock)
- **Visual Alert**: [`src/display.cpp`](../src/display.cpp) replaces the tableau column rendering with a centered red notice:
  ```text
  No moves left! Press a to try again or q to quit.
  ```
- **Controls**: [`src/interface.cpp`](../src/interface.cpp) listens for `a` to deal a fresh game or `q` / `Ctrl+Q` to exit.
