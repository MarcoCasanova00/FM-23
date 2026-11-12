// Inspect the raw lead waveform sample-by-sample, and test bass mode for real.
import { readFileSync } from "node:fs";
const WASI = { wasi_snapshot_preview1: { fd_write() { return 0; } } };
const { instance } = await WebAssembly.instantiate(readFileSync(process.argv[2] || "build/emu/emu.wasm"), WASI);
const ex = instance.exports;
ex._initialize(); ex.web_boot();
const N = 128;
const run = (ms) => { for (let i = 0; i < (ms / 1000) * 44100; i += N) ex.web_render(N); };

ex.web_enc(1, 2); run(300);           // DRUMS(0) -> LEAD(2)
console.log("kb mode:", ex.web_kb_mode());

ex.web_keys(1 << 18); run(120);        // lead note on
const L = new Float32Array(ex.memory.buffer, ex.web_out_l(), N);
ex.web_render(N);
const s = Array.from(L.slice(0, 32), v => v.toFixed(3));
console.log("lead first 32 samples:", s.join(" "));
run(400);
ex.web_keys(0); run(250);
console.log("after release (250ms):", ex.web_kb_mode());

// bass: from LEAD(2): +1 -> DRUMS(0), +1 -> BASS(1)
ex.web_enc(1, 2); run(300);
console.log("kb mode now:", ex.web_kb_mode());
ex.web_keys(1 << 8); run(300);
ex.web_render(N);
const B = new Float32Array(ex.memory.buffer, ex.web_out_l(), N);
const b = Array.from(B.slice(0, 32), v => v.toFixed(3));
console.log("bass first 32 samples:", b.join(" "));
ex.web_keys(0); run(400);
console.log("bass after release (400ms)");
