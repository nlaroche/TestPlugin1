import { useState, useEffect, useCallback } from 'react';
import { useSliderParam, useToggleParam } from './hooks/useJuceParam';
import { addEventListener, emitEvent, isInJuceWebView } from './lib/juce-bridge';

// Activation screen component
function ActivationScreen({ onActivated }: { onActivated: () => void }) {
  const [status, setStatus] = useState<'checking' | 'ready'>('checking');

  useEffect(() => {
    if (!isInJuceWebView()) {
      // Dev mode - skip activation
      setTimeout(() => onActivated(), 500);
      return;
    }

    const handleActivationState = (data: unknown) => {
      const d = data as { isConfigured?: boolean; isActivated?: boolean };
      if (!d.isConfigured) {
        // No activation required
        onActivated();
        return;
      }
      if (d.isActivated) {
        onActivated();
      } else {
        setStatus('ready');
      }
    };

    // Register listener BEFORE emitting request
    const unsub = addEventListener('activationState', handleActivationState);
    emitEvent('getActivationStatus', {});

    return unsub;
  }, [onActivated]);

  if (status === 'checking') {
    return (
      <div className="activation-screen">
        <div className="logo">EXAMPLE PLUGIN</div>
        <div className="checking">Checking activation...</div>
      </div>
    );
  }

  return (
    <div className="activation-screen">
      <div className="logo">EXAMPLE PLUGIN</div>
      <div className="activation-prompt">Activation required</div>
    </div>
  );
}

// Main plugin UI
function PluginUI() {
  const gain = useSliderParam('gain', 1.0);
  const mix = useSliderParam('mix', 1.0);
  const bypass = useToggleParam('bypass', false);

  const [levels, setLevels] = useState({ inputLevel: 0, outputLevel: 0 });

  useEffect(() => {
    const unsub = addEventListener('visualizerData', (data: unknown) => {
      const d = data as { inputLevel?: number; outputLevel?: number };
      setLevels({
        inputLevel: d.inputLevel ?? 0,
        outputLevel: d.outputLevel ?? 0
      });
    });
    return unsub;
  }, []);

  return (
    <div className={`plugin-ui ${bypass.value ? 'bypassed' : ''}`}>
      <header>
        <h1>EXAMPLE PLUGIN</h1>
        <button
          className={`bypass-btn ${bypass.value ? 'active' : ''}`}
          onClick={bypass.toggle}
        >
          {bypass.value ? 'BYPASSED' : 'ACTIVE'}
        </button>
      </header>

      <main>
        <div className="meters">
          <div className="meter">
            <div className="meter-label">IN</div>
            <div className="meter-bar">
              <div
                className="meter-fill"
                style={{ height: `${Math.min(100, levels.inputLevel * 100)}%` }}
              />
            </div>
          </div>
          <div className="meter">
            <div className="meter-label">OUT</div>
            <div className="meter-bar">
              <div
                className="meter-fill"
                style={{ height: `${Math.min(100, levels.outputLevel * 100)}%` }}
              />
            </div>
          </div>
        </div>

        <div className="controls">
          <div className="control">
            <label>GAIN</label>
            <input
              type="range"
              min={0}
              max={2}
              step={0.01}
              value={gain.value}
              onMouseDown={gain.dragStart}
              onMouseUp={gain.dragEnd}
              onTouchStart={gain.dragStart}
              onTouchEnd={gain.dragEnd}
              onChange={(e) => gain.setValue(parseFloat(e.target.value))}
            />
            <span className="value">{Math.round(gain.value * 100)}%</span>
          </div>

          <div className="control">
            <label>MIX</label>
            <input
              type="range"
              min={0}
              max={1}
              step={0.01}
              value={mix.value}
              onMouseDown={mix.dragStart}
              onMouseUp={mix.dragEnd}
              onTouchStart={mix.dragStart}
              onTouchEnd={mix.dragEnd}
              onChange={(e) => mix.setValue(parseFloat(e.target.value))}
            />
            <span className="value">{Math.round(mix.value * 100)}%</span>
          </div>
        </div>
      </main>

      <footer>
        <span>BeatConnect Example Plugin v1.0</span>
      </footer>
    </div>
  );
}

// Main App with activation flow
function App() {
  const [isActivated, setIsActivated] = useState(false);

  const handleActivated = useCallback(() => {
    setIsActivated(true);
  }, []);

  if (!isActivated) {
    return <ActivationScreen onActivated={handleActivated} />;
  }

  return <PluginUI />;
}

export default App;
