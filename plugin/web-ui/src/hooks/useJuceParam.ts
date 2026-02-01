/**
 * React Hooks for JUCE 8 Parameter Binding
 */

import { useState, useEffect, useCallback, useRef } from 'react';
import { getSliderState, getToggleState, isInJuceWebView, SliderState, ToggleState } from '../lib/juce-bridge';

// ==============================================================================
// useSliderParam - Continuous Float Parameters
// ==============================================================================

interface SliderParamReturn {
  value: number;
  setValue: (value: number) => void;
  dragStart: () => void;
  dragEnd: () => void;
  isConnected: boolean;
}

export function useSliderParam(paramId: string, defaultValue: number = 0.5): SliderParamReturn {
  const [value, setValueState] = useState(defaultValue);
  const stateRef = useRef<SliderState | null>(null);
  const isConnected = isInJuceWebView();

  // Re-fetch state when JUCE becomes available
  if (isConnected && !stateRef.current) {
    stateRef.current = getSliderState(paramId);
  }

  useEffect(() => {
    // Get state - this will be real if connected, mock if not
    const state = getSliderState(paramId);
    stateRef.current = state;

    if (isConnected) {
      setValueState(state.getScaledValue());
    }

    const listenerId = state.valueChangedEvent.addListener(() => {
      setValueState(state.getScaledValue());
    });

    return () => {
      state.valueChangedEvent.removeListener(listenerId);
    };
  }, [paramId, isConnected]);

  const setValue = useCallback((newValue: number) => {
    setValueState(newValue);
    stateRef.current?.setScaledValue(newValue);
  }, []);

  const dragStart = useCallback(() => {
    stateRef.current?.sliderDragStarted();
  }, []);

  const dragEnd = useCallback(() => {
    stateRef.current?.sliderDragEnded();
  }, []);

  return { value, setValue, dragStart, dragEnd, isConnected };
}

// ==============================================================================
// useToggleParam - Boolean Parameters
// ==============================================================================

interface ToggleParamReturn {
  value: boolean;
  setValue: (value: boolean) => void;
  toggle: () => void;
  isConnected: boolean;
}

export function useToggleParam(paramId: string, defaultValue: boolean = false): ToggleParamReturn {
  const [value, setValueState] = useState(defaultValue);
  const stateRef = useRef<ToggleState | null>(null);
  const isConnected = isInJuceWebView();

  // Re-fetch state when JUCE becomes available
  if (isConnected && !stateRef.current) {
    stateRef.current = getToggleState(paramId);
  }

  useEffect(() => {
    // Get state - this will be real if connected, mock if not
    const state = getToggleState(paramId);
    stateRef.current = state;

    if (isConnected) {
      setValueState(state.getValue());
    }

    const listenerId = state.valueChangedEvent.addListener(() => {
      setValueState(state.getValue());
    });

    return () => {
      state.valueChangedEvent.removeListener(listenerId);
    };
  }, [paramId, isConnected]);

  const setValue = useCallback((newValue: boolean) => {
    setValueState(newValue);
    stateRef.current?.setValue(newValue);
  }, []);

  const toggle = useCallback(() => {
    // Use local state for toggle since it's most up-to-date
    const newValue = !value;
    setValue(newValue);
  }, [value, setValue]);

  return { value, setValue, toggle, isConnected };
}
