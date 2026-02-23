#include <QtWidgets/QApplication>

#include "adapter_screen.h"
#include "screen_adapter.h"


int main(int argc, char* argv[]) {
  // Step 1 — MUST be called before QApplication.
  //   Qt5: enables AA_EnableHighDpiScaling + AA_UseHighDpiPixmaps.
  //   Qt6: sets PassThrough DPR rounding for fractional scale factors.
  ScreenAdapter::SetupHighDpiPolicy();

  QApplication app(argc, argv);

  // Step 2 — MUST be called immediately after QApplication.
  //   Uses default Config: design canvas 1920x1080, uniform scale,
  //   per-screen DPI adaptation enabled.
  ScreenAdapter::Instance()->Init();

  // ── Optional: custom config ─────────────────────────────────────────────
  // ScreenAdapter::Config cfg;
  // cfg.design_width    = 1280;   // your UI design canvas width
  // cfg.design_height   = 800;    // your UI design canvas height
  // cfg.design_dpi      = 96.0;
  // cfg.use_min_scale   = true;   // uniform scale (recommended)
  // cfg.font_follow_dpi = false;  // font tracks UI scale (recommended)
  // cfg.per_screen_dpi  = true;   // auto-adapt when window changes monitor
  // ScreenAdapter::Instance()->Init(cfg);
  //
  // You can also toggle per-screen DPI at any time after Init():
  // ScreenAdapter::Instance()->SetPerScreenDpiEnabled(false);
  // ───────────────────────────────────────────────────────────────────────

  AdapterScreen window;

#ifdef Q_OS_ANDROID
  window.ShowAdaptiveFullScreen();
#else
  window.show();
#endif

  return app.exec();
}
