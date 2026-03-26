#include "screen_adapter.h"

#include <QApplication>
#include <QDebug>
#include <QFont>
#include <QScreen>
#include <QWidget>
#include <QWindow>
#include <QtMath>

#ifdef SA_ANDROID
#ifdef SA_QT6
// Qt 6.0+: QJniObject replaces QAndroidJniObject
#include <QJniObject>
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
#include <QCoreApplication>
#endif
#else
// Qt5 Android JNI support
#include <QAndroidJniObject>
#include <QtAndroid>
#endif
#endif

ScreenAdapter* ScreenAdapter::s_instance_ = nullptr;

ScreenAdapter* ScreenAdapter::Instance() {
  if (!s_instance_) {
    Q_ASSERT_X(qApp, "ScreenAdapter::Instance",
               "QApplication must be constructed before calling Instance()");
    s_instance_ = new ScreenAdapter(qApp);
  }
  return s_instance_;
}

ScreenAdapter::ScreenAdapter(QObject* parent) : QObject(parent) {
  connect(qApp, &QGuiApplication::primaryScreenChanged, this,
          &ScreenAdapter::OnPrimaryScreenChanged);
}

void ScreenAdapter::SetupHighDpiPolicy() {
#ifdef SA_QT5
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
  QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#else
  // Qt6: high-DPI is on by default; PassThrough gives the most accurate
  // fractional DPR (e.g. 1.5x instead of rounded to 2x).
  QApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
}

void ScreenAdapter::Init(const Config& cfg) {
  cfg_ = cfg;
  initialized_ = true;
  per_screen_dpi_enabled_ = cfg.per_screen_dpi;

  QScreen* primary = QApplication::primaryScreen();
  Detect(primary);
  DetectDeviceType(primary);

  if (primary) {
    connect(primary, &QScreen::geometryChanged, this,
            &ScreenAdapter::OnScreenGeometryChanged, Qt::UniqueConnection);
  }

  qDebug().nospace() << "[ScreenAdapter] Init OK"
                     << " | Design=" << cfg_.design_width << "x"
                     << cfg_.design_height
                     << " | Screen=" << screen_size_.width() << "x"
                     << screen_size_.height() << " | DPR=" << dpr_
                     << " | DPI=" << screen_dpi_ << " | scale=" << s_
                     << " (sx=" << sx_ << " sy=" << sy_ << ")"
                     << " | fontScale=" << font_scale_
                     << " | perScreenDpi=" << per_screen_dpi_enabled_
                     << " | tablet=" << is_tablet_ << " mobile=" << is_mobile_;
}

void ScreenAdapter::Detect(QScreen* screen) {
  if (!screen) {
    screen = QApplication::primaryScreen();
    if (!screen) return;
  }

  dpr_ = screen->devicePixelRatio();
  screen_dpi_ = screen->logicalDotsPerInch();

  screen_size_ = screen->geometry().size();
  avail_size_ = screen->availableGeometry().size();

  const qreal sx = static_cast<qreal>(screen_size_.width()) / cfg_.design_width;
  const qreal sy =
      static_cast<qreal>(screen_size_.height()) / cfg_.design_height;
  sx_ = sx;
  sy_ = sy;
  s_ = cfg_.use_min_scale ? qMin(sx, sy) : sx;

  dpi_scale_ = screen_dpi_ / cfg_.design_dpi;
  font_scale_ = cfg_.font_follow_dpi ? dpi_scale_ : s_;
}

void ScreenAdapter::DetectDeviceType(QScreen* screen) {
  if (!screen) {
    return;
  }

  const QSizeF phys = screen->physicalSize();
  const qreal diag_mm =
      qSqrt(phys.width() * phys.width() + phys.height() * phys.height());
  const qreal diag_inch = diag_mm / 25.4;

  qDebug() << "[ScreenAdapter] Physical diagonal:" << diag_inch << "inches";

#ifdef SA_ANDROID
  is_mobile_ = (diag_inch < 7.0);
  is_tablet_ = !is_mobile_;
#else
  is_mobile_ = false;
  // 7–13 inch covers Surface Pro, iPad-with-keyboard, etc. on desktop OSes.
  is_tablet_ = (diag_inch >= 7.0 && diag_inch < 13.0);
#endif
}

qreal ScreenAdapter::ScaleForScreen(QScreen* screen) const {
  if (!screen) {
    return s_;
  }
  const qreal sx =
      static_cast<qreal>(screen->geometry().width()) / cfg_.design_width;
  const qreal sy =
      static_cast<qreal>(screen->geometry().height()) / cfg_.design_height;
  return cfg_.use_min_scale ? qMin(sx, sy) : sx;
}

void ScreenAdapter::RefreshForScreen(QScreen* screen) {
  Detect(screen);
  DetectDeviceType(screen);
  emit ScaleChanged();

  qDebug() << "[ScreenAdapter] Refreshed for screen:"
           << (screen ? screen->name() : QStringLiteral("(null)"))
           << "| new scale=" << s_;
}

QScreen* ScreenAdapter::ScreenOf(const QWidget* w) {
  if (!w) {
    return QApplication::primaryScreen();
  }

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  // Qt 5.14+ and Qt6: QWidget::screen() is available directly.
  return const_cast<QWidget*>(w)->screen();

#elif QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
  // Qt 5.10–5.13: reach through the window handle.
  const QWidget* top = w->window();
  if (top && top->windowHandle()) {
    return top->windowHandle()->screen();
  }
  return QApplication::screenAt(
      const_cast<QWidget*>(w)->mapToGlobal(w->rect().center()));

#else
  Q_UNUSED(w)
  return QApplication::primaryScreen();
#endif
}

void ScreenAdapter::SetScaledGeometry(QWidget* w, int x, int y, int width,
                                      int height) const {
  if (w) {
    w->setGeometry(Svx(x), Svy(y), Sv(width), Sv(height));
  }
}

void ScreenAdapter::CenterOnScreen(QWidget* w, QScreen* screen) const {
  if (!w) {
    return;
  }
  if (!screen) {
    screen = ScreenOf(w);
  }
  if (!screen) {
    screen = QApplication::primaryScreen();
  }
  if (!screen) return;

  const QRect avail = screen->availableGeometry();
  const QSize sz = w->frameGeometry().size();
  w->move(avail.x() + (avail.width() - sz.width()) / 2,
          avail.y() + (avail.height() - sz.height()) / 2);
}

void ScreenAdapter::ResizeByScreenPercent(QWidget* w, qreal w_pct, qreal h_pct,
                                          QScreen* screen) const {
  if (!w) {
    return;
  }
  if (!screen) {
    screen = ScreenOf(w);
  }
  if (!screen) {
    screen = QApplication::primaryScreen();
  }
  if (!screen) {
    return;
  }

  const QSize avail = screen->availableGeometry().size();
  w->resize(qRound(avail.width() * w_pct), qRound(avail.height() * h_pct));
}

void ScreenAdapter::ApplyFontScaleRecursive(QWidget* w) const {
  if (!w || qFuzzyCompare(font_scale_, 1.0)) {
    return;
  }

  static const char* k_orig_pt_key = "_sa_orig_pt";
  const auto apply_one = [this](QWidget* widget) {
    QFont f = widget->font();
    if (f.pointSize() <= 0) {
      return;
    }

    const QVariant stored = widget->property(k_orig_pt_key);
    const int orig_pt = stored.isValid() ? stored.toInt() : f.pointSize();
    if (!stored.isValid()) {
      widget->setProperty(k_orig_pt_key, orig_pt);
    }

    f.setPointSize(Sfs(orig_pt));
    widget->setFont(f);
  };

  apply_one(w);
  const auto children =
      w->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
  for (QWidget* child : children) {
    ApplyFontScaleRecursive(child);
  }
}

void ScreenAdapter::OnPrimaryScreenChanged(QScreen* screen) {
  Detect(screen);
  DetectDeviceType(screen);

  if (screen) {
    connect(screen, &QScreen::geometryChanged, this,
            &ScreenAdapter::OnScreenGeometryChanged, Qt::UniqueConnection);
  }
  emit ScaleChanged();
}

void ScreenAdapter::OnScreenGeometryChanged(const QRect& /*new_geometry*/) {
  Detect(nullptr);
  emit ScaleChanged();
}

#ifdef SA_ANDROID

namespace {

static constexpr jint k_flag_fullscreen = 0x00000400;
static constexpr jint k_flag_keep_screen_on = 0x00000080;
static constexpr jint k_ui_layout_stable = 0x00000010;
static constexpr jint k_ui_layout_hide_navigation = 0x00000100;
static constexpr jint k_ui_layout_fullscreen = 0x00000200;
static constexpr jint k_ui_hide_navigation = 0x00000002;
static constexpr jint k_ui_fullscreen = 0x00000004;
static constexpr jint k_ui_immersive_sticky = 0x00001000;

static constexpr jint k_immersive_flags =
    k_ui_layout_stable | k_ui_layout_hide_navigation | k_ui_layout_fullscreen |
    k_ui_hide_navigation | k_ui_fullscreen | k_ui_immersive_sticky;

#ifdef SA_QT6

static QJniObject GetActivity() {
  QJniObject activity = QJniObject::callStaticObjectMethod(
      "org/qtproject/qt/android/QtNative", "activity",
      "()Landroid/app/Activity;");
  if (!activity.isValid()) {
    activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt6/android/QtNative", "activity",
        "()Landroid/app/Activity;");
  }
  return activity;
}

static void ApplyImmersiveJNI(jint window_flags, jint ui_flags) {
  QJniObject activity = GetActivity();
  if (!activity.isValid()) {
    qWarning("[ScreenAdapter] Android: cannot get Activity via JNI");
    return;
  }
  QJniObject window =
      activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
  if (!window.isValid()) return;

  window.callMethod<void>("addFlags", "(I)V", window_flags);

  QJniObject decor_view =
      window.callObjectMethod("getDecorView", "()Landroid/view/View;");
  if (decor_view.isValid())
    decor_view.callMethod<void>("setSystemUiVisibility", "(I)V", ui_flags);
}

static void ApplyWindowFlagJNI(const char* method, jint flag) {
  QJniObject activity = GetActivity();
  if (!activity.isValid()) return;
  QJniObject window =
      activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
  if (window.isValid()) window.callMethod<void>(method, "(I)V", flag);
}

#else  // SA_QT5

static void ApplyImmersiveJNI(jint window_flags, jint ui_flags) {
  QtAndroid::runOnAndroidThread([window_flags, ui_flags]() {
    QAndroidJniObject activity = QtAndroid::androidActivity();
    if (!activity.isValid()) return;

    QAndroidJniObject window =
        activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
    if (!window.isValid()) return;

    window.callMethod<void>("addFlags", "(I)V", window_flags);

    QAndroidJniObject decor_view =
        window.callObjectMethod("getDecorView", "()Landroid/view/View;");
    if (decor_view.isValid())
      decor_view.callMethod<void>("setSystemUiVisibility", "(I)V", ui_flags);
  });
}

static void ApplyWindowFlagJNI(const char* method, jint flag) {
  QtAndroid::runOnAndroidThread([method, flag]() {
    QAndroidJniObject window = QtAndroid::androidActivity().callObjectMethod(
        "getWindow", "()Landroid/view/Window;");
    if (window.isValid()) window.callMethod<void>(method, "(I)V", flag);
  });
}

#endif  // SA_QT6

}  // anonymous namespace

void ScreenAdapter::SetFullScreen() {
  ApplyImmersiveJNI(k_flag_fullscreen, k_immersive_flags);
}

void ScreenAdapter::SetKeepScreenOn(bool on) {
  ApplyWindowFlagJNI(on ? "addFlags" : "clearFlags", k_flag_keep_screen_on);
}

#endif  // SA_ANDROID
