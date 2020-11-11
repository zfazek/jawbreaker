const colors = [
    "white",
    "yellow",
    "magenta",
    "blue",
    "red",
    "green",
];

function draw() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    print_score();
    print_stats();
    print_shape_score();
    for (let i = 0; i < TABLE_SIZE * TABLE_SIZE; i++) {
        const x = (i % TABLE_SIZE) * 2 * CIRCLE_RADIUS + CIRCLE_RADIUS;
        const y = Math.floor(i / TABLE_SIZE) * 2 * CIRCLE_RADIUS + CIRCLE_RADIUS;
        ctx.beginPath();
        ctx.arc(x, y, CIRCLE_RADIUS, 0, 2 * Math.PI);
        ctx.fillStyle = colors[table[i]];
        ctx.fill();
    }
}

function draw_border() {
    for (let i = 0; i < neighbors.length; i++) {
        const idx = neighbors[i];
        const x = idx % TABLE_SIZE;
        const y = Math.floor(idx / TABLE_SIZE);
        ctx.beginPath();
        ctx.rect(x * CIRCLE_DIAMETER, y * CIRCLE_DIAMETER,
            CIRCLE_DIAMETER, CIRCLE_DIAMETER);
        ctx.strokeStyle = "black";
        ctx.stroke();
    }
}

function print_score() {
    document.getElementById(score_id).innerHTML = "Score: " + score;
}

function print_shape_score() {
    const n = neighbors.length;
    const shape_score = n * (n - 1);
    document.getElementById(shape_score_id).innerHTML = "Shape: " + shape_score;
}

function print_stats() {
    let stats = [];
    for (let i = 0; i < colors.length; i++) {
        stats.push(0);
    }
    for (let i = 0; i < TABLE_SIZE * TABLE_SIZE; i++) {
        const color = table[i];
        stats[color]++;
    }
    let str = "<pre>";
    for (let i = 1; i < stats.length; i++) {
        str += colors[i] + ": " + stats[i] + "<br>";
    }
    str += "</pre>";
    document.getElementById(stats_id).innerHTML = str;
}
