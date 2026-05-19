import React, { useRef, useEffect, useState, useCallback } from 'react';
import ReactDOM from 'react-dom/client';
import './styles.css';

// ג”€ג”€ג”€ Types ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€
type BandInfo = { index: number; lowerHz: number; upperHz: number; energyDb: number; phaseRadians?: number };
type ProbeInfo = { uuid?: string; displayName?: string; peakDbfs?: number; rmsDbfs?: number; crestFactorDb?: number; noiseFloorDbfs?: number; snrDb?: number; lowBandTotalEnergyDb?: number; dominantLowFrequencyHz?: number; lowFrequencyCorrelation?: number; lowBands?: BandInfo[] };
type MatchmakerInfo = { lowEndConflict?: boolean; phaseDestruction?: boolean; clashScore?: number; phaseCorrelation?: number; conflictFrequencyHz?: number; advice?: string };
type FxChainHealthInfo = { probeCrestFactorDb?: number; analyzerCrestFactorDb?: number; crestFactorDeltaDb?: number; heavilyCompressed?: boolean; dynamicsWarning?: string; probeLowFrequencyCorrelation?: number; analyzerLowFrequencyCorrelation?: number; phaseWarning?: boolean; stereoWarning?: string; pdcMeasuredSamples?: number; pdcMeasuredMilliseconds?: number; pdcDriftWarning?: boolean; pdcStatus?: string; pdcListening?: boolean };
type RealtimeInfo = { peakDbfs?: number; rmsDbfs?: number; crestFactorDb?: number; lowFrequencyCorrelation?: number; lowBandEnergiesDb?: number[]; localLowBandEnergiesDb?: number[]; probeLowBandEnergiesDb?: number[] };
type Snapshot = { role?: 'probe' | 'analyzer'; instanceUuid?: string; peakDbfs?: number; rmsDbfs?: number; crestFactorDb?: number; noiseFloorDbfs?: number; snrDb?: number; pitchFrequencyHz?: number; autoGainDb?: number; autoAnalyzeActive?: boolean; autoAnalyzeProgress?: number; interfaceMaxInputDbU?: number; targetPeakDbfs?: number; measuredCalibrationPeakDbfs?: number; requiredGainOffsetDb?: number; activeProbeCount?: number; transport?: string; probes?: ProbeInfo[]; matchmaker?: MatchmakerInfo; fxChainHealth?: FxChainHealthInfo; realtime?: RealtimeInfo };

declare global { interface Window { __JUCE__?: { backend?: { addEventListener: (id: string, fn: (s: Snapshot) => void) => void; emitEvent: (id: string, p: unknown) => void } } } }

// ג”€ג”€ג”€ Utilities ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€
function emitCmd(payload: unknown) { window.__JUCE__?.backend?.emitEvent('gitproCommand', payload); }
function fmtDb(v?: number) { return Number.isFinite(v) ? v!.toFixed(1) : '--'; }
function pitchToNote(hz?: number) {
  if (!hz || hz <= 0 || !Number.isFinite(hz)) return { note: '--', octave: '', cents: 0 };
  const n = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
  const midi = 69 + 12 * Math.log2(hz / 440), r = Math.round(midi);
  return { note: n[((r % 12) + 12) % 12], octave: String(Math.floor(r / 12) - 1), cents: Math.round((midi - r) * 100) };
}
const BAND_LO = [40, 55, 70,  90, 115, 135];
const BAND_HI = [55, 70, 90, 115, 135, 150];

// ג”€ג”€ג”€ App ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€
function App() {
  const [snap, setSnap] = useState<Snapshot>({});
  useEffect(() => {
    const handler = (e: Event) => setSnap((e as CustomEvent<Snapshot>).detail);
    window.addEventListener('gitproSnapshot', handler);
    window.__JUCE__?.backend?.addEventListener('gitproSnapshot', setSnap);
    window.__JUCE__?.backend?.emitEvent('gitproCommand', { command: 'dashboardReady' });
    return () => window.removeEventListener('gitproSnapshot', handler);
  }, []);
  if (!snap.role) return <div className="app-root"><div className="waiting-text">Connecting to pluginג€¦</div></div>;
  return (
    <div className="app-root">
      {snap.role === 'probe'    && <ProbePlugin    snap={snap} />}
      {snap.role === 'analyzer' && <AnalyzerPlugin snap={snap} />}
    </div>
  );
}

// ג”€ג”€ג”€ Shared chrome ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€
function PluginHeader({ title, role }: { title: string; role: 'probe' | 'analyzer' }) {
  return (
    <div className="plugin-header">
      <div className="ph-left">
        <div className={`plugin-icon ${role}`}>
          {role === 'probe'
            ? <svg width="12" height="12" viewBox="0 0 16 16" fill="none"><circle cx="8" cy="8" r="5.5" stroke="rgba(20,20,30,.9)" strokeWidth="2"/><circle cx="8" cy="8" r="2" fill="rgba(20,20,30,.9)"/></svg>
            : <svg width="12" height="12" viewBox="0 0 16 16" fill="rgba(20,20,30,.9)"><path d="M1 14V9h2v5H1zm4 0V5h2v9H5zm4 0V2h2v12H9zm4 0V7h2v7h-2z"/></svg>}
        </div>
        <span className="plugin-title">{title}</span>
        <span className="plugin-version">v2.1</span>
      </div>
      <div className="ph-right"><span className="status-led" /><span>44.1kHz</span></div>
    </div>
  );
}
function PluginFooter({ label }: { label: string }) {
  return <div className="plugin-footer"><span>GitPro Suite</span><span>{label}</span><span>v2.1</span></div>;
}

// ג”€ג”€ג”€ Probe ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€
function ProbePlugin({ snap }: { snap: Snapshot }) {
  const pitch = pitchToNote(snap.pitchFrequencyHz);
  const inTune = Math.abs(pitch.cents) < 5 && (snap.pitchFrequencyHz ?? 0) > 20;
  const hasResult = Number.isFinite(snap.autoGainDb) && snap.autoGainDb !== 0;
  const sendCal = useCallback((iface: number, target: number) => {
    emitCmd({ command: 'setProbeCalibration', interfaceMaxInputDbU: iface, targetPeakDbfs: target });
  }, []);
  return (
    <div className="plugin-wrap">
      <PluginHeader title="THE PROBE" role="probe" />
      <div className="plugin-body">
        <div className="probe-meters">
          <VerticalMeters peakDbfs={snap.peakDbfs ?? -120} rmsDbfs={snap.rmsDbfs ?? -120} noiseFloorDbfs={snap.noiseFloorDbfs ?? -75} snrDb={snap.snrDb ?? 54} />
        </div>
        <div className="probe-center">
          <div className="probe-note-row">
            <div className="panel note-panel">
              <span className={`note-letter${inTune ? ' text-green' : ''}`}>{pitch.note}</span>
              {pitch.octave && <span className="note-octave">{pitch.octave}</span>}
              {(snap.pitchFrequencyHz ?? 0) > 20 && <span className="note-freq">{snap.pitchFrequencyHz!.toFixed(1)} Hz</span>}
            </div>
            <div className="panel cents-panel">
              <span className={`cents-val ${inTune ? 'in-tune' : 'out-tune'}`}>
                {pitch.note !== '--' ? `${pitch.cents > 0 ? '+' : ''}${pitch.cents}` : '--'}
              </span>
              <span className="cents-label">CENTS</span>
            </div>
          </div>
          <div className="probe-strobe-row">
            <StrobeCanvas pitchHz={snap.pitchFrequencyHz ?? 0} />
          </div>
        </div>
        <div className="probe-gain">
          <div className="metal-panel auto-gain">
            <span className="ag-title">Auto-Gain</span>
            <div>
              <div className="ag-label">MAX INPUT</div>
              <select className="ag-select" value={Math.round(snap.interfaceMaxInputDbU ?? 18)} onChange={e => sendCal(Number(e.target.value), snap.targetPeakDbfs ?? -12)}>
                <option value={12}>+12 dBu</option>
                <option value={18}>+18 dBu</option>
                <option value={24}>+24 dBu</option>
              </select>
            </div>
            <div>
              <div className="ag-label">TARGET</div>
              <select className="ag-select" value={Math.round(snap.targetPeakDbfs ?? -12)} onChange={e => sendCal(snap.interfaceMaxInputDbU ?? 18, Number(e.target.value))}>
                <option value={-18}>-18 dBFS</option>
                <option value={-12}>-12 dBFS</option>
                <option value={-6}>-6 dBFS</option>
              </select>
            </div>
            <button className={`ag-btn${snap.autoAnalyzeActive ? ' analyzing' : ''}`} disabled={snap.autoAnalyzeActive} onClick={() => { sendCal(snap.interfaceMaxInputDbU ?? 18, snap.targetPeakDbfs ?? -12); emitCmd({ command: 'startAutoAnalyze', seconds: 3 }); }}>
              {snap.autoAnalyzeActive ? 'ג€¦' : 'Analyze'}
            </button>
            {snap.autoAnalyzeActive && (
              <div className="progress-bar"><div className="progress-fill" style={{ width: `${Math.round((snap.autoAnalyzeProgress ?? 0) * 100)}%` }} /></div>
            )}
            <div className="panel ag-result">
              <span className={`rv ${hasResult ? 'has' : 'no'}`}>
                {hasResult ? `${snap.autoGainDb! > 0 ? '+' : ''}${snap.autoGainDb!.toFixed(1)}` : '--'}
              </span>
              <span className="ru">dB</span>
            </div>
          </div>
        </div>
      </div>
      <PluginFooter label="Input Utility / Tuner" />
    </div>
  );
}

// ג”€ג”€ג”€ Analyzer ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€
function AnalyzerPlugin({ snap }: { snap: Snapshot }) {
  const mm = snap.matchmaker ?? {}, fx = snap.fxChainHealth ?? {}, rt = snap.realtime ?? {};
  const p0 = snap.probes?.[0];
  const hasClash = Boolean(mm.lowEndConflict || mm.phaseDestruction);
  return (
    <div className="plugin-wrap">
      <PluginHeader title="THE ANALYZER" role="analyzer" />
      <div className="plugin-body analyzer-body">
        <div className="spectrum-wrap">
          <div className="spectrum-hdr">
            <span className="spectrum-title">Low-Band Matchmaker Spectrum</span>
            {hasClash && (mm.conflictFrequencyHz ?? 0) > 0 && (
              <span className="clash-badge">{Math.round(mm.conflictFrequencyHz!)} Hz CLASH</span>
            )}
          </div>
          <SpectrumCanvas probeBands={rt.probeLowBandEnergiesDb ?? p0?.lowBands?.map(b => b.energyDb) ?? []} localBands={rt.localLowBandEnergiesDb ?? []} conflictHz={mm.conflictFrequencyHz} hasClash={hasClash} />
        </div>
        <div className="analyzer-bottom">
          <div className="phase-col">
            <PhaseCanvas value={mm.phaseCorrelation ?? 1} />
          </div>
          <div className="stats-col">
            <div className="metal-panel stats-row">
              <StatCell label="True Peak" value={fmtDb(p0?.peakDbfs)} unit="dB" cls="amber" />
              <div className="stat-div" />
              <StatCell label="Crest" value={fmtDb(fx.analyzerCrestFactorDb)} unit="dB" cls="amber" />
              <div className="stat-div" />
              <StatCell label="Crest ־”" value={fmtDb(fx.crestFactorDeltaDb)} unit="dB" cls={fx.heavilyCompressed ? 'red' : 'green'} />
              <div className="stat-div" />
              <StatCell label="Stereo Evo" value={Number.isFinite(fx.analyzerLowFrequencyCorrelation) ? fx.analyzerLowFrequencyCorrelation!.toFixed(2) : '--'} cls={fx.phaseWarning ? 'red' : 'blue'} />
              <div className="stat-div" />
              <PdcCell pdcStatus={fx.pdcStatus ?? 'Idle'} samples={fx.pdcMeasuredSamples} ms={fx.pdcMeasuredMilliseconds} drift={fx.pdcDriftWarning ?? false} listening={fx.pdcListening ?? false} />
            </div>
          </div>
        </div>
      </div>
      <PluginFooter label="Master Analysis / Metering" />
    </div>
  );
}

function StatCell({ label, value, unit, cls }: { label: string; value: string; unit?: string; cls: string }) {
  return (
    <div className="stat-cell">
      <div style={{ display: 'flex', alignItems: 'baseline', gap: 2 }}>
        <span className={`stat-val ${cls}`}>{value}</span>
        {unit && <span className="stat-unit">{unit}</span>}
      </div>
      <span className="stat-lbl">{label}</span>
    </div>
  );
}

function PdcCell({ pdcStatus, samples, ms, drift, listening }: { pdcStatus: string; samples?: number; ms?: number; drift: boolean; listening: boolean }) {
  const hasSmp = Number.isFinite(samples);
  const ledCls = listening ? 'active' : drift ? 'err' : hasSmp ? 'ok' : '';
  const valCls = drift ? 'err' : hasSmp ? 'ok' : 'none';
  return (
    <div className="pdc-area">
      <span className={`pdc-led ${ledCls}`} />
      <button className={`pdc-btn${listening ? ' measuring' : ''}`} disabled={listening} onClick={() => emitCmd({ command: 'measurePdcLatency' })}>PDC</button>
      <div className="panel pdc-display">
        <span className={`pdc-val ${valCls}`}>{hasSmp ? String(Math.round(samples!)) : '--'}</span>
        <span className="pdc-sub">{hasSmp && Number.isFinite(ms) ? `${ms!.toFixed(1)}ms` : 'smp'}</span>
      </div>
    </div>
  );
}

// ג”€ג”€ג”€ Canvas: Vertical Meters ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€
function VerticalMeters({ peakDbfs, rmsDbfs, noiseFloorDbfs, snrDb }: { peakDbfs: number; rmsDbfs: number; noiseFloorDbfs: number; snrDb: number }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const rafRef = useRef(0);
  const liveRef = useRef({ peakDbfs, rmsDbfs, noiseFloorDbfs, snrDb });
  useEffect(() => { liveRef.current = { peakDbfs, rmsDbfs, noiseFloorDbfs, snrDb }; });
  useEffect(() => {
    const c = canvasRef.current!; const ctx = c.getContext('2d')!;
    const dpr = window.devicePixelRatio || 1;
    c.width = 60 * dpr; c.height = 280 * dpr; c.style.width = '60px'; c.style.height = '280px';
    const D = { peak: -120, rms: -120, hold: -120, holdTs: 0, noise: noiseFloorDbfs, snr: snrDb };
    const draw = (ts: number) => {
      const L = liveRef.current;
      D.peak = L.peakDbfs > D.peak ? L.peakDbfs : D.peak - 1.5;
      D.rms  = L.rmsDbfs  > D.rms  ? L.rmsDbfs  : D.rms  - 0.8;
      D.noise += 0.05 * (L.noiseFloorDbfs - D.noise); D.snr += 0.05 * (L.snrDb - D.snr);
      if (D.peak > D.hold) { D.hold = D.peak; D.holdTs = ts; }
      if (ts - D.holdTs > 1500) D.hold = Math.max(D.hold - 0.5, -60);
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      const w = 60, h = 280; ctx.fillStyle = 'oklch(0.06 0.01 250)'; ctx.fillRect(0, 0, w, h);
      const bW = 18, bH = h - 44, sY = 24;
      const bar = (x: number, pk: number, rms: number, hold: number) => {
        ctx.fillStyle = 'oklch(0.10 0.01 250)'; ctx.fillRect(x, sY, bW, bH);
        const rf = Math.max(0, ((rms + 60) / 60)) * bH;
        if (rf > 0) { const g = ctx.createLinearGradient(0, sY + bH, 0, sY); g.addColorStop(0, 'oklch(0.45 0.15 145)'); g.addColorStop(.7, 'oklch(0.55 0.18 145)'); g.addColorStop(.85, 'oklch(0.55 0.15 65)'); g.addColorStop(1, 'oklch(0.50 0.20 25)'); ctx.fillStyle = g; ctx.fillRect(x, sY + bH - rf, bW, rf); }
        const py = sY + bH - Math.max(0, ((pk + 60) / 60)) * bH;
        ctx.fillStyle = pk > -6 ? 'oklch(0.65 0.25 25)' : pk > -12 ? 'oklch(0.70 0.18 65)' : 'oklch(0.70 0.22 145)';
        ctx.fillRect(x, py - 2, bW, 3);
        const hy = sY + bH - Math.max(0, ((hold + 60) / 60)) * bH;
        ctx.fillStyle = 'oklch(0.90 0.01 250)'; ctx.fillRect(x, hy, bW, 1);
        ctx.fillStyle = 'oklch(0.06 0.01 250)'; for (let i = 0; i < bH; i += 3) ctx.fillRect(x, sY + i, bW, 1);
      };
      bar(6, D.peak, D.rms, D.hold); bar(36, D.peak, D.rms, D.hold);
      ctx.fillStyle = 'oklch(0.50 0.01 250)'; ctx.font = 'bold 8px monospace'; ctx.textAlign = 'center';
      ctx.fillText('L', 15, 14); ctx.fillText('R', 45, 14);
      ctx.font = '7px monospace'; ctx.textAlign = 'right';
      [0, -6, -12, -24, -48].forEach(db => { const y = sY + bH - ((db + 60) / 60) * bH; ctx.fillStyle = 'oklch(0.35 0.01 250)'; ctx.fillText(String(db), w - 2, y + 3); });
      ctx.fillStyle = D.peak > -6 ? 'oklch(0.65 0.25 25)' : 'oklch(0.65 0.18 145)'; ctx.font = 'bold 9px monospace'; ctx.textAlign = 'center'; ctx.fillText(String(Math.round(D.peak)), w / 2, h - 44);
      ctx.font = '6px monospace'; ctx.textAlign = 'left'; ctx.fillStyle = 'oklch(0.30 0.01 250)'; ctx.fillText('NF', 4, h - 28); ctx.fillStyle = 'oklch(0.55 0.15 200)'; ctx.font = 'bold 8px monospace'; ctx.textAlign = 'right'; ctx.fillText(String(Math.round(D.noise)), w - 4, h - 28);
      ctx.font = '6px monospace'; ctx.textAlign = 'left'; ctx.fillStyle = 'oklch(0.30 0.01 250)'; ctx.fillText('SNR', 4, h - 14);
      const sc = D.snr > 55 ? 'oklch(0.65 0.20 145)' : D.snr > 45 ? 'oklch(0.70 0.16 65)' : 'oklch(0.55 0.22 25)'; ctx.fillStyle = sc; ctx.font = 'bold 8px monospace'; ctx.textAlign = 'right'; ctx.fillText(String(Math.round(D.snr)), w - 4, h - 14);
      rafRef.current = requestAnimationFrame(draw);
    };
    rafRef.current = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(rafRef.current);
  }, []);
  return <div className="panel" style={{ height: '100%', borderRadius: 6, padding: 0 }}><canvas ref={canvasRef} style={{ display: 'block' }} /></div>;
}

// ג”€ג”€ג”€ Canvas: Strobe ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€
function StrobeCanvas({ pitchHz }: { pitchHz: number }) {
  const wrapRef = useRef<HTMLDivElement>(null); const canvasRef = useRef<HTMLCanvasElement>(null);
  const rafRef = useRef(0); const pitchRef = useRef(pitchHz);
  useEffect(() => { pitchRef.current = pitchHz; });
  useEffect(() => {
    const wrap = wrapRef.current!; const c = canvasRef.current!; const ctx = c.getContext('2d')!;
    const dpr = window.devicePixelRatio || 1;
    const resize = () => { const r = wrap.getBoundingClientRect(); c.width = r.width * dpr; c.height = r.height * dpr; c.style.width = r.width + 'px'; c.style.height = r.height + 'px'; };
    resize(); const ro = new ResizeObserver(resize); ro.observe(wrap);
    const draw = (ts: number) => {
      const hz = pitchRef.current; ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      const w = c.width / dpr, h = c.height / dpr; if (w <= 0 || h <= 0) { rafRef.current = requestAnimationFrame(draw); return; }
      let co = 0; if (hz > 20 && Number.isFinite(hz)) { const m = 69 + 12 * Math.log2(hz / 440); co = (m - Math.round(m)) * 100; }
      const inTune = Math.abs(co) < 5 && hz > 20;
      ctx.fillStyle = 'oklch(0.06 0.01 250)'; ctx.fillRect(0, 0, w, h);
      const spd = co * 0.4, sw = 10;
      for (let i = 0; i < Math.ceil(w / sw) + 2; i++) { const x = ((i * sw + ts * spd * 0.08) % (w + sw * 2)) - sw; ctx.fillStyle = `oklch(0.65 0.16 65 / ${inTune ? 0.6 : 0.3})`; ctx.fillRect(x, 15, sw / 2, h - 30); }
      ctx.strokeStyle = 'oklch(0.40 0.12 240)'; ctx.lineWidth = 1.5; ctx.setLineDash([4, 3]); ctx.beginPath(); ctx.moveTo(w / 2, 10); ctx.lineTo(w / 2, h - 10); ctx.stroke(); ctx.setLineDash([]);
      const nx = w / 2 + co * 2.5; ctx.shadowColor = inTune ? 'oklch(0.70 0.22 145)' : 'oklch(0.65 0.18 240)'; ctx.shadowBlur = 10; ctx.fillStyle = inTune ? 'oklch(0.80 0.22 145)' : 'oklch(0.70 0.18 240)'; ctx.fillRect(nx - 1.5, 8, 3, h - 16); ctx.shadowBlur = 0;
      ctx.fillStyle = 'oklch(0.35 0.01 250)'; ctx.font = '7px monospace'; ctx.textAlign = 'center';
      [-50, -25, 0, 25, 50].forEach(c2 => { const mx = w / 2 + c2 * 2.5; ctx.fillRect(mx - 0.5, h - 12, 1, c2 === 0 ? 8 : 4); if (c2 === -50 || c2 === 0 || c2 === 50) ctx.fillText(`${c2 > 0 ? '+' : ''}${c2}`, mx, h - 2); });
      rafRef.current = requestAnimationFrame(draw);
    };
    rafRef.current = requestAnimationFrame(draw);
    return () => { cancelAnimationFrame(rafRef.current); ro.disconnect(); };
  }, []);
  return <div ref={wrapRef} className="panel" style={{ height: '100%', borderRadius: 6, padding: 0 }}><canvas ref={canvasRef} style={{ display: 'block', width: '100%', height: '100%' }} /></div>;
}

// ג”€ג”€ג”€ Canvas: Spectrum ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€
function SpectrumCanvas({ probeBands, localBands, conflictHz, hasClash }: { probeBands: number[]; localBands: number[]; conflictHz?: number; hasClash: boolean }) {
  const wrapRef = useRef<HTMLDivElement>(null); const canvasRef = useRef<HTMLCanvasElement>(null);
  const rafRef = useRef(0);
  const dataRef = useRef({ probeBands, localBands, conflictHz, hasClash });
  useEffect(() => { dataRef.current = { probeBands, localBands, conflictHz, hasClash }; });
  useEffect(() => {
    const wrap = wrapRef.current!; const c = canvasRef.current!; const ctx = c.getContext('2d')!;
    const dpr = window.devicePixelRatio || 1;
    const resize = () => { const r = wrap.getBoundingClientRect(); c.width = r.width * dpr; c.height = r.height * dpr; c.style.width = r.width + 'px'; c.style.height = r.height + 'px'; };
    resize(); const ro = new ResizeObserver(resize); ro.observe(wrap);
    const draw = (ts: number) => {
      const { probeBands: pb, localBands: lb, conflictHz: clHz, hasClash: clash } = dataRef.current;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      const w = c.width / dpr, h = c.height / dpr; if (w <= 0 || h <= 0) { rafRef.current = requestAnimationFrame(draw); return; }
      ctx.fillStyle = 'oklch(0.06 0.01 250)'; ctx.fillRect(0, 0, w, h);
      const pL = 32, pR = 8, pT = 14, pB = 16;
      const fX = (hz: number) => pL + ((hz - 30) / 170) * (w - pL - pR);
      const dY = (db: number) => pT + (1 - Math.max(0, (db + 80) / 80)) * (h - pT - pB);
      ctx.strokeStyle = 'oklch(0.14 0.01 250)'; ctx.lineWidth = 0.5;
      [0, -20, -40, -60, -80].forEach(db => { const y = dY(db); ctx.beginPath(); ctx.moveTo(pL, y); ctx.lineTo(w - pR, y); ctx.stroke(); ctx.fillStyle = 'oklch(0.35 0.01 250)'; ctx.font = '7px monospace'; ctx.textAlign = 'right'; ctx.fillText(String(db), pL - 4, y + 3); });
      [40, 60, 80, 100, 120, 150].forEach(hz => { const x = fX(hz); ctx.beginPath(); ctx.moveTo(x, pT); ctx.lineTo(x, h - pB); ctx.stroke(); ctx.fillStyle = 'oklch(0.35 0.01 250)'; ctx.font = '7px monospace'; ctx.textAlign = 'center'; ctx.fillText(String(hz), x, h - 2); });
      const drawBands = (bands: number[], fc: string, sc: string) => {
        if (!bands.length) return;
        bands.forEach((db, i) => { if (i >= BAND_LO.length) return; const x1 = fX(BAND_LO[i]), x2 = fX(BAND_HI[i]), y = dY(db), bw = x2 - x1 - 2; if (bw <= 0) return; ctx.fillStyle = fc; ctx.fillRect(x1 + 1, y, bw, h - pB - y); ctx.strokeStyle = sc; ctx.lineWidth = 1.5; ctx.strokeRect(x1 + 1, y, bw, h - pB - y); });
      };
      drawBands(pb, 'oklch(0.60 0.18 240 / 0.3)', 'oklch(0.60 0.18 240)');
      drawBands(lb, 'oklch(0.70 0.16 65 / 0.25)',  'oklch(0.70 0.16 65)');
      if (clash && clHz && clHz > 0) { const cx = fX(clHz), pulse = 0.6 + Math.sin(ts * 0.006) * 0.3; ctx.shadowColor = 'oklch(0.55 0.22 25)'; ctx.shadowBlur = 8 * pulse; ctx.fillStyle = `oklch(0.55 0.22 25 / ${0.7 + pulse * 0.3})`; ctx.beginPath(); ctx.arc(cx, pT + 8, 4, 0, Math.PI * 2); ctx.fill(); ctx.shadowBlur = 0; ctx.fillStyle = 'oklch(0.98 0 0)'; ctx.font = 'bold 7px sans-serif'; ctx.textAlign = 'center'; ctx.fillText('!', cx, pT + 11); }
      ctx.fillStyle = 'oklch(0.60 0.18 240)'; ctx.fillRect(w - 65, 5, 7, 7); ctx.fillStyle = 'oklch(0.70 0.01 250)'; ctx.font = '8px monospace'; ctx.textAlign = 'left'; ctx.fillText('Probe', w - 55, 11);
      ctx.fillStyle = 'oklch(0.70 0.16 65)'; ctx.fillRect(w - 65, 17, 7, 7); ctx.fillText('Post-FX', w - 55, 23);
      rafRef.current = requestAnimationFrame(draw);
    };
    rafRef.current = requestAnimationFrame(draw);
    return () => { cancelAnimationFrame(rafRef.current); ro.disconnect(); };
  }, []);
  return <div ref={wrapRef} className="panel" style={{ flex: 1, borderRadius: 6, padding: 0 }}><canvas ref={canvasRef} style={{ display: 'block', width: '100%', height: '100%' }} /></div>;
}

// ג”€ג”€ג”€ Canvas: Phase Correlation ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€
function PhaseCanvas({ value }: { value: number }) {
  const canvasRef = useRef<HTMLCanvasElement>(null); const rafRef = useRef(0);
  const dispRef = useRef(value); const targetRef = useRef(value);
  useEffect(() => { targetRef.current = value; });
  useEffect(() => {
    const c = canvasRef.current!; const ctx = c.getContext('2d')!;
    const dpr = window.devicePixelRatio || 1;
    c.width = 200 * dpr; c.height = 72 * dpr; c.style.width = '200px'; c.style.height = '72px';
    const draw = () => {
      dispRef.current += 0.06 * (targetRef.current - dispRef.current);
      const v = Math.max(-1, Math.min(1, dispRef.current));
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      const w = 200, h = 72; ctx.fillStyle = 'oklch(0.06 0.01 250)'; ctx.fillRect(0, 0, w, h);
      const mX = 8, mW = w - 55, mH = 10, mY = 30;
      ctx.fillStyle = 'oklch(0.40 0.01 250)'; ctx.font = '7px monospace'; ctx.textAlign = 'left'; ctx.fillText('Phase Corr.', 8, 16);
      ctx.textAlign = 'center'; ctx.fillText('-1', mX + 4, mY - 3); ctx.fillText('0', mX + mW / 2, mY - 3); ctx.fillText('+1', mX + mW - 4, mY - 3);
      ctx.fillStyle = 'oklch(0.10 0.01 250)'; ctx.fillRect(mX, mY, mW, mH);
      ctx.fillStyle = 'oklch(0.20 0.01 250)'; ctx.fillRect(mX + mW / 2 - 0.5, mY - 1, 1, mH + 2);
      const col = v > 0.5 ? 'oklch(0.65 0.20 145)' : v > 0 ? 'oklch(0.70 0.16 65)' : 'oklch(0.55 0.22 25)';
      const cX = mX + mW / 2, fW = Math.abs(v) * (mW / 2); ctx.fillStyle = col;
      v >= 0 ? ctx.fillRect(cX, mY, fW, mH) : ctx.fillRect(cX - fW, mY, fW, mH);
      ctx.fillStyle = 'oklch(0.95 0 0)'; ctx.fillRect(cX + v * (mW / 2) - 1, mY - 2, 2, mH + 4);
      ctx.fillStyle = col; ctx.font = 'bold 14px monospace'; ctx.textAlign = 'left'; ctx.fillText(v.toFixed(2), w - 42, mY + 10);
      ctx.font = '7px monospace'; ctx.fillStyle = 'oklch(0.45 0.01 250)'; ctx.textAlign = 'center'; ctx.fillText(v > 0.5 ? 'STEREO' : v > 0 ? 'WIDE' : 'MONO', mX + mW / 2, mY + mH + 12);
      rafRef.current = requestAnimationFrame(draw);
    };
    rafRef.current = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(rafRef.current);
  }, []);
  return <div className="panel" style={{ height: '100%', borderRadius: 6, padding: 0 }}><canvas ref={canvasRef} style={{ display: 'block' }} /></div>;
}

// ג”€ג”€ג”€ Mount ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€ג”€
ReactDOM.createRoot(document.getElementById('root') as HTMLElement).render(
  <React.StrictMode><App /></React.StrictMode>
);
