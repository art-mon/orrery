# orrery — Artistic Direction

## Visual concept

The display should feel like a living painting, not a data dashboard. The aesthetic sits
at the intersection of two references:

- **Donkey Kong Country 2 (Rare, 1995)** — pre-rendered 3D backgrounds with layered
  atmospheric depth, moody curated palettes, and palette-cycling that shifts the entire
  mood of a scene through time. The same image looks completely different at dawn vs dusk.
- **French Impressionism** — colour built from broken strokes that optically mix at
  distance, warm/cool interplay, light quality over shape accuracy, visible texture in
  sky and ground.

At 64×32 pixels both references converge on the same technique: **colour mood over
shape detail**. Fine geometry is lost at this resolution; colour temperature, gradient
depth, and light direction survive perfectly.

---

## AI background generation

### Source image

A single base image is generated weekly by calling the DALL-E 3 API (1792×1024
landscape), then downscaled to 64×32 via LANCZOS. The aggressive downscale is
intentional — a detailed painterly image becomes naturally impressionist at pixel scale.

The base image is stored as `data/bg_weekly.json` (same format as `frame.json`).
The display never renders the raw image — it always renders through a time-of-day
color transform (see below).

### Weather-influenced prompts

Weather at generation time shapes the base image. The prompt builder reads the current
`daily.json` weather condition and selects a mood:

| Condition | Prompt mood |
|-----------|-------------|
| Clear | Golden light, blue sky, warm landscape, high contrast |
| Cloudy | Overcast diffuse light, muted palette, pewter and slate |
| Rain | Wet atmosphere, dark clouds, reflective ground, Sisley grey |
| Storm | Turner-style drama, near-black sky, dramatic light shafts |
| Snow | Pale cool light, Pissarro winter palette, muted blues |
| Mist / Fog | Corot-style sfumato, merged horizon, very low contrast |

Base prompt template:
> *"[mood description], painterly impressionist landscape in the style of [reference],
> [palette description], layered atmospheric depth with three silhouette planes,
> moody, no text, no UI elements, wide cinematic format"*

DKC2-specific additions when relevant:
> *"pre-rendered 3D aesthetic, burnished [colour] palette, dark foreground silhouette,
> atmospheric perspective"*

### Generation schedule

- **Trigger**: GitHub Actions cron, once per week (Sunday 00:00 UTC)
- **Separate workflow** from the hourly data-fetch (`fetch.yml`)
- **Cost**: ~$0.04 per image (DALL-E 3 standard, 1792×1024) ≈ €2/year

### Prompt variation strategy *(to-do)*

Seasonal and thematic variation — 4 prompts per weather condition (one per season),
cycling on a schedule. Also: APOD-influenced prompts when the day's astronomy image has
a dominant theme (nebula, aurora, eclipse, etc.).

---

## Time-of-day colour transform

The base image is a fixed pixel array. The display applies a live HSL transform every
frame, keyed to the current local time, producing the full day/night cycle without
generating additional images.

### Keyframes

Interpolation between keyframes is linear (lerp on H shift, S multiplier, L multiplier).

| Time  | Hue shift | Sat  | Light | Character |
|-------|-----------|------|-------|-----------|
| 00:00 | +150°     | ×0.15| ×0.05 | Near-black, faint indigo — midnight reference |
| 04:00 | +120°     | ×0.20| ×0.08 | Deep prussian blue, pre-dawn |
| 06:00 | −20°      | ×0.80| ×0.50 | Amber sunrise wash |
| 08:00 | −5°       | ×0.95| ×0.85 | Warm morning light |
| 12:00 |  0°       | ×1.00| ×1.00 | Base image — reference point |
| 15:00 | −8°       | ×1.05| ×0.95 | Warm afternoon |
| 18:00 | −15°      | ×1.10| ×0.75 | Golden hour |
| 20:00 | +30°      | ×0.70| ×0.45 | Dusk — purple-amber |
| 22:00 | +90°      | ×0.35| ×0.18 | Blue-violet night onset |
| 23:30 | +130°     | ×0.20| ×0.06 | Fading toward midnight |

### Midnight image transition

At 00:00 (local time) the display:
1. Fades the current image to black over ~60 seconds
2. Swaps in the new weekly base image (already downloaded, waits in memory)
3. Fades in from black starting at the 00:00 keyframe (deep indigo)

The transition is only visible if someone is watching at midnight. During normal use,
the weekly image change is gradual and subtle — the heavy colour grading means the
base image is never seen raw.

---

## Micro-animation (dynamic colour elements)

*(to-do — implement after AI pipeline is stable)*

Rather than animating geometry (clouds moving, rain falling), certain pixels
"breathe" — a slow per-pixel sinusoidal brightness/hue wobble where the oscillation
phase is derived from pixel position, so neighbouring pixels are slightly out of sync.

```
phase(x, y) = ((x × 17 + y × 31) % 100) / 100
wobble(tick) = sin(tick × 0.015 + phase × 2π) × amplitude
```

Amplitude and character vary by pixel type:
- **Warm pixels** (high R relative to B): fire/torch flicker — amplitude ±12 brightness
- **Green pixels** (high G relative to R, B): leaf shimmer — amplitude ±6 hue, ±4 brightness
- **Dark pixels** (L < 0.15): stable — no wobble (shadows don't move)
- **Bright sky pixels** (L > 0.75): subtle atmospheric shimmer — amplitude ±3 brightness

Effect is imperceptible frame-to-frame but gives the scene life over a 10–30s observation
window, similar to DKC2's pre-rendered animation cycles.

---

## Scene overlay layers

The AI background is always the bottom layer. Overlaid on top, in order:

1. **AI base image** — time-of-day transformed, optionally micro-animated
2. **Weather effects** — rain streaks, snow flakes, lightning flash (code-generated)
3. **Celestial objects** — sun/moon drawn procedurally, positioned by actual time/phase
4. **UI chrome** — time, temperature (top corners)
5. **Scrolling ticker** — bottom strip

Layers 2–5 are unaffected by the colour transform — they always render in their true
colours so readability is preserved.

---

## Artistic principles (for prompt writing and scene coding)

1. **Depth planes over surface detail** — three layers (sky / mid-ground / foreground),
   each with its own colour temperature. Foreground darkest and most saturated.
   Background lightest and most desaturated (atmospheric perspective).

2. **Warm light / cool shadow** — if the light source is warm (sunrise, golden hour),
   shadows lean cool (blue-violet). If overcast, the reverse.

3. **Broken colour** — avoid large flat fills. Even solid areas should have ±10–15
   per-channel noise to simulate optical mixing.

4. **Palette over accuracy** — a sunset that "feels" like DKC2 is more valuable than
   one that accurately depicts local sky physics.

5. **Strong horizon** — the horizon line must survive downscaling. Compositional elements
   should be large and horizontally aligned.
