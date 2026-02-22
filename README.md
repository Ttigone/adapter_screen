# Screen Adapter — Cross-Platform Qt UI Scaling Library

A lightweight, header-based library that provides consistent UI scaling across:

- Windows 10 / 11 (single and multi-monitor, Per-Monitor DPI Awareness v2)
- Linux (X11 and Wayland, multi-monitor)
- Android (phone and tablet, immersive full-screen)
- Qt 5 (5.9 +) and Qt 6

---

## File Overview

| File | Role |
|------|------|
| `screen_adapter.h` | Singleton class declaration + inline scaling functions |
| `screen_adapter.cpp` | Implementation: DPI detection, multi-screen, Android JNI |
| `adaptive_mixin.h` | CRTP mix-in template — gives any Widget/Dialog auto-adaptation |
| `adapter_screen.h/cpp` | Example window demonstrating the API |
| `main.cpp` | Entry point showing the correct initialisation sequence |

---

## Quick Start

### 1. Initialise (two mandatory calls)

```cpp
// main.cpp
#include "screen_adapter.h"
#include <QtWidgets/QApplication>

int main(int argc, char* argv[])
{
    // MUST come before QApplication.
    ScreenAdapter::SetupHighDpiPolicy();

    QApplication app(argc, argv);

    // MUST come immediately after QApplication.
    ScreenAdapter::Instance()->Init();

    MyWindow w;
    w.show();
    return app.exec();
}
```

### 2. Use scaling functions in any widget

```cpp
#include "screen_adapter.h"

MyWidget::MyWidget(QWidget* parent) : QWidget(parent)
{
    resize(SS(1280, 720));                               // scaled QSize
    btn->setGeometry(SR(10, 10, 120, 40));               // scaled QRect
    lbl->setFont(QFont("Arial", SFS(14)));               // scaled point size
}
```

### 3. Auto-adapt on monitor switch — one line change

```cpp
// Before:
class MyDialog : public QDialog { ... };

// After:
#include "adaptive_mixin.h"
class MyDialog : public AdaptiveMixin<QDialog> { ... };
```

Dragging the window to a 4K monitor now automatically re-runs `OnAdaptLayout()`.

---

## Configuration

Pass a `ScreenAdapter::Config` struct to `Init()` to override defaults:

```cpp
ScreenAdapter::Config cfg;
cfg.design_width    = 1280;  // reference canvas width in logical pixels
cfg.design_height   = 800;   // reference canvas height
cfg.design_dpi      = 96.0;  // DPI assumed when the UI was designed
cfg.use_min_scale   = true;  // uniform scale (keep aspect ratio)
cfg.font_follow_dpi = false; // font tracks UI scale (not physical DPI)
cfg.per_screen_dpi  = true;  // auto-recompute when window changes monitor
ScreenAdapter::Instance()->Init(cfg);
```

| Field | Default | Description |
|-------|---------|-------------|
| `design_width` | 1920 | Width of the design canvas in logical pixels |
| `design_height` | 1080 | Height of the design canvas in logical pixels |
| `design_dpi` | 96.0 | DPI the UI was designed at (96 = standard Windows) |
| `use_min_scale` | true | true = uniform scale; false = stretch to fill |
| `font_follow_dpi` | false | true = font keeps physical size; false = font scales with UI |
| `per_screen_dpi` | true | Whether to re-scale when window moves to a different monitor |

---

## Per-Screen DPI Adaptation

When `per_screen_dpi = true` (the default), the library hooks into
`QWindow::screenChanged`. This signal is delivered by Qt in three situations:

- **Windows 10 / 11** — the OS sends `WM_DPICHANGED` when a window enters a
  monitor with a different DPI setting (Per-Monitor DPI Awareness v2). Qt
  translates this into `QWindow::screenChanged`.
- **Linux X11** — `RRNotify` is generated when the window crosses monitor
  boundaries.
- **Linux Wayland** — `wl_surface::enter` is emitted on a different output.

When the signal fires and per-screen DPI is enabled, `ScreenAdapter` recomputes
all scale factors for the new screen and emits `ScaleChanged()`. Every window
that inherits `AdaptiveMixin` then calls `OnAdaptLayout()` to update itself.

To disable this behaviour at runtime:

```cpp
ScreenAdapter::Instance()->SetPerScreenDpiEnabled(false);
```

---

## Scaling Functions

All functions below are global `inline` functions (not macros). They delegate
to `ScreenAdapter::Instance()` and can be used anywhere after `Init()`.

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `SV(x)` | `int` | `int` | Scale integer by uniform factor |
| `SVF(x)` | `qreal` | `qreal` | Scale floating-point by uniform factor |
| `SVX(x)` | `int` | `int` | Scale along X axis only (non-uniform mode) |
| `SVY(y)` | `int` | `int` | Scale along Y axis only (non-uniform mode) |
| `SS(w, h)` | `int, int` | `QSize` | Build scaled QSize |
| `SR(x, y, w, h)` | `int x4` | `QRect` | Build scaled QRect |
| `SFS(pt)` | `int` | `int` | Scale font point size |
| `SF(font)` | `QFont` | `QFont` | Return font with scaled point size |
| `gSA()` | — | `ScreenAdapter*` | Singleton pointer shortcut |

The singleton also exposes lower-level methods:

```cpp
ScreenAdapter* sa = ScreenAdapter::Instance();
qreal scale  = sa->Scale();       // uniform scale factor
qreal sx     = sa->ScaleX();      // X-axis factor
qreal sy     = sa->ScaleY();      // Y-axis factor
qreal dpr    = sa->Dpr();         // device pixel ratio
bool  hi_dpi = sa->IsHighDpi();
bool  tablet = sa->IsTablet();
QSize screen = sa->ScreenSize();  // full screen logical size
QSize avail  = sa->AvailableSize(); // minus taskbar / status bar
```

---

## AdaptiveMixin in Detail

### Geometry strategies

```cpp
class MyWindow : public AdaptiveMixin<QWidget>
{
    Q_OBJECT
public:
    MyWindow(QWidget* parent = nullptr) : AdaptiveMixin<QWidget>(parent)
    {
        // --- Strategy A: design-space coordinates ---
        SetDesignGeometry(480, 240, 960, 600);

        // --- Strategy B: percentage of screen, auto-centred ---
        // SetScreenPercent(0.80, 0.70);

        // --- Strategy C: full-screen (Android gets immersive mode) ---
        // ShowAdaptiveFullScreen();

        // --- Optional: scale all child fonts automatically ---
        // SetAutoFontScale(true);
    }
};
```

### Re-layout callback

Override `OnAdaptLayout()` to re-position child widgets after a scale change.
If you use Qt layout managers (e.g. `QVBoxLayout`), the layout engine updates
children for you and you typically do not need this override.

```cpp
protected:
    void OnAdaptLayout() override
    {
        AdaptiveMixin<QWidget>::OnAdaptLayout(); // re-size the window itself

        // Re-apply manual child positions using scaling functions:
        btn_->setGeometry(SR(680, 500, 120, 44));
        lbl_->setFont(QFont("Arial", SFS(14)));
    }
```

### Type aliases for common cases

```cpp
using AdaptiveWidget = AdaptiveMixin<QWidget>;  // self-adapting QWidget
using AdaptiveDialog = AdaptiveMixin<QDialog>;  // self-adapting QDialog
```

---

## Android

### Full-screen immersive mode

```cpp
// In main.cpp after Init():
#ifdef Q_OS_ANDROID
    window.ShowAdaptiveFullScreen();
#else
    window.show();
#endif
```

`ShowAdaptiveFullScreen()` calls `ScreenAdapter::SetFullScreen()` which uses
JNI to:
- Qt 5: `QtAndroid::runOnAndroidThread` + `QAndroidJniObject`
- Qt 6: `QJniObject` directly

The call hides the status bar and navigation bar and sets sticky-immersive mode.
`SetAutoFontScale(true)` is recommended for phone / tablet layouts.

### Keep screen on

```cpp
ScreenAdapter::SetKeepScreenOn(true);
```

### Dedicated Android base class

For views that must always run full-screen on Android:

```cpp
#ifdef SA_ANDROID
class MyFullScreenView : public AndroidFullWidget { ... };
#endif
```

`AndroidFullWidget` inherits `AdaptiveMixin<QWidget>`, sets frameless window
flags, and calls `ShowAdaptiveFullScreen()` in its `showEvent`.

---

## Naming Conventions

This library follows the conventions below; use them in subclasses for consistency.

| Category | Convention | Example |
|----------|-----------|---------|
| Member functions | UpperCamelCase | `OnAdaptLayout()`, `SetDesignGeometry()` |
| Private/protected member variables | lower\_snake\_case\_ (trailing underscore) | `design_rect_`, `is_tablet_` |
| Public struct fields (Config) | lower\_snake\_case (no trailing underscore) | `design_width`, `per_screen_dpi` |
| Function parameters | lower\_snake\_case | `w_pct`, `diag_inch` |
| Local variables | lower\_snake\_case | `avail_size`, `orig_pt` |
| Platform guard macros | SA\_ prefix | `SA_QT5`, `SA_ANDROID`, `SA_WINDOWS` |

Global convenience functions (`SV`, `SS`, `SR`, `SFS`, `SF`, `gSA`) are kept
short intentionally. All other identifiers follow the table above.

---

## Uniform vs. Dual-Axis Scale

Setting `use_min_scale = true` (default) picks the **smaller** of the two scale
factors:

    s = min( screen_width / design_width,  screen_height / design_height )

This guarantees that the entire UI fits on-screen without clipping, preserving
the aspect ratio. Black bars may appear on one side.

Setting `use_min_scale = false` gives each axis its own factor (`sx`, `sy`). The
UI fills the screen completely but may appear stretched. This is suited for
always-full-screen UIs where stretching is acceptable.

---

## Font Scaling

Two strategies controlled by `Config::font_follow_dpi`:

| Value | Behaviour |
|-------|-----------|
| `false` (default) | Font point size multiplied by the UI scale factor. The font keeps the same visual proportion relative to other UI elements. Recommended for most apps. |
| `true` | Font point size multiplied by `screen_dpi / design_dpi`. The font appears at the same physical (millimetre) size regardless of resolution. Useful when text readability at a fixed physical size is critical. |

To also scale fonts of all child widgets automatically:

```cpp
SetAutoFontScale(true);   // call in the widget constructor
```

Or scale a specific widget tree manually at any time:

```cpp
ScreenAdapter::Instance()->ApplyFontScaleRecursive(my_widget);
```

---

## Qt5 / Qt6 Compatibility Notes

| Topic | Qt 5 | Qt 6 |
|-------|------|------|
| High-DPI enable | `AA_EnableHighDpiScaling` attribute | Default on; `PassThrough` rounding |
| Device pixel ratio | `QScreen::devicePixelRatio()` | Same |
| Widget screen | `windowHandle()->screen()` (5.10+) or `QWidget::screen()` (5.14+) | `QWidget::screen()` |
| Android JNI | `QAndroidJniObject` + `QtAndroid` | `QJniObject` |
| `screenChanged` signal | `QWindow::screenChanged` | Same |
