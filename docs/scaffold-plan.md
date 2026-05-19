# WebView IPC Scaffold Plan

This project is a GPLv3 JUCE plugin suite with two plugin targets: The Probe for pre-FX capture and diagnostics, and The Analyzer for post-FX or master-bus analysis. The first milestone is a buildable shell with stable module boundaries, a WebView dashboard host, pluggable IPC interfaces, discovery/routing concepts, and minimal passthrough plugin processors.

## Architecture Decisions

- Use JUCE CMake APIs and a shared static core library.
- Use `juce::WebBrowserComponent` for the Analyzer dashboard.
- Use React, Vite, and TypeScript for dashboard assets.
- Use `juce::dsp::FFT` for the first spectral-analysis implementation path.
- Define `IPCTransport` before concrete shared memory, named pipe, or socket transports.
- Define `DiscoveryRegistry` from day one so Probe/Analyzer routing is not hard-coded.
- Persist each plugin instance UUID in APVTS state so DAW session reloads keep identity stable.
- Keep WebView, filesystem, allocation-heavy work, and blocking IPC away from `processBlock`.
- Add native JUCE/OpenGL visuals later only for high-refresh views that WebView cannot serve well enough.

## Initial Repository Layout

- `CMakeLists.txt` configures JUCE, plugin formats, and project options.
- `modules/shared_core` contains transport interfaces, discovery models, loopback test transport, and DSP data models.
- `modules/shared_ui` contains the WebBrowserComponent shell and future C++/JavaScript bridge helpers.
- `plugins/TheProbe` contains the lightweight pre-FX sender plugin.
- `plugins/TheAnalyzer` contains the post-FX analyzer/dashboard plugin.
- `web/dashboard` contains the dashboard frontend source.

## First Build Goal

The first build should produce VST3 and Standalone targets for The Probe and The Analyzer on Windows with MSVC. The plugins should instantiate, pass audio through unchanged, keep stable APVTS UUIDs, and expose enough IPC/discovery types for the next pass to implement `SharedMemoryTransport` and a real registry backend.

## Windows Build Commands

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --parallel
```

For release:

```powershell
cmake -S . -B build-release -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release --parallel
```

## Next Milestones

1. Add `SharedMemoryTransport` and a development registry backend.
2. Add an in-process loopback test target for packet ordering and endpoint discovery.
3. Add WebView message bridge plumbing for registry snapshots and user commands.
4. Add latency ping packet scheduling and correlation scaffolding.
5. Add auto-gain and signal diagnostics engines after the transport path is proven.