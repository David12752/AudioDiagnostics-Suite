"use client"

import { useRef, useEffect, useState } from "react"

export function TheProbe() {
  return (
    <div className="w-full max-w-[840px] mx-auto bg-card rounded-lg border border-border overflow-hidden shadow-2xl">
      {/* Plugin Header */}
      <Header title="THE PROBE" icon="probe" version="2.1" />

      {/* Main Content - Fixed aspect ratio like real plugin */}
      <div className="h-[320px] p-3 flex gap-3">
        {/* Left: Vertical Meters */}
        <div className="w-[60px] flex-shrink-0">
          <VerticalMeters />
        </div>

        {/* Center: Tuner Area */}
        <div className="flex-1 flex flex-col gap-2">
          {/* Note + Cents Row */}
          <div className="h-[56px] flex gap-2">
            <NoteDisplay />
            <CentsDisplay />
          </div>
          {/* Strobe */}
          <div className="flex-1">
            <StrobeCanvas />
          </div>
        </div>

        {/* Right: Auto-Gain Panel */}
        <div className="w-[140px] flex-shrink-0">
          <AutoGainPanel />
        </div>
      </div>

      {/* Footer */}
      <Footer cpu="1.2%" label="Input Utility / Tuner" buffer="256" />
    </div>
  )
}

function Header({ title, icon, version }: { title: string; icon: "probe" | "analyzer"; version: string }) {
  const iconColor = icon === "probe" ? "from-led-amber to-led-amber/50" : "from-led-blue to-led-blue/50"
  return (
    <div className="h-8 bg-gradient-to-b from-metal-light to-metal-dark border-b border-border flex items-center justify-between px-3">
      <div className="flex items-center gap-2">
        <div className={`w-5 h-5 rounded bg-gradient-to-br ${iconColor} flex items-center justify-center`}>
          {icon === "probe" ? (
            <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor" className="text-background">
              <circle cx="8" cy="8" r="6" strokeWidth="2" stroke="currentColor" fill="none"/>
              <circle cx="8" cy="8" r="2" fill="currentColor"/>
            </svg>
          ) : (
            <svg width="12" height="12" viewBox="0 0 16 16" fill="currentColor" className="text-background">
              <path d="M1 14V9h2v5H1zm4 0V5h2v9H5zm4 0V2h2v12H9zm4 0V7h2v7h-2z"/>
            </svg>
          )}
        </div>
        <span className="font-bold text-foreground text-xs tracking-tight">{title}</span>
        <span className="text-[9px] text-muted-foreground font-mono">v{version}</span>
      </div>
      <div className="flex items-center gap-1.5 text-[9px] font-mono text-muted-foreground">
        <span className="w-1.5 h-1.5 rounded-full bg-led-green led-glow-green" />
        <span>44.1kHz</span>
      </div>
    </div>
  )
}

function Footer({ cpu, label, buffer }: { cpu: string; label: string; buffer: string }) {
  return (
    <div className="h-6 bg-metal-dark border-t border-border flex items-center justify-between px-3 text-[8px] font-mono text-muted-foreground">
      <span>CPU: {cpu}</span>
      <span>{label}</span>
      <span>Buffer: {buffer}</span>
    </div>
  )
}

function VerticalMeters() {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const animationRef = useRef<number>(0)
  const peakLRef = useRef(-20)
  const peakRRef = useRef(-20)
  const rmsLRef = useRef(-28)
  const rmsRRef = useRef(-28)
  const peakHoldLRef = useRef(-60)
  const peakHoldRRef = useRef(-60)
  const peakHoldTimerRef = useRef(0)
  const noiseFloorRef = useRef(-72)
  const snrRef = useRef(54)

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext("2d")
    if (!ctx) return

    const dpr = window.devicePixelRatio || 1
    canvas.width = 60 * dpr
    canvas.height = 280 * dpr
    canvas.style.width = "60px"
    canvas.style.height = "280px"

    const draw = (timestamp: number) => {
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0)
      const w = 60, h = 280

      // Simulate
      const base = -18 + Math.sin(timestamp * 0.002) * 10 + Math.random() * 4
      peakLRef.current = Math.max(base + Math.random() * 3, peakLRef.current - 1.2)
      peakRRef.current = Math.max(base + Math.random() * 3, peakRRef.current - 1.2)
      rmsLRef.current = Math.max(base - 6, rmsLRef.current - 0.6)
      rmsRRef.current = Math.max(base - 6, rmsRRef.current - 0.6)

      // Peak hold
      if (peakLRef.current > peakHoldLRef.current) {
        peakHoldLRef.current = peakLRef.current
        peakHoldTimerRef.current = timestamp
      }
      if (peakRRef.current > peakHoldRRef.current) {
        peakHoldRRef.current = peakRRef.current
      }
      if (timestamp - peakHoldTimerRef.current > 1500) {
        peakHoldLRef.current = Math.max(peakHoldLRef.current - 0.5, -60)
        peakHoldRRef.current = Math.max(peakHoldRRef.current - 0.5, -60)
      }

      // Background
      ctx.fillStyle = "oklch(0.06 0.01 250)"
      ctx.fillRect(0, 0, w, h)

      const barW = 18
      const barH = h - 40
      const startY = 24

      // Draw meter
      const drawMeter = (x: number, peak: number, rms: number, hold: number) => {
        // Background
        ctx.fillStyle = "oklch(0.10 0.01 250)"
        ctx.fillRect(x, startY, barW, barH)

        // RMS fill
        const rmsFill = Math.max(0, ((rms + 60) / 60)) * barH
        const rmsGrad = ctx.createLinearGradient(0, startY + barH, 0, startY)
        rmsGrad.addColorStop(0, "oklch(0.45 0.15 145)")
        rmsGrad.addColorStop(0.7, "oklch(0.55 0.18 145)")
        rmsGrad.addColorStop(0.85, "oklch(0.55 0.15 65)")
        rmsGrad.addColorStop(1, "oklch(0.50 0.20 25)")
        ctx.fillStyle = rmsGrad
        ctx.fillRect(x, startY + barH - rmsFill, barW, rmsFill)

        // Peak line (brighter)
        const peakY = startY + barH - Math.max(0, ((peak + 60) / 60)) * barH
        ctx.fillStyle = peak > -6 ? "oklch(0.65 0.25 25)" : peak > -12 ? "oklch(0.70 0.18 65)" : "oklch(0.70 0.22 145)"
        ctx.fillRect(x, peakY - 2, barW, 3)

        // Peak hold
        const holdY = startY + barH - Math.max(0, ((hold + 60) / 60)) * barH
        ctx.fillStyle = "oklch(0.90 0.01 250)"
        ctx.fillRect(x, holdY, barW, 1)

        // Segments
        ctx.fillStyle = "oklch(0.06 0.01 250)"
        for (let i = 0; i < barH; i += 3) {
          ctx.fillRect(x, startY + i, barW, 1)
        }
      }

      drawMeter(6, peakLRef.current, rmsLRef.current, peakHoldLRef.current)
      drawMeter(36, peakRRef.current, rmsRRef.current, peakHoldRRef.current)

      // Labels
      ctx.fillStyle = "oklch(0.50 0.01 250)"
      ctx.font = "bold 8px 'Geist Mono'"
      ctx.textAlign = "center"
      ctx.fillText("L", 15, 14)
      ctx.fillText("R", 45, 14)

      // dB scale
      ctx.font = "7px 'Geist Mono'"
      ctx.textAlign = "right"
      const dbs = [0, -6, -12, -24, -48]
      dbs.forEach(db => {
        const y = startY + barH - ((db + 60) / 60) * barH
        ctx.fillText(`${db}`, w - 2, y + 3)
      })

      // Peak readout
      ctx.fillStyle = peakLRef.current > -6 ? "oklch(0.65 0.25 25)" : "oklch(0.65 0.18 145)"
      ctx.font = "bold 9px 'Geist Mono'"
      ctx.textAlign = "center"
      ctx.fillText(`${Math.round(Math.max(peakLRef.current, peakRRef.current))}`, w / 2, h - 42)

      // Noise Floor + SNR LED readouts
      // Update values slowly
      if (Math.random() < 0.02) {
        noiseFloorRef.current = -75 + Math.random() * 10
        snrRef.current = 50 + Math.random() * 15
      }

      // Noise Floor
      ctx.fillStyle = "oklch(0.30 0.01 250)"
      ctx.font = "6px 'Geist Mono'"
      ctx.textAlign = "left"
      ctx.fillText("NF", 4, h - 26)
      ctx.fillStyle = "oklch(0.55 0.15 200)"
      ctx.font = "bold 8px 'Geist Mono'"
      ctx.textAlign = "right"
      ctx.fillText(`${Math.round(noiseFloorRef.current)}`, w - 4, h - 26)

      // SNR
      ctx.fillStyle = "oklch(0.30 0.01 250)"
      ctx.font = "6px 'Geist Mono'"
      ctx.textAlign = "left"
      ctx.fillText("SNR", 4, h - 14)
      const snrColor = snrRef.current > 55 ? "oklch(0.65 0.20 145)" : snrRef.current > 45 ? "oklch(0.70 0.16 65)" : "oklch(0.55 0.22 25)"
      ctx.fillStyle = snrColor
      ctx.font = "bold 8px 'Geist Mono'"
      ctx.textAlign = "right"
      ctx.fillText(`${Math.round(snrRef.current)}`, w - 4, h - 14)

      animationRef.current = requestAnimationFrame(draw)
    }

    animationRef.current = requestAnimationFrame(draw)
    return () => cancelAnimationFrame(animationRef.current)
  }, [])

  return (
    <div className="h-full rounded overflow-hidden panel-inset border border-border">
      <canvas ref={canvasRef} className="block" />
    </div>
  )
}

function NoteDisplay() {
  const [note] = useState("A")
  const [octave] = useState("4")
  const [freq] = useState("440.0")

  return (
    <div className="flex-1 bg-background rounded border border-border panel-inset flex items-center justify-center gap-1 px-3">
      <span className="text-3xl font-bold text-led-amber font-mono led-glow-amber leading-none">{note}</span>
      <span className="text-lg text-led-amber/70 font-mono">{octave}</span>
      <span className="text-[9px] text-muted-foreground font-mono ml-2">{freq}Hz</span>
    </div>
  )
}

function CentsDisplay() {
  const [cents] = useState(3)
  const inTune = Math.abs(cents) < 5

  return (
    <div className="w-[80px] bg-background rounded border border-border panel-inset flex flex-col items-center justify-center">
      <span className={`text-xl font-bold font-mono leading-none ${inTune ? "text-led-green led-glow-green" : "text-led-amber"}`}>
        {cents > 0 ? "+" : ""}{cents}
      </span>
      <span className="text-[8px] text-muted-foreground font-mono">CENTS</span>
    </div>
  )
}

function StrobeCanvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const containerRef = useRef<HTMLDivElement>(null)
  const animationRef = useRef<number>(0)

  useEffect(() => {
    const canvas = canvasRef.current
    const container = containerRef.current
    if (!canvas || !container) return
    const ctx = canvas.getContext("2d")
    if (!ctx) return

    const updateSize = () => {
      const dpr = window.devicePixelRatio || 1
      const rect = container.getBoundingClientRect()
      canvas.width = rect.width * dpr
      canvas.height = rect.height * dpr
      canvas.style.width = `${rect.width}px`
      canvas.style.height = `${rect.height}px`
    }
    updateSize()

    const draw = (timestamp: number) => {
      const dpr = window.devicePixelRatio || 1
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0)
      const rect = container.getBoundingClientRect()
      const w = rect.width, h = rect.height

      const pitchOffset = Math.sin(timestamp * 0.0008) * 12 + Math.sin(timestamp * 0.003) * 5

      // Background
      ctx.fillStyle = "oklch(0.06 0.01 250)"
      ctx.fillRect(0, 0, w, h)

      // Strobe bars
      const strobeSpeed = pitchOffset * 0.4
      const stripeW = 10
      const numStripes = Math.ceil(w / stripeW) + 2
      const inTune = Math.abs(pitchOffset) < 4

      for (let i = 0; i < numStripes; i++) {
        const x = ((i * stripeW + timestamp * strobeSpeed * 0.08) % (w + stripeW * 2)) - stripeW
        const intensity = inTune ? 0.9 : 0.5

        ctx.fillStyle = `oklch(0.65 0.16 65 / ${intensity * 0.7})`
        ctx.fillRect(x, 15, stripeW / 2, h - 30)
      }

      // Center reference
      ctx.strokeStyle = "oklch(0.40 0.12 240)"
      ctx.lineWidth = 1.5
      ctx.setLineDash([4, 3])
      ctx.beginPath()
      ctx.moveTo(w / 2, 10)
      ctx.lineTo(w / 2, h - 10)
      ctx.stroke()
      ctx.setLineDash([])

      // Needle
      const needleX = w / 2 + pitchOffset * 2.5
      ctx.shadowColor = inTune ? "oklch(0.70 0.22 145)" : "oklch(0.65 0.18 240)"
      ctx.shadowBlur = 10
      ctx.fillStyle = inTune ? "oklch(0.80 0.22 145)" : "oklch(0.70 0.18 240)"
      ctx.fillRect(needleX - 1.5, 8, 3, h - 16)
      ctx.shadowBlur = 0

      // Scale
      ctx.fillStyle = "oklch(0.35 0.01 250)"
      ctx.font = "7px 'Geist Mono'"
      ctx.textAlign = "center"
      for (let i = -50; i <= 50; i += 25) {
        const mx = w / 2 + i * 2.5
        ctx.fillRect(mx - 0.5, h - 12, 1, i === 0 ? 8 : 4)
        if (Math.abs(i) === 50 || i === 0) {
          ctx.fillText(`${i > 0 ? "+" : ""}${i}`, mx, h - 2)
        }
      }

      animationRef.current = requestAnimationFrame(draw)
    }

    animationRef.current = requestAnimationFrame(draw)
    return () => cancelAnimationFrame(animationRef.current)
  }, [])

  return (
    <div ref={containerRef} className="h-full rounded overflow-hidden panel-inset border border-border">
      <canvas ref={canvasRef} className="block w-full h-full" />
    </div>
  )
}

function AutoGainPanel() {
  const [maxInput, setMaxInput] = useState("-18 dBFS")
  const [targetLevel, setTargetLevel] = useState("-14 LUFS")
  const [isAnalyzing, setIsAnalyzing] = useState(false)
  const [offset, setOffset] = useState<string | null>(null)

  const handleAnalyze = () => {
    setIsAnalyzing(true)
    setTimeout(() => {
      setOffset(`+${(Math.random() * 4 + 1).toFixed(1)}`)
      setIsAnalyzing(false)
    }, 1200)
  }

  return (
    <div className="h-full bg-metal-dark rounded border border-border p-2 flex flex-col gap-2">
      <span className="text-[9px] text-muted-foreground font-mono uppercase tracking-wider text-center">Auto-Gain</span>

      <div className="flex flex-col gap-1.5">
        <label className="text-[8px] text-muted-foreground font-mono">MAX INPUT</label>
        <select
          value={maxInput}
          onChange={(e) => setMaxInput(e.target.value)}
          className="text-[9px] bg-background border border-border rounded px-1.5 py-1 font-mono text-foreground"
        >
          <option>-24 dBFS</option>
          <option>-18 dBFS</option>
          <option>-12 dBFS</option>
        </select>
      </div>

      <div className="flex flex-col gap-1.5">
        <label className="text-[8px] text-muted-foreground font-mono">TARGET</label>
        <select
          value={targetLevel}
          onChange={(e) => setTargetLevel(e.target.value)}
          className="text-[9px] bg-background border border-border rounded px-1.5 py-1 font-mono text-foreground"
        >
          <option>-23 LUFS</option>
          <option>-14 LUFS</option>
          <option>-9 LUFS</option>
        </select>
      </div>

      <button
        onClick={handleAnalyze}
        disabled={isAnalyzing}
        className={`w-full py-1.5 rounded text-[9px] font-mono uppercase border border-border tactile-button ${
          isAnalyzing ? "text-led-amber animate-pulse" : "text-foreground hover:border-accent"
        }`}
      >
        {isAnalyzing ? "..." : "Analyze"}
      </button>

      <div className="flex-1 bg-background rounded border border-border panel-inset flex flex-col items-center justify-center min-h-[48px]">
        <span className={`text-xl font-mono font-bold ${offset ? "text-led-green led-glow-green" : "text-muted-foreground/30"}`}>
          {offset || "--"}
        </span>
        <span className="text-[8px] text-muted-foreground font-mono">dB</span>
      </div>
    </div>
  )
}
