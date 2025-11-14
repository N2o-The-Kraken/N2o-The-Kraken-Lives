# The Kraken: Live Generative AI & Human Collaboration Suite

A vision document for a hybrid Digital Audio Workstation (DAW) and Unreal Engine experience that fuses live generative AI with human performance tools. The Kraken is designed as a modular platform that composers, producers, visual artists, and performers can use to create reactive shows, installations, and immersive storytelling.

## Table of Contents
1. [Core Goals](#core-goals)
2. [Experience Overview](#experience-overview)
3. [System Architecture](#system-architecture)
4. [Generative Intelligence](#generative-intelligence)
5. [Human-in-the-Loop Workflows](#human-in-the-loop-workflows)
6. [Audio-Visual Synchronization](#audio-visual-synchronization)
7. [Session Flow](#session-flow)
8. [Tooling & Integrations](#tooling--integrations)
9. [Collaboration & Versioning](#collaboration--versioning)
10. [Safety, Monitoring & Reliability](#safety-monitoring--reliability)
11. [Roadmap](#roadmap)
12. [Appendix: Example Use Cases](#appendix-example-use-cases)
13. [Prototype C++ Live Sequencer](#prototype-c-live-sequencer)

## Core Goals
- **Unified creative environment** that connects DAWs (Ableton Live, Logic Pro, REAPER, etc.) with Unreal Engine for real-time audiovisual authoring.
- **Adaptive generative intelligence** that can respond to performance cues, sensor data, and audience interaction without sacrificing human control.
- **Seamless live operation** so that musicians, VJs, and technical directors can iterate during rehearsals and execute during shows with minimal friction.
- **Extensible architecture** that supports plug-ins, scripting, and data-driven customization for both sound and visuals.

## Experience Overview
The Kraken sits between traditional DAW workflows and immersive real-time rendering, providing:
- **AI-assisted composition**: Generate stems, MIDI clips, procedural soundscapes, or mix suggestions using transformer-based models trained on curated datasets.
- **Reactive visuals**: Use Unreal Engine to render environments, characters, and particles that respond to musical features (tempo, amplitude envelopes, harmonic progressions).
- **Human performance layer**: Map hardware controllers, motion capture, lighting boards, or VR/AR interfaces to both audio and visual parameters.
- **Live feedback loop**: Capture user adjustments in the moment and feed them back into AI models as conditioning signals or reinforcement for future sessions.

## System Architecture
```
┌────────────────────┐      ┌─────────────────────┐      ┌──────────────────┐
│   DAW Connector     │◄────►│  Middleware Layer   │◄────►│ Unreal Engine RT │
│ (VST/AU/OSC Bridge) │      │ (gRPC / WebSocket)  │      │   Experience     │
└────────────────────┘      └─────────────────────┘      └──────────────────┘
          ▲                             ▲                         ▲
          │                             │                         │
┌────────────────────┐      ┌─────────────────────┐      ┌──────────────────┐
│ Generative Models  │─────►│   State Orchestrator│─────►│  Output Devices  │
│  (Audio + Control) │      │  (Session Graph)    │      │ (FOH, LED, XR)   │
└────────────────────┘      └─────────────────────┘      └──────────────────┘
```

### Key Components
- **DAW Connector**
  - Plug-ins (VST3/AU) or standalone bridge apps stream MIDI, audio stems, and automation data to the middleware.
  - Supports low-latency transport synchronization (Ableton Link, MIDI Time Code, SMPTE).
- **Middleware Layer**
  - Coordinates session state, handles messaging, applies rule-based safety guards, and routes control data.
  - Exposes REST/gRPC APIs and WebSocket endpoints for remote clients, dashboards, or custom hardware.
- **Generative Models**
  - Audio models (RAVE, Jukebox-style, diffusion) for stem generation; control models for tempo/harmony/automation curves.
  - Visual models (NeRFs, style-transfer, procedural geometry) that output textures, materials, or animation triggers for Unreal.
- **Unreal Engine Runtime**
  - Hosts interactive scenes, Blueprints, and C++ modules that interpret control messages and orchestrate real-time rendering.
  - Interfaces with DMX/Art-Net, OSC, and nDisplay clusters for large-scale outputs.
- **State Orchestrator**
  - Maintains the "session graph"—a node-based representation of musical sections, cues, transitions, and visual states.
  - Provides undo/redo history, branching experiments, and metadata tagging for later recall.

## Generative Intelligence
- **Model Conditioning Inputs**
  - Tempo, key, and chord progression extracted from the DAW.
  - Performer gestures captured via MIDI controllers, gamepads, haptic devices, or mocap rigs.
  - Environmental sensors (crowd noise, camera feeds, biosignals) for adaptive ambience.
- **Real-Time Inference**
  - Edge deployment using ONNX Runtime or TensorRT for GPU acceleration.
  - Diffusion or transformer pipelines pruned/quantized for low-latency streaming.
  - Dynamic prompt interpolation for smooth transitions between musical moods or visual motifs.
- **Learning Loop**
  - Reinforcement from human edits: manual parameter adjustments are logged and used to fine-tune reward models.
  - Playback history stored in a vector database for similarity search and quick recall of successful states.

## Human-in-the-Loop Workflows
- **Session Authoring**
  - Drag-and-drop node editor to arrange audio sections, visual cues, and AI behaviors.
  - Automation lanes for cross-modal mappings (e.g., spectral centroid → particle emitter intensity).
- **Rehearsal Mode**
  - Non-destructive sandbox where changes can be auditioned without affecting the live session.
  - Snapshot comparison to evaluate AI-generated alternatives versus human compositions.
- **Performance Mode**
  - Setlist timeline with cue stacking, failover scenes, and manual override buttons.
  - Real-time co-pilot chat for adjusting prompts, requesting new stems, or triggering visual variants.

## Audio-Visual Synchronization
- **Clocking**: Use Ableton Link or SMPTE LTC as the master clock; middleware distributes timing to Unreal via OSC.
- **Feature Extraction**: Analyze amplitude envelopes, spectral flux, and harmonic tension to drive material parameters and lighting cues.
- **State Transitions**: Define crossfades between audio stems and Unreal level sequences using deterministic easing plus probabilistic AI variations.
- **Safety Net**: Hard limits on gain staging, frequency bands, and GPU utilization protect against runaway AI outputs.

## Session Flow
1. **Project Setup**
   - Create a new Kraken session, link DAW project files, and import Unreal maps/assets.
   - Register generative models, define prompts, and set allowed parameter ranges.
2. **Ideation & Generation**
   - Request AI compositions or visual sketches; evaluate suggestions using side-by-side comparison tools.
   - Capture favorite results as reusable "scenes" or "motifs" in the session graph.
3. **Rehearsal Integration**
   - Connect controllers, calibrate latency, and rehearse transitions with human performers.
   - Adjust AI responsiveness (temperature, follow probability) to match artistic intent.
4. **Live Execution**
   - Deploy middleware to a redundant server pair; stream control data to Unreal and DAW rigs.
   - Monitor analytics dashboards for latency, CPU/GPU load, and rule violations.
5. **Post-Show Review**
   - Export logs, highlight standout moments, and feed curated data back into training sets.

## Tooling & Integrations
- **Supported DAWs**: Ableton Live, Logic Pro, REAPER, Bitwig, Pro Tools (via ARA2).
- **Controller Inputs**: MIDI 2.0, OSC, Open Sound Control, game controllers, XR controllers, body tracking (Kinect, OptiTrack), EEG/EMG wearables.
- **Visual Outputs**: Unreal Engine 5 (Nanite, Lumen), LED walls (NDI/Spout), VR headsets, AR overlays, projection mapping.
- **Collaboration Services**: Git LFS for media assets, Perforce or Unreal Revision Control for large binaries, Notion/Slack integration for cue sheets.

## Collaboration & Versioning
- **Session Graph Versioning**: Each node stores metadata, dependencies, and playback stats. Branching allows experiments without jeopardizing the master timeline.
- **Asset Management**: Automated import pipelines convert stems, MIDI, textures, and meshes into standardized formats; metadata captured via JSON sidecars.
- **Team Roles**: Define permissions for composers, sound designers, visual artists, show programmers, and operators. Role-based dashboards highlight relevant controls.

## Safety, Monitoring & Reliability
- **Operational Monitoring**
  - Grafana dashboards for system metrics; alerting for latency spikes or dropped frames.
  - AI moderation filters to catch inappropriate generated content during live shows.
- **Failover Strategies**
  - Hot-standby middleware instance; Unreal failover levels with pre-rendered loops; stem playback fallback via DAW.
  - Manual override macros that revert to human-crafted mixes and visuals within a bar/beat window.
- **Compliance & Logging**
  - Session logs timestamped and archived for audit; consent management for captured performer data.

## Roadmap
1. **Alpha (0-3 months)**
   - Build core middleware with OSC/gRPC bridges and session graph editor.
   - Integrate baseline audio and visual generative models.
   - Conduct closed rehearsals with internal creative team.
2. **Beta (3-6 months)**
   - Expand hardware support, implement redundancy, add analytics dashboards.
   - Launch plugin SDK for third-party developers; open community feedback loop.
3. **Release (6-12 months)**
   - Harden reliability, add cloud collaboration features, certify with major DAWs.
   - Package "show templates" for touring artists, installations, and streaming creators.

## Appendix: Example Use Cases
- **Immersive Concert**: DJ triggers AI remixes while Unreal landscapes morph with crowd energy, captured via sound meters and camera vision.
- **Interactive Installation**: Visitors' movements drive both generative soundscapes and volumetric light sculptures in a gallery setting.
- **Narrative XR Experience**: Story beats determined by player choices; AI composes adaptive score while Unreal renders branching cinematics.
- **Esports Broadcast**: Real-time highlights drive stingers, lower-thirds, and reactive music cues controlled by AI plus a human showcaller.

## Prototype C++ Live Sequencer
To complement the vision document, the repository now includes a **cross-platform C++ prototype** for a live performance sequencer that targets Windows 10/11 desktops and Android 7–15 devices. The goal is to provide a lightweight, timeline-driven scaffold that demonstrates how Kraken’s AI assistant, macro effect stacking, and controller routing can work together during a performance without loading every stem into memory at once.

### Highlights
- **Timeline-driven audio routing** with dynamically scheduled clips, infinite user-defined channel counts, and tempo-aware macro effects that can be stacked and linked for staged transitions.
- **Three AI operating modes**:
  1. *Passive Observe* – learns performer gestures during each session and retains evolving behaviour across sets.
  2. *Collaborative Recommend* – surfaces tracks from the Hard Groove Wav Scroll Records Season 1–9 library (100 tracks per season, stems stored as zipped four-way splits and only unpacked temporarily when triggered).
  3. *Autonomous Performer* – actively issues live performance suggestions aligned to the current beat grid.
- **Modular runtime orchestration** driven by `ModuleOrchestrator`, which stacks the mixer, timeline, controller, and clip scheduling modules from the main script. Each module failure automatically triggers the new AI debugger stub so issues are logged and surfaced for manual repair until a real remediation model is added.
- **Controller abstraction layer** with built-in mappings for the Akai APC 64, Numark Party Mix, and Native Instruments Traktor X1 so the rig can expand to six decks or more.
- **Platform-aware configuration** describing sample rates, buffer sizes, and interactive capabilities for both Windows and Android builds.

### Repository Layout
```
src/
  ai/              // AI assistant state machine and recommendation logic
  audio/           // Timeline, mixer, clip loader, macro effect stack
  controllers/     // Hardware abstraction layer for APC 64, Party Mix, X1
  platform/        // Platform configuration descriptors (Windows & Android)
  main.cpp         // Demo entry point running a simulated 20-step performance
```

### Build Instructions
The project uses CMake (C++20) and produces a console demo executable (`kraken_cli`).

#### JUCE-powered standalone UI
Kraken now bundles a JUCE 8 runtime that surfaces audio-device routing, MIDI discovery, and
AI/timeline status in a desktop window. Configure CMake with `-DKRAKEN_ENABLE_JUCE=ON`; when
combined with `-DKRAKEN_ALLOW_FETCHCONTENT=ON` the build will automatically fetch the JUCE 8.0.1
modules via `FetchContent`. Pass `--juce` (or `--juce-android` to mirror the Android platform
configuration) to `kraken_cli` to launch the interactive UI instead of the console-only demo loop.

#### Third-party audio dependencies
`kraken_core` now links against a small set of decoding libraries that are fetched automatically via
`FetchContent`:

- [minizip-ng](https://github.com/zlib-ng/minizip-ng) for streaming stems out of zipped stem bundles.
- [libsndfile](https://github.com/libsndfile/libsndfile) (or a system-provided `sndfile`) for WAV decoding.
- [minimp3](https://github.com/lieff/minimp3) for MP3 decoding.

No manual setup is required when using CMake 3.16+ with an internet connection. When working in
offline environments, make sure the libraries are installed and discoverable on the system include
and library paths before configuring the project.

#### Windows 10/11 (Visual Studio 2022)
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\\Release\\kraken_cli.exe
```

#### Android 7–15 (NDK r26+)
```bash
cmake -S . -B build-android \
  -DCMAKE_SYSTEM_NAME=Android \
  -DCMAKE_ANDROID_NDK=$ANDROID_NDK_HOME \
  -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
  -DCMAKE_ANDROID_STL_TYPE=c++_shared \
  -DCMAKE_ANDROID_API=24
cmake --build build-android --config Release
```
Package the resulting shared library or executable with your preferred Android shell/NativeActivity launcher. The prototype is deliberately dependency-light so it can slot into existing Unreal or DAW companion apps.

### Configuring stem locations
The runtime looks for a JSON manifest at `assets/stems/stems_manifest.json` that maps track IDs to local stem assets:

```json
{
  "TRACK_ID": { "type": "zip" | "wav_dir" | "audio_file", "path": "relative/or/absolute/path" }
}
```

Each source type is resolved to a `ClipSource` variant at runtime, preserving per-channel metadata so
that the new loader can stream and decode stems incrementally instead of loading raw archives into
memory.

- `zip` – single archive containing four-way stems (matches the legacy demo behaviour).
- `wav_dir` – directory containing per-stem WAV files; the resolver verifies the directory exists before loading.
- `audio_file` – single flattened render for quick demos.

When you install the project (`cmake --install <build>`), the `assets/stems` directory (including `stems_manifest.json`) is copied alongside the deployed stems so you can edit it on the target machine. Update `path` values in the installed manifest to point at your personal stem folders—absolute paths are supported, as are relative paths resolved against the manifest’s directory.

For portable setups, set the `KRAKEN_STEM_MANIFEST` environment variable to the manifest you want to use before launching `kraken_cli`. This allows the demo to target removable drives or synced cloud folders without rewriting the installed manifest.

### Next Steps
- Swap the stubbed clip loader for a streaming decoder (e.g., libsndfile or Oboe) that reads stems directly from zip archives into double-buffered ring buffers.
- Integrate a deterministic timing source (Ableton Link, MIDI Clock, or Android AAudio callbacks) to drive `Timeline::forEachActiveClip` instead of the demo loop.
- Extend `ControllerManager` to listen for real MIDI/USB HID inputs and publish OSC messages for Unreal/lighting rigs.
- Wrap the core library with platform-native front ends (Win32/Qt, Android NativeActivity or JNI) to expose the UI required for live shows.

---

*The Kraken aspires to empower artists to command vast creative oceans—combining the intuition of human performers with the adaptive intelligence of modern AI.*
