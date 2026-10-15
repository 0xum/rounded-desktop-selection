# Implementation and validation

The mod stays in one `*.wh.cpp` file, as required by Windhawk's mod format.
`render-tests.cpp` includes that source with `MVP_RENDER_TEST`, omitting the
Windhawk lifecycle/hook code while testing the same drawing functions.

## Rendering

`CListView::WndProc` establishes the current list-view window on each thread.
Only a desktop `SysListView32` under `SHELLDLL_DefView`, with a `Progman` or
`WorkerW` root, is eligible for custom painting. Other views call the original
painting function. Separate Explorer processes also load the mod so their file
windows can be styled; Explorer operations are restricted to file-window roots.

`CLVDrawManager::_PaintMarquee` receives a dirty repaint rectangle, not the
whole selection rectangle. Treating it as the selection shape creates rounded
borders on narrow repaint strips and leaves drawing trails.

The mod resolves the painting symbol, recognizes a particular x64 instruction
sequence and derives the member offsets used by the native routine to locate
its selection rectangle. It reads that geometry using checked
`ReadProcessMemory` calls. The pattern is intentionally narrow and is a
compatibility limit, not a general-purpose disassembler.

GDI+ renders the whole native selection shape while clipping to the dirty region
and to the original device-context clip. Anti-aliasing, alpha blending and DPI
scaling apply to the appearance. Native selection hit testing remains rectangular.

## Explorer integration

BeginPaint/EndPaint retain the paint-window context for memory DCs. The theme
hooks replace ListView item and Header hover/selection backgrounds only inside
Explorer windows. Sort arrows and text/icon rendering remain native.
File highlights composite directly over the existing pane. They do not request
an extra LISS_NORMAL pass, which other styling mods may render with a border.

The DirectUI marquee is identified through UIMarqueeSelector StartMarquee and
EndMarquee. Element::PaintBackground draws the rounded selector into a
premultiplied DIB, then AlphaBlend composites it onto the original surface.
This keeps the destination alpha for translucent themes.

Element::PaintBorder still runs its geometry pass. Its _Fill calls are suppressed
when the mod replaces the selector's or a status view-button's native border.
DirectUI view-button hover/selection uses exported state getters and is restricted
to compact controls in Explorer's lower-right status area.

Header/view-button radius is 4 DIP. File and marquee radius use the configured
CornerRadius. Each Explorer capability is enabled only after its hooks resolve.

The modern file focus outline is not the classic DrawFocusRect call. It is the
ItemsView theme's part 3/state 1 overlay. That exact overlay is suppressed while
the rounded file outline remains. Part 3/state 2 is the pending-selection
overlay observed while holding the mouse button; it uses the shared rounded
item renderer without a native background pass. Other states remain native.
The theme detail is also documented in the MIT-licensed
[File Explorer Focus Border Remover](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/explorer-hide-focus-border.wh.cpp).

## Lifecycle callbacks

- `Wh_ModInit`: load settings, start GDI+, resolve layout and register hooks.
- `Wh_ModSettingsChanged`: load updated appearance values.
- `Wh_ModUninit`: release the library reference and GDI+ resources after hook removal.

The mod records `Initialized` and `DesktopPaintObserved` in its Windhawk local
storage to help diagnosis. These are local diagnostic markers and can become
stale after process termination; check the actual loaded DLL to confirm state.

## Checks

```powershell
.\scripts\check-project.ps1
.\build.ps1
.\build.ps1 -Test
```

The project check validates PowerShell syntax and compares installer defaults
with the settings block in the source. The renderer tests produce
`build/render-preview.png`; the public preview is a copy of that generated image.

For changes to hooks, geometry, paint clipping or lifecycle, also validate the
loaded mod on a real desktop:

1. Compare native selection with the mod disabled.
2. Enable the mod and drag in all four directions.
3. Expand, shrink, release and cancel a selection; check for remaining trails.
4. Verify icon selection and regular File Explorer windows.
5. Change appearance settings, disable and re-enable the mod.
6. Test the target Windows build and monitor DPI configuration.

Do not claim broader compatibility from rendering tests alone. Record Windows,
Windhawk and comctl32 versions when adding a compatible layout pattern.

## Design references

- [Creating a new mod](https://github.com/ramensoftware/windhawk/wiki/Creating-a-new-mod)
- [Windhawk development tips](https://github.com/ramensoftware/windhawk/wiki/Development-tips)
- [Mod lifetime](https://github.com/ramensoftware/windhawk/wiki/Mod-lifetime)
- [Classic Selection Rectangle Color](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/classic-selection-rectangle-color.wh.cpp), a reference for the marquee hook symbols
- [GDI+ alpha compositing](https://learn.microsoft.com/en-us/windows/win32/gdiplus/-gdiplus-using-compositing-mode-to-control-alpha-blending-use)
