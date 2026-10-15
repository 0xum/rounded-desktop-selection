# Changelog

## 0.3.2 — 2026-10-15

- Independent Home/Quick access marquee styling through XAML Diagnostics.
- Shared color, fill/border opacity, radius, width and ExplorerMarquee setting.
- Restore original XAML values and remove callbacks when disabled/unloaded.
- User visually confirmed Home styling with File Explorer Styler disabled.
- Live checks passed for native-value restoration, re-enabling and mod reload.
- 38 renderer checks retained; 0.3.0/0.3.1 were local integration builds.

## 0.2.6 — 2026-10-14

- Style the pending item-selection overlay while holding the mouse button,
  using the same rounded translucent appearance as regular file highlights.
- 38 renderer checks, including held-click overlay routing.
- User visually confirmed the held-click fix without blue/black artifacts.
- Version 0.2.5 was a local diagnostic build; temporary tracing was removed.

## 0.2.4 — 2026-10-14

- Compose file highlights directly over the existing pane background, avoiding
  extra fill and borders from other mods' normal ListView rendering.
- User visually confirmed removal of the black file-row rectangle.

## 0.2.3 — 2026-10-14

- Rounded Explorer file hover/selection and a translucent DirectUI marquee.
- Header highlights with a smaller 4 DIP radius; native sort glyphs retained.
- View-button styling in Explorer's lower-right status area.
- Premultiplied-alpha compositing for translucent Explorer surfaces.
- DirectUI native-border suppression while preserving its layout pass.
- Independent Explorer feature settings, upgrade defaults and 37 renderer checks.

## 0.1.2 — 2026-10-14

Initial public-ready MVP.

- Configurable color, fill/border opacity, corner radius and border width.
- Desktop-only GDI+ rendering with anti-aliasing and DPI scaling.
- Native selection geometry and repaint clipping to prevent drawing trails.
- Native fallback when symbols, layout or geometry cannot be resolved.
- Single-file Windhawk source, configurable PowerShell build/install scripts,
  renderer tests and MIT license.
