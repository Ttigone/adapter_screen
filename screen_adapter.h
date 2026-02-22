#pragma once

#include <QFont>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QtGlobal>

class QScreen;
class QWidget;

// ─────────────────────────────────────────────────────────────────────────────
//  Qt version compatibility (internal)
// ─────────────────────────────────────────────────────────────────────────────
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  define SA_QT6 1
#else
#  define SA_QT5 1
#endif

#ifdef Q_OS_ANDROID
#  define SA_ANDROID 1
#endif
#ifdef Q_OS_WIN
#  define SA_WINDOWS 1
#endif

/**
 * @brief ScreenAdapter — cross-platform screen-scaling singleton.
 *
 * Core strategy
 * -------------
 * A "design resolution" (default 1920 x 1080) is the reference coordinate
 * space.  At run-time the logical screen size is measured and two scale
 * factors (sx_, sy_) are derived.  Every pixel-related coordinate, size or
 * font size is multiplied through one of those factors before being applied
 * to a widget.
 *
 * Quick-start
 * -----------
 * @code
 * // main.cpp  — BEFORE QApplication
 * ScreenAdapter::SetupHighDpiPolicy();
 * QApplication app(argc, argv);
 *
 * // main.cpp  — AFTER QApplication
 * ScreenAdapter::Instance()->Init();
 *
 * // Any widget constructor
 * resize(SS(1280, 720));
 * btn->setGeometry(SR(10, 10, 120, 40));
 * lbl->setFont(QFont("Arial", SFS(14)));
 * @endcode
 *
 * Multi-screen auto-awareness
 * ---------------------------
 * Inherit from AdaptiveMixin<T> instead of QWidget/QDialog.  When the window
 * is dragged to a different monitor the layout is refreshed automatically.
 */
class ScreenAdapter : public QObject
{
    Q_OBJECT

public:
    // ─────────────────────────────────────────────────────────────────────
    //  Configuration  (public struct — fields use plain lower_snake_case)
    // ─────────────────────────────────────────────────────────────────────
    struct Config
    {
        /// Reference UI width in logical pixels (your design canvas width).
        int    design_width    = 1920;
        /// Reference UI height in logical pixels.
        int    design_height   = 1080;
        /// Reference DPI assumed during UI design (96 on Windows, 72 on macOS).
        qreal  design_dpi      = 96.0;
        /**
         * true  = uniform scale: min(sx, sy) — content stays fully visible.
         * false = dual-axis scale: each axis scaled independently (full-bleed).
         */
        bool   use_min_scale   = true;
        /**
         * true  = font size tracks physical DPI (font appears same physical
         *         size regardless of resolution).
         * false = font size tracks the UI scale factor (font keeps the same
         *         visual proportion relative to other UI elements).
         */
        bool   font_follow_dpi = false;
        /**
         * true  = when the window moves to a monitor with a different DPI /
         *         resolution, the scale factors are automatically recomputed.
         *         Supports Windows 10/11 Per-Monitor DPI Awareness v2 and
         *         Linux (X11 / Wayland) multi-monitor setups.
         * false = always use the scale factors computed at Init() time;
         *         moving the window between monitors has no effect.
         */
        bool   per_screen_dpi  = true;
    };

    // ─────────────────────────────────────────────────────────────────────
    //  Singleton & initialisation
    // ─────────────────────────────────────────────────────────────────────

    static ScreenAdapter* Instance();

    /**
     * @brief Must be called BEFORE constructing QApplication / QGuiApplication.
     *
     * Qt5 : enables AA_EnableHighDpiScaling + AA_UseHighDpiPixmaps.
     * Qt6 : sets PassThrough rounding policy for the most accurate sub-pixel DPR.
     */
    static void SetupHighDpiPolicy();

    /**
     * @brief Must be called immediately AFTER constructing QApplication.
     * @param cfg  Optional custom config; defaults to 1920x1080 @ 96 DPI.
     */
    void Init(const Config& cfg = Config{});

    // ─────────────────────────────────────────────────────────────────────
    //  Scale factor accessors
    // ─────────────────────────────────────────────────────────────────────

    qreal ScaleX()    const noexcept { return sx_;         } ///< X-axis scale factor
    qreal ScaleY()    const noexcept { return sy_;         } ///< Y-axis scale factor
    qreal Scale()     const noexcept { return s_;          } ///< Uniform scale factor
    qreal DpiScale()  const noexcept { return dpi_scale_;  } ///< Screen DPI / design DPI
    qreal FontScale() const noexcept { return font_scale_; } ///< Font-specific scale
    qreal Dpr()       const noexcept { return dpr_;        } ///< Device pixel ratio

    // ─────────────────────────────────────────────────────────────────────
    //  Inline scaling utilities  (high-frequency, keep in header)
    // ─────────────────────────────────────────────────────────────────────

    /// Scale integer pixel value using the uniform scale factor.
    inline int   Sv (int v)              const noexcept { return qRound(v * s_);              }
    /// Scale floating-point pixel value using the uniform scale factor.
    inline qreal Sv (qreal v)            const noexcept { return v * s_;                      }
    /// Scale an X-axis integer value (uses sx_, meaningful in non-uniform mode).
    inline int   Svx(int v)              const noexcept { return qRound(v * sx_);             }
    /// Scale a Y-axis integer value (uses sy_, meaningful in non-uniform mode).
    inline int   Svy(int v)              const noexcept { return qRound(v * sy_);             }
    /// Build a scaled QSize from design-space integers.
    inline QSize Ss (int w, int h)       const noexcept { return { Sv(w),  Sv(h)  };         }
    inline QSize Ss (const QSize& sz)    const noexcept { return Ss(sz.width(), sz.height()); }
    /// Build a scaled QRect; position uses dual-axis, size uses uniform scale.
    inline QRect Sr (int x, int y, int w, int h) const noexcept
    {
        return { Svx(x), Svy(y), Sv(w), Sv(h) };
    }
    inline QRect Sr (const QRect& r)     const noexcept
    {
        return Sr(r.x(), r.y(), r.width(), r.height());
    }
    /// Scale a font point size (returns at least 1).
    inline int   Sfs(int pt)             const noexcept { return qMax(1, qRound(pt * font_scale_)); }
    /// Return a copy of the font with its point size scaled.
    inline QFont Sf (QFont f)            const
    {
        if (f.pointSize() > 0) f.setPointSize(Sfs(f.pointSize()));
        return f;
    }

    // ─────────────────────────────────────────────────────────────────────
    //  Screen information
    // ─────────────────────────────────────────────────────────────────────

    QSize ScreenSize()    const noexcept { return screen_size_; } ///< Full logical screen size
    QSize AvailableSize() const noexcept { return avail_size_;  } ///< Available area (minus taskbar)
    qreal ScreenDpi()     const noexcept { return screen_dpi_;  } ///< Logical DPI of current screen
    bool  IsHighDpi()     const noexcept { return dpr_ > 1.25;  } ///< True when DPR > 1.25
    bool  IsTablet()      const noexcept { return is_tablet_;   } ///< Physical diagonal 7–13 inches
    bool  IsMobile()      const noexcept { return is_mobile_;   } ///< Android phone < 7 inches
    bool  IsLandscape()   const noexcept
    {
        return screen_size_.width() > screen_size_.height();
    }

    // ─────────────────────────────────────────────────────────────────────
    //  Per-screen DPI toggle
    //  Controls whether moving a window to a different monitor automatically
    //  recomputes the scale factors (and re-lays out all AdaptiveMixin windows).
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Enable or disable dynamic per-screen DPI adaptation at runtime.
     *
     * When enabled (default from Config::per_screen_dpi = true):
     *   - Windows 10/11: leverages Per-Monitor DPI Awareness v2.  The OS
     *     delivers a WM_DPICHANGED message which Qt translates to a
     *     QWindow::screenChanged signal; this library hooks that signal.
     *   - Linux (X11/Wayland): Qt emits QWindow::screenChanged when the
     *     window crosses monitor boundaries; same hook applies.
     *
     * When disabled:
     *   - Scale factors remain as computed at Init() time regardless of
     *     which monitor the window is on.  Useful when you want consistent
     *     sizes across all monitors.
     */
    void SetPerScreenDpiEnabled(bool enable) { per_screen_dpi_enabled_ = enable; }
    bool IsPerScreenDpiEnabled() const noexcept { return per_screen_dpi_enabled_; }

    // ─────────────────────────────────────────────────────────────────────
    //  Multi-screen support
    // ─────────────────────────────────────────────────────────────────────

    /// Compute the uniform scale for a specific screen without changing state.
    qreal ScaleForScreen(QScreen* screen) const;

    /**
     * @brief Recompute scale factors for the given screen and emit ScaleChanged.
     *        Called automatically by AdaptiveMixin when QWindow::screenChanged fires.
     */
    void RefreshForScreen(QScreen* screen);

    /// Return the QScreen the widget currently resides on (Qt5 / Qt6 compatible).
    static QScreen* ScreenOf(const QWidget* w);

    // ─────────────────────────────────────────────────────────────────────
    //  Widget helpers
    // ─────────────────────────────────────────────────────────────────────

    /// Apply design-space geometry to a widget, scaled automatically.
    void SetScaledGeometry(QWidget* w, int x, int y, int width, int height) const;

    /// Center a widget on the given screen (nullptr = widget's own screen).
    void CenterOnScreen(QWidget* w, QScreen* screen = nullptr) const;

    /**
     * @brief Resize a widget to a fraction of the screen's available area.
     * @param w_pct  Width fraction [0.0, 1.0].
     * @param h_pct  Height fraction [0.0, 1.0].
     */
    void ResizeByScreenPercent(QWidget* w, qreal w_pct, qreal h_pct,
                               QScreen* screen = nullptr) const;

    /**
     * @brief Recursively apply font scaling to a widget and all its children.
     *
     * Stores the original point size in Qt property "_sa_orig_pt" so repeated
     * calls after a scale change always start from the original size.
     */
    void ApplyFontScaleRecursive(QWidget* w) const;

    // ─────────────────────────────────────────────────────────────────────
    //  Android-specific helpers
    // ─────────────────────────────────────────────────────────────────────
#ifdef SA_ANDROID
    /// Enter Android immersive full-screen (hides status bar + nav bar via JNI).
    static void SetFullScreen();
    /// Keep the screen on / allow it to turn off.
    static void SetKeepScreenOn(bool on);
#endif

signals:
    /**
     * @brief Emitted whenever scale factors change (monitor switch, resolution
     *        change, system DPI setting change).
     *        AdaptiveMixin connects this to re-layout all managed windows.
     */
    void ScaleChanged();

private slots:
    void OnPrimaryScreenChanged(QScreen* screen);
    void OnScreenGeometryChanged(const QRect& new_geometry);

private:
    explicit ScreenAdapter(QObject* parent = nullptr);

    void Detect(QScreen* screen);
    void DetectDeviceType(QScreen* screen);

    static ScreenAdapter* s_instance_;

    Config cfg_;
    bool   initialized_           = false;
    bool   per_screen_dpi_enabled_ = true;

    // Scale factors
    qreal  sx_          = 1.0;  ///< X-axis scale
    qreal  sy_          = 1.0;  ///< Y-axis scale
    qreal  s_           = 1.0;  ///< Uniform scale
    qreal  dpi_scale_   = 1.0;  ///< DPI-based scale
    qreal  font_scale_  = 1.0;  ///< Font scale
    qreal  dpr_         = 1.0;  ///< Device pixel ratio

    // Screen info
    QSize  screen_size_;
    QSize  avail_size_;
    qreal  screen_dpi_  = 96.0;

    // Device type
    bool   is_tablet_   = false;
    bool   is_mobile_   = false;
};

// =============================================================================
//  Global convenience functions
//  These replace the old #define macros.  Being real inline functions they
//  obey scope, overload resolution and type-checking.
// =============================================================================

/// Return the ScreenAdapter singleton pointer.
inline ScreenAdapter* gSA() { return ScreenAdapter::Instance(); }

/// Scale an integer pixel value (uniform scale).
inline int    SV  (int x)                      { return ScreenAdapter::Instance()->Sv(x);           }
/// Scale a floating-point pixel value (uniform scale).
inline qreal  SVF (qreal x)                    { return ScreenAdapter::Instance()->Sv(x);           }
/// Scale an X-axis integer value.
inline int    SVX (int x)                      { return ScreenAdapter::Instance()->Svx(x);          }
/// Scale a Y-axis integer value.
inline int    SVY (int y)                      { return ScreenAdapter::Instance()->Svy(y);          }
/// Build a scaled QSize(w, h).
inline QSize  SS  (int w, int h)               { return ScreenAdapter::Instance()->Ss(w, h);        }
/// Build a scaled QRect(x, y, w, h).
inline QRect  SR  (int x, int y, int w, int h) { return ScreenAdapter::Instance()->Sr(x, y, w, h); }
/// Scale a font point size.
inline int    SFS (int pt)                     { return ScreenAdapter::Instance()->Sfs(pt);         }
/// Return a font with its point size scaled.
inline QFont  SF  (const QFont& font)          { return ScreenAdapter::Instance()->Sf(font);        }
