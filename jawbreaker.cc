#include <algorithm>
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

#include "board.h"

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

// Per-move best (maximum) rollout score. A move is evaluated by the highest
// final score found among all random rollouts that start with it. Max (not
// mean) is deliberate: after the root move, rollouts play randomly, so the
// average measures "how good is this move under random continuation" and washes
// out the high-value, low-probability lines. The game's winning strategy
// (hoard one color, clear it in one large group last) depends on finding such a
// best-achievable line, which is exactly what the maximum captures.
struct MoveStats {
    map<int, int64_t> best_scores;

    void record(const int move, const int64_t score) {
        auto it = best_scores.find(move);
        if (it == best_scores.end() || score > it->second) {
            best_scores[move] = score;
        }
    }

    void merge(const MoveStats& other) {
        for (const auto& kv : other.best_scores) {
            record(kv.first, kv.second);
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
            out.record(move, board.score);
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
    for (const auto& entry : total.best_scores) {
        const int move = entry.first;
        const int64_t best = entry.second;
        stats.push_back({move, best});
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
