# Contributing

Keep changes focused and retain the single-file Windhawk mod format.
Use the metadata in `rounded-desktop-selection.wh.cpp` as the source of truth
for the mod ID and version. Update `CHANGELOG.md` when changing released behavior.

Run the project check, compile the mod, and run the renderer tests described in
[development.md](docs/development.md). Changes to hooks or Windows geometry
also need a real-desktop check; include the tested versions and limitations.

Keep binaries, local installation paths, logs and machine-specific diagnostic
tools out of commits. Do not include personal desktop screenshots when the
generated preview or a cropped reproduction is sufficient.

For bug reports, include Windows/Windhawk/comctl32 versions, appearance settings,
other selection-related mods, whether native selection works with this mod
disabled, and reproducible steps. Lively Wallpaper interactions should include
a comparison with Lively closed.

This project is licensed under MIT. Contributions must be compatible with that
license. Preserve attribution when incorporating third-party code.
