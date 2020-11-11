let canvas;
let ctx;
let score;
let shape_chosen;
let score_id;
let shape_score_id;
let stats_id;

let CIRCLE_RADIUS;
let CIRCLE_DIAMETER;

const TABLE_SIZE = 11;
let table = [];
let neighbors = [];

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
    console.log(width);
    canvas.width = width;
    canvas.height = width;
    CIRCLE_RADIUS = width / TABLE_SIZE / 2;
    CIRCLE_DIAMETER = 2 * CIRCLE_RADIUS;
    init();
}

function mouse_down(event) {
    const rect = canvas.getBoundingClientRect();
    const x = event.clientX - rect.left;
    const y = event.clientY - rect.top;
    const idx = Math.floor(y / CIRCLE_DIAMETER) * TABLE_SIZE +
        Math.floor(x / CIRCLE_DIAMETER);
    const color = table[idx];
    if (color == 0) {
        return;
    }
    neighbors = [];
    set_neighbors(idx, color);
    draw();
    if (neighbors.length == 1) {
        shape_chosen = -1;
        return;
    }
    if (shape_chosen == -1) {
        shape_chosen = idx;
        draw_border();
        return;
    }
    if (find_neighbor(shape_chosen)) {
        remove_neighbors();
        shape_chosen = -1;
        return;
    }
    shape_chosen = idx;
    draw_border();
}

function remove_neighbors() {
    for (let i = 0; i < neighbors.length; i++) {
        const idx = neighbors[i];
        table[idx] = 0;
    }
    const n = neighbors.length;
    score += n * (n - 1);
    fall();
    shift_right();
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
}

function set_neighbors(idx, color) {
    if (idx < 0 || idx >= TABLE_SIZE * TABLE_SIZE) {
        return;
    }
    if (table[idx] != color) {
        return;
    }
    if (!find_neighbor(idx)) {
        neighbors.push(idx);
    } else {
        return;
    }
    set_neighbors(idx - TABLE_SIZE, color);
    if (idx % TABLE_SIZE > 0) {
        set_neighbors(idx - 1, color);
    }
    if (idx % TABLE_SIZE < TABLE_SIZE - 1) {
        set_neighbors(idx + 1, color);
    }
    set_neighbors(idx + TABLE_SIZE, color);
}

function find_neighbor(idx) {
    for (let i = 0; i < neighbors.length; i++) {
        if (neighbors[i] == idx) {
            return true;
        }
    }
    return false;
}

function init() {
    score = 0;
    shape_score = "";
    shape_chosen = -1;
    table = [];
    neighbors = [];
    for (let i = 0; i < TABLE_SIZE * TABLE_SIZE; i++) {
        const color = Math.floor(Math.random() * (colors.length - 1) + 1);
        table.push(color);
    }
    draw();
}

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}
