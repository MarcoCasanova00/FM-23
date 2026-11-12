// Reproduce the "continuous screeching on key press" report.
// 1) Boot. 2) Switch keyboard instrument to LEAD (ALGO twice). 3) press/release a
// lead key, render several seconds, measure whether audio sustains after release.
// Also test bass mode and a held key + knob activity.
import { readFileSync } from "node:fs";
const WASI = { wasi_snapshot_preview1: { fd_write() { return 0; } } };
const { instance } = await WebAssembly.instantiate(readFileSync(process.argv[2] || "build/emu/emu.wasm"), WASI);
const ex = instance.exports;
ex._initialize();
ex.web_boot();

const N = 128;
const run = (ms) => { for (let i = 0; i < (ms / 1000) * 44100; i += N) ex.web_render(N); };
const meas = (ms) => {
  const L = new Float32Array(ex.memory.buffer, ex.web_out_l(), N);
  let rms = 0, n = 0, peak = 0, zcr = 0, prev = 0, nan = 0;
  for (let i = 0; i < (ms / 1000) * 44100; i += N) {
    ex.web_render(N);
    for (let j = 0; j < N; j++) {
      const v = L[j];
      if (!Number.isFinite(v)) nan++;
      const a = Math.abs(v); if (a > peak) peak = a;
      rms += v * v; n++;
      if ((prev >= 0) !== (v >= 0)) zcr++;
      prev = v;
    }
  }
  return { rms: Math.sqrt(rms / n), peak, zcr: zcr / n, nan };
};
const kb = () => ex.web_kb_mode();

run(300);
console.log("post-boot silent:", JSON.stringify(meas(600)), "kb mode", kb());

// cycle ALGO (EN_ALGO=1) twice on HOME to reach LEAD
ex.web_enc(1, 1); run(200);
console.log("after ALGO 1: kb mode", kb());
ex.web_enc(1, 1); run(200);
console.log("after ALGO 2: kb mode", kb());

// press lead key 18 for 300ms, release
ex.web_keys(1 << 18); run(300);
console.log("\nwhile held:", JSON.stringify(meas(500)));
ex.web_keys(0); run(200);
console.log("0.2s after release:", JSON.stringify(meas(300)));
console.log("1.0s after release:", JSON.stringify(meas(1000)));
console.log("2.0s after release:", JSON.stringify(meas(1000)));

// now a quick tap (100ms), pedal style
ex.web_keys(1 << 18); run(100); ex.web_keys(0); run(300);
console.log("\ntap: 0.3s after:", JSON.stringify(meas(600)));

// bass mode: ALGO once more -> BASS (1), press bass key 8
ex.web_enc(1, 3); run(200);   // 2->0->1 cycles; 3 steps from lead 2 -> drums0->bass1? (2+3)%3=2? use explicit: (2+1)%3=0, +1=1 => 2 steps
console.log("kb mode now", kb());
ex.web_keys(1 << 8); run(300);
console.log("bass key held:", JSON.stringify(meas(400)));
ex.web_keys(0); run(300);
console.log("bass 0.5s after release:", JSON.stringify(meas(500)));
