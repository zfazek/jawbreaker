#ifndef JAWBREAKER_BOARD_H
#define JAWBREAKER_BOARD_H

#include <algorithm>
#include <array>
#include <vector>

// Board dimensions. The playing field is a square TABLE_WIDTH x TABLE_WIDTH
// grid stored row-major in a flat vector of length TABLE_SIZE.
static constexpr int TABLE_WIDTH = 11;
static constexpr int TABLE_SIZE = TABLE_WIDTH * TABLE_WIDTH;

// All mutable game state is encapsulated in Board so that each worker thread
// can own an independent copy. This is what makes the Monte Carlo rollouts
// parallelizable: there is no shared mutable state in the hot path. Keeping the
// rules in this header also lets the test suite exercise the exact same logic
// the CGI binary uses, without pulling in main() or cgicc.
struct Board {
    std::vector<int> table;
    int score = 0;
    // `neighbors` holds the cells of the connected group found by the most
    // recent flood fill. `in_neighbors` is a membership mask for that group,
    // and `visited` marks cells already assigned to a group during get_moves().
    // Flat fixed-size arrays avoid the per-cell allocation and pointer chasing
    // of std::set / std::unordered_set in the Monte Carlo hot loop.
    std::vector<int> neighbors;
    std::array<bool, TABLE_SIZE> in_neighbors{};
    std::array<bool, TABLE_SIZE> visited{};
    // Reusable work stack for the iterative flood fill (avoids per-call
    // allocation in the Monte Carlo hot loop).
    std::vector<int> fill_stack;

    // Resets the current neighbor group (both the list and its membership mask).
    void clear_neighbors() {
        for (const int idx : neighbors) {
            in_neighbors[idx] = false;
        }
        neighbors.clear();
    }

    // Iterative stack-based flood fill. Starting from `start_idx`, collect all
    // same-colored cells connected 4-directionally into `neighbors`, skipping
    // cells already in the current group or marked visited. An explicit stack
    // avoids deep recursion (worst case 121 frames on a single-color board).
    void set_neighbors(const int start_idx, const int color) {
        fill_stack.clear();
        fill_stack.push_back(start_idx);
        while (!fill_stack.empty()) {
            const int idx = fill_stack.back();
            fill_stack.pop_back();
            if (idx < 0 || idx >= TABLE_SIZE) {
                continue;
            }
            if (table[idx] != color) {
                continue;
            }
            if (in_neighbors[idx]) {
                continue;
            }
            if (visited[idx]) {
                continue;
            }
            in_neighbors[idx] = true;
            neighbors.push_back(idx);
            fill_stack.push_back(idx - TABLE_WIDTH);
            if (idx % TABLE_WIDTH > 0) {
                fill_stack.push_back(idx - 1);
            }
            if (idx % TABLE_WIDTH < TABLE_WIDTH - 1) {
                fill_stack.push_back(idx + 1);
            }
            fill_stack.push_back(idx + TABLE_WIDTH);
        }
    }

    bool is_end() {
        for (int i = 0; i < TABLE_SIZE; i++) {
            const int color = table[i];
            if (color == 0) {
                continue;
            }
            clear_neighbors();
            set_neighbors(i, color);
            if (neighbors.size() > 1) {
                return false;
            }
        }
        return true;
    }

    void fall() {
        bool fallen = true;
        while (fallen) {
            fallen = false;
            for (int x = 0; x < TABLE_WIDTH; x++) {
                for (int y = TABLE_WIDTH - 2; y >= 0; y--) {
                    const int idx = y * TABLE_WIDTH + x;
                    if (table[idx] > 0 && table[idx + TABLE_WIDTH] == 0) {
                        const int color = table[idx];
                        table[idx] = 0;
                        table[idx + TABLE_WIDTH] = color;
                        fallen = true;
                    }
                }
            }
        }
    }

    void shift_right() {
        bool shifted = true;
        while (shifted) {
            shifted = false;
            for (int x = TABLE_WIDTH - 2; x >= 0; x--) {
                for (int y = 0; y < TABLE_WIDTH; y++) {
                    const int idx = y * TABLE_WIDTH + x;
                    if (table[idx] > 0 && table[idx + 1] == 0) {
                        const int color = table[idx];
                        table[idx] = 0;
                        table[idx + 1] = color;
                        shifted = true;
                    }
                }
            }
        }
    }

    void inc_score() {
        const int n = static_cast<int>(neighbors.size());
        score += n * (n - 1);
    }

    void remove_neighbors() {
        inc_score();
        for (const int idx : neighbors) {
            table[idx] = 0;
        }
        fall();
        shift_right();
    }

    void make_move(const int idx) {
        const int color = table[idx];
        if (color == 0) {
            return;
        }
        visited.fill(false);
        clear_neighbors();
        set_neighbors(idx, color);
        // A single isolated cell is not a removable group; removing it would be
        // a rules violation. Guarding here keeps make_move a true no-op for
        // size-1 groups, matching the JS implementation. (In normal AI use
        // get_moves only ever yields groups of 2+, so this path is not hit by
        // the Monte Carlo search, but direct callers rely on the guard.)
        if (neighbors.size() <= 1) {
            return;
        }
        remove_neighbors();
    }

    // Fills `moves` (cleared first) with one representative cell per removable
    // group. Takes a caller-owned buffer so the Monte Carlo hot loop can reuse
    // a single allocation across rollout steps instead of allocating per step.
    void get_moves_into(std::vector<int>& moves) {
        visited.fill(false);
        moves.clear();
        for (int i = 0; i < TABLE_SIZE; i++) {
            const int color = table[i];
            if (color == 0) {
                continue;
            }
            if (visited[i]) {
                continue;
            }
            clear_neighbors();
            set_neighbors(i, color);
            if (neighbors.size() > 1) {
                // Use the smallest index of the group as its representative
                // move, preserving the ordering the std::set version produced.
                int first_idx = neighbors[0];
                for (const int idx : neighbors) {
                    if (idx < first_idx) {
                        first_idx = idx;
                    }
                    visited[idx] = true;
                }
                moves.push_back(first_idx);
            }
        }
        std::sort(moves.begin(), moves.end());
    }

    std::vector<int> get_moves() {
        std::vector<int> moves;
        get_moves_into(moves);
        return moves;
    }
};

#endif  // JAWBREAKER_BOARD_H
