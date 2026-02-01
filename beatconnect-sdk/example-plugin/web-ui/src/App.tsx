import { useState, useEffect, useCallback, useRef } from 'react';
import { useSliderParam, useToggleParam } from './hooks/useJuceParam';
import { addEventListener, emitEvent, isInJuceWebView } from './lib/juce-bridge';

// ============================================================================
// Rotary Knob Component
// ============================================================================

interface KnobProps {
  value: number;
  min: number;
  max: number;
  label: string;
  unit?: string;
  onChange: (value: number) => void;
  onDragStart: () => void;
  onDragEnd: () => void;
}

function Knob({ value, min, max, label, unit = '%', onChange, onDragStart, onDragEnd }: KnobProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const isDragging = useRef(false);
  const startY = useRef(0);
  const startValue = useRef(0);

  // Normalize value to 0-1 range
  const normalizedValue = (value - min) / (max - min);

  // Arc parameters
  const size = 100;
  const strokeWidth = 6;
  const radius = (size - strokeWidth) / 2 - 8;
  const center = size / 2;

  // Arc angles: 135° to 405° (270° sweep)
  const startAngle = 135;
  const endAngle = 405;
  const sweepAngle = endAngle - startAngle;

  // Calculate arc paths
  const polarToCartesian = (angle: number) => {
    const rad = (angle - 90) * Math.PI / 180;
    return {
      x: center + radius * Math.cos(rad),
      y: center + radius * Math.sin(rad)
    };
  };

  const describeArc = (startA: number, endA: number) => {
    const start = polarToCartesian(endA);
    const end = polarToCartesian(startA);
    const largeArc = endA - startA <= 180 ? 0 : 1;
    return `M ${start.x} ${start.y} A ${radius} ${radius} 0 ${largeArc} 0 ${end.x} ${end.y}`;
  };

  const trackPath = describeArc(startAngle, endAngle);
  const valueAngle = startAngle + normalizedValue * sweepAngle;
  const valuePath = normalizedValue > 0.01 ? describeArc(startAngle, valueAngle) : '';

  // Indicator position
  const indicatorAngle = valueAngle;
  const indicatorRadius = radius - 12;
  const indicatorPos = polarToCartesian(indicatorAngle);
  const indicatorInner = {
    x: center + indicatorRadius * Math.cos((indicatorAngle - 90) * Math.PI / 180),
    y: center + indicatorRadius * Math.sin((indicatorAngle - 90) * Math.PI / 180)
  };

  // Display value
  const displayValue = unit === '%'
    ? Math.round(normalizedValue * 100)
    : value.toFixed(1);

  // Mouse/touch handling
  const handleStart = useCallback((clientY: number) => {
    isDragging.current = true;
    startY.current = clientY;
    startValue.current = value;
    onDragStart();
  }, [value, onDragStart]);

  const handleMove = useCallback((clientY: number) => {
    if (!isDragging.current) return;

    const deltaY = startY.current - clientY;
    const sensitivity = (max - min) / 200; // 200px for full range
    const newValue = Math.max(min, Math.min(max, startValue.current + deltaY * sensitivity));
    onChange(newValue);
  }, [min, max, onChange]);

  const handleEnd = useCallback(() => {
    if (isDragging.current) {
      isDragging.current = false;
      onDragEnd();
    }
  }, [onDragEnd]);

  useEffect(() => {
    const handleMouseMove = (e: MouseEvent) => handleMove(e.clientY);
    const handleMouseUp = () => handleEnd();
    const handleTouchMove = (e: TouchEvent) => handleMove(e.touches[0].clientY);
    const handleTouchEnd = () => handleEnd();

    window.addEventListener('mousemove', handleMouseMove);
    window.addEventListener('mouseup', handleMouseUp);
    window.addEventListener('touchmove', handleTouchMove);
    window.addEventListener('touchend', handleTouchEnd);

    return () => {
      window.removeEventListener('mousemove', handleMouseMove);
      window.removeEventListener('mouseup', handleMouseUp);
      window.removeEventListener('touchmove', handleTouchMove);
      window.removeEventListener('touchend', handleTouchEnd);
    };
  }, [handleMove, handleEnd]);

  return (
    <div className="control">
      <div
        ref={containerRef}
        className="knob-container"
        onMouseDown={(e) => handleStart(e.clientY)}
        onTouchStart={(e) => handleStart(e.touches[0].clientY)}
      >
        <svg className="knob-svg" viewBox={`0 0 ${size} ${size}`}>
          {/* Background track */}
          <path className="knob-track" d={trackPath} />

          {/* Value arc */}
          {valuePath && <path className="knob-value" d={valuePath} />}

          {/* Center circle */}
          <circle className="knob-center" cx={center} cy={center} r={radius - 16} />

          {/* Indicator line */}
          <line
            className="knob-indicator"
            x1={indicatorInner.x}
            y1={indicatorInner.y}
            x2={indicatorPos.x}
            y2={indicatorPos.y}
            strokeWidth="3"
            strokeLinecap="round"
            stroke="currentColor"
          />
        </svg>
        <span className="value">{displayValue}</span>
      </div>
      <label>{label}</label>
    </div>
  );
}

// ============================================================================
// Activation Screen
// ============================================================================

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

// ============================================================================
// Main Plugin UI
// ============================================================================

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
          <Knob
            value={gain.value}
            min={0}
            max={2}
            label="GAIN"
            onChange={gain.setValue}
            onDragStart={gain.dragStart}
            onDragEnd={gain.dragEnd}
          />

          <Knob
            value={mix.value}
            min={0}
            max={1}
            label="MIX"
            onChange={mix.setValue}
            onDragStart={mix.dragStart}
            onDragEnd={mix.dragEnd}
          />
        </div>
      </main>

      <footer>
        <span>BeatConnect Example Plugin v1.0</span>
      </footer>
    </div>
  );
}

// ============================================================================
// App with Activation Flow
// ============================================================================

function App() {
  const [isActivated, setIsActivated] = useState(false);

  const handleActivated = useCallback(() => {
    setIsActivated(true);
  }, []);

  // Disable right-click context menu (this is a plugin UI, not a webpage)
  useEffect(() => {
    const handleContextMenu = (e: MouseEvent) => e.preventDefault();
    document.addEventListener('contextmenu', handleContextMenu);
    return () => document.removeEventListener('contextmenu', handleContextMenu);
  }, []);

  if (!isActivated) {
    return <ActivationScreen onActivated={handleActivated} />;
  }

  return <PluginUI />;
}

export default App;
