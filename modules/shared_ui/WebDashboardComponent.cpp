#include "WebDashboardComponent.h"

namespace gitpro::ui
{
    WebDashboardComponent::WebDashboardComponent()
    : browser(juce::WebBrowserComponent::Options {}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withNativeIntegrationEnabled()
        .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2 {}
          .withUserDataFolder(juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("GitProDiagnosticsSuite")
            .getChildFile("WebView2"))
          .withStatusBarDisabled())
        .withEventListener(juce::Identifier("gitproCommand"), [this](juce::var payload)
        {
          JUCE_ASSERT_MESSAGE_THREAD;

          if (commandHandler != nullptr)
            commandHandler(juce::JSON::toString(payload));
        }))
    {
        JUCE_ASSERT_MESSAGE_THREAD;

        addAndMakeVisible(browser);

        const auto html = createInitialHtml();
        const auto url = juce::String("data:text/html;charset=utf-8,") + juce::URL::addEscapeChars(html, false);
        browser.goToURL(url);
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
    .grid { display: grid; grid-template-columns: repeat(3, minmax(180px, 1fr)); gap: 12px; }
    .panel { border: 1px solid #2b323b; border-radius: 8px; background: #171c22; padding: 16px; min-height: 120px; }
    .label { color: #a9b1bc; font-size: 12px; text-transform: uppercase; letter-spacing: 0; }
    .value { margin-top: 8px; font-size: 28px; font-weight: 700; }
    .probe-list { display: grid; gap: 10px; grid-column: 1 / -1; }
    .probe { border: 1px solid #2b323b; border-radius: 8px; background: #171c22; padding: 14px 16px; display: grid; grid-template-columns: 1fr repeat(3, minmax(90px, auto)); gap: 12px; align-items: center; }
    .probe-name { font-weight: 650; overflow-wrap: anywhere; }
    .metric { color: #dce2e8; text-align: right; }
    @media (max-width: 720px) { .grid { grid-template-columns: 1fr; } main { padding: 16px; } }
    @media (max-width: 720px) { .probe { grid-template-columns: 1fr; } .metric { text-align: left; } }
  </style>
</head>
<body>
  <main>
    <header>
      <h1>GitPro Analyzer</h1>
      <div class="status" id="status">Waiting for registry</div>
    </header>
    <section class="grid">
      <div class="panel"><div class="label">Active Probes</div><div class="value" id="activeProbes">0</div></div>
      <div class="panel"><div class="label">Transport</div><div class="value" id="transport">Idle</div></div>
      <div class="panel"><div class="label">Peak</div><div class="value" id="peak">--</div></div>
      <div class="probe-list" id="probeList"></div>
    </section>
  </main>
  <script>
    const activeProbes = document.getElementById('activeProbes');
    const transport = document.getElementById('transport');
    const peak = document.getElementById('peak');
    const status = document.getElementById('status');
    const probeList = document.getElementById('probeList');

    function formatDb(value) {
      return Number.isFinite(value) ? `${value.toFixed(1)} dB` : '--';
    }

    function renderSnapshot(snapshot) {
      const probes = Array.isArray(snapshot.probes) ? snapshot.probes : [];
      activeProbes.textContent = String(snapshot.activeProbeCount ?? probes.length);
      transport.textContent = snapshot.transport ?? 'Idle';
      status.textContent = snapshot.instanceUuid ? `Analyzer ${snapshot.instanceUuid}` : 'Analyzer active';
      peak.textContent = probes.length > 0 ? formatDb(probes[0].peakDbfs) : '--';
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
        row.append(identity, peakMetric, snrMetric, noiseMetric);
        return row;
      }));
    }

    window.addEventListener('gitproSnapshot', (event) => renderSnapshot(event.detail));

    if (window.__JUCE__?.backend) {
      window.__JUCE__.backend.addEventListener('gitproSnapshot', renderSnapshot);
      window.__JUCE__.backend.emitEvent('gitproCommand', { command: 'dashboardReady' });
    }
  </script>
</body>
</html>
)html";
    }
}