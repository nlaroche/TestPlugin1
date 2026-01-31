/**
 * React Hooks for JUCE 8 Parameter Binding
 *
 * These hooks provide easy React integration with JUCE 8's native WebView relay system.
 * They handle bidirectional sync automatically - changes from React update JUCE,
 * and changes from JUCE (automation, presets) update React.
 */

import { useState, useEffect, useCallback, useRef } from 'react';
import {
  getSliderState,
  getToggleState,
  getComboBoxState,
  addCustomEventListener,
  isInJuceWebView,
} from '../lib/juce-bridge';

// ==============================================================================
// useSliderParam - Continuous Float Parameters
// ==============================================================================

interface SliderParamOptions {
  /** Default value (0-1) when not running in JUCE */
  defaultValue?: number;
}

interface SliderParamReturn {
  /** Current value (0-1 normalized) */
  value: number;
  /** Set value (0-1 normalized) */
  setValue: (value: number) => void;
  /** Call when drag starts (for undo grouping) */
  onDragStart: () => void;
  /** Call when drag ends (for undo grouping) */
  onDragEnd: () => void;
  /** Whether running in JUCE WebView */
  isConnected: boolean;
}

/**
 * Hook for continuous float parameters (gain, mix, frequency, etc.)
 *
 * @param paramId - Must match the C++ relay identifier (e.g., "gain")
 * @param options - Configuration options
 *
 * @example
 * const { value, setValue, onDragStart, onDragEnd } = useSliderParam('gain');
 *
 * <input
 *   type="range"
 *   min={0} max={1} step={0.01}
 *   value={value}
 *   onMouseDown={onDragStart}
 *   onMouseUp={onDragEnd}
 *   onChange={(e) => setValue(parseFloat(e.target.value))}
 * />
 */
export function useSliderParam(
  paramId: string,
  options: SliderParamOptions = {}
): SliderParamReturn {
  const { defaultValue = 0.5 } = options;
  const [value, setValueState] = useState(defaultValue);
  const stateRef = useRef(getSliderState(paramId));
  const isConnected = isInJuceWebView();

  useEffect(() => {
    const state = stateRef.current;

    // Sync initial value from JUCE
    if (isConnected) {
      setValueState(state.getNormalisedValue());
    }

    // Listen for changes from JUCE (automation, presets, etc.)
    const listenerId = state.valueChangedEvent.addListener(() => {
      setValueState(state.getNormalisedValue());
    });

    return () => {
      state.valueChangedEvent.removeListener(listenerId);
    };
  }, [isConnected]);

  const setValue = useCallback((newValue: number) => {
    setValueState(newValue);
    stateRef.current.setNormalisedValue(newValue);
  }, []);

  const onDragStart = useCallback(() => {
    stateRef.current.sliderDragStarted();
  }, []);

  const onDragEnd = useCallback(() => {
    stateRef.current.sliderDragEnded();
  }, []);

  return { value, setValue, onDragStart, onDragEnd, isConnected };
}

// ==============================================================================
// useToggleParam - Boolean Parameters
// ==============================================================================

interface ToggleParamOptions {
  /** Default value when not running in JUCE */
  defaultValue?: boolean;
}

interface ToggleParamReturn {
  /** Current value */
  value: boolean;
  /** Set value */
  setValue: (value: boolean) => void;
  /** Toggle value */
  toggle: () => void;
  /** Whether running in JUCE WebView */
  isConnected: boolean;
}

/**
 * Hook for boolean parameters (bypass, enable, etc.)
 *
 * @param paramId - Must match the C++ relay identifier (e.g., "bypass")
 *
 * @example
 * const { value, toggle } = useToggleParam('bypass');
 *
 * <button onClick={toggle}>
 *   {value ? 'BYPASSED' : 'ACTIVE'}
 * </button>
 */
export function useToggleParam(
  paramId: string,
  options: ToggleParamOptions = {}
): ToggleParamReturn {
  const { defaultValue = false } = options;
  const [value, setValueState] = useState(defaultValue);
  const stateRef = useRef(getToggleState(paramId));
  const isConnected = isInJuceWebView();

  useEffect(() => {
    const state = stateRef.current;

    if (isConnected) {
      setValueState(state.getValue());
    }

    const listenerId = state.valueChangedEvent.addListener(() => {
      setValueState(state.getValue());
    });

    return () => {
      state.valueChangedEvent.removeListener(listenerId);
    };
  }, [isConnected]);

  const setValue = useCallback((newValue: boolean) => {
    setValueState(newValue);
    stateRef.current.setValue(newValue);
  }, []);

  const toggle = useCallback(() => {
    const newValue = !stateRef.current.getValue();
    setValue(newValue);
  }, [setValue]);

  return { value, setValue, toggle, isConnected };
}

// ==============================================================================
// useComboParam - Choice Parameters
// ==============================================================================

interface ComboParamOptions {
  /** Default index when not running in JUCE */
  defaultIndex?: number;
}

interface ComboParamReturn {
  /** Current selected index */
  index: number;
  /** Set selected index */
  setIndex: (index: number) => void;
  /** Available choices (from JUCE) */
  choices: string[];
  /** Whether running in JUCE WebView */
  isConnected: boolean;
}

/**
 * Hook for choice parameters (mode, algorithm, etc.)
 *
 * @param paramId - Must match the C++ relay identifier (e.g., "mode")
 *
 * @example
 * const { index, setIndex, choices } = useComboParam('mode');
 *
 * <select value={index} onChange={(e) => setIndex(parseInt(e.target.value))}>
 *   {choices.map((choice, i) => (
 *     <option key={i} value={i}>{choice}</option>
 *   ))}
 * </select>
 */
export function useComboParam(
  paramId: string,
  options: ComboParamOptions = {}
): ComboParamReturn {
  const { defaultIndex = 0 } = options;
  const [index, setIndexState] = useState(defaultIndex);
  const [choices, setChoices] = useState<string[]>([]);
  const stateRef = useRef(getComboBoxState(paramId));
  const isConnected = isInJuceWebView();

  useEffect(() => {
    const state = stateRef.current;

    if (isConnected) {
      setIndexState(state.getChoiceIndex());
      setChoices(state.getChoices());
    }

    const valueListener = state.valueChangedEvent.addListener(() => {
      setIndexState(state.getChoiceIndex());
    });

    const propsListener = state.propertiesChangedEvent.addListener(() => {
      setChoices(state.getChoices());
    });

    return () => {
      state.valueChangedEvent.removeListener(valueListener);
      state.propertiesChangedEvent.removeListener(propsListener);
    };
  }, [isConnected]);

  const setIndex = useCallback((newIndex: number) => {
    setIndexState(newIndex);
    stateRef.current.setChoiceIndex(newIndex);
  }, []);

  return { index, setIndex, choices, isConnected };
}

// ==============================================================================
// useVisualizerData - Custom Events from C++
// ==============================================================================

/**
 * Hook for receiving custom data from C++ (visualizers, meters, etc.)
 *
 * @param eventId - Event name used in C++ emitEventIfBrowserIsVisible()
 * @param initialValue - Initial value before first event
 *
 * @example
 * interface VizData { level: number; peak: number; }
 * const data = useVisualizerData<VizData>('visualizerData', { level: 0, peak: 0 });
 */
export function useVisualizerData<T>(eventId: string, initialValue: T): T {
  const [data, setData] = useState<T>(initialValue);

  useEffect(() => {
    const unsubscribe = addCustomEventListener(eventId, (payload) => {
      setData(payload as T);
    });

    return unsubscribe;
  }, [eventId]);

  return data;
}
