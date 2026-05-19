import React from 'react';
import ReactDOM from 'react-dom/client';
import './styles.css';

type ProbeSnapshot = {
  uuid?: string;
  displayName?: string;
  peakDbfs?: number;
  noiseFloorDbfs?: number;
  snrDb?: number;
  lowBandTotalEnergyDb?: number;
  dominantLowFrequencyHz?: number;
  lowFrequencyCorrelation?: number;
};

type MatchmakerSnapshot = {
  lowEndConflict?: boolean;
  phaseDestruction?: boolean;
  clashScore?: number;
  maskingScore?: number;
  phaseCorrelation?: number;
  conflictFrequencyHz?: number;
  advice?: string;
};

type DashboardSnapshot = {
  role?: 'probe' | 'analyzer';
  instanceUuid?: string;
  activeProbeCount?: number;
  transport?: string;
  probes?: ProbeSnapshot[];
  matchmaker?: MatchmakerSnapshot;
  peakDbfs?: number;
  pitchFrequencyHz?: number;
  targetPeakDbfs?: number;
  interfaceMaxInputDbU?: number;
  autoGainDb?: number;
  measuredCalibrationPeakDbfs?: number;
  requiredGainOffsetDb?: number;
  autoAnalyzeActive?: boolean;
  autoAnalyzeProgress?: number;
};

declare global {
  interface Window {
    __JUCE__?: {
      backend?: {
        addEventListener: (eventId: string, listener: (payload: DashboardSnapshot) => void) => void;
        emitEvent: (eventId: string, payload: unknown) => void;
      };
    };
  }
}

const emptySnapshot: DashboardSnapshot = {
  activeProbeCount: 0,
  transport: 'Idle',
  probes: []
};

function formatDb(value: number | undefined) {
  return Number.isFinite(value) ? `${value!.toFixed(1)} dB` : '--';
}

function formatFrequency(value: number | undefined) {
  return Number.isFinite(value) && value! > 0 ? `${Math.round(value!)} Hz` : '--';
}

function formatCorrelation(value: number | undefined) {
  return Number.isFinite(value) ? value!.toFixed(2) : '--';
}

function pitchToNote(frequency: number | undefined) {
  if (!Number.isFinite(frequency) || frequency! <= 0) return { note: '--', cents: 0 };
  const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  const midi = 69 + 12 * Math.log2(frequency! / 440);
  const rounded = Math.round(midi);
  const cents = Math.round((midi - rounded) * 100);
  const octave = Math.floor(rounded / 12) - 1;
  return { note: `${noteNames[((rounded % 12) + 12) % 12]}${octave}`, cents };
}

function emitCommand(payload: unknown) {
  window.__JUCE__?.backend?.emitEvent('gitproCommand', payload);
}

function App() {
  const [snapshot, setSnapshot] = React.useState<DashboardSnapshot>(emptySnapshot);
  const probes = snapshot.probes ?? [];
  const firstProbe = probes[0];
  const matchmaker = snapshot.matchmaker ?? {};
  const hasWarning = Boolean(matchmaker.lowEndConflict || matchmaker.phaseDestruction);
  const isProbe = snapshot.role === 'probe';
  const note = pitchToNote(snapshot.pitchFrequencyHz);

  React.useEffect(() => {
    const onSnapshot = (event: Event) => {
      setSnapshot((event as CustomEvent<DashboardSnapshot>).detail);
    };

    window.addEventListener('gitproSnapshot', onSnapshot);
    window.__JUCE__?.backend?.addEventListener('gitproSnapshot', setSnapshot);
    window.__JUCE__?.backend?.emitEvent('gitproCommand', { command: 'dashboardReady' });

    return () => window.removeEventListener('gitproSnapshot', onSnapshot);
  }, []);

  const probeDashboard = (
    <section className="grid">
      <article>
        <p>Peak</p>
        <strong>{formatDb(snapshot.peakDbfs)}</strong>
      </article>
      <article>
        <p>Audio Interface Max Input</p>
        <select
          value={Math.round(snapshot.interfaceMaxInputDbU ?? 18)}
          onChange={(event) => emitCommand({ command: 'setProbeCalibration', interfaceMaxInputDbU: Number(event.target.value), targetPeakDbfs: snapshot.targetPeakDbfs ?? -12 })}
        >
          <option value={12}>+12 dBu</option>
          <option value={18}>+18 dBu</option>
          <option value={24}>+24 dBu</option>
        </select>
      </article>
      <article>
        <p>Target Amp Sim</p>
        <select
          value={Math.round(snapshot.targetPeakDbfs ?? -12)}
          onChange={(event) => emitCommand({ command: 'setProbeCalibration', interfaceMaxInputDbU: snapshot.interfaceMaxInputDbU ?? 18, targetPeakDbfs: Number(event.target.value) })}
        >
          <option value={-18}>Vintage Amp Sim (-18 dBFS)</option>
          <option value={-12}>Modern Amp Sim (-12 dBFS)</option>
          <option value={-6}>Hot Re-Amp Target (-6 dBFS)</option>
        </select>
      </article>
      <article>
        <p>Gain Calibration</p>
        <button onClick={() => emitCommand({ command: 'startAutoAnalyze', seconds: 3 })}>{snapshot.autoAnalyzeActive ? 'Analyzing...' : 'Auto-Analyze Gain'}</button>
        <div className="progress"><div className="progressFill" style={{ width: `${Math.round((snapshot.autoAnalyzeProgress ?? 0) * 100)}%` }} /></div>
        <strong>{formatDb(snapshot.autoGainDb)}</strong>
      </article>
      <article className="tuner">
        <p>Strobe Tuner</p>
        <div className="tunerReadout"><strong className="note">{note.note}</strong><span>{note.cents > 0 ? '+' : ''}{note.cents} cents</span></div>
        <div className="strobe"><div className="needle" style={{ transform: `translateX(${180 + Math.max(-50, Math.min(50, note.cents)) * 3.2}%)` }} /></div>
        <span>{Number.isFinite(snapshot.pitchFrequencyHz) && snapshot.pitchFrequencyHz! > 0 ? `${snapshot.pitchFrequencyHz!.toFixed(1)} Hz` : '-- Hz'}</span>
      </article>
      <article className="advice">
        <p>Calibration Result</p>
        <strong>Measured {formatDb(snapshot.measuredCalibrationPeakDbfs)} | Required offset {formatDb(snapshot.requiredGainOffsetDb)}</strong>
      </article>
    </section>
  );

  const analyzerDashboard = (
    <section className="grid">
      <article>
        <p>Active Probes</p>
        <strong>{snapshot.activeProbeCount ?? probes.length}</strong>
      </article>
      <article>
        <p>Transport</p>
        <strong>{snapshot.transport ?? 'Idle'}</strong>
      </article>
      <article>
        <p>Peak</p>
        <strong>{formatDb(firstProbe?.peakDbfs)}</strong>
      </article>
      <article className={hasWarning ? 'warning' : 'clear'}>
        <p>Clash Score</p>
        <strong>{Math.round(matchmaker.clashScore ?? 0)}</strong>
      </article>
      <article>
        <p>Phase Correlation</p>
        <strong>{formatCorrelation(matchmaker.phaseCorrelation)}</strong>
      </article>
      <article>
        <p>Conflict Frequency</p>
        <strong>{formatFrequency(matchmaker.conflictFrequencyHz)}</strong>
      </article>
      <article className="advice">
        <p>Advice</p>
        <strong>{matchmaker.advice ?? 'Waiting for at least two active Probes.'}</strong>
      </article>
      <section className="probeList">
        {probes.map((probe) => (
          <article className="probe" key={probe.uuid ?? probe.displayName}>
            <div>
              <h2>{probe.displayName ?? 'Probe'}</h2>
              <p>{probe.uuid ?? ''}</p>
            </div>
            <span>Peak {formatDb(probe.peakDbfs)}</span>
            <span>SNR {formatDb(probe.snrDb)}</span>
            <span>Noise {formatDb(probe.noiseFloorDbfs)}</span>
            <span>Low {formatDb(probe.lowBandTotalEnergyDb)}</span>
            <span>LF Corr {formatCorrelation(probe.lowFrequencyCorrelation)}</span>
          </article>
        ))}
      </section>
    </section>
  );

  return (
    <main>
      <header>
        <h1>{isProbe ? 'The Probe' : 'GitPro Analyzer'}</h1>
        <span>{snapshot.instanceUuid ? `${isProbe ? 'Probe' : 'Analyzer'} ${snapshot.instanceUuid}` : 'Waiting for registry'}</span>
      </header>
      {isProbe ? probeDashboard : analyzerDashboard}
    </main>
  );
}

ReactDOM.createRoot(document.getElementById('root') as HTMLElement).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);