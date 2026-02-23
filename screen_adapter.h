#ifndef SCREEN_ADAPTER_H
#define SCREEN_ADAPTER_H

#include <QFont>
#include <QObject>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <QtGlobal>

class QWidget;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define SA_QT6 1
#else
#define SA_QT5 1
#endif

#ifdef Q_OS_ANDROID
#define SA_ANDROID 1
#endif
#ifdef Q_OS_WIN
#define SA_WINDOWS 1
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
 * Inherit from AdaptiveMixin<T> instead of QWidget/QDialog.  When the window
 * is dragged to a different monitor the layout is refreshed automatically.
 */
class ScreenAdapter : public QObject {
  Q_OBJECT
 public:
  struct Config {
    int design_width = 1920;
    int design_height = 1080;
    qreal design_dpi = 96.0;
    bool use_min_scale = true;
    bool font_follow_dpi = false;
    bool per_screen_dpi = true;
  };

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

  qreal ScaleX() const noexcept { return sx_; }
  qreal ScaleY() const noexcept { return sy_; }
  qreal Scale() const noexcept { return s_; }
  qreal DpiScale() const noexcept { return dpi_scale_; }
  qreal FontScale() const noexcept { return font_scale_; }
  qreal Dpr() const noexcept { return dpr_; }

  inline int Sv(int v) const noexcept { return qRound(v * s_); }
  inline qreal Sv(qreal v) const noexcept { return v * s_; }
  inline int Svx(int v) const noexcept { return qRound(v * sx_); }
  inline int Svy(int v) const noexcept { return qRound(v * sy_); }
  inline QSize Ss(int w, int h) const noexcept { return {Sv(w), Sv(h)}; }
  inline QSize Ss(const QSize& sz) const noexcept {
    return Ss(sz.width(), sz.height());
  }
  inline QRect Sr(int x, int y, int w, int h) const noexcept {
    return {Svx(x), Svy(y), Sv(w), Sv(h)};
  }
  inline QRect Sr(const QRect& r) const noexcept {
    return Sr(r.x(), r.y(), r.width(), r.height());
  }
  inline int Sfs(int pt) const noexcept {
    return qMax(1, qRound(pt * font_scale_));
  }
  inline QFont Sf(QFont f) const {
    if (f.pointSize() > 0) f.setPointSize(Sfs(f.pointSize()));
    return f;
  }

  QSize ScreenSize() const noexcept { return screen_size_; }
  QSize AvailableSize() const noexcept { return avail_size_; }
  qreal ScreenDpi() const noexcept { return screen_dpi_; }
  bool IsHighDpi() const noexcept { return dpr_ > 1.25; }
  bool IsTablet() const noexcept { return is_tablet_; }
  bool IsMobile() const noexcept { return is_mobile_; }
  bool IsLandscape() const noexcept {
    return screen_size_.width() > screen_size_.height();
  }

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
  bool IsPerScreenDpiEnabled() const noexcept {
    return per_screen_dpi_enabled_;
  }
  qreal ScaleForScreen(QScreen* screen) const;
  void RefreshForScreen(QScreen* screen);
  static QScreen* ScreenOf(const QWidget* w);
  void SetScaledGeometry(QWidget* w, int x, int y, int width, int height) const;
  void CenterOnScreen(QWidget* w, QScreen* screen = nullptr) const;

  /**
   * @brief Resize a widget to a fraction of the screen's available area.
   * @param w_pct  Width fraction [0.0, 1.0].
   * @param h_pct  Height fraction [0.0, 1.0].
   */
  void ResizeByScreenPercent(QWidget* w, qreal w_pct, qreal h_pct,
                             QScreen* screen = nullptr) const;
  void ApplyFontScaleRecursive(QWidget* w) const;

#ifdef SA_ANDROID
  static void SetFullScreen();
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
  bool initialized_ = false;
  bool per_screen_dpi_enabled_ = true;

  qreal sx_ = 1.0;
  qreal sy_ = 1.0;
  qreal s_ = 1.0;
  qreal dpi_scale_ = 1.0;
  qreal font_scale_ = 1.0;
  qreal dpr_ = 1.0;

  QSize screen_size_;
  QSize avail_size_;
  qreal screen_dpi_ = 96.0;

  bool is_tablet_ = false;
  bool is_mobile_ = false;
};

inline ScreenAdapter* gSA() { return ScreenAdapter::Instance(); }

inline int SV(int x) { return ScreenAdapter::Instance()->Sv(x); }
inline qreal SVF(qreal x) { return ScreenAdapter::Instance()->Sv(x); }
inline int SVX(int x) { return ScreenAdapter::Instance()->Svx(x); }
inline int SVY(int y) { return ScreenAdapter::Instance()->Svy(y); }
inline QSize SS(int w, int h) { return ScreenAdapter::Instance()->Ss(w, h); }
inline QRect SR(int x, int y, int w, int h) {
  return ScreenAdapter::Instance()->Sr(x, y, w, h);
}
inline int SFS(int pt) { return ScreenAdapter::Instance()->Sfs(pt); }
inline QFont SF(const QFont& font) {
  return ScreenAdapter::Instance()->Sf(font);
}

#endif  // SCREEN_ADAPTER_H