/**
 * DelayWave - Web UI
 * Modern, premium design with proper arc knobs
 */

import { useState, useEffect, useCallback, useRef } from 'react';
import { useSliderParam, useToggleParam } from './hooks/useJuceParam';
import { addEventListener, emitEvent, isInJuceWebView } from './lib/juce-bridge';

// ============================================================================
// Rotary Knob Component - Fixed Arc Direction
// ============================================================================

interface KnobProps {
  value: number;
  min: number;
  max: number;
  label: string;
  unit?: string;
  decimals?: number;
  onChange: (value: number) => void;
  onDragStart: () => void;
  onDragEnd: () => void;
}

function Knob({ value, min, max, label, unit = '%', decimals = 0, onChange, onDragStart, onDragEnd }: KnobProps) {
  const isDragging = useRef(false);
  const startY = useRef(0);
  const startValue = useRef(0);

  // Normalize value to 0-1 range
  const normalizedValue = Math.max(0, Math.min(1, (value - min) / (max - min)));

  // Arc parameters
  const size = 80;
  const strokeWidth = 4;
  const radius = (size / 2) - strokeWidth - 4;
  const center = size / 2;

  // SVG angles: 0°=right, 90°=down, 180°=left, 270°=up
  // Knob arc: from 135° (bottom-left) clockwise 270° to 45° (bottom-right)
  const startAngle = 135;  // bottom-left
  const totalSweep = 270;  // degrees of rotation

  // Convert angle to SVG point (standard SVG coordinates, no rotation)
  const angleToPoint = (angleDeg: number) => {
    const rad = angleDeg * Math.PI / 180;
    return {
      x: center + radius * Math.cos(rad),
      y: center + radius * Math.sin(rad)
    };
  };

  // Current angle based on value (increases clockwise from 135°)
  const currentAngle = startAngle + normalizedValue * totalSweep;

  // Create arc path - always clockwise (sweep-flag=1)
  const createArc = (fromAngle: number, toAngle: number) => {
    const fromPoint = angleToPoint(fromAngle);
    const toPoint = angleToPoint(toAngle % 360);

    // Calculate sweep distance
    let sweep = toAngle - fromAngle;
    if (sweep < 0) sweep += 360;

    const largeArc = sweep > 180 ? 1 : 0;
    return `M ${fromPoint.x} ${fromPoint.y} A ${radius} ${radius} 0 ${largeArc} 1 ${toPoint.x} ${toPoint.y}`;
  };

  // Track: full arc from 135° to 405° (=45°)
  const trackPath = createArc(startAngle, startAngle + totalSweep);

  // Value: partial arc from 135° to current position
  const valuePath = normalizedValue > 0.01 ? createArc(startAngle, currentAngle) : '';

  // Indicator dot position
  const indicatorPos = angleToPoint(currentAngle % 360);

  // Format display value
  const formatValue = () => {
    if (unit === '%') {
      return Math.round(normalizedValue * 100);
    }
    if (decimals === 0) {
      return Math.round(value);
    }
    return value.toFixed(decimals);
  };

  // Mouse/touch handling
  const handleStart = useCallback((clientY: number) => {
    isDragging.current = true;
    startY.current = clientY;
    startValue.current = value;
    onDragStart();
    document.body.style.cursor = 'grabbing';
  }, [value, onDragStart]);

  const handleMove = useCallback((clientY: number) => {
    if (!isDragging.current) return;
    const deltaY = startY.current - clientY;
    const range = max - min;
    const sensitivity = range / 150;
    const newValue = Math.max(min, Math.min(max, startValue.current + deltaY * sensitivity));
    onChange(newValue);
  }, [min, max, onChange]);

  const handleEnd = useCallback(() => {
    if (isDragging.current) {
      isDragging.current = false;
      onDragEnd();
      document.body.style.cursor = '';
    }
  }, [onDragEnd]);

  useEffect(() => {
    const onMouseMove = (e: MouseEvent) => handleMove(e.clientY);
    const onMouseUp = () => handleEnd();
    const onTouchMove = (e: TouchEvent) => {
      e.preventDefault();
      handleMove(e.touches[0].clientY);
    };
    const onTouchEnd = () => handleEnd();

    window.addEventListener('mousemove', onMouseMove);
    window.addEventListener('mouseup', onMouseUp);
    window.addEventListener('touchmove', onTouchMove, { passive: false });
    window.addEventListener('touchend', onTouchEnd);

    return () => {
      window.removeEventListener('mousemove', onMouseMove);
      window.removeEventListener('mouseup', onMouseUp);
      window.removeEventListener('touchmove', onTouchMove);
      window.removeEventListener('touchend', onTouchEnd);
    };
  }, [handleMove, handleEnd]);

  return (
    <div className="knob">
      <div
        className="knob-body"
        onMouseDown={(e) => handleStart(e.clientY)}
        onTouchStart={(e) => handleStart(e.touches[0].clientY)}
      >
        <svg viewBox={`0 0 ${size} ${size}`} className="knob-svg">
          {/* Track */}
          <path d={trackPath} className="knob-track" />
          {/* Value arc */}
          {valuePath && <path d={valuePath} className="knob-arc" />}
          {/* Center */}
          <circle cx={center} cy={center} r={radius - 12} className="knob-center" />
          {/* Indicator dot */}
          <circle cx={indicatorPos.x} cy={indicatorPos.y} r="4" className="knob-dot" />
        </svg>
        <div className="knob-value">
          <span className="knob-number">{formatValue()}</span>
          <span className="knob-unit">{unit}</span>
        </div>
      </div>
      <div className="knob-label">{label}</div>
    </div>
  );
}

// ============================================================================
// Level Meter Component
// ============================================================================

function Meter({ value, label }: { value: number; label: string }) {
  const height = Math.min(100, Math.max(0, value * 100));

  return (
    <div className="meter">
      <div className="meter-track">
        <div className="meter-fill" style={{ height: `${height}%` }} />
      </div>
      <div className="meter-label">{label}</div>
    </div>
  );
}

// ============================================================================
// Activation Screen
// ============================================================================

type ActivationStatus = 'checking' | 'ready' | 'activating' | 'error' | 'success';

interface ActivationResult {
  success: boolean;
  status: string;
  message?: string;
}

function ActivationScreen({ onActivated }: { onActivated: () => void }) {
  const [status, setStatus] = useState<ActivationStatus>('checking');
  const [code, setCode] = useState('');
  const [error, setError] = useState('');

  useEffect(() => {
    if (!isInJuceWebView()) {
      // Dev mode - skip activation
      setTimeout(() => onActivated(), 300);
      return;
    }

    const handleActivationState = (data: unknown) => {
      const d = data as { isConfigured?: boolean; isActivated?: boolean };
      if (!d.isConfigured || d.isActivated) {
        onActivated();
      } else {
        setStatus('ready');
      }
    };

    const handleActivationResult = (data: unknown) => {
      const result = data as ActivationResult;
      if (result.success) {
        setStatus('success');
        setTimeout(() => onActivated(), 1000);
      } else {
        setStatus('error');
        setError(result.message || getErrorMessage(result.status));
      }
    };

    const unsub1 = addEventListener('activationState', handleActivationState);
    const unsub2 = addEventListener('activationResult', handleActivationResult);
    emitEvent('getActivationStatus', {});

    return () => {
      unsub1();
      unsub2();
    };
  }, [onActivated]);

  const getErrorMessage = (status: string): string => {
    switch (status) {
      case 'Invalid': return 'Invalid activation code. Please check and try again.';
      case 'Revoked': return 'This activation code has been revoked.';
      case 'MaxReached': return 'Maximum activations reached for this code.';
      case 'NetworkError': return 'Network error. Please check your connection.';
      case 'ServerError': return 'Server error. Please try again later.';
      default: return 'Activation failed. Please try again.';
    }
  };

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (!code.trim()) {
      setError('Please enter an activation code');
      return;
    }
    setStatus('activating');
    setError('');
    emitEvent('activate', { code: code.trim() });
  };

  const formatCode = (value: string): string => {
    // Remove non-alphanumeric except dashes
    const cleaned = value.replace(/[^a-zA-Z0-9-]/g, '').toUpperCase();
    return cleaned;
  };

  if (status === 'checking') {
    return (
      <div className="activation">
        <div className="activation-logo">DELAYWAVE</div>
        <div className="activation-status">Loading...</div>
      </div>
    );
  }

  if (status === 'success') {
    return (
      <div className="activation">
        <div className="activation-logo">DELAYWAVE</div>
        <div className="activation-success">Activated!</div>
      </div>
    );
  }

  return (
    <div className="activation">
      <div className="activation-logo">DELAYWAVE</div>
      <div className="activation-title">Enter Activation Code</div>

      <form className="activation-form" onSubmit={handleSubmit}>
        <input
          type="text"
          className="activation-input"
          placeholder="XXXX-XXXX-XXXX-XXXX"
          value={code}
          onChange={(e) => setCode(formatCode(e.target.value))}
          disabled={status === 'activating'}
          autoFocus
        />

        {error && <div className="activation-error">{error}</div>}

        <button
          type="submit"
          className="activation-button"
          disabled={status === 'activating' || !code.trim()}
        >
          {status === 'activating' ? 'Activating...' : 'Activate'}
        </button>
      </form>

      <div className="activation-help">
        Enter the activation code from your purchase email
      </div>
    </div>
  );
}

// ============================================================================
// Main Plugin UI
// ============================================================================

function PluginUI() {
  // Core delay parameters
  const time = useSliderParam('time', 300);
  const feedback = useSliderParam('feedback', 0.4);
  const mix = useSliderParam('mix', 0.5);

  // Modulation
  const modRate = useSliderParam('modRate', 0.5);
  const modDepth = useSliderParam('modDepth', 0.3);

  // Filter
  const tone = useSliderParam('tone', 0.7);

  // Bypass
  const bypass = useToggleParam('bypass', false);

  // Levels
  const [levels, setLevels] = useState({ input: 0, output: 0 });

  useEffect(() => {
    const unsub = addEventListener('visualizerData', (data: unknown) => {
      const d = data as { inputLevel?: number; outputLevel?: number };
      setLevels({
        input: d.inputLevel ?? 0,
        output: d.outputLevel ?? 0
      });
    });
    return unsub;
  }, []);

  return (
    <div className={`plugin ${bypass.value ? 'bypassed' : ''}`}>
      {/* Header */}
      <header className="header">
        <div className="logo">DELAYWAVE</div>
        <button
          className={`bypass-btn ${bypass.value ? 'active' : ''}`}
          onClick={bypass.toggle}
        >
          {bypass.value ? 'BYPASSED' : 'ACTIVE'}
        </button>
      </header>

      {/* Main controls */}
      <main className="main">
        {/* Meters */}
        <div className="meters">
          <Meter value={levels.input} label="IN" />
          <Meter value={levels.output} label="OUT" />
        </div>

        {/* Primary Controls */}
        <div className="control-section">
          <div className="section-title">DELAY</div>
          <div className="control-row">
            <Knob
              value={time.value}
              min={10}
              max={1000}
              label="TIME"
              unit="ms"
              onChange={time.setValue}
              onDragStart={time.dragStart}
              onDragEnd={time.dragEnd}
            />
            <Knob
              value={feedback.value}
              min={0}
              max={0.95}
              label="FEEDBACK"
              unit="%"
              onChange={feedback.setValue}
              onDragStart={feedback.dragStart}
              onDragEnd={feedback.dragEnd}
            />
            <Knob
              value={mix.value}
              min={0}
              max={1}
              label="MIX"
              unit="%"
              onChange={mix.setValue}
              onDragStart={mix.dragStart}
              onDragEnd={mix.dragEnd}
            />
          </div>
        </div>

        {/* Secondary Controls */}
        <div className="control-section">
          <div className="section-title">MODULATION</div>
          <div className="control-row">
            <Knob
              value={modRate.value}
              min={0.1}
              max={10}
              label="RATE"
              unit="Hz"
              decimals={1}
              onChange={modRate.setValue}
              onDragStart={modRate.dragStart}
              onDragEnd={modRate.dragEnd}
            />
            <Knob
              value={modDepth.value}
              min={0}
              max={1}
              label="DEPTH"
              unit="%"
              onChange={modDepth.setValue}
              onDragStart={modDepth.dragStart}
              onDragEnd={modDepth.dragEnd}
            />
            <Knob
              value={tone.value}
              min={0}
              max={1}
              label="TONE"
              unit="%"
              onChange={tone.setValue}
              onDragStart={tone.dragStart}
              onDragEnd={tone.dragEnd}
            />
          </div>
        </div>
      </main>

      {/* Footer */}
      <footer className="footer">
        BeatConnect · DelayWave v1.0
      </footer>
    </div>
  );
}

// ============================================================================
// App
// ============================================================================

function App() {
  const [ready, setReady] = useState(false);

  const handleActivated = useCallback(() => {
    setReady(true);
  }, []);

  if (!ready) {
    return <ActivationScreen onActivated={handleActivated} />;
  }

  return <PluginUI />;
}

export default App;
