/**
 * {{PLUGIN_NAME}} - Web UI
 *
 * This React app runs inside JUCE's WebBrowserComponent via WebView2/WKWebView.
 * It uses JUCE 8's native relay system for automatic bidirectional parameter sync.
 *
 * Parameter IDs used here must EXACTLY match:
 * 1. C++ ParameterIDs.h (e.g., ParamIDs::gain)
 * 2. C++ relay creation (e.g., WebSliderRelay("gain"))
 */

import { useSliderParam, useToggleParam, useVisualizerData } from './hooks/useJuceParam';
import { isInJuceWebView } from './lib/juce-bridge';

// ==============================================================================
// Parameter IDs (must match C++ ParameterIDs.h)
// ==============================================================================
const ParamIDs = {
  gain: 'gain',
  mix: 'mix',
  bypass: 'bypass',
  // mode: 'mode',
} as const;

// ==============================================================================
// Visualizer Data Type (matches C++ sendVisualizerData())
// ==============================================================================
interface VisualizerData {
  inputLevel?: number;
  outputLevel?: number;
  isPlaying?: boolean;
}

// ==============================================================================
// Main App Component
// ==============================================================================
function App() {
  // Parameter hooks - automatically sync with JUCE
  const gain = useSliderParam(ParamIDs.gain, { defaultValue: 0.5 });
  const mix = useSliderParam(ParamIDs.mix, { defaultValue: 1.0 });
  const bypass = useToggleParam(ParamIDs.bypass, { defaultValue: false });

  // Example: ComboBox parameter
  // const mode = useComboParam(ParamIDs.mode, { defaultIndex: 0 });

  // Visualizer data from C++ (30 Hz updates)
  const vizData = useVisualizerData<VisualizerData>('visualizerData', {});

  // Connection status indicator
  const isConnected = isInJuceWebView();

  return (
    <div className="plugin-container">
      {/* Header */}
      <header className="plugin-header">
        <h1>{'{{PLUGIN_NAME}}'}</h1>
        <div className="header-controls">
          {/* Connection indicator */}
          <span className={`connection-indicator ${isConnected ? 'connected' : ''}`}>
            {isConnected ? 'Connected' : 'Dev Mode'}
          </span>

          {/* Bypass button */}
          <button
            className={`bypass-button ${bypass.value ? 'bypassed' : ''}`}
            onClick={bypass.toggle}
          >
            {bypass.value ? 'BYPASSED' : 'ACTIVE'}
          </button>
        </div>
      </header>

      {/* Main Controls */}
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
            value={gain.value}
            onMouseDown={gain.onDragStart}
            onMouseUp={gain.onDragEnd}
            onTouchStart={gain.onDragStart}
            onTouchEnd={gain.onDragEnd}
            onChange={(e) => gain.setValue(parseFloat(e.target.value))}
            className="slider"
          />
          <span className="value">{(gain.value * 100).toFixed(0)}%</span>
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
            value={mix.value}
            onMouseDown={mix.onDragStart}
            onMouseUp={mix.onDragEnd}
            onTouchStart={mix.onDragStart}
            onTouchEnd={mix.onDragEnd}
            onChange={(e) => mix.setValue(parseFloat(e.target.value))}
            className="slider"
          />
          <span className="value">{(mix.value * 100).toFixed(0)}%</span>
        </div>

        {/* Example: ComboBox control */}
        {/* {mode.choices.length > 0 && (
          <div className="control-group">
            <label htmlFor="mode">Mode</label>
            <select
              id="mode"
              value={mode.index}
              onChange={(e) => mode.setIndex(parseInt(e.target.value))}
              className="select"
            >
              {mode.choices.map((choice, i) => (
                <option key={i} value={i}>{choice}</option>
              ))}
            </select>
          </div>
        )} */}
      </main>

      {/* Visualizer Section (optional) */}
      {vizData.inputLevel !== undefined && (
        <section className="visualizer">
          <div className="meter">
            <div
              className="meter-fill"
              style={{ width: `${(vizData.inputLevel ?? 0) * 100}%` }}
            />
          </div>
        </section>
      )}

      {/* Footer */}
      <footer className="plugin-footer">
        <span>BeatConnect Plugin SDK</span>
      </footer>
    </div>
  );
}

export default App;
