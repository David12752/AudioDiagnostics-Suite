"use client"

import { useState } from "react"
import { TheProbe } from "./the-probe"
import { TheAnalyzer } from "./the-analyzer"

type View = "probe" | "analyzer"

export function AudioDiagnostics() {
  const [activeView, setActiveView] = useState<View>("probe")

  return (
    <div className="min-h-screen bg-background flex flex-col items-center justify-center p-4 md:p-8">
      {/* Plugin Selector */}
      <div className="mb-6 flex items-center gap-2 p-1 bg-metal-dark rounded-lg border border-border">
        <PluginTab
          active={activeView === "probe"}
          onClick={() => setActiveView("probe")}
          label="The Probe"
          color="amber"
        />
        <PluginTab
          active={activeView === "analyzer"}
          onClick={() => setActiveView("analyzer")}
          label="The Analyzer"
          color="blue"
        />
      </div>

      {/* Active Plugin */}
      {activeView === "probe" ? <TheProbe /> : <TheAnalyzer />}

      {/* Attribution */}
      <div className="mt-6 text-center text-[10px] text-muted-foreground font-mono">
        AudioDiagnostics Suite by AD Labs
      </div>
    </div>
  )
}

function PluginTab({
  active,
  onClick,
  label,
  color,
}: {
  active: boolean
  onClick: () => void
  label: string
  color: "amber" | "blue"
}) {
  const activeColor = color === "amber" ? "bg-led-amber/20 text-led-amber border-led-amber/30" : "bg-led-blue/20 text-led-blue border-led-blue/30"
  const dotColor = color === "amber" ? "bg-led-amber led-glow-amber" : "bg-led-blue led-glow-blue"

  return (
    <button
      onClick={onClick}
      className={`
        flex items-center gap-2 px-5 py-2.5 rounded-md font-mono text-sm transition-all duration-200 border
        ${active ? activeColor : "text-muted-foreground border-transparent hover:text-foreground hover:bg-muted/50"}
      `}
    >
      <span className={`w-2 h-2 rounded-full ${active ? dotColor : "bg-muted-foreground/30"}`} />
      {label}
    </button>
  )
}
