import { useEffect, useState, useCallback } from 'react'

// ==============================================================================
// JUCE Bridge Types
// ==============================================================================
declare global {
  interface Window {
    chrome?: {
      webview?: {
        postMessage: (msg: string) => void
      }
    }
  }
}

interface ParameterState {
  [key: string]: number
}

interface JuceMessage {
  type: string
  data: unknown
}

// ==============================================================================
// JUCE Communication Utilities
// ==============================================================================
function sendToJuce(type: string, data: unknown) {
  const message = JSON.stringify({ type, data })

  // WebView2 channel
  if (window.chrome?.webview?.postMessage) {
    window.chrome.webview.postMessage(message)
    return
  }

  // Fallback: webkit message handler (macOS)
  // @ts-expect-error webkit is injected by JUCE on macOS
  if (window.webkit?.messageHandlers?.juce?.postMessage) {
    // @ts-expect-error webkit is injected by JUCE on macOS
    window.webkit.messageHandlers.juce.postMessage(message)
    return
  }

  console.log('[Web] JUCE bridge not available, message:', type, data)
}

function setParameter(id: string, value: number) {
  sendToJuce('setParameter', { id, value })
}

// ==============================================================================
// Main App Component
// ==============================================================================
function App() {
  const [params, setParams] = useState<ParameterState>({
    gain: 0.5,
    mix: 1.0,
    bypass: 0,
  })

  // Handle messages from JUCE
  const handleJuceMessage = useCallback((event: MessageEvent) => {
    try {
      const msg: JuceMessage = typeof event.data === 'string'
        ? JSON.parse(event.data)
        : event.data

      if (msg.type === 'parameterState') {
        // Full parameter state update
        setParams(msg.data as ParameterState)
      } else if (msg.type === 'parameterChanged') {
        // Single parameter update
        const { id, value } = msg.data as { id: string; value: number }
        setParams(prev => ({ ...prev, [id]: value }))
      }
    } catch {
      // Ignore non-JSON messages
    }
  }, [])

  // Set up message listener
  useEffect(() => {
    window.addEventListener('message', handleJuceMessage)

    // Tell JUCE we're ready
    sendToJuce('ready', {})

    return () => {
      window.removeEventListener('message', handleJuceMessage)
    }
  }, [handleJuceMessage])

  // ==============================================================================
  // UI Handlers
  // ==============================================================================
  const handleGainChange = (value: number) => {
    setParams(prev => ({ ...prev, gain: value }))
    setParameter('gain', value)
  }

  const handleMixChange = (value: number) => {
    setParams(prev => ({ ...prev, mix: value }))
    setParameter('mix', value)
  }

  const handleBypassToggle = () => {
    const newValue = params.bypass > 0.5 ? 0 : 1
    setParams(prev => ({ ...prev, bypass: newValue }))
    setParameter('bypass', newValue)
  }

  // ==============================================================================
  // Render
  // ==============================================================================
  return (
    <div className="plugin-container">
      <header className="plugin-header">
        <h1>{{PLUGIN_NAME}}</h1>
        <button
          className={`bypass-button ${params.bypass > 0.5 ? 'active' : ''}`}
          onClick={handleBypassToggle}
        >
          {params.bypass > 0.5 ? 'BYPASSED' : 'ACTIVE'}
        </button>
      </header>

      <main className="plugin-controls">
        {/* Gain Control */}
        <div className="control-group">
          <label htmlFor="gain">Gain</label>
          <input
            id="gain"
            type="range"
            min={0}
            max={1}
            step={0.01}
            value={params.gain}
            onChange={(e) => handleGainChange(parseFloat(e.target.value))}
            className="slider"
          />
          <span className="value">{(params.gain * 100).toFixed(0)}%</span>
        </div>

        {/* Mix Control */}
        <div className="control-group">
          <label htmlFor="mix">Mix</label>
          <input
            id="mix"
            type="range"
            min={0}
            max={1}
            step={0.01}
            value={params.mix}
            onChange={(e) => handleMixChange(parseFloat(e.target.value))}
            className="slider"
          />
          <span className="value">{(params.mix * 100).toFixed(0)}%</span>
        </div>
      </main>

      <footer className="plugin-footer">
        <span>BeatConnect Plugin SDK Template</span>
      </footer>
    </div>
  )
}

export default App
