---
name: bc-create-plugin
description: Friendly getting-started guide for building your first BeatConnect plugin. Use when someone wants to create a new audio plugin without C++ knowledge.
---

# /bc-create-plugin Skill

The friendly getting-started experience for building your first BeatConnect plugin. No C++ knowledge required.

## Usage

```
/bc-create-plugin
```

Just type `/bc-create-plugin` and follow the conversation.

## Supported Plugin Types

**Currently supported:**
- ✅ **Effects** - Delay, reverb, distortion, filter, compressor, EQ, modulation, utility
- ✅ **Synthesizers** - Subtractive, FM, wavetable, additive

**Not yet supported:**
- ❌ **Samplers** - Requires sample loading/streaming architecture
- ❌ **Drum machines** - Sample-based, not yet supported
- ❌ **ROMplers** - Sample-based instruments

If a user asks for an unsupported type, politely explain and suggest an effect or synth instead.

## What This Skill Does

Guides you through building a complete audio plugin by asking simple questions, then generates everything for you:

1. **Asks what you want to build** - Effect type, parameters, UI style
2. **Scaffolds the project** - Uses `/new-plugin` internally with your choices
3. **Implements the audio processing** - Writes the DSP code based on your description
4. **Helps you test locally** - Builds and runs the plugin
5. **Pushes to GitHub** - So BeatConnect cloud can build the final release

---

## Conversation Flow

### Step 1: What Kind of Plugin?

Ask the user what they want to build. Be friendly and give examples:

```
What kind of audio plugin do you want to build?

**Effects:**
- **Delay** - Echo/repeat effects (ping pong, tape delay, etc.)
- **Reverb** - Space and ambience (room, hall, plate, etc.)
- **Distortion** - Saturation and grit (tube, tape, bitcrush, etc.)
- **Filter** - Frequency shaping (lowpass, highpass, resonant, etc.)
- **Compressor** - Dynamics control
- **Modulation** - Chorus, flanger, phaser, tremolo
- **EQ** - Parametric, graphic, tilt EQ
- **Utility** - Gain, pan, stereo tools

**Instruments:**
- **Synth** - Subtractive, FM, wavetable synthesizers

What sounds interesting to you?
```

**IMPORTANT:** If the user asks for a sampler, drum machine, or sample-based instrument, explain:
```
The BeatConnect SDK currently supports **effects** and **synthesizers**.
Sample-based instruments (samplers, drum machines, ROMplers) aren't supported yet - they require a different architecture for loading and streaming audio files.

Would you like to build an effect or a synth instead?
```

### Step 2: What Parameters?

Based on their choice, suggest common parameters and ask if they want to customize:

For a **Delay**:
```
Great choice! A delay typically has these controls:

- **Time** - How long before the echo (ms or synced to tempo)
- **Feedback** - How many repeats
- **Mix** - Dry/wet balance

Want these standard controls, or do you have something specific in mind?
(e.g., "add a filter on the feedback" or "make it a ping-pong delay")
```

For a **Distortion**:
```
Nice! A distortion usually has:

- **Drive** - How much saturation
- **Tone** - Brightness control
- **Mix** - Blend with clean signal

Want the standard setup, or something custom?
(e.g., "add a 3-band EQ" or "make it sound like a tube amp")
```

### Step 3: Plugin Name

Ask for a name:

```
What do you want to call your plugin?

Keep it short and catchy - this will be the name users see in their DAW.
Examples: "TapeDelay", "WarmDrive", "SpaceVerb"

Your plugin name:
```

### Step 4: Scaffold and Implement

Once you have all the info:

1. **Use `/new-plugin`** to scaffold:
   ```
   /new-plugin <PluginName>
   ```

2. **Answer the skill's questions** with the gathered info:
   - Plugin type: Effect
   - Parameters: [the ones discussed]
   - Company name: Use their GitHub username or "BeatConnect"

3. **Implement the DSP** in `PluginProcessor.cpp`:
   - Look at `example-plugin/` for reference patterns
   - Use JUCE's built-in DSP classes where possible
   - Keep it simple - this is their first plugin!

4. **Update the React UI** in `web-ui/src/App.tsx`:
   - Add sliders/knobs for each parameter
   - Use the hooks from `useJuceParam.ts`
   - Keep the UI clean and functional

### Step 5: Build and Test

Guide them through local testing:

```
Let's build and test your plugin locally!

1. First, install web dependencies:
   cd web-ui && npm install

2. Start the web dev server (for hot reload):
   npm run dev

3. In another terminal, build the plugin:
   cmake -B build -D<PLUGIN_NAME>_DEV_MODE=ON
   cmake --build build

4. Run the standalone version to test:
   ./build/<PluginName>_artefacts/Standalone/<PluginName>

You should see your plugin UI! Try the controls and make sure audio passes through.
```

### Step 6: Push to GitHub

Help them commit and push:

```
Your plugin is working locally. Let's push it to GitHub so BeatConnect can build the release versions (VST3, AU, AAX) for Windows and macOS.

Ready to commit and push?
```

Then:
```bash
git add -A
git commit -m "Initial plugin implementation"
git push origin main
```

Finally:
```
Done! Now go to your BeatConnect dashboard:
1. Create a new Project
2. Link this GitHub repository
3. BeatConnect will automatically build your plugin

You'll get downloadable installers for Windows and macOS once the build completes!
```

---

## Key Principles

1. **Keep it conversational** - This is for noobs, not C++ experts
2. **One step at a time** - Don't overwhelm with options
3. **Suggest sensible defaults** - They can customize later
4. **Explain what's happening** - But don't drown them in technical details
5. **Celebrate progress** - Building a plugin is exciting!

---

## Common Plugin Recipes

### Simple Delay
- Parameters: time (ms), feedback (0-1), mix (0-1)
- DSP: `juce::dsp::DelayLine` with feedback loop
- UI: 3 knobs

### Tape Delay
- Parameters: time (ms), feedback (0-1), mix (0-1), wow (0-1), saturation (0-1)
- DSP: DelayLine + modulated read position + soft clipper
- UI: 5 knobs with vintage styling

### Simple Distortion
- Parameters: drive (0-1), tone (0-1), mix (0-1)
- DSP: Waveshaper (tanh or soft clip) + lowpass filter
- UI: 3 knobs

### Tube Amp Style
- Parameters: gain (0-1), bass/mid/treble EQ, output (0-1)
- DSP: Multiple gain stages with EQ between
- UI: Amp-style layout with chicken head knobs

### Simple Reverb
- Parameters: size (0-1), damping (0-1), mix (0-1)
- DSP: `juce::dsp::Reverb` (built-in!)
- UI: 3 knobs

### Plate Reverb
- Parameters: decay (0-1), predelay (ms), damping (0-1), mix (0-1)
- DSP: Custom plate algorithm or Freeverb variation
- UI: 4 knobs with metallic styling

### Chorus
- Parameters: rate (Hz), depth (0-1), mix (0-1)
- DSP: Modulated delay line (short delay, LFO modulation)
- UI: 3 knobs

### Compressor
- Parameters: threshold (dB), ratio, attack (ms), release (ms), makeup (dB)
- DSP: `juce::dsp::Compressor`
- UI: 5 knobs + gain reduction meter

---

## Error Recovery

If something goes wrong during the process:

**Build errors**:
```
Don't worry, build errors are normal! Paste the error message and I'll help you fix it.
```

**Plugin doesn't load in DAW**:
```
Let's debug this. First, try the Standalone version - if that works, the issue is with the plugin format. Check the build output for any warnings about VST3/AU.
```

**UI is black/blank**:
```
This is usually a constructor order issue. Let me check your PluginEditor.cpp - the WebView needs to be created in a specific order.
```

---

## Reference

- Full technical details: `/new-plugin` skill
- Working example: `example-plugin/` directory
- Patterns and troubleshooting: `docs/patterns.md`
- SDK documentation: `CLAUDE.md`
