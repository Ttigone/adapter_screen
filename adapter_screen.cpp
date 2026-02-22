#include "adapter_screen.h"

AdapterScreen::AdapterScreen(QWidget* parent)
    : AdaptiveMixin<QWidget>(parent)
    , ui_(new Ui::AdapterScreenClass())
{
    ui_->setupUi(this);
    InitUi();

    // ── Strategy A: fixed design-space geometry (auto-scaled) ──────────────
    // The design canvas is 1920x1080.  This window is centred at 960x600
    // in that coordinate space.
    SetDesignGeometry(480, 240, 960, 600);

    // ── Strategy B (alternative): percentage of screen, auto-centred ───────
    // SetScreenPercent(0.80, 0.70);

    // ── Strategy C (Android full-screen) ───────────────────────────────────
    // ShowAdaptiveFullScreen();  // or call from main.cpp before show()

    // ── Optional: scale all child-widget fonts automatically ───────────────
    // SetAutoFontScale(true);
}

AdapterScreen::~AdapterScreen()
{
    delete ui_;
}

void AdapterScreen::InitUi()
{
    // Example: create child widgets manually and position them with the
    // SR() / SS() / SFS() convenience functions.
    // All coordinates refer to the 1920x1080 design canvas; they are
    // multiplied by the current scale factor at run-time.

    label_title_ = new QLabel(tr("Screen Adaptation Demo"), this);
    label_title_->setAlignment(Qt::AlignCenter);
    label_title_->setFont(QFont(QStringLiteral("Arial"), SFS(22), QFont::Bold));
    label_title_->setGeometry(SR(30, 20, 900, 60));

    btn_ok_ = new QPushButton(tr("OK"), this);
    btn_ok_->setFont(QFont(QStringLiteral("Arial"), SFS(14)));
    btn_ok_->setGeometry(SR(680, 500, 120, 44));

    btn_cancel_ = new QPushButton(tr("Cancel"), this);
    btn_cancel_->setFont(QFont(QStringLiteral("Arial"), SFS(14)));
    btn_cancel_->setGeometry(SR(820, 500, 120, 44));

    connect(btn_ok_,     &QPushButton::clicked, this, &AdapterScreen::close);
    connect(btn_cancel_, &QPushButton::clicked, this, &AdapterScreen::close);
}

void AdapterScreen::OnAdaptLayout()
{
    // Step 1: let the base class handle the window's own geometry.
    AdaptiveMixin<QWidget>::OnAdaptLayout();

    // Step 2: re-apply positions for manually-placed children.
    //   If you use a Qt layout manager instead (e.g. QVBoxLayout), delete
    //   everything below — the layout engine handles children automatically.
    if (label_title_) {
        label_title_->setGeometry(SR(30, 20, 900, 60));
        label_title_->setFont(QFont(QStringLiteral("Arial"), SFS(22), QFont::Bold));
    }
    if (btn_ok_) {
        btn_ok_->setGeometry(SR(680, 500, 120, 44));
        btn_ok_->setFont(QFont(QStringLiteral("Arial"), SFS(14)));
    }
    if (btn_cancel_) {
        btn_cancel_->setGeometry(SR(820, 500, 120, 44));
        btn_cancel_->setFont(QFont(QStringLiteral("Arial"), SFS(14)));
    }
}
