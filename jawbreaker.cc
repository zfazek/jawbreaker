#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "cgicc/Cgicc.h"
#include "cgicc/HTMLClasses.h"
#include "cgicc/HTTPPlainHeader.h"

using std::array;
using std::atol;
using std::chrono::system_clock;
using std::chrono::time_point;
using std::chrono::milliseconds;
using std::cout;
using std::endl;
using std::map;
using std::mt19937;
using std::pair;
using std::sort;
using std::string;
using std::thread;
using std::to_string;
using std::uniform_int_distribution;
using std::vector;

static constexpr int TABLE_WIDTH = 11;
static constexpr int TABLE_SIZE = TABLE_WIDTH * TABLE_WIDTH;

// All mutable game state is encapsulated in Board so that each worker thread
// can own an independent copy. This is what makes the Monte Carlo rollouts
// parallelizable: there is no shared mutable state in the hot path.
struct Board {
    vector<int> table;
    int score = 0;
    // `neighbors` holds the cells of the connected group found by the most
    // recent flood fill. `in_neighbors` is a membership mask for that group,
    // and `visited` marks cells already assigned to a group during get_moves().
    // Flat fixed-size arrays avoid the per-cell allocation and pointer chasing
    // of std::set / std::unordered_set in the Monte Carlo hot loop.
    vector<int> neighbors;
    array<bool, TABLE_SIZE> in_neighbors{};
    array<bool, TABLE_SIZE> visited{};
    // Reusable work stack for the iterative flood fill (avoids per-call
    // allocation in the Monte Carlo hot loop).
    vector<int> fill_stack;

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
        remove_neighbors();
    }

    // Fills `moves` (cleared first) with one representative cell per removable
    // group. Takes a caller-owned buffer so the Monte Carlo hot loop can reuse
    // a single allocation across rollout steps instead of allocating per step.
    void get_moves_into(vector<int>& moves) {
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
        sort(moves.begin(), moves.end());
    }

    vector<int> get_moves() {
        vector<int> moves;
        get_moves_into(moves);
        return moves;
    }
};

// Per-move accumulated rollout statistics (sum of scores and rollout count),
// used to evaluate each move by its mean (expected) score.
struct MoveStats {
    map<int, int64_t> score_sums;
    map<int, int64_t> rollout_counts;

    void merge(const MoveStats& other) {
        for (const auto& kv : other.score_sums) {
            score_sums[kv.first] += kv.second;
        }
        for (const auto& kv : other.rollout_counts) {
            rollout_counts[kv.first] += kv.second;
        }
    }
};

// Runs random Monte Carlo rollouts until the shared deadline, accumulating
// per-move statistics into `out`. Each thread calls this with its own Board
// copy, its own RNG seed, and its own MoveStats, so there is no data sharing.
void run_rollouts(const Board& board_orig,
                  const int score_orig,
                  const uint32_t seed,
                  const time_point<system_clock>& start_time,
                  const milliseconds max_duration,
                  MoveStats& out) {
    mt19937 rng(seed);
    Board board = board_orig;
    const vector<int> root_moves = board.get_moves();
    vector<int> rollout_moves;  // Reused across all rollout steps.
    while (true) {
        for (const int move : root_moves) {
            board.table = board_orig.table;
            board.score = score_orig;
            board.make_move(move);
            while (!board.is_end()) {
                board.get_moves_into(rollout_moves);
                uniform_int_distribution<size_t> dist(
                        0, rollout_moves.size() - 1);
                const size_t random_move = dist(rng);
                board.make_move(rollout_moves[random_move]);
            }
            out.score_sums[move] += board.score;
            out.rollout_counts[move] += 1;
        }
        const time_point<system_clock> now = system_clock::now();
        if (now - start_time > max_duration) {
            break;
        }
    }
}

string get_best_move(const Board& board, const int score_orig) {
    const milliseconds MAX_DURATION = milliseconds(2000);
    const time_point<system_clock> start_time = system_clock::now();

    // Use (available CPUs - 1) threads by default, leaving one core free.
    unsigned hw = thread::hardware_concurrency();
    const unsigned num_threads = hw > 1 ? hw - 1 : 1;

    const uint32_t base_seed = static_cast<uint32_t>(time(0));
    vector<MoveStats> per_thread(num_threads);
    vector<thread> workers;
    workers.reserve(num_threads);
    for (unsigned t = 0; t < num_threads; ++t) {
        // Distinct seed per thread so rollouts are independent.
        const uint32_t seed = base_seed + t * 0x9E3779B9u + 1u;
        workers.emplace_back(run_rollouts, std::cref(board), score_orig, seed,
                             std::cref(start_time), MAX_DURATION,
                             std::ref(per_thread[t]));
    }
    for (thread& w : workers) {
        w.join();
    }

    // Merge all thread-local statistics.
    MoveStats total;
    for (const MoveStats& s : per_thread) {
        total.merge(s);
    }

    vector<pair<int, int64_t>> stats;
    for (const auto& sum : total.score_sums) {
        const int move = sum.first;
        const int64_t sum_score = sum.second;
        const int64_t count = total.rollout_counts.at(move);
        // Mean rollout score for this move (expected value estimate).
        const int64_t mean = count > 0 ? sum_score / count : 0;
        stats.push_back({move, mean});
    }
    sort(stats.begin(), stats.end(),
            [=](pair<int, int64_t>& a, pair<int, int64_t>& b) {
                return a.second > b.second;
                });
    string str = "{";
    bool first = true;
    for (const auto& stat : stats) {
        if (!first) {
            str += ",";
        }
        first = false;
        str += "\"" + to_string(stat.first) + "\":" +
            to_string(stat.second);
    }
    str += "}";
    return str;
}

// Parses and bound-checks a score string from untrusted input. Returns false
// if the string contains non-digits or is out of the accepted range. An empty
// string is treated as score 0.
bool parse_score(const string& s, int& out) {
    if (s.empty()) {
        out = 0;
        return true;
    }
    for (const char c : s) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    constexpr long MAX_SCORE = 1000000L;
    const long parsed = atol(s.c_str());
    if (parsed < 0 || parsed > MAX_SCORE) {
        return false;
    }
    out = static_cast<int>(parsed);
    return true;
}

int main(int argc, char**) {
    string board_str;
    string score_str;
    if (argc > 1) {
        board_str = "2443124225235431444531225412222153542133454111413455241"
            "1432325552421143333313211553433352155545521115525112214"
            "55215423251";
    } else {
        cgicc::Cgicc cgi;
        cout << cgicc::HTTPPlainHeader();
        cgicc::form_iterator it_table = cgi.getElement("table");
        if (it_table == cgi.getElements().end()) {
            return 0;
        }
        board_str = cgi("table");
        // #1: score is now a separate form field, decoupled from the board
        // string. Absent score defaults to 0.
        cgicc::form_iterator it_score = cgi.getElement("score");
        if (it_score != cgi.getElements().end()) {
            score_str = cgi("score");
        }
    }
    // #4: compare string length against the board size using a matching
    // unsigned type to avoid signed/unsigned comparison ambiguity. The board
    // string must now be exactly the board size (score is a separate field).
    const size_t table_size = static_cast<size_t>(TABLE_SIZE);
    if (board_str.size() != table_size) {
        return 0;
    }
    // #2: validate that the board cells are digits in the valid color range.
    // Color 0 means empty; playable colors are 1-5. Any out-of-range or
    // non-digit character means the request is malformed, so reject it.
    Board board;
    for (size_t i = 0; i < table_size; ++i) {
        const char c = board_str[i];
        if (c < '0' || c > '5') {
            return 0;
        }
        board.table.push_back(c - '0');
    }
    // #3: parse and bound-check the optional score from untrusted input.
    int parsed_score = 0;
    if (!parse_score(score_str, parsed_score)) {
        return 0;
    }
    if (!board.is_end()) {
        cout << get_best_move(board, parsed_score) << endl;
    }
    return 0;
}
