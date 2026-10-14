# Configuration

## Appearance

The mod exposes these settings in Windhawk. The JSON file
`settings.default.json` supplies the same defaults to the local installer.

| Setting | Default | Accepted values |
| --- | --- | --- |
| Color | `#E81123` | `#RRGGBB` |
| FillOpacity | 20 | Integer, 0–100% |
| BorderOpacity | 80 | Integer, 0–100% |
| CornerRadius | 8 | Integer, 0–64 DIP |
| BorderWidth | 1 | Integer, 0–8 DIP |

DIP means device-independent pixel: values scale with the desktop window DPI.
Zero radius produces square corners. Zero border width removes the border.
At zero fill opacity, the fill is invisible. Very thin rectangles use a solid
strip at the greater of the configured fill and border opacities.

The C++ mod clamps numeric settings to the listed ranges and falls back to
`#E81123` for an invalid color. The installer rejects invalid JSON settings
before making installation changes. Windhawk settings use annotations supported
by version 1.7.3; newer color-picker annotations are not required.

Editing JSON does not apply changes to an already installed mod. Use Windhawk's
Settings for live changes. For a new local installation with custom defaults:

```powershell
Copy-Item settings.default.json settings.local.json
# Edit settings.local.json, then use elevated PowerShell:
.\install.ps1 -SettingsPath .\settings.local.json
```

`update.ps1` preserves the existing installed appearance settings.

## Build configuration

`project.config.json` contains the shared defaults:

```json
{
  "windhawkRoot": null,
  "buildDirectory": "build"
}
```

When `windhawkRoot` is null, scripts look for Windhawk in the native Windows
Program Files directory. Engine/compiler versions and installation storage paths
come from Windhawk's INI files, rather than being pinned in the scripts.

For a different location, copy the file to `project.config.local.json` and set
`windhawkRoot` to the absolute installation directory. This local file takes
precedence automatically and is ignored by Git. `-ConfigPath` can select an
explicit configuration file on all build/install/state scripts.

`buildDirectory` is resolved relative to the repository when it is not absolute.
The default `build/` is ignored by Git; ignore any custom output directory too.
Scripts build for x64 only. Build supports the paths configured by portable
Windhawk, but direct installation scripts require its installed HKLM backend.
