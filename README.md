# AudioDiagnostics-Suite
Intelligent Diagnostic Suite for DAWs: Auto-gain staging for amp sims, signal integrity analysis, and cross-track interaction monitoring. Open-source, cross-platform, precision-engineered.

Overview
AudioDiagnostics Suite is an open-source, intelligent utility suite designed to bring laboratory-grade analysis and precision gain staging to the modern DAW workflow. Built for producers, guitarists, and mixing engineers, this suite solves the "black box" problem of plugin chains by providing transparent, actionable insights into signal integrity, gain staging, and cross-track interaction.

The suite consists of two integrated plugins:

The Probe: A pre-FX utility that captures the raw signal, calibrates gain, and acts as the "source of truth" for your channel.

The Analyzer: A post-FX/Master Bus dashboard that tracks the signal's evolution through the FX chain, monitors latency, and identifies cross-track conflicts.

Key Features
Intelligent Gain Staging: Automatically calibrate hardware input levels to match the internal sweet-spot of virtual amp simulators and instruments.

Real-time Diagnostics: Monitor SNR (Signal-to-Noise Ratio), Dynamic Range utilization, and True Peak levels.

FX Chain Analysis: Measure latency, phase integrity, and spectral transformation from the start to the end of your processing chain.

The Matchmaker: An advanced cross-track analysis engine that detects frequency masking, phase cancellation, and headroom issues between interacting tracks (e.g., Kick vs. Synth Bass).

Modern Web-Driven UI: A high-performance, responsive dashboard powered by React and accelerated by native OpenGL for high-refresh visual feedback.

Architecture
Cross-Platform & Cross-DAW: Built with JUCE for maximum compatibility with VST3, AU, and AAX across Windows and macOS.

Pluggable IPC: Uses a robust inter-plugin communication transport system designed to handle the constraints of sandboxed hosts like Logic Pro and Pro Tools.

Open Source: Licensed under GPLv3. We believe in transparency and community-driven development.

Why AudioDiagnostics?
Most engineers rely on subjective guessing when it comes to gain staging and channel health. Our goal is to provide quantifiable, actionable data that helps you make informed decisions, ensuring your digital signal chain remains as clean and precise as the analog world.

Tech Stack
Framework: JUCE 7/8

DSP: Built-in JUCE DSP modules and PFFFT.

Frontend: React / Vite (via juce::WebBrowserComponent).

Language: C++20 / TypeScript.

Contributing
We welcome contributions! Whether you are a DSP expert, a UI/UX designer, or someone who wants to help with documentation or testing across different DAWs, feel free to open a Pull Request or join our discussions.
