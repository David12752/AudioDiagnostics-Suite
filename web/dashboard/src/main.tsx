import React from 'react';
import ReactDOM from 'react-dom/client';
import './styles.css';

type ProbeSnapshot = {
  uuid?: string;
  displayName?: string;
  peakDbfs?: number;
  noiseFloorDbfs?: number;
  snrDb?: number;
};

type DashboardSnapshot = {
  instanceUuid?: string;
  activeProbeCount?: number;
  transport?: string;
  probes?: ProbeSnapshot[];
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

function App() {
  const [snapshot, setSnapshot] = React.useState<DashboardSnapshot>(emptySnapshot);
  const probes = snapshot.probes ?? [];
  const firstProbe = probes[0];

  React.useEffect(() => {
    const onSnapshot = (event: Event) => {
      setSnapshot((event as CustomEvent<DashboardSnapshot>).detail);
    };

    window.addEventListener('gitproSnapshot', onSnapshot);
    window.__JUCE__?.backend?.addEventListener('gitproSnapshot', setSnapshot);
    window.__JUCE__?.backend?.emitEvent('gitproCommand', { command: 'dashboardReady' });

    return () => window.removeEventListener('gitproSnapshot', onSnapshot);
  }, []);

  return (
    <main>
      <header>
        <h1>GitPro Analyzer</h1>
        <span>{snapshot.instanceUuid ? `Analyzer ${snapshot.instanceUuid}` : 'Waiting for registry'}</span>
      </header>
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
            </article>
          ))}
        </section>
      </section>
    </main>
  );
}

ReactDOM.createRoot(document.getElementById('root') as HTMLElement).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);