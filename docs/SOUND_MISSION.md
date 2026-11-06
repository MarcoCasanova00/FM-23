# Sound-Design Mission (verbatim, saved by the agent)

You are an embedded-audio DSP engineer and an experimental tekno sound designer.

MISSION

Develop a coherent, playable synthesis bank for this firmware, covering:

- French mental tekno.
- Italian mental/acid/tribe tekno.
- Tribe tekno from the free-party and sound-system scene.
- Hardtekk, including punchy, rubbery, abrasive, and whistle-like kick variants.
- Supporting synthesized percussion, bass, leads, metallic voices, and controlled feedback textures.

The spelling and musical scope matter: TEKNO, not generic TECHNO.

Do not make a generic techno preset bank and rename it.
Do not default to festival hard-techno rumble, EDM supersaws, hardstyle kicks,
or psytrance bass patterns unless explicitly implementing a separate crossover.

The deliverable is original synthesis implemented through this firmware's
actual capabilities—not a collection of borrowed samples or unverified
claims about famous artists' patches.


1. ESTABLISH THE ACTUAL PLATFORM

Inspect the checked-out repository before proposing code changes.

Determine:

- Firmware/project identity and current branch.
- Target hardware, processor, and relevant DSP capabilities.
- Actual sample rate and audio block size.
- Available oscillators, synthesis engines, filters, envelopes, and effects.
- Existing modulation routing, sequencer automation, and parameter locks.
- Voice allocation and maximum simultaneous voices.
- CPU, RAM, flash, and effect-buffer constraints.
- Preset, project, and persistence formats.
- Existing native or host-side DSP tests and rendering tools.
- Existing licensing restrictions.

Do not assume that a named engine exists because a different firmware has it.
Do not infer an engine's actual capabilities from its marketing name.
Do not confuse an FM synthesis engine with the hardware model's name.

Reuse existing working implementations where suitable.

You may substantially improve or extend the synthesis architecture if needed.
You are not limited to superficial preset changes.

However, do not undertake unrelated UI, boot, storage, or hardware changes
merely because this task permits creative sound-design work.


2. EVIDENCE AND REFERENCE DISCIPLINE

Use these sources as starting points, following relevant links where useful:

Artist tutorials:
https://www.youtube.com/watch?v=eCQ0TfYJPLo
https://www.youtube.com/watch?v=Nx2D1sWQ3Ko
https://www.youtube.com/watch?v=0q8zZhTxcVI

French-language production discussion:
https://fr.audiofanzine.com/groove-machine/korg/electribe-2014/forums/t.689392,tuto-faire-un-kick-tribe-hardcore-acid-core.html
https://fr.audiofanzine.com/ableton/forums/t.810025,comment-faire-un-kick-mental-avec-ableton,p.2.html
https://fr.audiofanzine.com/groove-machine/elektron/syntakt/forums/t.821168,comment-creez-vous-vos-kicks-acid-mental.html

Tekno community discussion:
https://www.reddit.com/r/Tekno/comments/1qfbj26/how_is_this_fx_sound_made_the_distorted_synth/

Hardtekk community:
https://hardtekkhub.de/d/15-hardtekk-kicks-simple/2

Manufacturer documentation:
https://www.korg.com/us/products/dj/volca_kick/index.php
http://www.sherman.be/index.php/products
https://files.kraftmusic.com/media/ownersmanual/Sherman_Filterbank_2_Abusers_Manual.pdf

French listening references:
https://kejamackitek.bandcamp.com/album/live-mental-resistance
https://fkymusic.bandcamp.com/album/petites-voix-live-set

Italian listening references:
https://soundcloud.com/hesed-atm/hesed-liveset-at-tekno-frequencies-vol2-in-piacenza-12112016
https://giolaroyz.bandcamp.com/track/giolaroyz-x-demox731-illusion
https://kaoslog.bandcamp.com/album/hashtag23-htr-00

Maintain three explicit evidence classes:

DOCUMENTED:
A source directly describes or demonstrates the technique.

INFERRED:
A plausible mechanism suggested by a reference, but not verified as the
original artist's method.

PROPOSED:
Our own engineering or sound-design starting point.

All numerical recipes in this prompt are PROPOSED unless independently verified.

Never claim:
- An exact artist patch without direct evidence.
- An exact oscillator waveform inferred solely from a genre tag.
- That a Reddit suggestion establishes the original production method.
- That you listened to or analyzed audio you could not actually access.
- That an algorithm is a circuit-accurate hardware emulation without validation.

Treat French and Italian reference groups as separate listening sets, not
as proof of two universal national synthesis recipes.

If references disagree, preserve alternative hypotheses and test them.


3. SONIC DESIGN OBJECTIVES

Prioritize a palette containing:

- Compact, physical kicks with a clearly controlled low-frequency body.
- Elastic and rubbery kick/bass relationships.
- Tonal, nasal, hollow, wooden, metallic, and membrane-like percussion.
- Restrained as well as aggressive distortion.
- Hypnotic low leads and snake-like, flute/reed-adjacent voices.
- Acid-adjacent articulation without making every voice a 303 imitation.
- Resonant growls, squeals, and feedback-like rrrr textures.
- Sounds that remain useful when reduced to mono.
- Strong rhythmic interaction between kick, bass, percussion, and lead.

These are design targets, not claims that every track in the scene shares
all these features.

A convincing bank must contain contrasts:
dry/wet, clean/dirty, short/long, tonal/noisy, stable/unstable.

Do not maximize distortion, stereo width, resonance, and reverb everywhere.


4. REQUIRED PARAMETER SEMANTICS

Use physical units and explicit conventions.

Pitch:
- Hz, semitones, or cents.
- State whether an envelope operates in Hz or logarithmic pitch.

Envelopes:
- Define whether decay means a time constant, time to a threshold, or
  a finite segment duration.
- Do not transfer DAW knob values directly into milliseconds.

For the seed recipes below:
- tauP means the pitch-envelope exponential time constant.
- D60 means exponential amplitude decay to -60 dB.
- A and R mean attack and release durations.

PM/FM:
- Always state carrier and modulator frequencies separately.
- C:M = 1:2 means modulator frequency is twice carrier frequency.
- In the PM recipes, modulation index I is in radians.

One possible pitch-envelope model:

pitchSemitones(t) = pitchDepthSemitones * exp(-t / tauP)

frequency(t) = finalFrequency * 2^(pitchSemitones(t) / 12)

One possible amplitude-decay model:

amplitude(t) = exp(-ln(1000) * t / D60)

One possible two-operator PM model:

output = amplitudeEnvelope *
         sin(carrierPhase + indexEnvelope * sin(modulatorPhase))

Advance phases from the actual instantaneous frequencies.

Do not describe time-varying PM as mathematically identical to every form
of linear FM.

Filters:
- Specify topology where relevant.
- Q and normalized resonance are not interchangeable.
- A resonance setting of "80%" is meaningless without the engine's mapping.

Drive:
- Specify input gain, transfer function, output compensation, and order.
- Normalized drive values from different engines are not comparable.


5. SHARED SYNTHESIS BUILDING BLOCKS

Implement or reuse only the blocks justified by the platform and recipes:

- Sine and triangle oscillators.
- Band-limited saw and pulse oscillators.
- Two-operator PM/FM with independent amplitude and index envelopes.
- Noise with independent filtering and amplitude envelope.
- A short transient/impulse source.
- Resonant low-pass, band-pass, and high-pass filtering.
- A stable resonator or damped-mode generator.
- Soft saturation and an optional more abrasive shaping mode.
- DC removal where necessary.
- A small modulation matrix.
- Short filtered delay, if affordable.
- A carefully bounded feedback path, if affordable.

Useful extensions, not mandatory on inadequate hardware:

- Hard sync.
- Separate clean-sub and dirty-mid paths.
- Two resonant filters in series or parallel.
- Audio-rate filter modulation.
- A small number of independent modal resonances.
- A short modulated delay on the texture path.

Do not emulate an entire large synthesizer when a small specialized voice
achieves the result.


6. ORIGINAL SEED RECIPES

These are starting points for implementation and listening tests.
They are not guaranteed authentic presets.

Tune each recipe in the actual firmware.
Record meaningful changes and why they improved the result.

A. DRUMS

D01 — DRY TRIBE FOUNDATION

Source:
- Sine, with a small optional triangle contribution.

Starting range:
- Final pitch: 45–65 Hz.
- Pitch depth: +24 to +42 semitones.
- tauP: 4–12 ms.
- Amplitude D60: 100–220 ms.
- Very short attack, with discontinuities controlled.

Routing:
oscillator -> gentle saturation -> optional tonal cleanup -> output

Add a separate 1–4 ms filtered transient only if needed.

Macros:
BODY, PUNCH, LENGTH, GRAIN.

Acceptance:
The dry sound must work before adding delay, reverb, or master clipping.


D02 — ELASTIC MENTAL KICK

Source:
- Sine/triangle body.
- Independent resonant texture branch.

Starting range:
- Final pitch: 43–60 Hz.
- Pitch depth: +24 to +40 semitones.
- tauP: 10–22 ms.
- Body D60: 160–320 ms.

Texture branch:
- Feed the body into one or two resonant filters.
- Explore resonant bands around 250–1200 Hz.
- Give texture its own envelope and saturation.

Routing:
clean body + enveloped resonant texture -> compensated output

Optionally test a very short delay on texture only.

Macros:
BEND, RUBBER, NASAL, TAIL.

Do not obtain "rubber" merely by making the kick louder or longer.
Do not let texture processing remove the low-frequency foundation.


D03 — RESONATOR KICK

Source:
- Short excitation into a damped resonator, or a safely implemented
  self-oscillating filter voice.

Starting range:
- Resonance pitch: 45–70 Hz.
- Decaying upward pitch offset.
- Independent excitation level and body damping.

Routing:
excitation -> resonator -> drive -> tone shaping

Macros:
EXCITE, TUNE, DAMP, DRIVE.

Treat this as an original resonator design, not a claimed hardware clone.


D04 — HARDTEKK PUNCH / WHISTLE

Source:
- Short pitched body.
- Separately controllable resonant mid/high component.

Starting range:
- Body final pitch: 55–85 Hz.
- Pitch depth: +24 to +48 semitones.
- tauP: 3–10 ms.
- Body D60: 70–180 ms.
- Whistle resonance exploration: 700–2500 Hz.

Routing:
body -> drive/filter interaction
plus separately enveloped whistle component

Macros:
THUMP, BARK, WHISTLE, LENGTH.

Compare filter-before-drive and drive-before-filter.
Do not assume they are equivalent.

The whistle must be optional.
The voice must remain usable without it.


D05 — TRIBAL TOM / BONGO

Source:
- Sine or triangle.
- Optional weak secondary damped mode.

Starting range:
- Final pitch: 90–220 Hz.
- Pitch depth: +5 to +18 semitones.
- tauP: 8–25 ms.
- D60: 90–320 ms.

Macros:
SIZE, SKIN, DAMP, HIT.

Use restrained pitch/velocity variation for performance.
Keep deterministic rendering available for tests.


D06 — METALLIC FM PERCUSSION

Source:
- Two sine operators.
- Initial C:M experiments: 1:1.414, 1:2.73, 1:3.51.

Starting range:
- Carrier: 150–700 Hz.
- Initial PM index: 1–5 radians.
- Index D60: 20–100 ms.
- Amplitude D60: 40–180 ms.

Macros:
METAL, RATIO, SNAP, DECAY.

Avoid calling arbitrary inharmonic ratios "the mental tekno ratio."
Ratios are exploration choices.


D07 — SYNTHETIC SNARE

Source:
- Short tonal body plus independently enveloped noise.

Starting range:
- Body: 160–240 Hz.
- Body D60: 35–90 ms.
- Noise high-pass: approximately 1–2.5 kHz.
- Noise D60: 70–180 ms.

Routing:
body + filtered noise -> restrained saturation -> output

Macros:
BODY, WIRE, SNAP, LENGTH.

Noise must not become an undifferentiated bright wash.


D08 — HATS / METALLIC TICK

Source alternatives:
- Filtered noise.
- A small bank of inharmonically related pulse oscillators.
- A mixture of both.

Original frequency seed:
400, 562, 752, 1016, 1324, 1732 Hz.

These are experimental values, not a historical circuit transcription.

Starting range:
- High-pass exploration: 4–8 kHz.
- Closed D60: 15–65 ms.
- Open D60: 100–300 ms.

Macros:
METAL, AIR, LENGTH, DIRT.

Implement a closed/open choke relationship if the sequencer supports it.
Use band-limited sources or another justified anti-aliasing strategy.


D09 — CLAP / RIM

Provide two related but distinct modes.

Clap:
- Filtered-noise bursts around 0, 9, and 20 ms.
- Quieter tail with D60 around 60–150 ms.

Rim:
- Two short damped modes.
- Explore approximately 700 and 1800 Hz.
- D60 around 15–60 ms.
- Optional tiny noise transient.

Macros:
SPREAD, WOOD, BRIGHT, TAIL.


B. BASS

B01 — KICK-DERIVED OFFBEAT BASS

Use the kick's settled oscillator/body as the starting foundation.

Support two implementations:
- A continuous body whose amplitude reopens after the kick.
- A separately triggered bass voice with matched tuning and comparable timbre.

Do not assume these implementations produce identical phase behavior.

Starting range:
- Fundamental: 43–80 Hz.
- Bass attack: 1–8 ms.
- Decay/release fitted to the available rhythmic gap.
- Distortion and filter settings independent from the kick.

For an offbeat pattern, position the bass halfway between quarter-note kicks.
Make the timing tempo-relative, not fixed in milliseconds.

Macros:
RETURN, WEIGHT, EDGE, DUCK.

Do not repeat the kick's full pitch transient on every bass note unless
deliberately making a second percussive event.


B02 — RUBBER FM BASS

Source:
- Sine carrier and sine modulator.
- Start at C:M = 1:2.
- Compare 1:1 and 1:3 as distinct alternatives.

Starting range:
- Fundamental: 45–100 Hz.
- Initial PM index: 0.3–3 radians.
- Index D60: 60–200 ms.
- Independent amplitude articulation.

Routing:
PM voice -> resonant LP -> saturation
plus optional restrained clean sub

Macros:
CHEW, HOLLOW, BODY, LENGTH.

Retain a stable foundation as the index moves.


B03 — SAW / PULSE TRIBE BASS

Source:
- One saw or pulse oscillator.
- Optional sine one octave below.

Starting range:
- Main oscillator: 70–160 Hz.
- LP cutoff exploration: 120–1600 Hz.
- Filter-envelope excursion: 1–4 octaves.
- Envelope D60: 60–220 ms.

Macros:
BITE, OPEN, SUB, ACCENT.

Avoid unison stacks by default.
The sound should articulate clearly in mono.


B04 — ACID-ADJACENT BASS

Source:
- One saw or pulse.
- Resonant low-pass filter.
- Independent amp and filter envelopes.

Starting range:
- Filter-envelope excursion: 2–5 octaves.
- Filter decay: 80–260 ms.
- Legato glide: 30–120 ms.

Implement accent as a coordinated change to selected parameters:
amplitude, filter excitation, envelope amount, and/or drive.

Explicitly define which envelopes retrigger on legato notes.

Do not label this an accurate TB-303 model unless that is independently
implemented and verified.


C. LEADS AND MENTAL VOICES

L01 — HOLLOW LOW LEAD

Source:
- Two square/pulse oscillators one octave apart.
- Small optional detuning: approximately 3–9 cents.

Routing:
oscillator mix -> resonant low-pass -> overdrive -> low-frequency cleanup
-> filtered delay

Starting range:
- Amp attack: 1–6 ms.
- Plucked or short-gated articulation.
- Filter envelope: 1–4 octaves.
- Keep the upper oscillator quieter initially.

Macros:
HOLLOW, FRICTION, OPEN, SPACE.

Stereo variation is optional, never essential to the identity.
Validate the mono version first.


L02 — SNAKE / FLUTE-ADJACENT LEAD

Source:
- Sine/triangle foundation.
- Optional weak PM at C:M = 1:2.
- Optional very quiet filtered breath noise.

Starting range:
- PM index: 0.15–1.2 radians.
- Attack: 5–20 ms.
- Release: 30–100 ms.
- Delayed vibrato: 4–7 Hz, approximately 5–20 cents.
- Optional glide: 30–100 ms.

Routing:
source -> gentle spectral shaping -> controlled saturation -> filtered delay

Macros:
BREATH, REED, BEND, DISTANCE.

Keep at least one version relatively pure and intimate.
Do not turn every flute-like patch into a harsh FM brass sound.


L03 — NASAL FM REED

Source:
- Sine carrier.
- Start with C:M = 1:3.
- Compare 1:2 and 1:4.

Starting range:
- PM index: 0.4–2 radians.
- Index envelope independent from amp.
- Optional resonant band-pass around 700–2500 Hz.

Macros:
NOSE, WOOD, PRESSURE, DECAY.

Compare integer-ratio stability with a small ratio offset.
Use offset deliberately, not as uncontrolled randomization.


L04 — SYNC RAZOR

Use only if a sufficiently clean sync implementation exists or is justified.

Source:
- Master oscillator establishes note pitch.
- Slave oscillator begins around 2–6 times master frequency.
- Envelope or slow modulation moves slave frequency.

Routing:
sync oscillator -> resonant filter -> compensated drive

Macros:
TEAR, SWEEP, BODY, LENGTH.

If sync is unavailable, propose a cheaper alternative.
Do not silently substitute naive oscillator resets and call it validated.


L05 — MENTAL RRRR / RESONANT GROWL

Treat this as a hypothesis-driven sound family, not one settled recipe.

Implement and compare:

A:
Pitched tom -> resonant filtering -> saturation.

B:
Saw/pulse -> moving resonant band -> saturation.

C:
Short excitation -> filtered nonlinear feedback loop.

For feedback experiments:
- Start with a short delay in approximately the 2–15 ms range.
- Start with low feedback.
- Add damping and a saturating element inside the loop.
- Make excitation and loop gain independently controllable.

Macros:
GROWL, RESONANCE, INSTABILITY, DAMPING.

The voice must have bounded internal state, finite output, and an immediate
safe reset/mute path.

A limiter after an unstable feedback loop is not an adequate stability design.


L06 — ACID / FILTER SCREAM

Source:
- Saw or pulse.
- Resonant filter with envelope modulation.
- Optional audio-rate cutoff modulation.

Starting exploration:
- Modulation rate: 25–250 Hz.
- Begin with shallow modulation depth.
- Compare pre-filter and post-filter saturation.

Macros:
SCREAM, FORMANT, DRIVE, MOTION.

Keep this voice separate from the main low-frequency foundation.
Do not confuse brightness with useful aggression.


L07 — INHARMONIC BELL / ALIEN PING

Source:
- Two-operator PM.
- Start at C:M = 1:2.73.
- Compare 1:1.414 and 1:3.51.

Starting range:
- PM index: 1–4 radians.
- Index D60: 20–160 ms.
- Amplitude D60: 80–500 ms.

Macros:
GLASS, METAL, LENGTH, ECHO.

Provide a dry percussive version and a restrained delayed version.


7. PROCESSING ARCHITECTURE

Where useful, separate:

LOW FOUNDATION:
stable tuning, controlled envelope, limited stereo processing.

CHARACTER:
resonance, distortion, FM sidebands, sync, noise, or feedback texture.

SPACE:
filtered delay/reverb receiving only appropriate material.

This separation is an option, not a mandatory expensive architecture for
every voice.

Compare these signal orders explicitly:

- Oscillator -> filter -> distortion.
- Oscillator -> distortion -> filter.
- Oscillator -> filter -> distortion -> cleanup filter.
- Clean foundation mixed with a processed character branch.

Do not hardcode one order as universally correct.

When adding an effect, determine whether the improvement comes from
timbre, dynamics, stereo width, or simply increased loudness.

Loudness-match comparisons.

Avoid blanket rules such as:
- Always high-pass every kick at 40 Hz.
- Always remove 300 Hz.
- Always compress kick and bass together.
- Always add reverb.
- Always use maximum resonance.

Make cleanup depend on the actual voice and musical role.


8. RHYTHMIC AUDITION CONTEXT

This task is primarily synthesis, but sounds must be auditioned musically.

Provide short test patterns containing:

- Straight kick with offbeat bass.
- A sparse, dry mental pattern.
- A denser tribe-percussion pattern.
- A 3-step lead cycle against a 4-step kick interval.
- A hardtekk kick-focused pattern.
- A restrained acid-accent and glide test.

Use several test tempos, for example 150, 170, and 190 BPM.
These are engineering test points, not official genre boundaries.

Do not force every test into identical patterns.
Do not add a complete song-generator rewrite unless the existing architecture
requires a small integration change.


9. MACROS AND PRESET DESIGN

Expose a small number of meaningful performance controls per voice.

Good macros coordinate compatible parameters while preserving the sound's role.

Examples:
- RUBBER changes pitch-envelope shape and character-envelope timing.
- METAL changes index and modulator relationship within tested bounds.
- SCREAM changes resonance, drive, and compensating output gain.
- SPACE changes effect send and damping without flooding the sub.
- LENGTH changes amplitude decay while respecting choke behavior.

Do not map a macro to arbitrary unrelated parameters.

Each preset must document:
- Voice purpose.
- Source and oscillator topology.
- Frequency relationships.
- Envelope semantics and values.
- Filter topology and settings.
- Processing order.
- Modulation sources and destinations.
- Macro mappings.
- Gain staging.
- Expected CPU/voice cost.
- Evidence classification.
- Known limitations.

Save synthesis state and effect dependencies reliably.
A preset must not require undocumented global settings to sound correct.


10. EMBEDDED DSP REQUIREMENTS

- No dynamic allocation, blocking I/O, or logging in the audio callback.
- Avoid denormals and unbounded state.
- Smooth appropriate continuous controls.
- Preserve intentional fast drum transients.
- Do not interpolate discrete modes through invalid states.
- Define retrigger and oscillator-phase behavior.
- Handle rapid retriggers, voice stealing, and choke events.
- Use a justified anti-aliasing strategy for bright oscillators and nonlinearities.
- Benchmark oversampling before enabling it broadly.
- Keep parameter and internal feedback ranges bounded.
- Handle NaN, infinity, and runaway states.
- Protect delay-buffer bounds.
- Provide safe initialization and reset.
- Avoid hidden dependence on a particular block size.
- Keep randomized render tests reproducible using a fixed seed.

Do not assume oversampling makes every nonlinear or feedback process safe.


11. HARDWARE AND DATA SAFETY

Sound-design freedom does not permit careless platform changes.

Do not modify bootloader, partition layout, flash addresses, update protocol,
power management, or recovery mechanisms unless separately required and reviewed.

Do not automatically flash the hardware.

Before any optional flashing step:
- Identify the exact target.
- Confirm the established update and recovery process.
- Preserve a known-good firmware.
- Back up user presets/projects where supported.
- Identify changes affecting persistent data.
- Report unresolved risks.

A successful build is not proof that firmware cannot brick a device.
Do not promise zero risk.

Also protect the audio output from unexpectedly loud resonances, feedback,
DC, and abrupt full-scale transitions.


12. VALIDATION AND LISTENING

First implement a small representative set:

- D01 dry kick.
- D02 elastic kick.
- D04 hardtekk kick.
- D05 tom.
- D06 FM percussion.
- D08 hats.
- B01 kick-derived bass.
- B02 rubber FM bass.
- L01 hollow lead.
- L02 snake lead.
- L05 resonant growl.
- L07 alien ping.

Expand only after validating this set.

Where possible, render using the same DSP implementation as the firmware,
not an unrelated approximation.

For each voice, render:
- Dry isolated hits or notes.
- Several velocities.
- Several pitches where applicable.
- Fast retriggers.
- Representative macro positions.
- A short musical pattern.
- Effect-off and effect-on versions.

Check:
- Finite output.
- Peak level and headroom.
- DC.
- Unexpected sustained output after release.
- Stable reset/mute behavior.
- Clicks and discontinuities.
- Low-frequency consistency.
- Mono compatibility.
- CPU and memory use under realistic simultaneous voices.

For high resonance, maximum drive, and feedback settings, add stress tests.

Separate:
- What automated tests establish.
- What measured benchmarks establish.
- What listening establishes.
- What remains unverified.

If you cannot listen, say so.
Provide renders for human review rather than declaring the aesthetic validated.


13. DELIVERABLES

Produce:

1. A concise capability audit of this repository.
2. An evidence ledger with URLs and documented/inferred/proposed labels.
3. An implementation plan with CPU and memory trade-offs.
4. A modular, original synthesis implementation.
5. A documented preset bank.
6. Host-rendered audition audio where supported.
7. Automated DSP and persistence tests.
8. Actual benchmark results, or an explicit statement that benchmarking
   was unavailable.
9. A short human-listening checklist.
10. A change log identifying compatibility and hardware risks.

Do not invent benchmark numbers, listening results, or successful tests.

Report any missing capability before replacing it with an approximation.
Explain the approximation and its audible limitations.


DEFINITION OF DONE

The bank is not merely a set of genre names.

It contains meaningfully different oscillator/envelope/filter architectures.
Its kicks and basses interact coherently.
Its leads include hollow, flute/reed-like, nasal, metallic, and abrasive options.
Its mental textures have controlled instability rather than uncontrolled noise.
Its hardtekk voices are distinguishable from the mental/tribe foundations.
It remains useful with restrained effects and in mono.
It respects the actual embedded platform.
Its implementation, tests, and remaining uncertainties are documented.

Be adventurous in sound design and rigorous in engineering.
