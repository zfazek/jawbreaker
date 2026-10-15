// Cross-language equivalence test for the JavaScript game logic in main.js.
//
// Zero dependencies: uses Node's built-in test runner (node:test), assert, and
// the vm module. Run with:  node --test tests/game.test.mjs
//
// main.js is a plain browser script (not an ES module): its game functions
// operate on top-level `let` globals (table, neighbors, score, ...) and some
// functions touch browser-only globals (document, canvas) or are async with
// animation delays (fall/shift_right call draw() and await sleep()). To test
// the real main.js code without a browser we load its source into a vm
// sandbox and stub out draw()/sleep() and the browser globals.
//
// Important: top-level `let`/`const` in a vm script are lexical bindings, not
// properties of the sandbox object, so we drive and inspect state by running
// small snippets *inside* the same context (which can see those bindings)
// rather than by reading/writing sandbox properties.

import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import vm from "node:vm";

const __dirname = dirname(fileURLToPath(import.meta.url));
const ROOT = join(__dirname, "..");

const fixtures = JSON.parse(
    readFileSync(join(__dirname, "fixtures.json"), "utf8"));

// Loads main.js into a fresh sandbox with stubs for rendering, timing, and
// browser globals, and returns the vm context.
function loadGame() {
    const sandbox = {
        console: { log() {}, error() {} },
        draw() {},                               // rendering no-op
        sleep() { return Promise.resolve(); },   // immediate, no real delay
        document: undefined,
        window: undefined,
        Math,
        Promise,
        setTimeout,
        __state: null,   // channel for passing the initial board in
    };
    sandbox.globalThis = sandbox;
    const ctx = vm.createContext(sandbox);
    const src = readFileSync(join(ROOT, "main.js"), "utf8");
    vm.runInContext(src, ctx, { filename: "main.js" });
    return ctx;
}

// Applies one move inside the context, mirroring C++ Board::make_move: flood
// fill the group at idx and, if it is a removable group of 2+, remove it
// (which also falls and shifts). Returns a promise (remove_neighbors is async).
const APPLY_MOVE = `
(async (idx) => {
    const color = table[idx];
    if (color === 0) { return; }
    neighbors = [];
    set_neighbors(idx, color);
    if (neighbors.length <= 1) { return; }
    await remove_neighbors();
})
`;

for (const fixture of fixtures.cases) {
    test(`JS logic: ${fixture.name}`, async () => {
        const ctx = loadGame();
        ctx.__state = fixture.board.slice();
        // Initialize the game globals (lexical bindings) via in-context code.
        vm.runInContext(
            "table = __state.slice(); score = 0; neighbors = [];" +
            " score_history = [0]; fall_finished = false; shift_finished = true;",
            ctx);
        const applyMove = vm.runInContext(APPLY_MOVE, ctx);
        for (const move of fixture.moves) {
            await applyMove(move);
        }
        const score = vm.runInContext("score", ctx);
        const table = vm.runInContext("table.slice()", ctx);

        assert.equal(score, fixture.expected_score,
            `score mismatch for ${fixture.name}`);
        assert.deepEqual(table, fixture.expected_board,
            `board mismatch for ${fixture.name}`);
    });
}
