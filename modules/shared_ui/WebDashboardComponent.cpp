#include "WebDashboardComponent.h"

#include <cstring>

namespace gitpro::ui
{
    WebDashboardComponent::WebDashboardComponent()
    : browser(juce::WebBrowserComponent::Options {}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withNativeIntegrationEnabled()
        .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2 {}
          .withUserDataFolder(getWebViewUserDataFolder())
          .withStatusBarDisabled())
        .withResourceProvider([](const juce::String& path) { return createDashboardResource(path); })
        .withEventListener(juce::Identifier("gitproCommand"), [this](juce::var payload)
        {
          JUCE_ASSERT_MESSAGE_THREAD;

          if (commandHandler != nullptr)
            commandHandler(juce::JSON::toString(payload));
        }))
    {
        JUCE_ASSERT_MESSAGE_THREAD;

        addAndMakeVisible(browser);
  browser.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    }

    void WebDashboardComponent::resized()
    {
        JUCE_ASSERT_MESSAGE_THREAD;
        browser.setBounds(getLocalBounds());
    }

    void WebDashboardComponent::publishJsonSnapshot(const juce::String& snapshotJson)
    {
        JUCE_ASSERT_MESSAGE_THREAD;
        latestSnapshotJson = snapshotJson;

      auto parsed = juce::JSON::parse(snapshotJson);

      if (parsed.isVoid())
        return;

      browser.emitEventIfBrowserIsVisible(juce::Identifier("gitproSnapshot"), parsed);
      browser.evaluateJavascript("window.dispatchEvent(new CustomEvent('gitproSnapshot', { detail: " + snapshotJson + " }));");
    }

    void WebDashboardComponent::setCommandHandler(CommandHandler handler)
    {
        JUCE_ASSERT_MESSAGE_THREAD;
        commandHandler = std::move(handler);
    }

    juce::File WebDashboardComponent::getWebViewUserDataFolder()
    {
      auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("GitProDiagnosticsSuite")
        .getChildFile("WebView2");

      folder.createDirectory();
      return folder;
    }

    std::optional<juce::WebBrowserComponent::Resource> WebDashboardComponent::createDashboardResource(const juce::String& path)
    {
      if (path != "/" && path != "/index.html")
        return std::nullopt;

      const auto html = createInitialHtml();
      const auto* rawHtml = html.toRawUTF8();
      const auto byteCount = std::strlen(rawHtml);

      juce::WebBrowserComponent::Resource resource;
      resource.mimeType = "text/html; charset=utf-8";
      resource.data.resize(byteCount);

      if (byteCount > 0)
        std::memcpy(resource.data.data(), rawHtml, byteCount);

      return resource;
    }

    juce::String WebDashboardComponent::createInitialHtml()
    {
        return juce::String(R"html(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>GitPro</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: oklch(0.15 0.01 250); --card: oklch(0.18 0.01 250); --fg: oklch(0.92 0.01 250);
      --muted: oklch(0.55 0.01 250); --border: oklch(0.30 0.01 250);
      --metal-dark: oklch(0.20 0.01 250); --metal-light: oklch(0.35 0.01 250);
      --led-amber: oklch(0.80 0.20 65); --led-blue: oklch(0.70 0.22 240);
      --led-green: oklch(0.70 0.22 145); --led-red: oklch(0.60 0.25 25);
      font-family: 'Segoe UI', system-ui, sans-serif;
    }
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
    body { background: var(--bg); color: var(--fg); min-height: 100vh; display: flex; align-items: center; justify-content: center; padding: 12px; }
    .hidden { display: none !important; }
    .plugin-wrap { width: 100%; max-width: 880px; background: var(--card); border-radius: 8px; border: 1px solid var(--border); overflow: hidden; box-shadow: 0 25px 50px -12px oklch(0 0 0 / 0.6); }
    .plugin-header { height: 32px; background: linear-gradient(to bottom, var(--metal-light), var(--metal-dark)); border-bottom: 1px solid var(--border); display: flex; align-items: center; justify-content: space-between; padding: 0 12px; }
    .ph-left { display: flex; align-items: center; gap: 8px; }
    .plugin-icon { width: 20px; height: 20px; border-radius: 4px; display: flex; align-items: center; justify-content: center; }
    .plugin-icon.probe { background: linear-gradient(135deg, var(--led-amber), oklch(0.80 0.20 65 / 0.5)); }
    .plugin-icon.analyzer { background: linear-gradient(135deg, var(--led-blue), oklch(0.70 0.22 240 / 0.5)); }
    .plugin-title { font-weight: 700; font-size: 11px; letter-spacing: 0.5px; }
    .plugin-version { font-size: 9px; color: var(--muted); font-family: monospace; }
    .ph-right { display: flex; align-items: center; gap: 6px; font-size: 9px; font-family: monospace; color: var(--muted); }
    .status-led { width: 6px; height: 6px; border-radius: 50%; background: var(--led-green); box-shadow: 0 0 8px 2px oklch(0.70 0.22 145 / 0.6); }
    .plugin-body { height: 320px; padding: 12px; display: flex; gap: 12px; }
    .plugin-footer { height: 24px; background: var(--metal-dark); border-top: 1px solid var(--border); display: flex; align-items: center; justify-content: space-between; padding: 0 12px; font-size: 8px; font-family: monospace; color: var(--muted); }
    .probe-meters { width: 60px; flex-shrink: 0; }
    .probe-center { flex: 1; display: flex; flex-direction: column; gap: 8px; }
    .probe-note-row { height: 56px; display: flex; gap: 8px; }
    .probe-strobe-row { flex: 1; }
    .probe-gain { width: 144px; flex-shrink: 0; }
    .analyzer-body { flex-direction: column !important; }
    .spectrum-wrap { flex: 1; min-height: 0; display: flex; flex-direction: column; gap: 4px; }
    .analyzer-bottom { height: 72px; display: flex; gap: 8px; }
    .phase-col { width: 200px; flex-shrink: 0; }
    .stats-col { flex: 1; }
    .panel { border: 1px solid var(--border); border-radius: 6px; background: oklch(0.10 0.01 250); box-shadow: inset 0 2px 4px oklch(0 0 0 / 0.3); overflow: hidden; }
    .metal-panel { background: var(--metal-dark); border: 1px solid var(--border); border-radius: 6px; }
    .note-panel { flex: 1; display: flex; align-items: center; justify-content: center; gap: 4px; padding: 0 12px; }
    .note-letter { font-size: 34px; font-weight: 800; font-family: monospace; color: var(--led-amber); text-shadow: 0 0 10px oklch(0.80 0.20 65 / 0.7); line-height: 1; }
    .note-octave { font-size: 18px; font-family: monospace; color: oklch(0.80 0.20 65 / 0.7); }
    .note-freq { font-size: 9px; font-family: monospace; color: var(--muted); margin-left: 8px; }
    .cents-panel { width: 80px; display: flex; flex-direction: column; align-items: center; justify-content: center; }
    .cents-val { font-size: 20px; font-weight: 700; font-family: monospace; line-height: 1; }
    .cents-val.in-tune { color: var(--led-green); text-shadow: 0 0 8px oklch(0.70 0.22 145 / 0.6); }
    .cents-val.out-tune { color: var(--led-amber); }
    .cents-label { font-size: 8px; font-family: monospace; color: var(--muted); }
    .auto-gain { height: 100%; display: flex; flex-direction: column; gap: 8px; padding: 8px; }
    .ag-title { font-size: 9px; font-family: monospace; text-transform: uppercase; letter-spacing: 0.08em; color: var(--muted); text-align: center; }
    .ag-label { font-size: 8px; font-family: monospace; color: var(--muted); margin-bottom: 2px; }
    .ag-select { width: 100%; font-size: 9px; font-family: monospace; background: oklch(0.10 0.01 250); border: 1px solid var(--border); border-radius: 4px; padding: 4px 6px; color: var(--fg); }
    .ag-btn { width: 100%; padding: 6px; border-radius: 4px; font-size: 9px; font-family: monospace; text-transform: uppercase; border: 1px solid var(--border); background: transparent; color: var(--fg); cursor: pointer; box-shadow: 0 3px 6px oklch(0 0 0 / 0.3), inset 0 1px 0 oklch(1 0 0 / 0.1); }
    .ag-btn:active { transform: translateY(1px); }
    .ag-btn.analyzing { color: var(--led-amber); animation: blink 1s infinite; }
    .ag-result { flex: 1; display: flex; flex-direction: column; align-items: center; justify-content: center; min-height: 48px; font-family: monospace; }
    .ag-result .rv { font-size: 22px; font-weight: 700; }
    .ag-result .rv.has { color: var(--led-green); text-shadow: 0 0 8px oklch(0.70 0.22 145 / 0.6); }
    .ag-result .rv.no { color: oklch(0.55 0.01 250 / 0.3); }
    .ag-result .ru { font-size: 8px; color: var(--muted); }
    .progress-bar { height: 4px; border-radius: 2px; background: var(--border); overflow: hidden; }
    .progress-fill { height: 100%; background: var(--led-green); transition: width 100ms linear; }
    .spectrum-hdr { display: flex; align-items: center; justify-content: space-between; padding: 0 4px; }
    .spectrum-title { font-size: 9px; font-family: monospace; text-transform: uppercase; letter-spacing: 0.08em; color: var(--muted); }
    .clash-badge { font-size: 9px; font-family: monospace; padding: 2px 6px; border-radius: 3px; background: oklch(0.60 0.25 25 / 0.2); color: var(--led-red); }
    .stats-row { display: flex; align-items: center; gap: 6px; height: 100%; padding: 4px 8px; overflow: hidden; }
    .stat-cell { display: flex; flex-direction: column; align-items: center; justify-content: center; min-width: 38px; }
    .stat-val { font-size: 12px; font-weight: 700; font-family: monospace; line-height: 1.2; }
    .stat-val.amber { color: var(--led-amber); } .stat-val.green { color: var(--led-green); }
    .stat-val.blue  { color: var(--led-blue);  } .stat-val.red   { color: var(--led-red);   }
    .stat-unit { font-size: 6px; color: var(--muted); font-family: monospace; }
    .stat-lbl { font-size: 6px; font-family: monospace; text-transform: uppercase; color: var(--muted); line-height: 1.2; text-align: center; }
    .stat-div { width: 1px; height: 32px; background: var(--border); flex-shrink: 0; }
    .pdc-area { display: flex; align-items: center; gap: 4px; }
    .pdc-led { width: 8px; height: 8px; border-radius: 50%; flex-shrink: 0; background: oklch(0.55 0.01 250 / 0.3); }
    .pdc-led.active { background: var(--led-amber); animation: blink 1s infinite; }
    .pdc-led.ok  { background: var(--led-green); box-shadow: 0 0 8px 2px oklch(0.70 0.22 145 / 0.6); }
    .pdc-led.err { background: var(--led-red); }
    .pdc-btn { padding: 2px 6px; border-radius: 3px; font-size: 7px; font-family: monospace; text-transform: uppercase; border: 1px solid var(--border); background: transparent; color: var(--muted); cursor: pointer; box-shadow: 0 3px 6px oklch(0 0 0 / 0.3), inset 0 1px 0 oklch(1 0 0 / 0.1); }
    .pdc-btn:hover { color: var(--fg); } .pdc-btn:active { transform: translateY(1px); }
    .pdc-btn.measuring { color: var(--led-amber); }
    .pdc-display { width: 52px; height: 28px; display: flex; flex-direction: column; align-items: center; justify-content: center; }
    .pdc-val { font-size: 9px; font-weight: 700; font-family: monospace; }
    .pdc-val.ok { color: var(--led-green); } .pdc-val.err { color: var(--led-red); }
    .pdc-val.none { color: oklch(0.55 0.01 250 / 0.3); }
    .pdc-sub { font-size: 5px; font-family: monospace; color: var(--muted); }
    .loading { color: var(--muted); font-family: monospace; font-size: 13px; text-align: center; padding: 40px; }
    @keyframes blink { 0%, 100% { opacity: 1; } 50% { opacity: 0.4; } }
  </style>
</head>
<body>
  <!-- Loading -->
  <div id="load-ui"><div class="loading">Connecting to plugin&hellip;</div></div>

  <!-- Probe UI -->
  <div id="probe-ui" class="plugin-wrap hidden">
    <div class="plugin-header">
      <div class="ph-left">
        <div class="plugin-icon probe">
          <svg width="12" height="12" viewBox="0 0 16 16" fill="none">
            <circle cx="8" cy="8" r="5.5" stroke="rgba(20,20,30,0.9)" stroke-width="2"/>
            <circle cx="8" cy="8" r="2" fill="rgba(20,20,30,0.9)"/>
          </svg>
        </div>
        <span class="plugin-title">THE PROBE</span>
        <span class="plugin-version">v2.1</span>
      </div>
      <div class="ph-right"><span class="status-led"></span><span>44.1kHz</span></div>
    </div>
    <div class="plugin-body">
      <div class="probe-meters">
        <div class="panel" style="height:100%;border-radius:6px;padding:0;">
          <canvas id="meters-canvas" style="display:block;"></canvas>
        </div>
      </div>
      <div class="probe-center">
        <div class="probe-note-row">
          <div class="panel note-panel">
            <span class="note-letter" id="note-letter">--</span>
            <span class="note-octave" id="note-octave"></span>
            <span class="note-freq"   id="note-freq"></span>
          </div>
          <div class="panel cents-panel">
            <span class="cents-val out-tune" id="cents-val">--</span>
            <span class="cents-label">CENTS</span>
          </div>
        </div>
        <div class="probe-strobe-row">
          <div class="panel" style="height:100%;border-radius:6px;padding:0;" id="strobe-wrap">
            <canvas id="strobe-canvas" style="display:block;width:100%;height:100%;"></canvas>
          </div>
        </div>
      </div>
      <div class="probe-gain">
        <div class="metal-panel auto-gain">
          <span class="ag-title">Auto-Gain</span>
          <div>
            <div class="ag-label">MAX INPUT</div>
            <select id="ag-interface" class="ag-select">
              <option value="12">+12 dBu</option>
              <option value="18" selected>+18 dBu</option>
              <option value="24">+24 dBu</option>
            </select>
          </div>
          <div>
            <div class="ag-label">TARGET</div>
            <select id="ag-target" class="ag-select">
              <option value="-18">-18 dBFS</option>
              <option value="-12" selected>-12 dBFS</option>
              <option value="-6">-6 dBFS</option>
            </select>
          </div>
          <button id="ag-btn" class="ag-btn">Analyze</button>
          <div class="progress-bar" id="ag-prog-wrap" style="display:none;">
            <div class="progress-fill" id="ag-prog" style="width:0%"></div>
          </div>
          <div class="panel ag-result">
            <span class="rv no" id="ag-result">--</span>
            <span class="ru">dB</span>
          </div>
        </div>
      </div>
    </div>
    <div class="plugin-footer">
      <span>GitPro Suite</span><span>Input Utility / Tuner</span><span>v2.1</span>
    </div>
  </div>

  <!-- Analyzer UI -->
  <div id="analyzer-ui" class="plugin-wrap hidden">
    <div class="plugin-header">
      <div class="ph-left">
        <div class="plugin-icon analyzer">
          <svg width="12" height="12" viewBox="0 0 16 16" fill="rgba(20,20,30,0.9)">
            <path d="M1 14V9h2v5H1zm4 0V5h2v9H5zm4 0V2h2v12H9zm4 0V7h2v7h-2z"/>
          </svg>
        </div>
        <span class="plugin-title">THE ANALYZER</span>
        <span class="plugin-version">v2.1</span>
      </div>
      <div class="ph-right"><span class="status-led"></span><span>44.1kHz</span></div>
    </div>
    <div class="plugin-body analyzer-body">
      <div class="spectrum-wrap">
        <div class="spectrum-hdr">
          <span class="spectrum-title">Low-Band Matchmaker Spectrum</span>
          <span class="clash-badge hidden" id="clash-badge">-- Hz CLASH</span>
        </div>
        <div class="panel" style="flex:1;border-radius:6px;padding:0;" id="spectrum-wrap">
          <canvas id="spectrum-canvas" style="display:block;width:100%;height:100%;"></canvas>
        </div>
      </div>
      <div class="analyzer-bottom">
        <div class="phase-col">
          <div class="panel" style="height:100%;border-radius:6px;padding:0;">
            <canvas id="phase-canvas" style="display:block;"></canvas>
          </div>
        </div>
        <div class="stats-col">
          <div class="metal-panel stats-row">
            <div class="stat-cell">
              <div style="display:flex;align-items:baseline;gap:2px;">
                <span class="stat-val amber" id="st-peak">--</span><span class="stat-unit">dB</span>
              </div>
              <span class="stat-lbl">True Peak</span>
            </div>
            <div class="stat-div"></div>
            <div class="stat-cell">
              <div style="display:flex;align-items:baseline;gap:2px;">
                <span class="stat-val amber" id="st-crest">--</span><span class="stat-unit">dB</span>
              </div>
              <span class="stat-lbl">Crest</span>
            </div>
            <div class="stat-div"></div>
            <div class="stat-cell">
              <div style="display:flex;align-items:baseline;gap:2px;">
                <span class="stat-val green" id="st-delta">--</span><span class="stat-unit">dB</span>
              </div>
              <span class="stat-lbl">Crest &Delta;</span>
            </div>
            <div class="stat-div"></div>
            <div class="stat-cell">
              <div style="display:flex;align-items:baseline;gap:2px;">
                <span class="stat-val blue" id="st-stereo">--</span>
              </div>
              <span class="stat-lbl">Stereo Evo</span>
            </div>
            <div class="stat-div"></div>
            <div class="pdc-area">
              <span class="pdc-led" id="pdc-led"></span>
              <button class="pdc-btn" id="pdc-btn">PDC</button>
              <div class="panel pdc-display">
                <span class="pdc-val none" id="pdc-val">--</span>
                <span class="pdc-sub" id="pdc-sub">smp</span>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
    <div class="plugin-footer">
      <span>GitPro Suite</span><span>Master Analysis / Metering</span><span>v2.1</span>
    </div>
  </div>

  )html")
      + R"html(
  <script>
    // ===== State =====
    const S = {
      role: null,
      peakDbfs: -120, rmsDbfs: -120, noiseFloorDbfs: -75, snrDb: 54,
      pitchHz: 0, autoGainDb: 0, autoAnalyzeActive: false, autoAnalyzeProgress: 0,
      interfaceMaxInputDbU: 18, targetPeakDbfs: -12,
      matchmaker: {}, fxChainHealth: {}, probes: [], realtime: {}
    };
    const D = { peak: -120, rms: -120, peakHold: -120, peakHoldTs: 0, noise: -75, snr: 54, phaseCorr: 1 };
    const BAND_LO = [40, 55, 70,  90, 115, 135];
    const BAND_HI = [55, 70, 90, 115, 135, 150];

    // ===== Canvas refs =====
    let mCvs, mCtx, sCvs, sCtx, spCvs, spCtx, pCvs, pCtx;
    let mRAF = 0, sRAF = 0, spRAF = 0, pRAF = 0;

    function initCanvases(role) {
      const dpr = window.devicePixelRatio || 1;
      if (role === 'probe') {
        mCvs = document.getElementById('meters-canvas');
        mCtx = mCvs.getContext('2d');
        mCvs.width = 60 * dpr; mCvs.height = 280 * dpr;
        mCvs.style.width = '60px'; mCvs.style.height = '280px';

        sCvs = document.getElementById('strobe-canvas');
        sCtx = sCvs.getContext('2d');
        const sw = document.getElementById('strobe-wrap');
        const rsz = () => {
          const r = sw.getBoundingClientRect();
          sCvs.width = r.width * dpr; sCvs.height = r.height * dpr;
          sCvs.style.width = r.width + 'px'; sCvs.style.height = r.height + 'px';
        };
        rsz(); new ResizeObserver(rsz).observe(sw);
      } else {
        spCvs = document.getElementById('spectrum-canvas');
        spCtx = spCvs.getContext('2d');
        const sc = document.getElementById('spectrum-wrap');
        const rszSp = () => {
          const r = sc.getBoundingClientRect();
          spCvs.width = r.width * dpr; spCvs.height = r.height * dpr;
          spCvs.style.width = r.width + 'px'; spCvs.style.height = r.height + 'px';
        };
        rszSp(); new ResizeObserver(rszSp).observe(sc);

        pCvs = document.getElementById('phase-canvas');
        pCtx = pCvs.getContext('2d');
        pCvs.width = 200 * dpr; pCvs.height = 72 * dpr;
        pCvs.style.width = '200px'; pCvs.style.height = '72px';
      }
    }

    function startLoops(role) {
      if (role === 'probe') {
        cancelAnimationFrame(mRAF); cancelAnimationFrame(sRAF);
        (function lm(ts) { drawMeters(ts); mRAF = requestAnimationFrame(lm); })(0);
        (function ls(ts) { drawStrobe(ts); sRAF = requestAnimationFrame(ls); })(0);
      } else {
        cancelAnimationFrame(spRAF); cancelAnimationFrame(pRAF);
        (function lsp(ts) { drawSpectrum(ts); spRAF = requestAnimationFrame(lsp); })(0);
        (function lp(ts)  { drawPhase(ts);   pRAF  = requestAnimationFrame(lp);  })(0);
      }
    }

    // ===== Meters =====
    function drawMeters(ts) {
      if (!mCtx) return;
      const dpr = window.devicePixelRatio || 1;
      D.peak = S.peakDbfs > D.peak ? S.peakDbfs : D.peak - 1.5;
      D.rms  = S.rmsDbfs  > D.rms  ? S.rmsDbfs  : D.rms  - 0.8;
      D.noise += 0.05 * (S.noiseFloorDbfs - D.noise);
      D.snr   += 0.05 * (S.snrDb - D.snr);
      if (D.peak > D.peakHold) { D.peakHold = D.peak; D.peakHoldTs = ts; }
      if (ts - D.peakHoldTs > 1500) D.peakHold = Math.max(D.peakHold - 0.5, -60);
      const ctx = mCtx;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      const w = 60, h = 280;
      ctx.fillStyle = 'oklch(0.06 0.01 250)'; ctx.fillRect(0, 0, w, h);
      const barW = 18, barH = h - 44, sY = 24;
      const drawBar = (x, pk, rms, hold) => {
        ctx.fillStyle = 'oklch(0.10 0.01 250)'; ctx.fillRect(x, sY, barW, barH);
        const rf = Math.max(0, ((rms + 60) / 60)) * barH;
        if (rf > 0) {
          const g = ctx.createLinearGradient(0, sY + barH, 0, sY);
          g.addColorStop(0, 'oklch(0.45 0.15 145)'); g.addColorStop(0.70, 'oklch(0.55 0.18 145)');
          g.addColorStop(0.85,'oklch(0.55 0.15 65)'); g.addColorStop(1,   'oklch(0.50 0.20 25)');
          ctx.fillStyle = g; ctx.fillRect(x, sY + barH - rf, barW, rf);
        }
        const py = sY + barH - Math.max(0, ((pk + 60) / 60)) * barH;
        ctx.fillStyle = pk > -6 ? 'oklch(0.65 0.25 25)' : pk > -12 ? 'oklch(0.70 0.18 65)' : 'oklch(0.70 0.22 145)';
        ctx.fillRect(x, py - 2, barW, 3);
        const hy = sY + barH - Math.max(0, ((hold + 60) / 60)) * barH;
        ctx.fillStyle = 'oklch(0.90 0.01 250)'; ctx.fillRect(x, hy, barW, 1);
        ctx.fillStyle = 'oklch(0.06 0.01 250)';
        for (let i = 0; i < barH; i += 3) ctx.fillRect(x, sY + i, barW, 1);
      };
      drawBar(6,  D.peak, D.rms, D.peakHold);
      drawBar(36, D.peak, D.rms, D.peakHold);
      ctx.fillStyle = 'oklch(0.50 0.01 250)'; ctx.font = 'bold 8px monospace'; ctx.textAlign = 'center';
      ctx.fillText('L', 15, 14); ctx.fillText('R', 45, 14);
      ctx.font = '7px monospace'; ctx.textAlign = 'right';
      [0, -6, -12, -24, -48].forEach(db => {
        const y = sY + barH - ((db + 60) / 60) * barH;
        ctx.fillStyle = 'oklch(0.35 0.01 250)'; ctx.fillText(String(db), w - 2, y + 3);
      });
      ctx.fillStyle = D.peak > -6 ? 'oklch(0.65 0.25 25)' : 'oklch(0.65 0.18 145)';
      ctx.font = 'bold 9px monospace'; ctx.textAlign = 'center';
      ctx.fillText(String(Math.round(D.peak)), w / 2, h - 44);
      ctx.font = '6px monospace'; ctx.textAlign = 'left';
      ctx.fillStyle = 'oklch(0.30 0.01 250)'; ctx.fillText('NF', 4, h - 28);
      ctx.fillStyle = 'oklch(0.55 0.15 200)'; ctx.font = 'bold 8px monospace'; ctx.textAlign = 'right';
      ctx.fillText(String(Math.round(D.noise)), w - 4, h - 28);
      ctx.font = '6px monospace'; ctx.textAlign = 'left';
      ctx.fillStyle = 'oklch(0.30 0.01 250)'; ctx.fillText('SNR', 4, h - 14);
      const sc2 = D.snr > 55 ? 'oklch(0.65 0.20 145)' : D.snr > 45 ? 'oklch(0.70 0.16 65)' : 'oklch(0.55 0.22 25)';
      ctx.fillStyle = sc2; ctx.font = 'bold 8px monospace'; ctx.textAlign = 'right';
      ctx.fillText(String(Math.round(D.snr)), w - 4, h - 14);
    }

)html"
    + R"html(
    // ===== Strobe =====
    function pitchToNote(hz) {
      if (!hz || hz <= 0 || !isFinite(hz)) return { note: '--', octave: '', cents: 0 };
      const n = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
      const midi = 69 + 12 * Math.log2(hz / 440), r = Math.round(midi);
      return { note: n[((r % 12) + 12) % 12], octave: String(Math.floor(r / 12) - 1), cents: Math.round((midi - r) * 100) };
    }

    function drawStrobe(ts) {
      if (!sCtx || !sCvs) return;
      const dpr = window.devicePixelRatio || 1;
      const ctx = sCtx;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      const w = sCvs.width / dpr, h = sCvs.height / dpr;
      if (w <= 0 || h <= 0) return;
      let co = 0;
      if (S.pitchHz > 20 && isFinite(S.pitchHz)) {
        const m = 69 + 12 * Math.log2(S.pitchHz / 440);
        co = (m - Math.round(m)) * 100;
      }
      const inTune = Math.abs(co) < 5 && S.pitchHz > 20;
      ctx.fillStyle = 'oklch(0.06 0.01 250)'; ctx.fillRect(0, 0, w, h);
      const spd = co * 0.4, sw = 10;
      for (let i = 0; i < Math.ceil(w / sw) + 2; i++) {
        const x = ((i * sw + ts * spd * 0.08) % (w + sw * 2)) - sw;
        ctx.fillStyle = 'oklch(0.65 0.16 65 / ' + (inTune ? 0.6 : 0.3) + ')';
        ctx.fillRect(x, 15, sw / 2, h - 30);
      }
      ctx.strokeStyle = 'oklch(0.40 0.12 240)'; ctx.lineWidth = 1.5; ctx.setLineDash([4, 3]);
      ctx.beginPath(); ctx.moveTo(w / 2, 10); ctx.lineTo(w / 2, h - 10); ctx.stroke();
      ctx.setLineDash([]);
      const nx = w / 2 + co * 2.5;
      ctx.shadowColor = inTune ? 'oklch(0.70 0.22 145)' : 'oklch(0.65 0.18 240)'; ctx.shadowBlur = 10;
      ctx.fillStyle   = inTune ? 'oklch(0.80 0.22 145)' : 'oklch(0.70 0.18 240)';
      ctx.fillRect(nx - 1.5, 8, 3, h - 16); ctx.shadowBlur = 0;
      ctx.fillStyle = 'oklch(0.35 0.01 250)'; ctx.font = '7px monospace'; ctx.textAlign = 'center';
      [-50, -25, 0, 25, 50].forEach(c => {
        const mx = w / 2 + c * 2.5;
        ctx.fillRect(mx - 0.5, h - 12, 1, c === 0 ? 8 : 4);
        if (c === -50 || c === 0 || c === 50) ctx.fillText((c > 0 ? '+' : '') + c, mx, h - 2);
      });
    }

    // ===== Spectrum =====
    function drawSpectrum(ts) {
      if (!spCtx || !spCvs) return;
      const dpr = window.devicePixelRatio || 1;
      const ctx = spCtx;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      const w = spCvs.width / dpr, h = spCvs.height / dpr;
      if (w <= 0 || h <= 0) return;
      ctx.fillStyle = 'oklch(0.06 0.01 250)'; ctx.fillRect(0, 0, w, h);
      const pL = 32, pR = 8, pT = 14, pB = 16;
      const gW = w - pL - pR, gH = h - pT - pB;
      const fX = hz => pL + ((hz - 30) / 170) * gW;
      const dY = db => pT + (1 - Math.max(0, (db + 80) / 80)) * gH;
      ctx.strokeStyle = 'oklch(0.14 0.01 250)'; ctx.lineWidth = 0.5;
      [0, -20, -40, -60, -80].forEach(db => {
        const y = dY(db); ctx.beginPath(); ctx.moveTo(pL, y); ctx.lineTo(w - pR, y); ctx.stroke();
        ctx.fillStyle = 'oklch(0.35 0.01 250)'; ctx.font = '7px monospace'; ctx.textAlign = 'right';
        ctx.fillText(String(db), pL - 4, y + 3);
      });
      [40, 60, 80, 100, 120, 150].forEach(hz => {
        const x = fX(hz); ctx.beginPath(); ctx.moveTo(x, pT); ctx.lineTo(x, h - pB); ctx.stroke();
        ctx.fillStyle = 'oklch(0.35 0.01 250)'; ctx.font = '7px monospace'; ctx.textAlign = 'center';
        ctx.fillText(String(hz), x, h - 2);
      });
      const rt = S.realtime || {}, p0 = (S.probes || [])[0] || {};
      const pb = rt.probeLowBandEnergiesDb || (p0.lowBands ? p0.lowBands.map(b => b.energyDb) : []);
      const lb = rt.localLowBandEnergiesDb || [];
      const drawBands = (bands, fc, sc) => {
        if (!bands || !bands.length) return;
        bands.forEach((db, i) => {
          if (i >= BAND_LO.length) return;
          const x1 = fX(BAND_LO[i]), x2 = fX(BAND_HI[i]), y = dY(db), bw = x2 - x1 - 2;
          if (bw <= 0) return;
          ctx.fillStyle = fc; ctx.fillRect(x1 + 1, y, bw, h - pB - y);
          ctx.strokeStyle = sc; ctx.lineWidth = 1.5; ctx.strokeRect(x1 + 1, y, bw, h - pB - y);
        });
      };
      drawBands(pb, 'oklch(0.60 0.18 240 / 0.3)', 'oklch(0.60 0.18 240)');
      drawBands(lb, 'oklch(0.70 0.16 65  / 0.25)', 'oklch(0.70 0.16 65)');
      const mm = S.matchmaker || {};
      if ((mm.lowEndConflict || mm.phaseDestruction) && mm.conflictFrequencyHz > 0) {
        const cx = fX(mm.conflictFrequencyHz), pulse = 0.6 + Math.sin(ts * 0.006) * 0.3;
        ctx.shadowColor = 'oklch(0.55 0.22 25)'; ctx.shadowBlur = 8 * pulse;
        ctx.fillStyle = 'oklch(0.55 0.22 25 / ' + (0.7 + pulse * 0.3) + ')';
        ctx.beginPath(); ctx.arc(cx, pT + 8, 4, 0, Math.PI * 2); ctx.fill();
        ctx.shadowBlur = 0;
        ctx.fillStyle = 'oklch(0.98 0 0)'; ctx.font = 'bold 7px sans-serif'; ctx.textAlign = 'center';
        ctx.fillText('!', cx, pT + 11);
      }
      ctx.fillStyle = 'oklch(0.60 0.18 240)'; ctx.fillRect(w - 65, 5, 7, 7);
      ctx.fillStyle = 'oklch(0.70 0.01 250)'; ctx.font = '8px monospace'; ctx.textAlign = 'left';
      ctx.fillText('Probe', w - 55, 11);
      ctx.fillStyle = 'oklch(0.70 0.16 65)'; ctx.fillRect(w - 65, 17, 7, 7);
      ctx.fillText('Post-FX', w - 55, 23);
    }

    // ===== Phase Correlation =====
    function drawPhase() {
      if (!pCtx || !pCvs) return;
      const dpr = window.devicePixelRatio || 1;
      D.phaseCorr += 0.06 * (((S.matchmaker || {}).phaseCorrelation ?? 1) - D.phaseCorr);
      const v = Math.max(-1, Math.min(1, D.phaseCorr));
      const ctx = pCtx;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      const w = 200, h = 72;
      ctx.fillStyle = 'oklch(0.06 0.01 250)'; ctx.fillRect(0, 0, w, h);
      const mX = 8, mW = w - 55, mH = 10, mY = 30;
      ctx.fillStyle = 'oklch(0.40 0.01 250)'; ctx.font = '7px monospace'; ctx.textAlign = 'left';
      ctx.fillText('Phase Corr.', 8, 16); ctx.textAlign = 'center';
      ctx.fillText('-1', mX + 4, mY - 3); ctx.fillText('0', mX + mW/2, mY - 3); ctx.fillText('+1', mX + mW - 4, mY - 3);
      ctx.fillStyle = 'oklch(0.10 0.01 250)'; ctx.fillRect(mX, mY, mW, mH);
      ctx.fillStyle = 'oklch(0.20 0.01 250)'; ctx.fillRect(mX + mW/2 - 0.5, mY - 1, 1, mH + 2);
      const col = v > 0.5 ? 'oklch(0.65 0.20 145)' : v > 0 ? 'oklch(0.70 0.16 65)' : 'oklch(0.55 0.22 25)';
      const cX = mX + mW/2, fW = Math.abs(v) * (mW/2);
      ctx.fillStyle = col;
      v >= 0 ? ctx.fillRect(cX, mY, fW, mH) : ctx.fillRect(cX - fW, mY, fW, mH);
      ctx.fillStyle = 'oklch(0.95 0 0)'; ctx.fillRect(cX + v * (mW/2) - 1, mY - 2, 2, mH + 4);
      ctx.fillStyle = col; ctx.font = 'bold 14px monospace'; ctx.textAlign = 'left';
      ctx.fillText(v.toFixed(2), w - 42, mY + 10);
      ctx.font = '7px monospace'; ctx.fillStyle = 'oklch(0.45 0.01 250)'; ctx.textAlign = 'center';
      ctx.fillText(v > 0.5 ? 'STEREO' : v > 0 ? 'WIDE' : 'MONO', mX + mW/2, mY + mH + 12);
    }

)html"
    + R"html(
    // ===== DOM text updates =====
    function updateDOM() {
      if (S.role === 'probe') {
        const { note, octave, cents } = pitchToNote(S.pitchHz);
        const inTune = Math.abs(cents) < 5 && S.pitchHz > 20;
        document.getElementById('note-letter').textContent = note;
        document.getElementById('note-octave').textContent = octave;
        document.getElementById('note-freq').textContent = S.pitchHz > 20 ? S.pitchHz.toFixed(1) + ' Hz' : '';
        const cv = document.getElementById('cents-val');
        cv.textContent = note !== '--' ? (cents > 0 ? '+' : '') + cents : '--';
        cv.className = 'cents-val ' + (inTune ? 'in-tune' : 'out-tune');
        const hasRes = isFinite(S.autoGainDb) && S.autoGainDb !== 0;
        const rv = document.getElementById('ag-result');
        rv.textContent = hasRes ? (S.autoGainDb > 0 ? '+' : '') + S.autoGainDb.toFixed(1) : '--';
        rv.className = 'rv ' + (hasRes ? 'has' : 'no');
        document.getElementById('ag-prog-wrap').style.display = S.autoAnalyzeActive ? 'block' : 'none';
        document.getElementById('ag-prog').style.width = Math.round((S.autoAnalyzeProgress || 0) * 100) + '%';
        const btn = document.getElementById('ag-btn');
        btn.textContent = S.autoAnalyzeActive ? '...' : 'Analyze';
        btn.className = 'ag-btn' + (S.autoAnalyzeActive ? ' analyzing' : '');
        btn.disabled = S.autoAnalyzeActive;
        document.getElementById('ag-interface').value = String(Math.round(S.interfaceMaxInputDbU ?? 18));
        document.getElementById('ag-target').value = String(Math.round(S.targetPeakDbfs ?? -12));
      } else {
        const mm = S.matchmaker || {}, h = S.fxChainHealth || {}, p0 = (S.probes || [])[0] || {};
        const hasClash = mm.lowEndConflict || mm.phaseDestruction;
        const badge = document.getElementById('clash-badge');
        badge.classList.toggle('hidden', !hasClash || !(mm.conflictFrequencyHz > 0));
        if (hasClash && mm.conflictFrequencyHz > 0) badge.textContent = Math.round(mm.conflictFrequencyHz) + ' Hz CLASH';
        const fdb = v => isFinite(v) ? v.toFixed(1) : '--';
        document.getElementById('st-peak').textContent  = fdb(p0.peakDbfs);
        document.getElementById('st-crest').textContent = fdb(h.analyzerCrestFactorDb);
        const de = document.getElementById('st-delta');
        de.textContent = fdb(h.crestFactorDeltaDb); de.className = 'stat-val ' + (h.heavilyCompressed ? 'red' : 'green');
        const se = document.getElementById('st-stereo');
        se.textContent = isFinite(h.analyzerLowFrequencyCorrelation) ? h.analyzerLowFrequencyCorrelation.toFixed(2) : '--';
        se.className = 'stat-val ' + (h.phaseWarning ? 'red' : 'blue');
        const isMeas = h.pdcListening, hasSmp = isFinite(h.pdcMeasuredSamples), hasDrift = h.pdcDriftWarning;
        document.getElementById('pdc-led').className = 'pdc-led ' + (isMeas ? 'active' : hasDrift ? 'err' : hasSmp ? 'ok' : '');
        const pb = document.getElementById('pdc-btn');
        pb.className = 'pdc-btn ' + (isMeas ? 'measuring' : ''); pb.disabled = isMeas;
        const pv = document.getElementById('pdc-val');
        pv.textContent = hasSmp ? String(Math.round(h.pdcMeasuredSamples)) : '--';
        pv.className = 'pdc-val ' + (hasDrift ? 'err' : hasSmp ? 'ok' : 'none');
        document.getElementById('pdc-sub').textContent = hasSmp && isFinite(h.pdcMeasuredMilliseconds) ? h.pdcMeasuredMilliseconds.toFixed(1) + 'ms' : 'smp';
      }
    }

    // ===== Snapshot handler =====
    function renderSnapshot(snap) {
      if (snap.realtime)      Object.assign(S.realtime,      snap.realtime);
      if (snap.matchmaker)    Object.assign(S.matchmaker,    snap.matchmaker);
      if (snap.fxChainHealth) Object.assign(S.fxChainHealth, snap.fxChainHealth);
      if (snap.probes)        S.probes = snap.probes;
      Object.assign(S, snap);
      S.pitchHz = snap.pitchFrequencyHz ?? S.pitchHz;
      if (!S.role && snap.role) {
        S.role = snap.role;
        document.getElementById('load-ui').style.display = 'none';
        document.getElementById('probe-ui').classList.toggle('hidden', snap.role !== 'probe');
        document.getElementById('analyzer-ui').classList.toggle('hidden', snap.role !== 'analyzer');
        initCanvases(snap.role);
        startLoops(snap.role);
      }
      updateDOM();
    }

    // ===== JUCE bridge =====
    function emitCmd(p) { window.__JUCE__?.backend?.emitEvent('gitproCommand', p); }
    function sendCal() {
      emitCmd({ command: 'setProbeCalibration',
                interfaceMaxInputDbU: Number(document.getElementById('ag-interface').value),
                targetPeakDbfs:       Number(document.getElementById('ag-target').value) });
    }
    window.addEventListener('gitproSnapshot', e => renderSnapshot(e.detail));
    if (window.__JUCE__?.backend) {
      window.__JUCE__.backend.addEventListener('gitproSnapshot', renderSnapshot);
      window.__JUCE__.backend.emitEvent('gitproCommand', { command: 'dashboardReady' });
    }
    document.getElementById('ag-interface').addEventListener('change', sendCal);
    document.getElementById('ag-target').addEventListener('change', sendCal);
    document.getElementById('ag-btn').addEventListener('click', () => { sendCal(); emitCmd({ command: 'startAutoAnalyze', seconds: 3 }); });
    document.getElementById('pdc-btn').addEventListener('click', () => emitCmd({ command: 'measurePdcLatency' }));
  </script>
</body>
</html>
)html";
    }
}
