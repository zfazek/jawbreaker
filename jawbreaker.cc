#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "cgicc/Cgicc.h"
#include "cgicc/HTMLClasses.h"
#include "cgicc/HTTPPlainHeader.h"

using std::atoi;
using std::chrono::system_clock;
using std::chrono::time_point;
using std::chrono::milliseconds;
using std::cout;
using std::endl;
using std::map;
using std::pair;
using std::rand;
using std::set;
using std::sort;
using std::string;
using std::to_string;
using std::unordered_set;
using std::vector;

static constexpr int TABLE_WIDTH = 11;
static constexpr int TABLE_SIZE = TABLE_WIDTH * TABLE_WIDTH;
static int score;

static vector<int> table;
static set<int> neighbors;
static unordered_set<int> visited;

void set_neighbors(const int idx, const int color) {
    if (idx < 0 || idx >= TABLE_SIZE) {
        return;
    }
    if (table[idx] != color) {
        return;
    }
    if (neighbors.find(idx) != neighbors.end()) {
        return;
    }
    if (visited.find(idx) != visited.end()) {
        return;
    }
    neighbors.insert(idx);
    set_neighbors(idx - TABLE_WIDTH, color);
    if (idx % TABLE_WIDTH > 0) {
        set_neighbors(idx - 1, color);
    }
    if (idx % TABLE_WIDTH < TABLE_WIDTH - 1) {
        set_neighbors(idx + 1, color);
    }
    set_neighbors(idx + TABLE_WIDTH, color);
}

bool is_end() {
    for (int i = 0; i < TABLE_SIZE; i++) {
        const int color = table[i];
        if (color == 0) {
            continue;
        }
        neighbors.clear();
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
    const int n = neighbors.size();
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
    visited.clear();
    neighbors.clear();
    set_neighbors(idx, color);
    remove_neighbors();
}

vector<int> get_moves() {
    visited.clear();
    set<int> moves;
    for (int i = 0; i < TABLE_SIZE; i++) {
        const int color = table[i];
        if (color == 0) {
            continue;
        }
        if (visited.find(i) != visited.end()) {
            continue;
        }
        neighbors.clear();
        set_neighbors(i, color);
        if (neighbors.size() > 1) {
            const int first_idx = *neighbors.cbegin();
            moves.insert(first_idx);
            for (const int idx : neighbors) {
                visited.insert(idx);
            }
        }
    }
    vector<int> ret;
    for (const int move : moves) {
        ret.push_back(move);
    }
    return ret;
}

string get_best_move(const int score_orig) {
    srand(time(0));
    const vector<int> table_orig = table;
    map<int, int64_t> temp_stats;
    constexpr unsigned NUMBER_OF_GAMES = 10000;
    const milliseconds MAX_DURATION = milliseconds(8000);
    const time_point<system_clock> start_time = system_clock::now();
    for (unsigned i = 0; i < NUMBER_OF_GAMES; ++i) {
        table = table_orig;
        const vector<int> moves = get_moves();
        for (const int move : moves) {
            table = table_orig;
            score = score_orig;
            make_move(move);
            while (!is_end()) {
                const vector<int> moves = get_moves();
                const int random_move = rand() % moves.size();
                make_move(moves[random_move]);
            }
            if (temp_stats.find(move) != temp_stats.end()) {
                if (score > temp_stats.at(move)) {
                    temp_stats[move] = score;
                }
            } else {
                temp_stats[move] = score;
            }
        }
        const time_point<system_clock> now = system_clock::now();
        if (now - start_time > MAX_DURATION) {
            cout << "i: " << i << endl;
            break;
        }
    }
    vector<pair<int, int64_t>> stats;
    for (const auto stat : temp_stats) {
        stats.push_back(stat);
    }
    sort(stats.begin(), stats.end(),
            [=](pair<int, int64_t>& a, pair<int, int64_t>& b) {
                return a.second > b.second;
                });
    string str = "{";
    for (const auto& stat : stats) {
        str += "\"" + to_string(stat.first) + "\":" +
            to_string(stat.second) + ",";
    }
    str = str.substr(0, str.size() - 1);
    str += "}";
    return str;
}

int main(int argc, char**) {
    string input;
    if (argc > 1) {
        input = "2443124225235431444531225412222153542133454111413455241"
            "1432325552421143333313211553433352155545521115525112214"
            "55215423251";
    } else {
        cgicc::Cgicc cgi;
        cout << cgicc::HTTPPlainHeader();
        cgicc::form_iterator it_pos = cgi.getElement("table");
        if (it_pos != cgi.getElements().end()) {
            input = cgi("table");
        } else {
            return 0;
        }
    }
    if (input.size() < TABLE_SIZE) {
        return 0;
    }
    for (unsigned i = 0; i < TABLE_SIZE; ++i) {
        table.push_back(input[i] - '0');
    }
    int score = 0;
    if (input.size() > TABLE_SIZE) {
        score = atoi(input.substr(TABLE_SIZE).c_str());
    }
    if (!is_end()) {
        cout << get_best_move(score) << endl;
    }
}
