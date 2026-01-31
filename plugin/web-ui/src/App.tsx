/**
 * DelayWave - Web UI
 * A wavey modulated delay effect
 */

import { useSliderParam, useToggleParam } from './hooks/useJuceParam';
import { isInJuceWebView } from './lib/juce-bridge';

// Parameter IDs (must match C++ ParameterIDs.h)
const ParamIDs = {
  time: 'time',
  feedback: 'feedback',
  mix: 'mix',
  modRate: 'modRate',
  modDepth: 'modDepth',
  tone: 'tone',
  bypass: 'bypass',
} as const;

// Knob component for cleaner UI
interface KnobProps {
  id: string;
  label: string;
  value: number;
  min: number;
  max: number;
  step?: number;
  unit?: string;
  formatValue?: (v: number) => string;
  onDragStart: () => void;
  onDragEnd: () => void;
  onChange: (v: number) => void;
}

function Knob({ id, label, value, min, max, step = 0.01, unit = '', formatValue, onDragStart, onDragEnd, onChange }: KnobProps) {
  const displayValue = formatValue ? formatValue(value) : `${Math.round(value)}${unit}`;

  return (
    <div className="knob-group">
      <label htmlFor={id}>{label}</label>
      <input
        id={id}
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        onMouseDown={onDragStart}
        onMouseUp={onDragEnd}
        onTouchStart={onDragStart}
        onTouchEnd={onDragEnd}
        onChange={(e) => onChange(parseFloat(e.target.value))}
        className="knob"
      />
      <span className="value">{displayValue}</span>
    </div>
  );
}

function App() {
  // Core delay parameters
  const time = useSliderParam(ParamIDs.time, { defaultValue: 300 });
  const feedback = useSliderParam(ParamIDs.feedback, { defaultValue: 0.4 });
  const mix = useSliderParam(ParamIDs.mix, { defaultValue: 0.5 });

  // Modulation parameters (the wavey stuff!)
  const modRate = useSliderParam(ParamIDs.modRate, { defaultValue: 0.5 });
  const modDepth = useSliderParam(ParamIDs.modDepth, { defaultValue: 0.3 });

  // Tone control
  const tone = useSliderParam(ParamIDs.tone, { defaultValue: 0.7 });

  // Bypass
  const bypass = useToggleParam(ParamIDs.bypass, { defaultValue: false });

  const isConnected = isInJuceWebView();

  return (
    <div className="plugin-container">
      {/* Header */}
      <header className="plugin-header">
        <div className="logo">
          <h1>DelayWave</h1>
          <span className="tagline">wavey delays</span>
        </div>
        <div className="header-controls">
          <span className={`connection-indicator ${isConnected ? 'connected' : ''}`}>
            {isConnected ? '●' : 'DEV'}
          </span>
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
        {/* Delay Section */}
        <section className="control-section delay-section">
          <h2>Delay</h2>
          <div className="knob-row">
            <Knob
              id="time"
              label="Time"
              value={time.value}
              min={10}
              max={1000}
              step={1}
              unit="ms"
              onDragStart={time.onDragStart}
              onDragEnd={time.onDragEnd}
              onChange={time.setValue}
            />
            <Knob
              id="feedback"
              label="Feedback"
              value={feedback.value}
              min={0}
              max={0.95}
              formatValue={(v) => `${Math.round(v * 100)}%`}
              onDragStart={feedback.onDragStart}
              onDragEnd={feedback.onDragEnd}
              onChange={feedback.setValue}
            />
            <Knob
              id="mix"
              label="Mix"
              value={mix.value}
              min={0}
              max={1}
              formatValue={(v) => `${Math.round(v * 100)}%`}
              onDragStart={mix.onDragStart}
              onDragEnd={mix.onDragEnd}
              onChange={mix.setValue}
            />
          </div>
        </section>

        {/* Modulation Section */}
        <section className="control-section mod-section">
          <h2>Wave</h2>
          <div className="knob-row">
            <Knob
              id="modRate"
              label="Rate"
              value={modRate.value}
              min={0.1}
              max={10}
              step={0.01}
              formatValue={(v) => `${v.toFixed(1)}Hz`}
              onDragStart={modRate.onDragStart}
              onDragEnd={modRate.onDragEnd}
              onChange={modRate.setValue}
            />
            <Knob
              id="modDepth"
              label="Depth"
              value={modDepth.value}
              min={0}
              max={1}
              formatValue={(v) => `${Math.round(v * 100)}%`}
              onDragStart={modDepth.onDragStart}
              onDragEnd={modDepth.onDragEnd}
              onChange={modDepth.setValue}
            />
            <Knob
              id="tone"
              label="Tone"
              value={tone.value}
              min={0}
              max={1}
              formatValue={(v) => v < 0.3 ? 'Dark' : v > 0.7 ? 'Bright' : 'Warm'}
              onDragStart={tone.onDragStart}
              onDragEnd={tone.onDragEnd}
              onChange={tone.setValue}
            />
          </div>
        </section>
      </main>

      {/* Footer */}
      <footer className="plugin-footer">
        <span>BeatConnect</span>
      </footer>
    </div>
  );
}

export default App;
