#include <ctime>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "cgicc/Cgicc.h"
#include "cgicc/HTMLClasses.h"
#include "cgicc/HTTPPlainHeader.h"

using std::cout;
using std::endl;
using std::rand;
using std::set;
using std::string;
using std::vector;

constexpr int TABLE_SIZE = 11;
static int score;

vector<int> table;
set<int> neighbors;

void set_neighbors(const int idx, const int color) {
    if (idx < 0 || idx >= TABLE_SIZE * TABLE_SIZE) {
        return;
    }
    if (table[idx] != color) {
        return;
    }
    if (neighbors.find(idx)!= neighbors.end()) {
        return;
    }
    neighbors.insert(idx);
    set_neighbors(idx - TABLE_SIZE, color);
    if (idx % TABLE_SIZE > 0) {
        set_neighbors(idx - 1, color);
    }
    if (idx % TABLE_SIZE < TABLE_SIZE - 1) {
        set_neighbors(idx + 1, color);
    }
    set_neighbors(idx + TABLE_SIZE, color);
}

bool is_end() {
    for (int i = 0; i < TABLE_SIZE * TABLE_SIZE; i++) {
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
        for (int x = 0; x < TABLE_SIZE; x++) {
            for (int y = TABLE_SIZE - 2; y >= 0; y--) {
                const int idx = y * TABLE_SIZE + x;
                if (table[idx] > 0 && table[idx + TABLE_SIZE] == 0) {
                    const int color = table[idx];
                    table[idx] = 0;
                    table[idx + TABLE_SIZE] = color;
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
        for (int x = TABLE_SIZE - 2; x >= 0; x--) {
            for (int y = 0; y < TABLE_SIZE; y++) {
                const int idx = y * TABLE_SIZE + x;
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

void remove_neighbors() {
    const int n = neighbors.size();
    score += n * (n - 1);
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
    neighbors.clear();
    set_neighbors(idx, color);
    remove_neighbors();
}

vector<int> get_moves() {
    set<int> moves;
    for (int i = 0; i < TABLE_SIZE * TABLE_SIZE; i++) {
        const int color = table[i];
        if (color == 0) {
            continue;
        }
        neighbors.clear();
        set_neighbors(i, color);
        if (neighbors.size() > 1) {
            const int first_idx = *neighbors.cbegin();
            if (moves.find(first_idx) == moves.end()) {
                moves.insert(first_idx);
            }
        }
    }
    vector<int> ret;
    for (const int move : moves) {
        ret.push_back(move);
    }
    return ret;
}

int main(int argc, char**) {
    string input;
    if (argc == 2) {
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
    for (unsigned i = 0; i < input.size(); ++i) {
        table.push_back(input[i] - '0');
    }
    if (table.size() != 121) {
        return 0;
    }
    srand(time(0));
    score = 0;
    if (!is_end()) {
        const vector<int> moves = get_moves();
        const int move = rand() % moves.size();
        cout << moves[move] << endl;
    }
}
