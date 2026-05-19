"use client"

import { useRef, useEffect, useState } from "react"

export function TheAnalyzer() {
  return (
    <div className="w-full max-w-[840px] mx-auto bg-card rounded-lg border border-border overflow-hidden shadow-2xl">
      {/* Plugin Header */}
      <Header title="THE ANALYZER" icon="analyzer" version="2.1" />

      {/* Main Content */}
      <div className="h-[320px] p-3 flex flex-col gap-2">
        {/* Spectrum - dominant */}
        <div className="flex-1 min-h-0">
          <SpectrumCanvas />
        </div>

        {/* Bottom row: Phase + Stats */}
        <div className="h-[70px] flex gap-2">
          <div className="w-[200px] flex-shrink-0">
            <PhaseCorrelation />
          </div>
          <div className="flex-1">
            <StatsRow />
          </div>
        </div>
      </div>

      {/* Footer */}
      <Footer cpu="2.4%" label="Master Analysis / Metering" buffer="256" />
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

function SpectrumCanvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const containerRef = useRef<HTMLDivElement>(null)
  const animationRef = useRef<number>(0)
  const kickDataRef = useRef<number[]>(new Array(128).fill(0))
  const bassDataRef = useRef<number[]>(new Array(128).fill(0))

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
      const numBins = 128

      // Generate spectrum data
      for (let i = 0; i < numBins; i++) {
        const freq = 20 * Math.pow(1000, i / numBins)
        
        // Kick - centered around 60-80Hz
        const kickResponse = Math.exp(-Math.pow((freq - 70) / 35, 2)) * 0.85
        const kickNoise = Math.random() * 0.08 + Math.sin(timestamp * 0.003 + i * 0.1) * 0.08
        kickDataRef.current[i] = Math.max(0, kickResponse + kickNoise) * (0.7 + Math.random() * 0.3)

        // Bass - wider, centered around 80-120Hz
        const bassResponse = Math.exp(-Math.pow((freq - 90) / 50, 2)) * 0.75 +
                            Math.exp(-Math.pow((freq - 180) / 70, 2)) * 0.25
        const bassNoise = Math.random() * 0.06 + Math.sin(timestamp * 0.004 + i * 0.15) * 0.06
        bassDataRef.current[i] = Math.max(0, bassResponse + bassNoise) * (0.6 + Math.random() * 0.4)
      }

      // Background
      ctx.fillStyle = "oklch(0.06 0.01 250)"
      ctx.fillRect(0, 0, w, h)

      const padL = 35, padR = 10, padT = 16, padB = 18
      const graphW = w - padL - padR
      const graphH = h - padT - padB

      // Grid
      ctx.strokeStyle = "oklch(0.14 0.01 250)"
      ctx.lineWidth = 0.5

      // Horizontal (dB)
      for (let i = 0; i <= 4; i++) {
        const y = padT + (i * graphH) / 4
        ctx.beginPath()
        ctx.moveTo(padL, y)
        ctx.lineTo(w - padR, y)
        ctx.stroke()
        
        ctx.fillStyle = "oklch(0.35 0.01 250)"
        ctx.font = "7px 'Geist Mono'"
        ctx.textAlign = "right"
        ctx.fillText(`${-i * 15}`, padL - 4, y + 3)
      }

      // Vertical (freq)
      const freqMarkers = [30, 60, 100, 200, 500, 1000, 2000, 5000, 10000]
      freqMarkers.forEach(freq => {
        const x = padL + (Math.log10(freq / 20) / Math.log10(1000)) * graphW
        ctx.beginPath()
        ctx.moveTo(x, padT)
        ctx.lineTo(x, h - padB)
        ctx.stroke()

        ctx.fillStyle = "oklch(0.35 0.01 250)"
        ctx.font = "7px 'Geist Mono'"
        ctx.textAlign = "center"
        ctx.fillText(freq >= 1000 ? `${freq / 1000}k` : `${freq}`, x, h - 4)
      })

      // Draw curves
      const drawCurve = (data: number[], color: string, fillAlpha: number) => {
        ctx.beginPath()
        ctx.moveTo(padL, h - padB)

        for (let i = 0; i < data.length; i++) {
          const x = padL + (i / (data.length - 1)) * graphW
          const y = padT + (1 - data[i]) * graphH
          
          if (i === 0) {
            ctx.lineTo(x, y)
          } else {
            const prevX = padL + ((i - 1) / (data.length - 1)) * graphW
            const prevY = padT + (1 - data[i - 1]) * graphH
            const cpX = (prevX + x) / 2
            ctx.quadraticCurveTo(prevX, prevY, cpX, (prevY + y) / 2)
          }
        }
        ctx.lineTo(w - padR, h - padB)
        ctx.closePath()

        ctx.fillStyle = color.replace(")", ` / ${fillAlpha})`)
        ctx.fill()
        ctx.strokeStyle = color
        ctx.lineWidth = 1.5
        ctx.stroke()
      }

      drawCurve(kickDataRef.current, "oklch(0.60 0.18 240)", 0.25)
      drawCurve(bassDataRef.current, "oklch(0.70 0.16 65)", 0.2)

      // Clash marker at 70Hz
      const clashX = padL + (Math.log10(70 / 20) / Math.log10(1000)) * graphW
      const pulse = 0.6 + Math.sin(timestamp * 0.006) * 0.3
      ctx.shadowColor = "oklch(0.55 0.22 25)"
      ctx.shadowBlur = 8 * pulse
      ctx.fillStyle = `oklch(0.55 0.22 25 / ${0.7 + pulse * 0.3})`
      ctx.beginPath()
      ctx.arc(clashX, padT + 10, 4, 0, Math.PI * 2)
      ctx.fill()
      ctx.shadowBlur = 0
      ctx.fillStyle = "oklch(0.98 0 0)"
      ctx.font = "bold 7px 'Geist'"
      ctx.textAlign = "center"
      ctx.fillText("!", clashX, padT + 13)

      // Legend
      ctx.fillStyle = "oklch(0.60 0.18 240)"
      ctx.fillRect(w - 70, 6, 8, 8)
      ctx.fillStyle = "oklch(0.60 0.01 250)"
      ctx.font = "8px 'Geist'"
      ctx.textAlign = "left"
      ctx.fillText("Kick", w - 58, 13)

      ctx.fillStyle = "oklch(0.70 0.16 65)"
      ctx.fillRect(w - 70, 18, 8, 8)
      ctx.fillText("Bass", w - 58, 25)

      animationRef.current = requestAnimationFrame(draw)
    }

    animationRef.current = requestAnimationFrame(draw)
    return () => cancelAnimationFrame(animationRef.current)
  }, [])

  return (
    <div className="h-full flex flex-col gap-1">
      <div className="flex items-center justify-between px-1">
        <span className="text-[9px] text-muted-foreground font-mono uppercase tracking-wider">Matchmaker Spectrum</span>
        <span className="text-[9px] font-mono px-1.5 py-0.5 rounded bg-destructive/20 text-led-red">70Hz CLASH</span>
      </div>
      <div ref={containerRef} className="flex-1 rounded overflow-hidden panel-inset border border-border">
        <canvas ref={canvasRef} className="block w-full h-full" />
      </div>
    </div>
  )
}

function PhaseCorrelation() {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const animationRef = useRef<number>(0)
  const valueRef = useRef(0.7)
  const targetRef = useRef(0.7)

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext("2d")
    if (!ctx) return

    const dpr = window.devicePixelRatio || 1
    canvas.width = 200 * dpr
    canvas.height = 50 * dpr
    canvas.style.width = "200px"
    canvas.style.height = "50px"

    const draw = (timestamp: number) => {
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0)
      const w = 200, h = 50

      // Random target
      if (Math.random() < 0.015) {
        targetRef.current = 0.3 + Math.random() * 0.6
        if (Math.random() < 0.08) targetRef.current = -0.1 + Math.random() * 0.3
      }
      valueRef.current += (targetRef.current - valueRef.current) * 0.04
      const value = Math.max(-1, Math.min(1, valueRef.current + Math.sin(timestamp * 0.008) * 0.02))

      // Background
      ctx.fillStyle = "oklch(0.06 0.01 250)"
      ctx.fillRect(0, 0, w, h)

      const meterX = 8, meterW = w - 55, meterH = 10, meterY = 28

      // Labels
      ctx.fillStyle = "oklch(0.40 0.01 250)"
      ctx.font = "7px 'Geist Mono'"
      ctx.textAlign = "left"
      ctx.fillText("Phase", 8, 14)

      ctx.textAlign = "center"
      ctx.fillText("-1", meterX + 4, meterY - 2)
      ctx.fillText("0", meterX + meterW / 2, meterY - 2)
      ctx.fillText("+1", meterX + meterW - 4, meterY - 2)

      // Track
      ctx.fillStyle = "oklch(0.10 0.01 250)"
      ctx.fillRect(meterX, meterY, meterW, meterH)

      // Center
      ctx.fillStyle = "oklch(0.20 0.01 250)"
      ctx.fillRect(meterX + meterW / 2 - 0.5, meterY - 1, 1, meterH + 2)

      // Color
      const color = value > 0.5 ? "oklch(0.65 0.20 145)" : value > 0 ? "oklch(0.70 0.16 65)" : "oklch(0.55 0.22 25)"

      // Fill
      const centerX = meterX + meterW / 2
      const fillW = Math.abs(value) * (meterW / 2)
      ctx.fillStyle = color
      if (value >= 0) {
        ctx.fillRect(centerX, meterY, fillW, meterH)
      } else {
        ctx.fillRect(centerX - fillW, meterY, fillW, meterH)
      }

      // Needle
      const needleX = centerX + value * (meterW / 2)
      ctx.fillStyle = "oklch(0.95 0 0)"
      ctx.fillRect(needleX - 1, meterY - 2, 2, meterH + 4)

      // Value
      ctx.fillStyle = color
      ctx.font = "bold 14px 'Geist Mono'"
      ctx.textAlign = "left"
      ctx.fillText(value.toFixed(2), w - 42, meterY + 10)

      // Status
      ctx.font = "7px 'Geist Mono'"
      ctx.fillStyle = "oklch(0.45 0.01 250)"
      ctx.textAlign = "center"
      const status = value > 0.5 ? "STEREO" : value > 0 ? "WIDE" : "MONO"
      ctx.fillText(status, meterX + meterW / 2, meterY + meterH + 10)

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

function StatsRow() {
  const [pdcResult, setPdcResult] = useState<string | null>(null)
  const [measuring, setMeasuring] = useState(false)
  const [pdcStatus, setPdcStatus] = useState<"idle" | "measuring" | "success" | "error">("idle")
  const [crestDelta, setCrestDelta] = useState("+1.2")
  const [stereoEvo, setStereoEvo] = useState("0.82")

  const measurePDC = () => {
    setMeasuring(true)
    setPdcStatus("measuring")
    setTimeout(() => {
      const success = Math.random() > 0.1
      if (success) {
        setPdcResult(`${Math.floor(Math.random() * 1200 + 400)}`)
        setPdcStatus("success")
      } else {
        setPdcResult("ERR")
        setPdcStatus("error")
      }
      setMeasuring(false)
    }, 1200)
  }

  // Simulate data updates
  useEffect(() => {
    const interval = setInterval(() => {
      setCrestDelta(`${Math.random() > 0.5 ? "+" : "-"}${(Math.random() * 2).toFixed(1)}`)
      setStereoEvo((0.6 + Math.random() * 0.35).toFixed(2))
    }, 3000)
    return () => clearInterval(interval)
  }, [])

  const pdcStatusColors = {
    idle: "bg-muted-foreground/30",
    measuring: "bg-led-amber animate-pulse",
    success: "bg-led-green led-glow-green",
    error: "bg-led-red led-glow-red"
  }

  return (
    <div className="h-full bg-metal-dark rounded border border-border px-2 py-1.5 flex items-center gap-1.5 overflow-hidden">
      <StatCell label="True Peak" value="-0.3" unit="dB" color="amber" />
      <Divider />
      <StatCell label="LUFS-I" value="-14.2" color="green" />
      <Divider />
      <StatCell label="Crest" value="-6.2" unit="dB" color="amber" />
      <Divider />
      {/* Crest Factor Delta - new */}
      <StatCell label="Crest Delta" value={crestDelta} unit="dB" color={crestDelta.startsWith("+") ? "green" : "amber"} />
      <Divider />
      {/* Stereo Evolution - new */}
      <StatCell label="Stereo Evo" value={stereoEvo} color="blue" />
      <Divider />
      {/* PDC with Status LED */}
      <div className="flex items-center gap-1">
        <div className="flex items-center gap-1">
          <span className={`w-2 h-2 rounded-full ${pdcStatusColors[pdcStatus]}`} />
          <button
            onClick={measurePDC}
            disabled={measuring}
            className={`px-1.5 py-0.5 rounded text-[7px] font-mono uppercase border border-border tactile-button ${
              measuring ? "text-led-amber" : "text-muted-foreground hover:text-foreground"
            }`}
          >
            PDC
          </button>
        </div>
        <div className="w-[40px] h-[24px] bg-background rounded border border-border panel-inset flex flex-col items-center justify-center">
          <span className={`text-[9px] font-mono font-bold ${pdcStatus === "success" ? "text-led-green" : pdcStatus === "error" ? "text-led-red" : "text-muted-foreground/30"}`}>
            {pdcResult || "--"}
          </span>
          <span className="text-[5px] text-muted-foreground font-mono">smp</span>
        </div>
      </div>
    </div>
  )
}

function StatCell({ label, value, unit, color }: { label: string; value: string; unit?: string; color: "green" | "amber" | "blue" | string }) {
  const colors: Record<string, string> = {
    green: "text-led-green",
    amber: "text-led-amber",
    blue: "text-led-blue",
  }

  return (
    <div className="flex flex-col items-center justify-center min-w-[38px]">
      <div className="flex items-baseline gap-0.5">
        <span className={`text-xs font-mono font-bold ${colors[color] || colors.amber}`}>{value}</span>
        {unit && <span className="text-[6px] text-muted-foreground font-mono">{unit}</span>}
      </div>
      <span className="text-[6px] text-muted-foreground font-mono uppercase leading-tight">{label}</span>
    </div>
  )
}

function Divider() {
  return <div className="w-px h-8 bg-border" />
}
