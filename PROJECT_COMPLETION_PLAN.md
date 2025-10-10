# Kraken Completion Plan

## 1. Current Prototype Snapshot
- The repository currently builds a console demo (`kraken_cli`) and an optional JUCE-based UI (`kraken_cli --juce`) that instantiate the AI assistant, a metadata-only mixer/timeline, and display controller/MIDI diagnostics. The JUCE runtime synthesizes placeholder oscillators but does not render real clip audio, so both modes remain non-performant proofs of concept.
- Core subsystems such as clip loading, streaming, controller IO, and AI feedback loops are implemented as lightweight stubs intended for demonstration, not production.

## 2. Audio Asset Ingestion & Streaming
- `ClipLoader.cpp` still contains unresolved duplicate code paths, repeated `#define MINIMP3_IMPLEMENTATION` blocks, and inconsistent helper implementations that will not compile or run reliably. This file needs to be split into clean translation units for ZIP, WAV-directory, and single-file sources, with one clear `ClipLoader::load` entry point.
- Build-time feature toggles (`KRAKEN_ENABLE_ZIP`, `KRAKEN_ENABLE_MP3`, libsndfile/minizip/minimp3 detection) must map to compile-time macros so the loader knows which formats are actually available. CMake should set `KRAKEN_HAS_*` definitions and gate optional code.
- The loader should expose asynchronous or buffered streaming so large stems are not fully resident in memory. Right now, `Clip::prepare` always loads everything eagerly and `Clip::release` simply drops the buffer. Implement background streaming jobs, disk caches, and predictable chunk sizes wired into the engine.
- ZIP bundle handling is stubbed—there is no extraction to temp storage, error recovery, or cleanup. Implement secure extraction (size limits, path sanitization) and caching for frequently accessed stems.
- MP3 decoding relies on `minimp3` but there is no platform abstraction for hardware decoders. Provide compile-time/runtime selection that prefers OS codecs when available and falls back to bundled decoders otherwise.

## 3. Sequencer Timeline & Mixer Engine
- The `Mixer` class only stores channel names/settings without computing gains, panning, send routing, or summing audio buffers. Flesh out DSP processing, bus architecture, effect send returns, and thread-safe parameter automation.
- `Timeline::forEachActiveClip` simply iterates scheduled clips based on beat math. It needs transport state (playhead, looping, quantization rules), concurrency controls, and integration with the audio render thread. Implement clip state machines (pre-roll, playing, release), crossfades, and macro effect envelopes.
- Develop a real audio render callback that consumes active clips, pulls streaming chunks via `ClipStream`, applies `EffectChain` processing, and mixes results into the platform’s audio device buffers.
- The `EffectChain` structure stores metadata but never processes audio. Add runtime effect modules (filters, delays, granular, etc.), macro mapping, modulation sources, and automation curves.

## 4. Controller & Human Interface Layer
- `ControllerManager` only relays synthetic messages from stub controller classes. Implement actual hardware backends: MIDI (WinMM/DirectMusic, CoreMIDI, ALSA), HID, OSC, and custom device APIs. Provide hotplugging, LED feedback, and latency compensation.
- Build a mapping layer from physical controls to engine actions, including shift layers, deck focus, macro triggers, and per-artist profiles. Persist mappings and expose editing UI.
- Integrate motion capture/gesture inputs described in the vision document (VR controllers, body tracking) via plugins to feed AI conditioning signals.

## 5. Generative AI & Recommendation Systems
- `AIAssistant` currently generates pseudo-random recommendations from a synthetic library. Replace this with real model integration: sequence tagging, reinforcement from live edits, and dataset-backed retrieval. Define model serving endpoints (local GPU, remote service) and serialization of prompts/conditioning signals.
- Implement feedback ingestion (accept/reject signals) into a persistent store to adapt recommendations across sessions. Capture timeline context, crowd telemetry, and controller gestures for conditioning.
- Design guardrails for live inference latency, fallback behaviours, and user-overrides to ensure safe stage performance.

## 6. Platform Abstraction & Packaging
- `Platform` just formats configuration strings. Implement actual audio device backends (WASAPI/ASIO on Windows, AAudio/Oboe on Android), latency negotiation, thread priorities, and failover strategies.
- Add build presets/toolchains for Windows, Android, and potentially macOS/Linux as per roadmap, including signing, installer generation, and Unreal Engine bridge packaging.
- Flesh out asset deployment: bundling stem archives, metadata, Unreal content, and configuration into per-show packages with versioning and checksum validation.

## 7. Safety, Monitoring, and Reliability
- Integrate telemetry: performance counters, logging, Grafana exporters, and runbooks for failover as described in the README’s safety section. Implement watchdog timers for AI modules and controller dropouts.
- Add sandboxing/safety filters for AI-generated content (audio levels, harmonic constraints) and integrate manual override macros that revert to predefined scenes within beat-aligned windows.

## 8. Collaboration, Tooling, and Tests
- Expand tests beyond the single smoke test in `tests/AIAssistantTests.cpp`. Add unit/integration coverage for clip loading, timeline scheduling, controller routing, and AI feedback persistence.
- Set up CI (GitHub Actions) to run builds, static analysis, formatting, and artifact packaging on supported platforms. Include end-to-end scripted rehearsals that validate timeline transitions and effect automation.
- Provide authoring tools (GUI) for the session graph, effect routing, and AI prompt management, aligning with the roadmap in `README.md`.
- Document contribution workflows, coding standards, and release processes so new collaborators can extend the system without reintroducing merge conflicts.

## 9. Data & Content Pipeline
- Build importers that transform artist stems (ZIP or multi-track sessions) into the standardized clip format expected by `ClipLoader`, including loudness normalization and metadata extraction.
- Automate Unreal Engine asset preparation (level sequences, DMX cues) to stay synchronized with audio timeline changes.
- Establish secure storage/distribution for proprietary stems and model weights, including access control and auditing.

---

Delivering the above will move the Kraken prototype from a console demonstration toward the production-ready hybrid DAW/Unreal live performance system described in the README. Each workstream can be pursued in parallel but should converge through defined milestones (middleware MVP, controller integrations, AI co-pilot beta, production rehearsals) to reach the roadmap’s release goals.
