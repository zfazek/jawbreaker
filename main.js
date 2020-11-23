let canvas;
let ctx;
let score;
let shape_chosen;
let score_id;
let shape_score_id;
let stats_id;
let fall_finished;
let shift_finished;
let ready;
let tables_idx;

let CIRCLE_RADIUS;
let CIRCLE_DIAMETER;

const TABLE_SIZE = 11;
let table = [];
let table_orig = [];
let neighbors = [];
let scores = [];
let move_history = [];
let score_history = [];
let tables = []

let saved_tables = [];
let table_1 = [
    3,3,1,4,5,4,1,3,1,2,1,
    4,5,2,2,1,4,4,3,4,3,5,
    5,2,5,4,2,3,5,4,5,2,3,
    2,1,1,1,1,1,4,1,4,5,5,
    4,4,4,5,3,3,5,1,5,3,4,
    3,4,5,4,2,3,1,5,3,5,5,
    4,4,1,3,2,3,5,3,3,2,4,
    5,2,4,1,2,1,3,2,2,5,3,
    5,4,3,4,5,5,2,5,4,2,1,
    4,1,3,4,3,2,4,4,3,1,5,
    4,4,1,4,2,1,1,1,1,4,3,
];
let table_2 = [1,3,1,5,2,3,4,4,3,2,4,4,3,2,4,1,5,5,4,5,2,5,4,5,1,3,3,5,3,4,4,3,4,2,3,1,1,5,2,5,2,5,1,3,5,5,3,4,1,2,3,3,5,2,4,3,1,1,1,1,4,3,3,5,1,5,3,1,5,1,1,5,3,2,3,3,4,4,2,5,5,5,3,2,5,5,3,5,2,4,4,4,1,2,5,1,5,1,3,4,1,1,3,2,3,2,1,2,5,3,2,5,2,3,4,3,5,4,5,2,2,];
let table_3 = [2,2,2,1,5,2,2,3,5,5,2,3,1,2,2,1,2,3,1,3,4,2,1,4,5,5,1,5,4,5,3,4,1,5,2,3,4,5,3,3,4,3,5,1,1,3,2,4,3,5,2,2,5,3,2,1,4,5,5,5,1,4,2,5,5,3,4,4,2,2,5,1,4,5,1,2,2,3,5,2,4,2,5,3,4,3,5,4,4,1,4,1,3,2,2,1,4,4,4,4,2,3,3,1,2,1,2,1,1,5,5,1,2,1,5,4,4,3,5,5,1,];
let table_4 = [1,1,4,5,2,2,3,1,4,3,5,1,1,4,5,3,1,2,2,3,5,5,2,5,5,4,1,3,5,4,4,4,1,1,1,4,3,1,3,5,4,2,5,2,3,3,3,3,4,4,3,4,3,2,5,2,1,1,2,2,1,5,5,4,2,2,1,3,5,3,1,1,3,4,5,4,5,2,4,1,4,3,1,2,5,2,4,2,3,5,1,4,4,3,2,1,5,4,4,2,2,4,1,1,3,4,3,5,3,5,1,2,3,2,1,1,1,1,4,3,5,];
let table_5 = [2,4,4,3,1,2,4,2,2,5,2,3,5,4,3,1,4,4,4,5,3,1,2,2,5,4,1,2,2,2,2,1,5,3,5,4,2,1,3,3,4,5,4,1,1,1,4,1,3,4,5,5,2,4,1,1,4,3,2,3,2,5,5,5,2,4,2,1,1,4,3,3,3,3,3,1,3,2,1,1,5,5,3,4,3,3,3,5,2,1,5,5,5,4,5,5,2,1,1,1,5,5,2,5,1,1,2,2,1,4,5,5,2,1,5,4,2,3,2,5,1,];

function main() {
    canvas = document.getElementById("canvas");
    ctx = canvas.getContext("2d");
    canvas.addEventListener("mousedown", mouse_down);
    if (window.innerWidth > window.innerHeight) {
        score_id = "score_1";
        shape_score_id = "shape_score_1";
        stats_id = "stats_1";
    } else {
        score_id = "score_2";
        shape_score_id = "shape_score_2";
        stats_id = "stats_2";
    }
    const width = Math.min(window.innerWidth, window.innerHeight) * 0.95;
    canvas.width = width;
    canvas.height = width;
    CIRCLE_RADIUS = width / TABLE_SIZE / 2;
    CIRCLE_DIAMETER = 2 * CIRCLE_RADIUS;
    init(0);
}

function mouse_down(event) {
    const rect = canvas.getBoundingClientRect();
    const x = event.clientX - rect.left;
    const y = event.clientY - rect.top;
    const idx = Math.floor(y / CIRCLE_DIAMETER) * TABLE_SIZE +
        Math.floor(x / CIRCLE_DIAMETER);
    make_move(idx);
    draw();
}

function make_move(idx) {
    const color = table[idx];
    if (color == 0) {
        return;
    }
    neighbors = [];
    set_neighbors(idx, color);
    if (neighbors.length == 1) {
        shape_chosen = -1;
        return;
    }
    if (shape_chosen == -1) {
        shape_chosen = idx;
        return;
    }
    if (find_in_array(shape_chosen, neighbors)) {
        neighbors.sort(function(a, b){return a-b});
        move_history.push(neighbors[0]);
        console.log("Move history: ", move_history);
        tables_idx++;
        tables[tables_idx] = table.slice();
        remove_neighbors();
        shape_chosen = -1;
        return;
    }
    shape_chosen = idx;
}

function is_end() {
    for (let i = 0; i < TABLE_SIZE * TABLE_SIZE; i++) {
        const color = table[i];
        if (color == 0) {
            continue;
        }
        neighbors = [];
        set_neighbors(i, color);
        if (neighbors.length > 1) {
            return false;
        }
    }
    console.log("end");
    return true;
}

async function ai() {
    if (!is_end()) {
        while (!ready) {
            await sleep(10);
        }
        ready = false;
        while (!shift_finished) {
            await sleep(10);
        }
        let str = "";
        for (let i = 0; i < TABLE_SIZE * TABLE_SIZE; i++) {
            str += table[i];
        }
        $.ajax({
            url: "/cgi-bin/jawbreaker.cgi",
            type: "post",
            data: "table=" + str + score,
            success: async function(data) {
                if (data != "") {
                    console.log(data);
                    const json = JSON.parse(data);
                    let max = 0;
                    let max_key;
                    for (let key of Object.keys(json)) {
                        if (json[key] > max) {
                            max = json[key];
                            max_key = key;
                        }
                    }
                    const move = parseInt(max_key);
                    if (!isNaN(move)) {
                        make_move(move);
                        draw();
                        await sleep(1000);
                        make_move(move);
                        draw();
                    }
                }
                ready = true;
            }
        });
    }
}

function remove_neighbors() {
    for (let i = 0; i < neighbors.length; i++) {
        const idx = neighbors[i];
        table[idx] = 0;
    }
    const n = neighbors.length;
    score += n * (n - 1);
    score_history.push(score);
    fall();
    shift_right();
    is_end();
    neighbors = [];
}

async function fall() {
    fall_finished = false;
    let fallen = true;
    while (fallen) {
        fallen = false;
        for (let x = 0; x < TABLE_SIZE; x++) {
            for (let y = TABLE_SIZE - 2; y >= 0; y--) {
                const idx = y * TABLE_SIZE + x;
                if (table[idx] > 0 && table[idx + TABLE_SIZE] == 0) {
                    const color = table[idx];
                    table[idx] = 0;
                    table[idx + TABLE_SIZE] = color;
                    fallen = true;
                }
            }
        }
        draw();
        await sleep(100);
    }
    fall_finished = true;
}

async function shift_right() {
    shift_finished = false;
    while (!fall_finished) {
        await sleep(10);
    }
    let shifted = true;
    while (shifted) {
        shifted = false;
        for (let x = TABLE_SIZE - 2; x >= 0; x--) {
            for (let y = 0; y < TABLE_SIZE; y++) {
                const idx = y * TABLE_SIZE + x;
                if (table[idx] > 0 && table[idx + 1] == 0) {
                    const color = table[idx];
                    table[idx] = 0;
                    table[idx + 1] = color;
                    shifted = true;
                }
            }
        }
        draw();
        await sleep(100);
    }
    shift_finished = true;
}

function set_neighbors(idx, color) {
    if (idx < 0 || idx >= TABLE_SIZE * TABLE_SIZE) {
        return;
    }
    if (table[idx] != color) {
        return;
    }
    if (find_in_array(idx, neighbors)) {
        return;
    }
    neighbors.push(idx);
    set_neighbors(idx - TABLE_SIZE, color);
    if (idx % TABLE_SIZE > 0) {
        set_neighbors(idx - 1, color);
    }
    if (idx % TABLE_SIZE < TABLE_SIZE - 1) {
        set_neighbors(idx + 1, color);
    }
    set_neighbors(idx + TABLE_SIZE, color);
}

function find_in_array(idx, arr) {
    for (let i = 0; i < arr.length; i++) {
        if (arr[i] == idx) {
            return true;
        }
    }
    return false;
}

function move_back() {
    if (tables_idx >= 0) {
        table = tables[tables_idx].slice();
        tables_idx--;
        move_history.pop();
        score_history.pop();
        score = score_history[score_history.length - 1];
        shape_chosen = -1;
        draw();
    }
}

function init(idx) {
    table_orig = [];
    scores = [];
    init_variables();
    let color;
    for (let i = 0; i < TABLE_SIZE * TABLE_SIZE; i++) {
        if (idx == 0) {
            color = Math.floor(Math.random() * (colors.length - 1) + 1);
        } else {
            color = saved_tables[idx - 1][i];
        }
        table.push(color);
        table_orig.push(color);
    }
    let str = "[";
    for (let i = 0; i < TABLE_SIZE * TABLE_SIZE; i++) {
        str += table[i] + ",";
    }
    str += "]";
    console.log(str);
    draw();
}

function init_variables() {
    fall_finished = false;
    shift_finished = true;
    ready = true;
    score = 0;
    shape_chosen = -1;
    table = [];
    neighbors = [];
    saved_tables = [];
    saved_tables.push(table_1);
    saved_tables.push(table_2);
    saved_tables.push(table_3);
    saved_tables.push(table_4);
    saved_tables.push(table_5);
    move_history = [];
    score_history = [0];
    tables = [];
    tables_idx = -1;
}

function reset() {
    scores.push(score);
    console.log("Previous scores: ", scores);
    init_variables();
    table = table_orig.slice();
    draw();
}

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}
