/**
 * JUCE 8 Frontend Bridge Library
 *
 * This module provides TypeScript wrappers for JUCE 8's native WebView integration.
 * It interfaces with window.__JUCE__ which is injected by the WebBrowserComponent.
 *
 * IMPORTANT: The relay identifier used here must EXACTLY match the identifier
 * used in C++ when creating WebSliderRelay, WebToggleButtonRelay, etc.
 */

// ==============================================================================
// Type Definitions
// ==============================================================================

interface JuceBackend {
  addEventListener(eventId: string, fn: (payload: unknown) => void): [string, number];
  removeEventListener(token: [string, number]): void;
  emitEvent(eventId: string, payload: unknown): void;
}

interface JuceInitialisationData {
  __juce__platform: string[];
  __juce__functions: string[];
  __juce__registeredGlobalEventIds: string[];
  __juce__sliders: string[];
  __juce__toggles: string[];
  __juce__comboBoxes: string[];
}

declare global {
  interface Window {
    __JUCE__?: {
      backend: JuceBackend;
      initialisationData: JuceInitialisationData;
    };
  }
}

// Event type constants (internal to JUCE)
const BasicControl_valueChangedEventId = 'valueChanged';
const BasicControl_propertiesChangedId = 'propertiesChanged';
const SliderControl_sliderDragStartedEventId = 'sliderDragStarted';
const SliderControl_sliderDragEndedEventId = 'sliderDragEnded';

// ==============================================================================
// Listener List Helper
// ==============================================================================

type ListenerCallback = (payload?: unknown) => void;

class ListenerList {
  private listeners = new Map<number, ListenerCallback>();
  private listenerId = 0;

  addListener(fn: ListenerCallback): number {
    const newId = this.listenerId++;
    this.listeners.set(newId, fn);
    return newId;
  }

  removeListener(id: number): void {
    this.listeners.delete(id);
  }

  callListeners(payload?: unknown): void {
    for (const fn of this.listeners.values()) {
      fn(payload);
    }
  }
}

// ==============================================================================
// Utility Functions
// ==============================================================================

/**
 * Check if running inside JUCE WebView (vs browser development)
 */
export function isInJuceWebView(): boolean {
  return typeof window.__JUCE__ !== 'undefined' &&
         typeof window.__JUCE__.backend !== 'undefined';
}

// ==============================================================================
// SliderState - Continuous Parameter Control
// ==============================================================================

interface SliderProperties {
  start: number;
  end: number;
  skew: number;
  name: string;
  label: string;
  numSteps: number;
  interval: number;
  parameterIndex: number;
}

/**
 * SliderState manages bidirectional sync with a WebSliderRelay on the C++ side.
 */
export class SliderState {
  readonly name: string;
  private identifier: string;
  private scaledValue = 0;
  private properties: SliderProperties = {
    start: 0,
    end: 1,
    skew: 1,
    name: '',
    label: '',
    numSteps: 100,
    interval: 0,
    parameterIndex: -1,
  };

  valueChangedEvent = new ListenerList();
  propertiesChangedEvent = new ListenerList();

  constructor(name: string) {
    this.name = name;
    this.identifier = '__juce__slider' + name;

    if (isInJuceWebView()) {
      window.__JUCE__!.backend.addEventListener(this.identifier, (event) =>
        this.handleEvent(event as Record<string, unknown>)
      );

      window.__JUCE__!.backend.emitEvent(this.identifier, {
        eventType: 'requestInitialUpdate',
      });
    }
  }

  /** Set value from 0-1 normalized range */
  setNormalisedValue(newValue: number): void {
    this.scaledValue = this.snapToLegalValue(
      this.normalisedToScaledValue(newValue)
    );

    if (isInJuceWebView()) {
      window.__JUCE__!.backend.emitEvent(this.identifier, {
        eventType: BasicControl_valueChangedEventId,
        value: this.scaledValue,
      });
    }
  }

  /** Call when user starts dragging (for undo grouping) */
  sliderDragStarted(): void {
    if (isInJuceWebView()) {
      window.__JUCE__!.backend.emitEvent(this.identifier, {
        eventType: SliderControl_sliderDragStartedEventId,
      });
    }
  }

  /** Call when user stops dragging (for undo grouping) */
  sliderDragEnded(): void {
    if (isInJuceWebView()) {
      window.__JUCE__!.backend.emitEvent(this.identifier, {
        eventType: SliderControl_sliderDragEndedEventId,
      });
    }
  }

  private handleEvent(event: Record<string, unknown>): void {
    if (event.eventType === BasicControl_valueChangedEventId) {
      this.scaledValue = event.value as number;
      this.valueChangedEvent.callListeners();
    }
    if (event.eventType === BasicControl_propertiesChangedId) {
      const { eventType: _, ...rest } = event;
      this.properties = rest as SliderProperties;
      this.propertiesChangedEvent.callListeners();
    }
  }

  /** Get value in parameter's native range */
  getScaledValue(): number {
    return this.scaledValue;
  }

  /** Get value in 0-1 normalized range */
  getNormalisedValue(): number {
    const range = this.properties.end - this.properties.start;
    if (range === 0) return 0;
    return Math.pow(
      (this.scaledValue - this.properties.start) / range,
      this.properties.skew
    );
  }

  /** Get parameter properties (range, label, etc.) */
  getProperties(): SliderProperties {
    return { ...this.properties };
  }

  private normalisedToScaledValue(normalisedValue: number): number {
    return (
      Math.pow(normalisedValue, 1 / this.properties.skew) *
        (this.properties.end - this.properties.start) +
      this.properties.start
    );
  }

  private snapToLegalValue(value: number): number {
    const interval = this.properties.interval;
    if (interval === 0) return value;

    const start = this.properties.start;
    const clamp = (val: number, min = 0, max = 1) => Math.max(min, Math.min(max, val));

    return clamp(
      start + interval * Math.floor((value - start) / interval + 0.5),
      this.properties.start,
      this.properties.end
    );
  }
}

// ==============================================================================
// ToggleState - Boolean Parameter Control
// ==============================================================================

/**
 * ToggleState manages bidirectional sync with a WebToggleButtonRelay on the C++ side.
 */
export class ToggleState {
  readonly name: string;
  private identifier: string;
  private _value = false;

  valueChangedEvent = new ListenerList();
  propertiesChangedEvent = new ListenerList();

  constructor(name: string) {
    this.name = name;
    this.identifier = '__juce__toggle' + name;

    if (isInJuceWebView()) {
      window.__JUCE__!.backend.addEventListener(this.identifier, (event) =>
        this.handleEvent(event as Record<string, unknown>)
      );

      window.__JUCE__!.backend.emitEvent(this.identifier, {
        eventType: 'requestInitialUpdate',
      });
    }
  }

  getValue(): boolean {
    return this._value;
  }

  setValue(newValue: boolean): void {
    this._value = newValue;

    if (isInJuceWebView()) {
      window.__JUCE__!.backend.emitEvent(this.identifier, {
        eventType: BasicControl_valueChangedEventId,
        value: this._value,
      });
    }
  }

  private handleEvent(event: Record<string, unknown>): void {
    if (event.eventType === BasicControl_valueChangedEventId) {
      this._value = event.value as boolean;
      this.valueChangedEvent.callListeners();
    }
    if (event.eventType === BasicControl_propertiesChangedId) {
      this.propertiesChangedEvent.callListeners();
    }
  }
}

// ==============================================================================
// ComboBoxState - Choice Parameter Control
// ==============================================================================

interface ComboBoxProperties {
  name: string;
  parameterIndex: number;
  choices: string[];
}

/**
 * ComboBoxState manages bidirectional sync with a WebComboBoxRelay on the C++ side.
 */
export class ComboBoxState {
  readonly name: string;
  private identifier: string;
  private _value = 0;
  private _properties: ComboBoxProperties = {
    name: '',
    parameterIndex: -1,
    choices: [],
  };

  valueChangedEvent = new ListenerList();
  propertiesChangedEvent = new ListenerList();

  constructor(name: string) {
    this.name = name;
    this.identifier = '__juce__comboBox' + name;

    if (isInJuceWebView()) {
      window.__JUCE__!.backend.addEventListener(this.identifier, (event) =>
        this.handleEvent(event as Record<string, unknown>)
      );

      window.__JUCE__!.backend.emitEvent(this.identifier, {
        eventType: 'requestInitialUpdate',
      });
    }
  }

  /** Get the current selected index */
  getChoiceIndex(): number {
    const numChoices = this._properties.choices.length;
    if (numChoices <= 1) return 0;
    return Math.round(this._value * (numChoices - 1));
  }

  /** Set the selected index */
  setChoiceIndex(index: number): void {
    const numItems = this._properties.choices.length;
    this._value = numItems > 1 ? index / (numItems - 1) : 0;

    if (isInJuceWebView()) {
      window.__JUCE__!.backend.emitEvent(this.identifier, {
        eventType: BasicControl_valueChangedEventId,
        value: this._value,
      });
    }
  }

  /** Get available choices */
  getChoices(): string[] {
    return [...this._properties.choices];
  }

  private handleEvent(event: Record<string, unknown>): void {
    if (event.eventType === BasicControl_valueChangedEventId) {
      this._value = event.value as number;
      this.valueChangedEvent.callListeners();
    }
    if (event.eventType === BasicControl_propertiesChangedId) {
      const { eventType: _, ...rest } = event;
      this._properties = rest as ComboBoxProperties;
      this.propertiesChangedEvent.callListeners();
    }
  }
}

// ==============================================================================
// State Caches (Singleton pattern)
// ==============================================================================

const sliderStates = new Map<string, SliderState>();
const toggleStates = new Map<string, ToggleState>();
const comboBoxStates = new Map<string, ComboBoxState>();

export function getSliderState(name: string): SliderState {
  if (!sliderStates.has(name)) {
    sliderStates.set(name, new SliderState(name));
  }
  return sliderStates.get(name)!;
}

export function getToggleState(name: string): ToggleState {
  if (!toggleStates.has(name)) {
    toggleStates.set(name, new ToggleState(name));
  }
  return toggleStates.get(name)!;
}

export function getComboBoxState(name: string): ComboBoxState {
  if (!comboBoxStates.has(name)) {
    comboBoxStates.set(name, new ComboBoxState(name));
  }
  return comboBoxStates.get(name)!;
}

// ==============================================================================
// Custom Event Listener (for non-parameter data like visualizers)
// ==============================================================================

export function addCustomEventListener(
  eventId: string,
  callback: (data: unknown) => void
): () => void {
  if (!isInJuceWebView()) {
    console.log('[JUCE Bridge] Not in WebView, custom event listener ignored:', eventId);
    return () => {};
  }

  const token = window.__JUCE__!.backend.addEventListener(eventId, callback);

  return () => {
    window.__JUCE__!.backend.removeEventListener(token);
  };
}
