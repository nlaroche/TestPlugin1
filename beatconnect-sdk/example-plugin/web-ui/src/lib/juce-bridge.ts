/**
 * JUCE 8 WebView Bridge
 *
 * Uses JUCE's built-in state accessors (getSliderState, getToggleState, etc.)
 * These are injected by JUCE when relays are registered with .withOptionsFrom()
 */

declare global {
  interface Window {
    __JUCE__?: {
      backend: {
        addEventListener: (event: string, callback: (data: unknown) => void) => [string, number];
        removeEventListener: (token: [string, number]) => void;
        emitEvent: (event: string, data: unknown) => void;
      };
      initialisationData?: {
        __juce__platform?: string;
      };
      getSliderState?: (id: string) => SliderState;
      getToggleState?: (id: string) => ToggleState;
      getComboBoxState?: (id: string) => ComboBoxState;
    };
  }
}

// =============================================================================
// Types
// =============================================================================

export interface SliderState {
  getNormalisedValue: () => number;
  setNormalisedValue: (value: number) => void;
  getScaledValue: () => number;
  setScaledValue: (value: number) => void;
  sliderDragStarted: () => void;
  sliderDragEnded: () => void;
  getProperties: () => { start: number; end: number; name: string };
  valueChangedEvent: {
    addListener: (callback: () => void) => number;
    removeListener: (id: number) => void;
  };
}

export interface ToggleState {
  getValue: () => boolean;
  setValue: (value: boolean) => void;
  valueChangedEvent: {
    addListener: (callback: () => void) => number;
    removeListener: (id: number) => void;
  };
}

export interface ComboBoxState {
  getChoiceIndex: () => number;
  setChoiceIndex: (index: number) => void;
  getChoices: () => string[];
  valueChangedEvent: {
    addListener: (callback: () => void) => number;
    removeListener: (id: number) => void;
  };
}

// =============================================================================
// Detection
// =============================================================================

export function isInJuceWebView(): boolean {
  return typeof window !== 'undefined' && window.__JUCE__ !== undefined;
}

// =============================================================================
// Mock States for Development
// =============================================================================

const mockSliderState: SliderState = {
  getNormalisedValue: () => 0.5,
  setNormalisedValue: () => {},
  getScaledValue: () => 50,
  setScaledValue: () => {},
  sliderDragStarted: () => {},
  sliderDragEnded: () => {},
  getProperties: () => ({ start: 0, end: 100, name: 'mock' }),
  valueChangedEvent: {
    addListener: () => 0,
    removeListener: () => {}
  }
};

const mockToggleState: ToggleState = {
  getValue: () => false,
  setValue: () => {},
  valueChangedEvent: {
    addListener: () => 0,
    removeListener: () => {}
  }
};

const mockComboBoxState: ComboBoxState = {
  getChoiceIndex: () => 0,
  setChoiceIndex: () => {},
  getChoices: () => [],
  valueChangedEvent: {
    addListener: () => 0,
    removeListener: () => {}
  }
};

// =============================================================================
// State Accessors
// =============================================================================

export function getSliderState(id: string): SliderState {
  if (isInJuceWebView() && window.__JUCE__?.getSliderState) {
    return window.__JUCE__.getSliderState(id);
  }
  return mockSliderState;
}

export function getToggleState(id: string): ToggleState {
  if (isInJuceWebView() && window.__JUCE__?.getToggleState) {
    return window.__JUCE__.getToggleState(id);
  }
  return mockToggleState;
}

export function getComboBoxState(id: string): ComboBoxState {
  if (isInJuceWebView() && window.__JUCE__?.getComboBoxState) {
    return window.__JUCE__.getComboBoxState(id);
  }
  return mockComboBoxState;
}

// =============================================================================
// Event System
// =============================================================================

type EventCallback = (data: unknown) => void;
const eventListeners = new Map<string, Set<EventCallback>>();

export function addEventListener(event: string, callback: EventCallback): () => void {
  if (!eventListeners.has(event)) {
    eventListeners.set(event, new Set());

    if (isInJuceWebView() && window.__JUCE__?.backend) {
      window.__JUCE__.backend.addEventListener(event, (data) => {
        const listeners = eventListeners.get(event);
        if (listeners) {
          listeners.forEach((cb) => cb(data));
        }
      });
    }
  }

  eventListeners.get(event)!.add(callback);

  return () => {
    eventListeners.get(event)?.delete(callback);
  };
}

export function emitEvent(event: string, data: unknown): void {
  if (isInJuceWebView() && window.__JUCE__?.backend) {
    window.__JUCE__.backend.emitEvent(event, data);
  }
}
