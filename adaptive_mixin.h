#ifndef ADAPTIVE_MIXIN_H
#define ADAPTIVE_MIXIN_H

#include <QDialog>
#include <QEvent>
#include <QShowEvent>
#include <QWidget>
#include <QWindow>
#include <type_traits>

#include "screen_adapter.h"

template <class BaseWidget>
class AdaptiveMixin : public BaseWidget {
  static_assert(std::is_base_of<QWidget, BaseWidget>::value,
                "AdaptiveMixin<T> requires T to derive from QWidget");

 public:
  explicit AdaptiveMixin(QWidget* parent = nullptr) : BaseWidget(parent) {
    QObject::connect(ScreenAdapter::Instance(), &ScreenAdapter::ScaleChanged,
                     this, [this] { OnAdaptLayout(); });
  }

  void SetDesignGeometry(int x, int y, int w, int h) {
    design_rect_ = QRect(x, y, w, h);
    ScreenAdapter::Instance()->SetScaledGeometry(this, x, y, w, h);
  }

  void SetScreenPercent(qreal w_pct, qreal h_pct) {
    design_rect_ = QRect();
    screen_w_pct_ = w_pct;
    screen_h_pct_ = h_pct;
    use_screen_pct_ = true;

    ScreenAdapter::Instance()->ResizeByScreenPercent(this, w_pct, h_pct);
    ScreenAdapter::Instance()->CenterOnScreen(this);
  }

  // Default true to preserve existing behaviour.
  // Set false to avoid resize-fighting while dragging windows across monitors.
  void SetResizeOnScaleChange(bool enable) { resize_on_scale_change_ = enable; }

  // Default false: do not force recenter when monitor/DPI changes.
  // Keeping this off avoids geometry-fighting while user drags windows
  // across monitors with different DPI.
  void SetRecenterOnScaleChange(bool enable) { recenter_on_scale_change_ = enable; }

  void ShowAdaptiveFullScreen() {
    is_full_screen_ = true;
#ifdef SA_ANDROID
    ScreenAdapter::SetFullScreen();
#endif
    BaseWidget::showFullScreen();
  }

  void SetAutoFontScale(bool enable) { auto_font_scale_ = enable; }

 protected:
  virtual void OnAdaptLayout() {
    if (is_full_screen_) {
      BaseWidget::showFullScreen();
    } else if (use_screen_pct_) {
      if (resize_on_scale_change_) {
        ScreenAdapter::Instance()->ResizeByScreenPercent(this, screen_w_pct_,
                                                         screen_h_pct_);
      }
      if (recenter_on_scale_change_) {
        ScreenAdapter::Instance()->CenterOnScreen(this);
      }
    } else if (design_rect_.isValid()) {
      ScreenAdapter::Instance()->SetScaledGeometry(
          this, design_rect_.x(), design_rect_.y(), design_rect_.width(),
          design_rect_.height());
    }
  }

  void showEvent(QShowEvent* e) override {
    BaseWidget::showEvent(e);
    ConnectScreenSignal();

    if (auto_font_scale_)
      ScreenAdapter::Instance()->ApplyFontScaleRecursive(this);

#ifdef SA_ANDROID
    if (is_full_screen_)
      ScreenAdapter::SetFullScreen();
#endif
  }

  bool event(QEvent* e) override {
    if (e->type() == QEvent::WinIdChange) ConnectScreenSignal();
    return BaseWidget::event(e);
  }

 private:
  void ConnectScreenSignal() {
    QWindow* win = BaseWidget::windowHandle();
    if (!win || win == tracked_window_) return;

    if (screen_conn_) QObject::disconnect(screen_conn_);

    tracked_window_ = win;

    screen_conn_ = QObject::connect(
        win, &QWindow::screenChanged, this, [this](QScreen* screen) {
          if (ScreenAdapter::Instance()->IsPerScreenDpiEnabled()) {
            ScreenAdapter::Instance()->RefreshForScreen(screen);
          } else {
            // If per-screen DPI is disabled, no ScaleChanged will be emitted.
            // Keep a single local adaptation pass in this branch only.
            OnAdaptLayout();
          }

          if (auto_font_scale_)
            ScreenAdapter::Instance()->ApplyFontScaleRecursive(this);
#ifdef SA_ANDROID
          if (is_full_screen_) ScreenAdapter::SetFullScreen();
#endif
        });
  }

  QRect design_rect_;
  qreal screen_w_pct_ = 0.8;
  qreal screen_h_pct_ = 0.8;
  bool use_screen_pct_ = false;
  bool is_full_screen_ = false;
  bool auto_font_scale_ = false;
  bool resize_on_scale_change_ = true;
  bool recenter_on_scale_change_ = false;
  QWindow* tracked_window_ = nullptr;
  QMetaObject::Connection screen_conn_;
};

using AdaptiveWidget = AdaptiveMixin<QWidget>;
using AdaptiveDialog = AdaptiveMixin<QDialog>;

#endif  // ADAPTIVE_MIXIN_H
