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
        return R"html(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>GitPro Analyzer</title>
  <style>
    :root { color-scheme: dark; font-family: Inter, Segoe UI, sans-serif; background: #111418; color: #f4f5f6; }
    body { margin: 0; min-height: 100vh; background: #111418; }
    main { box-sizing: border-box; min-height: 100vh; padding: 24px; display: grid; grid-template-rows: auto 1fr; gap: 20px; }
    header { display: flex; justify-content: space-between; align-items: center; gap: 16px; }
    h1 { margin: 0; font-size: 22px; font-weight: 650; letter-spacing: 0; }
    .status { color: #a9b1bc; font-size: 13px; }
    .grid { display: grid; grid-template-columns: repeat(4, minmax(150px, 1fr)); gap: 12px; }
    .panel { border: 1px solid #2b323b; border-radius: 8px; background: #171c22; padding: 16px; min-height: 120px; }
    .label { color: #a9b1bc; font-size: 12px; text-transform: uppercase; letter-spacing: 0; }
    .value { margin-top: 8px; font-size: 28px; font-weight: 700; }
    select, button { width: 100%; box-sizing: border-box; margin-top: 10px; border: 1px solid #36414d; border-radius: 6px; background: #101419; color: #f4f5f6; padding: 10px 12px; font: inherit; }
    button { cursor: pointer; background: #2363d1; border-color: #3473df; font-weight: 650; }
    button:active { transform: translateY(1px); }
    .hidden { display: none !important; }
    .warning { border-color: #b94747; background: #24191b; }
    .clear { border-color: #36694b; }
    .advice { grid-column: 1 / -1; min-height: 72px; }
    .tuner { grid-column: span 2; min-height: 170px; }
    .tuner-readout { display: flex; align-items: baseline; gap: 12px; margin-top: 12px; }
    .note { font-size: 54px; font-weight: 800; }
    .cents { font-size: 24px; color: #a9b1bc; }
    .strobe { height: 12px; margin-top: 18px; border-radius: 999px; background: #26313c; overflow: hidden; }
    .needle { height: 100%; width: 22%; border-radius: 999px; background: #64d486; transform: translateX(180%); transition: transform 90ms linear; }
    .progress { height: 10px; margin-top: 12px; border-radius: 999px; background: #26313c; overflow: hidden; }
    .progress-fill { height: 100%; width: 0%; background: #64d486; transition: width 100ms linear; }
    .probe-list { display: grid; gap: 10px; grid-column: 1 / -1; }
    .probe { border: 1px solid #2b323b; border-radius: 8px; background: #171c22; padding: 14px 16px; display: grid; grid-template-columns: 1fr repeat(5, minmax(90px, auto)); gap: 12px; align-items: center; }
    .probe-name { font-weight: 650; overflow-wrap: anywhere; }
    .metric { color: #dce2e8; text-align: right; }
    @media (max-width: 720px) { .grid { grid-template-columns: 1fr; } main { padding: 16px; } }
    @media (max-width: 720px) { .probe { grid-template-columns: 1fr; } .metric { text-align: left; } }
  </style>
</head>
<body>
  <main>
    <header>
      <h1 id="appTitle">GitPro Analyzer</h1>
      <div class="status" id="status">Waiting for registry</div>
    </header>
    <section class="grid">
      <div class="panel analyzer-only"><div class="label">Active Probes</div><div class="value" id="activeProbes">0</div></div>
      <div class="panel analyzer-only"><div class="label">Transport</div><div class="value" id="transport">Idle</div></div>
      <div class="panel"><div class="label">Peak</div><div class="value" id="peak">--</div></div>
      <div class="panel probe-only hidden"><div class="label">Audio Interface Max Input</div><select id="interfaceSelect"><option value="12">+12 dBu</option><option value="18" selected>+18 dBu</option><option value="24">+24 dBu</option></select></div>
      <div class="panel probe-only hidden"><div class="label">Target Amp Sim</div><select id="targetSelect"><option value="-18">Vintage Amp Sim (-18 dBFS)</option><option value="-12" selected>Modern Amp Sim (-12 dBFS)</option><option value="-6">Hot Re-Amp Target (-6 dBFS)</option></select></div>
      <div class="panel probe-only hidden"><div class="label">Gain Calibration</div><button id="autoAnalyzeButton">Auto-Analyze Gain</button><div class="progress"><div class="progress-fill" id="analyzeProgress"></div></div><div class="metric" id="gainSummary">0.0 dB</div></div>
      <div class="panel tuner probe-only hidden"><div class="label">Strobe Tuner</div><div class="tuner-readout"><div class="note" id="tunerNote">--</div><div class="cents" id="tunerCents">--</div></div><div class="strobe"><div class="needle" id="tunerNeedle"></div></div><div class="metric" id="tunerFrequency">-- Hz</div></div>
      <div class="panel analyzer-only" id="clashPanel"><div class="label">Clash Score</div><div class="value" id="clashScore">0</div></div>
      <div class="panel analyzer-only"><div class="label">Phase Correlation</div><div class="value" id="phaseCorrelation">--</div></div>
      <div class="panel analyzer-only"><div class="label">Conflict Frequency</div><div class="value" id="conflictFrequency">--</div></div>
      <div class="panel advice analyzer-only"><div class="label">Advice</div><div class="value" id="advice">Waiting for at least two active Probes.</div></div>
      <div class="probe-list analyzer-only" id="probeList"></div>
    </section>
  </main>
  <script>
    const appTitle = document.getElementById('appTitle');
    const activeProbes = document.getElementById('activeProbes');
    const transport = document.getElementById('transport');
    const peak = document.getElementById('peak');
    const interfaceSelect = document.getElementById('interfaceSelect');
    const targetSelect = document.getElementById('targetSelect');
    const autoAnalyzeButton = document.getElementById('autoAnalyzeButton');
    const analyzeProgress = document.getElementById('analyzeProgress');
    const gainSummary = document.getElementById('gainSummary');
    const tunerNote = document.getElementById('tunerNote');
    const tunerCents = document.getElementById('tunerCents');
    const tunerNeedle = document.getElementById('tunerNeedle');
    const tunerFrequency = document.getElementById('tunerFrequency');
    const clashPanel = document.getElementById('clashPanel');
    const clashScore = document.getElementById('clashScore');
    const phaseCorrelation = document.getElementById('phaseCorrelation');
    const conflictFrequency = document.getElementById('conflictFrequency');
    const advice = document.getElementById('advice');
    const status = document.getElementById('status');
    const probeList = document.getElementById('probeList');

    function formatDb(value) {
      return Number.isFinite(value) ? `${value.toFixed(1)} dB` : '--';
    }

    function setMode(role) {
      document.querySelectorAll('.probe-only').forEach((element) => element.classList.toggle('hidden', role !== 'probe'));
      document.querySelectorAll('.analyzer-only').forEach((element) => element.classList.toggle('hidden', role === 'probe'));
      appTitle.textContent = role === 'probe' ? 'The Probe' : 'GitPro Analyzer';
    }

    function emitCommand(payload) {
      window.__JUCE__?.backend?.emitEvent('gitproCommand', payload);
    }

    function sendCalibration() {
      emitCommand({ command: 'setProbeCalibration', interfaceMaxInputDbU: Number(interfaceSelect.value), targetPeakDbfs: Number(targetSelect.value) });
    }

    function pitchToNote(frequency) {
      if (!Number.isFinite(frequency) || frequency <= 0) return { note: '--', cents: 0 };
      const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
      const midi = 69 + 12 * Math.log2(frequency / 440);
      const rounded = Math.round(midi);
      const cents = Math.round((midi - rounded) * 100);
      const octave = Math.floor(rounded / 12) - 1;
      return { note: `${noteNames[((rounded % 12) + 12) % 12]}${octave}`, cents };
    }

    function renderProbe(snapshot) {
      setMode('probe');
      status.textContent = snapshot.instanceUuid ? `Probe ${snapshot.instanceUuid}` : 'Probe active';
      peak.textContent = formatDb(snapshot.peakDbfs);
      interfaceSelect.value = String(Math.round(snapshot.interfaceMaxInputDbU ?? 18));
      targetSelect.value = String(Math.round(snapshot.targetPeakDbfs ?? -12));
      const note = pitchToNote(snapshot.pitchFrequencyHz);
      tunerNote.textContent = note.note;
      tunerCents.textContent = Number.isFinite(snapshot.pitchFrequencyHz) && snapshot.pitchFrequencyHz > 0 ? `${note.cents > 0 ? '+' : ''}${note.cents} cents` : '--';
      tunerNeedle.style.transform = `translateX(${180 + Math.max(-50, Math.min(50, note.cents)) * 3.2}%)`;
      tunerFrequency.textContent = Number.isFinite(snapshot.pitchFrequencyHz) && snapshot.pitchFrequencyHz > 0 ? `${snapshot.pitchFrequencyHz.toFixed(1)} Hz` : '-- Hz';
      analyzeProgress.style.width = `${Math.round((snapshot.autoAnalyzeProgress ?? 0) * 100)}%`;
      autoAnalyzeButton.textContent = snapshot.autoAnalyzeActive ? 'Analyzing...' : 'Auto-Analyze Gain';
      gainSummary.textContent = `Gain ${formatDb(snapshot.autoGainDb)} | Measured ${formatDb(snapshot.measuredCalibrationPeakDbfs)} | Offset ${formatDb(snapshot.requiredGainOffsetDb)}`;
    }

    function renderSnapshot(snapshot) {
      if (snapshot.role === 'probe') {
        renderProbe(snapshot);
        return;
      }

      setMode('analyzer');
      const probes = Array.isArray(snapshot.probes) ? snapshot.probes : [];
      activeProbes.textContent = String(snapshot.activeProbeCount ?? probes.length);
      transport.textContent = snapshot.transport ?? 'Idle';
      status.textContent = snapshot.instanceUuid ? `Analyzer ${snapshot.instanceUuid}` : 'Analyzer active';
      peak.textContent = probes.length > 0 ? formatDb(probes[0].peakDbfs) : '--';
      const matchmaker = snapshot.matchmaker ?? {};
      clashScore.textContent = Number.isFinite(matchmaker.clashScore) ? `${Math.round(matchmaker.clashScore)}` : '0';
      phaseCorrelation.textContent = Number.isFinite(matchmaker.phaseCorrelation) ? matchmaker.phaseCorrelation.toFixed(2) : '--';
      conflictFrequency.textContent = Number.isFinite(matchmaker.conflictFrequencyHz) && matchmaker.conflictFrequencyHz > 0 ? `${Math.round(matchmaker.conflictFrequencyHz)} Hz` : '--';
      advice.textContent = matchmaker.advice ?? 'Waiting for at least two active Probes.';
      clashPanel.classList.toggle('warning', Boolean(matchmaker.lowEndConflict || matchmaker.phaseDestruction));
      clashPanel.classList.toggle('clear', !Boolean(matchmaker.lowEndConflict || matchmaker.phaseDestruction));
      probeList.replaceChildren(...probes.map((probe) => {
        const row = document.createElement('div');
        row.className = 'probe';
        const identity = document.createElement('div');
        const name = document.createElement('div');
        name.className = 'probe-name';
        name.textContent = probe.displayName ?? 'Probe';
        const uuid = document.createElement('div');
        uuid.className = 'label';
        uuid.textContent = probe.uuid ?? '';
        identity.append(name, uuid);
        const peakMetric = document.createElement('div');
        peakMetric.className = 'metric';
        peakMetric.textContent = `Peak ${formatDb(probe.peakDbfs)}`;
        const snrMetric = document.createElement('div');
        snrMetric.className = 'metric';
        snrMetric.textContent = `SNR ${formatDb(probe.snrDb)}`;
        const noiseMetric = document.createElement('div');
        noiseMetric.className = 'metric';
        noiseMetric.textContent = `Noise ${formatDb(probe.noiseFloorDbfs)}`;
        const lowMetric = document.createElement('div');
        lowMetric.className = 'metric';
        lowMetric.textContent = `Low ${formatDb(probe.lowBandTotalEnergyDb)}`;
        const phaseMetric = document.createElement('div');
        phaseMetric.className = 'metric';
        phaseMetric.textContent = `LF Corr ${Number.isFinite(probe.lowFrequencyCorrelation) ? probe.lowFrequencyCorrelation.toFixed(2) : '--'}`;
        row.append(identity, peakMetric, snrMetric, noiseMetric, lowMetric, phaseMetric);
        return row;
      }));
    }

    window.addEventListener('gitproSnapshot', (event) => renderSnapshot(event.detail));

    if (window.__JUCE__?.backend) {
      window.__JUCE__.backend.addEventListener('gitproSnapshot', renderSnapshot);
      window.__JUCE__.backend.emitEvent('gitproCommand', { command: 'dashboardReady' });
    }

    interfaceSelect.addEventListener('change', sendCalibration);
    targetSelect.addEventListener('change', sendCalibration);
    autoAnalyzeButton.addEventListener('click', () => {
      sendCalibration();
      emitCommand({ command: 'startAutoAnalyze', seconds: 3 });
    });
  </script>
</body>
</html>
)html";
    }
}