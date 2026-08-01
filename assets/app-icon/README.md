# App icon sources

Shared by the macOS and iOS apps.

| file | what it is |
|---|---|
| `source-artwork-1254.png` | original artwork, 1254x1254, opaque, edge-to-edge |
| `mangekyo-icon-light-1024.png` | processed master: 824 body centred in a 1024 canvas, transparent outside the squircle |
| `mangekyo-icon-dark-1024.png` | dark variant of the same, light plate repainted, blue chip preserved |

The masters follow Apple's macOS grid: the icon body occupies 824 of the 1024
canvas with a 22.37% corner radius. The source is *not* usable directly — it has
no alpha channel, so its corners would render as an opaque square.

## Where each is used

- **macOS Dock icon** — `mangekyo-icon-light-1024.png` only.
  `AppIcon.appiconset` is light-only on purpose: `actool` reports dark entries in
  a mac app icon set as "unassigned children" and discards them. macOS has no
  dark Dock icon before 26.
- **In-app artwork (About page)** — both, via `AppIconArt.imageset`. Ordinary
  image sets do honour a `luminosity` appearance, so this one switches with the
  system theme.
- **macOS 26 Liquid Glass** — not wired up. Needs a `.icon` bundle authored in
  Icon Composer (`/Applications/Xcode.app/Contents/Applications/Icon Composer.app`),
  which is GUI-only. Feed it these two masters, then point
  `ASSETCATALOG_COMPILER_APPICON_NAME` at the result; the `.icns` stays as the
  fallback for macOS 25 and earlier.

## Regenerating

`scripts/make-app-icon.swift` turns the source artwork into both masters.
