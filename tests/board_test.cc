// Cross-language equivalence test for the C++ game logic in board.h.
//
// Zero dependencies: a plain assertion program, wired into CTest via add_test.
// It loads the shared tests/fixtures.json (same file the JS test uses), applies
// each case's moves to a Board, and asserts the resulting score and board match
// the fixture's expected values. If the C++ and JS implementations ever drift,
// one of the two test suites fails.
//
// The fixtures file path is passed as argv[1] (CTest supplies the absolute
// path). A small hand-rolled JSON parser handles the limited subset of JSON
// used by the fixtures (objects, arrays, strings, integers) so no third-party
// JSON library is required.

#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "board.h"

namespace {

// ---- Minimal JSON parser (objects / arrays / strings / ints) -------------
struct JsonValue {
    enum Type { OBJECT, ARRAY, STRING, NUMBER } type;
    std::map<std::string, JsonValue> obj;
    std::vector<JsonValue> arr;
    std::string str;
    long num = 0;
};

struct JsonParser {
    const std::string& s;
    size_t i = 0;
    explicit JsonParser(const std::string& src) : s(src) {}

    void skip_ws() {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
            ++i;
        }
    }

    JsonValue parse() { skip_ws(); return parse_value(); }

    JsonValue parse_value() {
        skip_ws();
        const char c = s[i];
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') return parse_string();
        return parse_number();
    }

    JsonValue parse_object() {
        JsonValue v; v.type = JsonValue::OBJECT;
        ++i;  // consume '{'
        skip_ws();
        if (s[i] == '}') { ++i; return v; }
        while (true) {
            skip_ws();
            JsonValue key = parse_string();
            skip_ws();
            assert(s[i] == ':');
            ++i;
            v.obj[key.str] = parse_value();
            skip_ws();
            if (s[i] == ',') { ++i; continue; }
            assert(s[i] == '}');
            ++i;
            break;
        }
        return v;
    }

    JsonValue parse_array() {
        JsonValue v; v.type = JsonValue::ARRAY;
        ++i;  // consume '['
        skip_ws();
        if (s[i] == ']') { ++i; return v; }
        while (true) {
            v.arr.push_back(parse_value());
            skip_ws();
            if (s[i] == ',') { ++i; continue; }
            assert(s[i] == ']');
            ++i;
            break;
        }
        return v;
    }

    JsonValue parse_string() {
        JsonValue v; v.type = JsonValue::STRING;
        assert(s[i] == '"');
        ++i;
        std::string out;
        while (s[i] != '"') {
            if (s[i] == '\\') {  // minimal escape handling
                ++i;
                out.push_back(s[i]);
            } else {
                out.push_back(s[i]);
            }
            ++i;
        }
        ++i;  // consume closing quote
        v.str = out;
        return v;
    }

    JsonValue parse_number() {
        JsonValue v; v.type = JsonValue::NUMBER;
        size_t start = i;
        if (s[i] == '-') ++i;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
            ++i;
        }
        v.num = std::atol(s.substr(start, i - start).c_str());
        return v;
    }
};

std::vector<int> to_int_vector(const JsonValue& v) {
    std::vector<int> out;
    for (const JsonValue& e : v.arr) {
        out.push_back(static_cast<int>(e.num));
    }
    return out;
}

std::string board_to_string(const std::vector<int>& b) {
    std::ostringstream o;
    o << "[";
    for (size_t i = 0; i < b.size(); ++i) {
        o << b[i];
        if (i + 1 < b.size()) o << ",";
    }
    o << "]";
    return o.str();
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: board_test <fixtures.json>\n";
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "cannot open fixtures: " << argv[1] << "\n";
        return 2;
    }
    std::stringstream buf;
    buf << in.rdbuf();
    const std::string text = buf.str();

    JsonParser parser(text);
    JsonValue root = parser.parse();
    const JsonValue& cases = root.obj.at("cases");

    int passed = 0;
    int failed = 0;
    for (const JsonValue& c : cases.arr) {
        const std::string name = c.obj.at("name").str;
        const std::vector<int> board = to_int_vector(c.obj.at("board"));
        const std::vector<int> moves = to_int_vector(c.obj.at("moves"));
        const int expected_score =
            static_cast<int>(c.obj.at("expected_score").num);
        const std::vector<int> expected_board =
            to_int_vector(c.obj.at("expected_board"));

        Board b;
        b.table = board;
        b.score = 0;
        for (const int mv : moves) {
            b.make_move(mv);
        }

        bool ok = true;
        if (b.score != expected_score) {
            std::cerr << "FAIL " << name << ": score " << b.score
                      << " != expected " << expected_score << "\n";
            ok = false;
        }
        if (b.table != expected_board) {
            std::cerr << "FAIL " << name << ": board mismatch\n"
                      << "  got      " << board_to_string(b.table) << "\n"
                      << "  expected " << board_to_string(expected_board)
                      << "\n";
            ok = false;
        }
        if (ok) {
            std::cout << "PASS " << name << "\n";
            ++passed;
        } else {
            ++failed;
        }
    }

    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
