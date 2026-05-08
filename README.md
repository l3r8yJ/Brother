# StarBoy

A wearable AI pendant with a living face. Built on ESP32-S3 with a 1.32" AMOLED display.

> **Status:** firmware skeleton complete. Eyes animation module written and compiling. Hardware on the way.

## What is this

StarBoy is a digital pet you can wear. It watches the world through a microphone and motion sensors, reacts to how it's handled, and talks back through a built-in AI. Each unit ships with its own personality — from gentle and curious to defiant and loud.

The signature look: a glossy black obsidian body, with a circular AMOLED screen at its center showing animated amber-glowing eyes.

## Hardware

- **Board:** Waveshare ESP32-S3-Touch-AMOLED-1.32
- **Display:** 466×466 AMOLED, capacitive touch
- **Audio:** ES8311 codec, built-in mic, external speaker (8Ω 2W)
- **Motion:** LSM6DS3 (added via I2C)
- **Power:** 3.7V Li-Po, USB-C charging

## Stack

- **Framework:** Arduino on ESP-IDF (via PlatformIO)
- **AI:** hybrid — local wake-word detection (esp-sr), cloud STT/LLM/TTS for full conversations
- **UI:** custom eye-rendering engine, optionally LVGL for menus

## Project layout

```
starboy/
├── starboy-firmware/   # ESP32 firmware, PlatformIO project
├── design/             # renders, moodboard, philosophy
├── hardware/           # schematics, BOM, datasheets
├── docs/               # plans, notes, ADRs
├── CLAUDE.md           # context for Claude Code
├── PROJECT_PLAN.md     # detailed roadmap with checklist
└── README.md
```

## Working with Claude Code

This project uses Claude Code for development assistance. Start a session in the project root:

```bash
cd ~/code/starboy
claude
```

Claude Code will automatically read `CLAUDE.md` for full project context. Then point it at the current plan:

```
Read CLAUDE.md and PROJECT_PLAN.md, then continue from where we left off.
```

## Inspirations

The original [StarBoy by hesjustalittleguy](https://hesjustalittleguy.com/) — for the spirit of a "completely standalone digital creature." This project takes that idea and adds an AI brain via cloud APIs.

## License

TBD.
