/**
 * React Hook for User Preset Management
 *
 * Manages user presets (save/load/rename/delete) and communication with C++ backend.
 */

import { useState, useEffect, useCallback } from 'react';
import { isInJuceWebView, addCustomEventListener } from '../lib/juce-bridge';

export interface PresetInfo {
  name: string;
  isFactory: boolean;
}

export interface PresetList {
  factory: PresetInfo[];
  user: PresetInfo[];
  currentPreset?: string;
  isCurrentFactory?: boolean;
}

interface PresetState {
  factory: PresetInfo[];
  user: PresetInfo[];
  isLoading: boolean;
  error?: string;
  currentPreset?: string;
  isCurrentFactory?: boolean;
}

/**
 * Hook for managing presets
 */
export function usePresets() {
  const [state, setState] = useState<PresetState>({
    factory: [],
    user: [],
    isLoading: true,
  });

  // Initialize listeners for C++ events
  useEffect(() => {
    if (!isInJuceWebView()) {
      // Development mode - use mock data
      console.log('[Presets] Development mode - using mock data');
      setState({
        factory: [
          { name: 'Default', isFactory: true },
          { name: 'Clean', isFactory: true },
          { name: 'Warm', isFactory: true },
          { name: 'Bright', isFactory: true },
        ],
        user: [
          { name: 'My Custom Preset', isFactory: false },
        ],
        isLoading: false,
      });
      return;
    }

    console.log('[Presets] Setting up C++ event listeners');

    // Listen for preset list updates from C++
    const unsubList = addCustomEventListener('presetList', (data: unknown) => {
      console.log('[Presets] presetList event received:', data);
      const list = data as PresetList;
      setState(s => ({
        ...s,
        factory: list.factory || [],
        user: list.user || [],
        isLoading: false,
        currentPreset: list.currentPreset || s.currentPreset,
        isCurrentFactory: list.isCurrentFactory ?? s.isCurrentFactory,
      }));
    });

    // Listen for save result
    const unsubSave = addCustomEventListener('savePresetResult', (data: unknown) => {
      const result = data as { success: boolean; name?: string; error?: string };
      console.log('[Presets] savePresetResult:', result);

      if (result.success && result.name) {
        setState(s => ({
          ...s,
          currentPreset: result.name,
          isCurrentFactory: false,
          error: undefined,
        }));
      } else {
        setState(s => ({
          ...s,
          error: result.error || 'Failed to save preset',
        }));
      }
    });

    // Listen for load result
    const unsubLoad = addCustomEventListener('loadPresetResult', (data: unknown) => {
      const result = data as { success: boolean; name?: string; isFactory?: boolean; error?: string };
      console.log('[Presets] loadPresetResult:', result);

      if (result.success && result.name) {
        setState(s => ({
          ...s,
          currentPreset: result.name,
          isCurrentFactory: result.isFactory,
          error: undefined,
        }));
      } else {
        setState(s => ({
          ...s,
          error: result.error || 'Failed to load preset',
        }));
      }
    });

    // Listen for rename result
    const unsubRename = addCustomEventListener('renamePresetResult', (data: unknown) => {
      const result = data as { success: boolean; oldName?: string; newName?: string; error?: string };
      console.log('[Presets] renamePresetResult:', result);

      if (!result.success) {
        setState(s => ({
          ...s,
          error: result.error || 'Failed to rename preset',
        }));
      }
    });

    // Listen for delete result
    const unsubDelete = addCustomEventListener('deletePresetResult', (data: unknown) => {
      const result = data as { success: boolean; name?: string; error?: string };
      console.log('[Presets] deletePresetResult:', result);

      if (!result.success) {
        setState(s => ({
          ...s,
          error: result.error || 'Failed to delete preset',
        }));
      }
    });

    // Request initial preset list from C++
    console.log('[Presets] Requesting initial preset list');
    window.__JUCE__!.backend.emitEvent('getPresetList', {});

    return () => {
      unsubList();
      unsubSave();
      unsubLoad();
      unsubRename();
      unsubDelete();
    };
  }, []);

  // Save current state as a new user preset
  const savePreset = useCallback((name: string) => {
    if (!name.trim()) {
      setState(s => ({ ...s, error: 'Preset name cannot be empty' }));
      return;
    }

    if (!isInJuceWebView()) {
      console.log('[Presets] Mock save:', name);
      setState(s => ({
        ...s,
        user: [...s.user.filter(p => p.name !== name), { name, isFactory: false }],
        currentPreset: name,
        isCurrentFactory: false,
      }));
      return;
    }

    window.__JUCE__!.backend.emitEvent('saveUserPreset', { name: name.trim() });
  }, []);

  // Load a preset
  const loadPreset = useCallback((name: string, isFactory: boolean) => {
    if (!isInJuceWebView()) {
      console.log('[Presets] Mock load:', name, isFactory);
      setState(s => ({
        ...s,
        currentPreset: name,
        isCurrentFactory: isFactory,
      }));
      return;
    }

    window.__JUCE__!.backend.emitEvent('loadUserPreset', { name, isFactory });
  }, []);

  // Rename a user preset
  const renamePreset = useCallback((oldName: string, newName: string) => {
    if (!newName.trim()) {
      setState(s => ({ ...s, error: 'Preset name cannot be empty' }));
      return;
    }

    if (!isInJuceWebView()) {
      console.log('[Presets] Mock rename:', oldName, '->', newName);
      setState(s => ({
        ...s,
        user: s.user.map(p => p.name === oldName ? { ...p, name: newName } : p),
        currentPreset: s.currentPreset === oldName ? newName : s.currentPreset,
      }));
      return;
    }

    window.__JUCE__!.backend.emitEvent('renameUserPreset', { oldName, newName: newName.trim() });
  }, []);

  // Delete a user preset
  const deletePreset = useCallback((name: string) => {
    if (!isInJuceWebView()) {
      console.log('[Presets] Mock delete:', name);
      setState(s => ({
        ...s,
        user: s.user.filter(p => p.name !== name),
        currentPreset: s.currentPreset === name ? undefined : s.currentPreset,
      }));
      return;
    }

    window.__JUCE__!.backend.emitEvent('deleteUserPreset', { name });
  }, []);

  // Clear error
  const clearError = useCallback(() => {
    setState(s => ({ ...s, error: undefined }));
  }, []);

  // Check if a name already exists
  const presetExists = useCallback((name: string) => {
    const trimmed = name.trim().toLowerCase();
    return state.factory.some(p => p.name.toLowerCase() === trimmed) ||
           state.user.some(p => p.name.toLowerCase() === trimmed);
  }, [state.factory, state.user]);

  // Reset/re-apply current preset
  const resetPreset = useCallback(() => {
    if (!isInJuceWebView()) {
      console.log('[Presets] Mock reset');
      return;
    }

    window.__JUCE__!.backend.emitEvent('resetPreset', {});
  }, []);

  return {
    // State
    factoryPresets: state.factory,
    userPresets: state.user,
    isLoading: state.isLoading,
    error: state.error,
    currentPreset: state.currentPreset,
    isCurrentFactory: state.isCurrentFactory,

    // Actions
    savePreset,
    loadPreset,
    renamePreset,
    deletePreset,
    clearError,
    presetExists,
    resetPreset,
  };
}
