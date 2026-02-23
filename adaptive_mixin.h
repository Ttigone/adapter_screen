#ifndef ADAPTIVE_MIXIN_H
#define ADAPTIVE_MIXIN_H

#include <QDialog>
#include <QEvent>
#include <QShowEvent>
#include <QWidget>
#include <QWindow>
#include <type_traits>

#include "screen_adapter.h"

/**
 * @brief AdaptiveMixin<BaseWidget> — CRTP mix-in template for screen
 * adaptation.
 *
 * Adds full screen-adaptation to any QWidget-derived class with a single
 * base-class substitution:
 *
 *   class MyWindow : public AdaptiveMixin<QWidget>   { Q_OBJECT ... };
 *   class MyDialog : public AdaptiveMixin<QDialog>   { Q_OBJECT ... };
 *
 * Capabilities provided automatically:
 *   - Responds to QWindow::screenChanged (window dragged to a different
 * monitor).
 *   - Responds to ScreenAdapter::ScaleChanged (resolution / DPI change).
 *   - Three geometry initialisation strategies (see below).
 *   - Optional recursive font scaling.
 *   - Android immersive full-screen helper.
 *
 * Geometry strategy A — design coordinates (auto-scaled):
 *   SetDesignGeometry(200, 100, 960, 600);
 *
 * Geometry strategy B — percentage of screen area:
 *   SetScreenPercent(0.80, 0.70);   // 80% wide, 70% tall, centred
 *
 * Geometry strategy C — full-screen (Android gets immersive mode):
 *   ShowAdaptiveFullScreen();
 *
 * NOTE: Template classes cannot contain Q_OBJECT, so signals/slots must be
 *       declared in the concrete subclass.
 */
template <class BaseWidget>
class AdaptiveMixin : public BaseWidget {
  static_assert(std::is_base_of<QWidget, BaseWidget>::value,
                "AdaptiveMixin<T> requires T to derive from QWidget");

 public:
  explicit AdaptiveMixin(QWidget* parent = nullptr) : BaseWidget(parent) {
    // Connect to the global ScaleChanged signal.
    // Using 'this' as context ensures the connection is removed when the
    // widget is destroyed, preventing dangling calls.
    QObject::connect(ScreenAdapter::Instance(), &ScreenAdapter::ScaleChanged,
                     this, [this] { OnAdaptLayout(); });
  }

  /**
   * @brief Strategy A: set geometry using design-space coordinates.
   * @param x, y, w, h  Logical pixels in the design reference resolution.
   *
   * The coordinates are stored and re-applied whenever OnAdaptLayout() is
   * called (e.g. after a monitor switch or system DPI change).
   */
  void SetDesignGeometry(int x, int y, int w, int h) {
    design_rect_ = QRect(x, y, w, h);
    ScreenAdapter::Instance()->SetScaledGeometry(this, x, y, w, h);
  }

  /**
   * @brief Strategy B: size the widget as a fraction of the screen area and
   * centre it.
   * @param w_pct  Width fraction [0.0, 1.0].
   * @param h_pct  Height fraction [0.0, 1.0].
   */
  void SetScreenPercent(qreal w_pct, qreal h_pct) {
    design_rect_ = QRect();  // clear fixed coords
    screen_w_pct_ = w_pct;
    screen_h_pct_ = h_pct;
    use_screen_pct_ = true;

    ScreenAdapter::Instance()->ResizeByScreenPercent(this, w_pct, h_pct);
    ScreenAdapter::Instance()->CenterOnScreen(this);
  }

  /**
   * @brief Strategy C: show full-screen; on Android also enters immersive mode.
   */
  void ShowAdaptiveFullScreen() {
    is_full_screen_ = true;
#ifdef SA_ANDROID
    ScreenAdapter::SetFullScreen();
#endif
    BaseWidget::showFullScreen();
  }

  // ─────────────────────────────────────────────────────────────────────
  //  Options
  // ─────────────────────────────────────────────────────────────────────

  /**
   * @brief Enable / disable automatic recursive font scaling on show and
   *        on every ScaleChanged event.  Default: disabled.
   */
  void SetAutoFontScale(bool enable) { auto_font_scale_ = enable; }

 protected:
  // ─────────────────────────────────────────────────────────────────────
  //  Override this to re-position child widgets after a scale change.
  // ─────────────────────────────────────────────────────────────────────

  /**
   * @brief Called whenever scale factors change (monitor switch, DPI change).
   *
   * Default implementation re-applies whichever geometry strategy was set.
   * If you use Qt layout managers (QVBoxLayout, QGridLayout, ...) you often
   * do NOT need to override this — the layout engine resizes children for you.
   *
   * When you do override, call the base first (optional) then re-set child
   * geometries using SR() / SS() / SFS():
   * @code
   * void MyWindow::OnAdaptLayout() {
   *     AdaptiveMixin<QWidget>::OnAdaptLayout(); // resize the window itself
   *     btn_->setGeometry(SR(10, 10, 120, 40));
   *     lbl_->setFont(QFont("Arial", SFS(14)));
   * }
   * @endcode
   */
  virtual void OnAdaptLayout() {
    if (is_full_screen_) {
      BaseWidget::showFullScreen();
    } else if (use_screen_pct_) {
      ScreenAdapter::Instance()->ResizeByScreenPercent(this, screen_w_pct_,
                                                       screen_h_pct_);
      ScreenAdapter::Instance()->CenterOnScreen(this);
    } else if (design_rect_.isValid()) {
      ScreenAdapter::Instance()->SetScaledGeometry(
          this, design_rect_.x(), design_rect_.y(), design_rect_.width(),
          design_rect_.height());
    }
    // Pure Qt-layout windows: nothing extra needed here.
  }

  // ─────────────────────────────────────────────────────────────────────
  //  Qt event overrides
  // ─────────────────────────────────────────────────────────────────────

  void showEvent(QShowEvent* e) override {
    BaseWidget::showEvent(e);

    // The QWindow handle is valid from the first showEvent; connect here.
    ConnectScreenSignal();

    if (auto_font_scale_)
      ScreenAdapter::Instance()->ApplyFontScaleRecursive(this);

#ifdef SA_ANDROID
    if (is_full_screen_)
      ScreenAdapter::SetFullScreen();  // Re-assert; system can reset it.
#endif
  }

  bool event(QEvent* e) override {
    // WinIdChange fires when the QWindow is created or re-parented.
    if (e->type() == QEvent::WinIdChange) ConnectScreenSignal();
    return BaseWidget::event(e);
  }

 private:
  // ─────────────────────────────────────────────────────────────────────
  //  Per-monitor signal management
  // ─────────────────────────────────────────────────────────────────────
  void ConnectScreenSignal() {
    QWindow* win = BaseWidget::windowHandle();
    if (!win || win == tracked_window_) return;

    if (screen_conn_) QObject::disconnect(screen_conn_);

    tracked_window_ = win;

    // QWindow::screenChanged fires when:
    //   Windows 10/11 — WM_DPICHANGED (Per-Monitor DPI Awareness v2).
    //   Linux X11      — RRNotify when the window crosses monitor boundaries.
    //   Linux Wayland  — wl_surface::enter on a different output.
    screen_conn_ = QObject::connect(
        win, &QWindow::screenChanged, this, [this](QScreen* screen) {
          // Only recompute global scale factors when the feature is on.
          if (ScreenAdapter::Instance()->IsPerScreenDpiEnabled()) {
            ScreenAdapter::Instance()->RefreshForScreen(screen);
          }
          OnAdaptLayout();
          if (auto_font_scale_)
            ScreenAdapter::Instance()->ApplyFontScaleRecursive(this);
#ifdef SA_ANDROID
          if (is_full_screen_) ScreenAdapter::SetFullScreen();
#endif
        });
  }

  // ─────────────────────────────────────────────────────────────────────
  //  State
  // ─────────────────────────────────────────────────────────────────────
  QRect design_rect_;                    ///< Strategy A design coords
  qreal screen_w_pct_ = 0.8;             ///< Strategy B width fraction
  qreal screen_h_pct_ = 0.8;             ///< Strategy B height fraction
  bool use_screen_pct_ = false;          ///< Using strategy B?
  bool is_full_screen_ = false;          ///< Using strategy C?
  bool auto_font_scale_ = false;         ///< Auto font scaling enabled?
  QWindow* tracked_window_ = nullptr;    ///< QWindow we've connected
  QMetaObject::Connection screen_conn_;  ///< screenChanged connection
};

using AdaptiveWidget = AdaptiveMixin<QWidget>;  ///< Adaptive QWidget
using AdaptiveDialog = AdaptiveMixin<QDialog>;  ///< Adaptive QDialog

#ifdef SA_ANDROID
/**
 * @brief AndroidFullWidget — convenience base for always-immersive Android
 * views.
 *
 * Automatically: frameless, full-screen, immersive mode.
 */
class AndroidFullWidget : public AdaptiveMixin<QWidget> {
 public:
  explicit AndroidFullWidget(QWidget* parent = nullptr)
      : AdaptiveMixin<QWidget>(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
  }

 protected:
  void showEvent(QShowEvent* e) override {
    AdaptiveMixin<QWidget>::showEvent(e);
    ShowAdaptiveFullScreen();
  }
};
#endif  // SA_ANDROID

#endif  // ADAPTIVE_MIXIN_H