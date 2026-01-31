/**
 * React Hooks for JUCE 8 Parameter Binding
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
  defaultValue?: number;
}

interface SliderParamReturn {
  value: number;
  setValue: (value: number) => void;
  onDragStart: () => void;
  onDragEnd: () => void;
  isConnected: boolean;
}

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

    if (isConnected) {
      setValueState(state.getNormalisedValue());
    }

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
  defaultValue?: boolean;
}

interface ToggleParamReturn {
  value: boolean;
  setValue: (value: boolean) => void;
  toggle: () => void;
  isConnected: boolean;
}

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
  defaultIndex?: number;
}

interface ComboParamReturn {
  index: number;
  setIndex: (index: number) => void;
  choices: string[];
  isConnected: boolean;
}

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
