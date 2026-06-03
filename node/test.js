"use strict";

/**
 * Example usage of the kerong Node.js native addon.
 *
 * The default target is a ZNE-100TL base unit on 192.168.1.200:5000.
 * Pass --host / --port / --board / --lock to point at your hardware
 * (or a simulator).
 */

const path = require("path");
const fs = require("fs");

function findAddon() {
    if (process.env.KERONG_ADDON_PATH) return process.env.KERONG_ADDON_PATH;
    const candidates = [
        path.join(__dirname, "kerong.node"),
        path.join(__dirname, "build", "Release", "kerong.node"),
        path.join(__dirname, "build", "Debug", "kerong.node"),
        path.join(__dirname, "..", "build", "node", "kerong.node"),
        path.join(__dirname, "..", "build", "Release", "kerong.node"),
    ];
    for (const p of candidates) {
        if (fs.existsSync(p)) return p;
    }
    return candidates[0];
}

let kerong;
try {
    kerong = require(findAddon());
} catch (e) {
    console.error(
        `Could not load the kerong addon. Build it first with:\n` +
        `  cd ${path.join(__dirname)} && npm install && npm run build\n` +
        `or via the top-level CMake:\n` +
        `  cmake -S .. -B ../build -DKERONG_BUILD_NODE=ON && cmake --build ../build --target kerong_node\n` +
        `Error: ${e.message}`
    );
    process.exit(1);
}

function parseArgs(argv) {
    const out = {
        host: "192.168.1.200",
        port: 5000,
        timeout: 1000,
        board: 0,
        lock: 0,
    };
    for (let i = 2; i < argv.length; i++) {
        const a = argv[i];
        if (a === "--host") out.host = argv[++i];
        else if (a === "--port") out.port = parseInt(argv[++i], 10);
        else if (a === "--timeout") out.timeout = parseInt(argv[++i], 10);
        else if (a === "--board") out.board = parseInt(argv[++i], 10);
        else if (a === "--lock") out.lock = parseInt(argv[++i], 10);
        else if (a === "--help" || a === "-h") {
            console.log(
                "Usage: node test.js [--host IP] [--port N] [--timeout MS] " +
                "[--board N] [--lock N]"
            );
            process.exit(0);
        }
    }
    return out;
}

async function main() {
    const args = parseArgs(process.argv);
    const client = new kerong.KerongClient();

    try {
        await client.connect(args.host, args.port, args.timeout);
        console.log(`connected to ${args.host}:${args.port}`);

        await client.unlock(args.board, args.lock);
        console.log(`unlock(board=${args.board}, lock=${args.lock}) sent`);

        await new Promise((r) => setTimeout(r, 100));

        const state = await client.getState(args.board);
        console.log("board       =", state.boardAddress);
        console.log("locks_1_8   =", "0x" + state.locks_1_8.toString(16).padStart(2, "0"));
        console.log("locks_9_16  =", "0x" + state.locks_9_16.toString(16).padStart(2, "0"));
        console.log("ir_1_8      =", "0x" + state.ir_1_8.toString(16).padStart(2, "0"));
        console.log("ir_9_16     =", "0x" + state.ir_9_16.toString(16).padStart(2, "0"));

        for (let i = 0; i < 16; i++) {
            const lockNo = i + 1;
            const locked = state.isLocked(i);
            const item = state.isItemDetected(i);
            console.log(
                `  lock ${String(lockNo).padStart(2, " ")}: ` +
                `${locked ? "locked  " : "unlocked"}, item=${item ? "yes" : "no"}`
            );
        }
    } catch (e) {
        console.error("kerong error:", e && e.message ? e.message : e);
        process.exitCode = 1;
    } finally {
        client.disconnect();
        console.log("disconnected");
    }
}

main();
