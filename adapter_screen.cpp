#include "adapter_screen.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

AdapterScreen::AdapterScreen(QWidget* parent)
    : AdaptiveMixin<QWidget>(parent), ui_(new Ui::AdapterScreenClass()) {
  ui_->setupUi(this);
  InitUi();

  // =========================================================================
  // 窗口尺寸策略（三选一，取消注释使用）
  // =========================================================================
  //
  // --- 策略 B：占屏幕可用区域的百分比，自动居中（推荐，完全自适应）---------
  //
  //   不需要关心 design_width / design_height。
  //   窗口始终是屏幕可用区域的指定比例，内部子控件由 Qt 布局管理器自动伸缩。
  //   无论分辨率是 720p、1080p 还是 4K，都不会显示不全或过小。
  //
  SetScreenPercent(0.60, 0.65);  // 宽占 60%，高占 65%，自动居中

  // --- 策略 A：基于设计坐标系的绝对像素定位（适合迁移已有像素坐标的旧 UI）--
  //
  //   design_width/height 是「你写 SR() / SS() 坐标时假设的画布尺寸」，
  //   不是「目标屏幕分辨率」。运行时屏幕尺寸永远是自动读取的，无需手动配置。
  //
  //   例：你在 1920×1080 的屏幕上写了 label->setGeometry(SR(30, 20, 900, 60))
  //       Init() 默认 design_width=1920，运行到 1280×720 屏幕时自动缩放 0.667x。
  //
  // SetDesignGeometry(480, 240, 960, 600);

  // --- 策略 C：全屏（Android 推荐，桌面可选）---------------------------------
  // ShowAdaptiveFullScreen();

  // --- 可选：对所有子控件字体同步缩放 ----------------------------------------
  // SetAutoFontScale(true);
}

AdapterScreen::~AdapterScreen() { delete ui_; }

void AdapterScreen::InitUi() {
  // =========================================================================
  // 子控件布局方式（两种，对应上面两种窗口策略）
  // =========================================================================
  //
  // --- 方式 1：Qt 布局管理器（与策略 B 配合，完全自适应，推荐）--------------
  //
  //   子控件不写任何 setGeometry()，布局引擎根据窗口大小自动分配空间。
  //   字体用 SFS() 缩放——这是唯一需要 ScreenAdapter 的地方。
  //
  auto* root_layout  = new QVBoxLayout(this);
  auto* btn_layout   = new QHBoxLayout();

  label_title_ = new QLabel(tr("Screen Adaptation Demo"), this);
  label_title_->setAlignment(Qt::AlignCenter);
  // SFS() 让字体随屏幕 DPI 缩放；布局管理器负责位置和大小，无需 setGeometry。
  label_title_->setFont(QFont(QStringLiteral("Arial"), SFS(22), QFont::Bold));

  btn_ok_ = new QPushButton(tr("OK"), this);
  btn_ok_->setFont(QFont(QStringLiteral("Arial"), SFS(14)));
  btn_ok_->setMinimumHeight(SV(36));  // 最小高度随 DPI 缩放，但不固定死

  btn_cancel_ = new QPushButton(tr("Cancel"), this);
  btn_cancel_->setFont(QFont(QStringLiteral("Arial"), SFS(14)));
  btn_cancel_->setMinimumHeight(SV(36));

  btn_layout->addStretch();
  btn_layout->addWidget(btn_ok_);
  btn_layout->addWidget(btn_cancel_);

  root_layout->addWidget(label_title_, 1);   // stretch=1，占剩余空间
  root_layout->addStretch(3);                // 中间空白
  root_layout->addLayout(btn_layout);
  root_layout->setContentsMargins(SV(16), SV(16), SV(16), SV(16));
  root_layout->setSpacing(SV(10));

  setLayout(root_layout);

  // --- 方式 2：绝对坐标定位（与策略 A 配合，迁移旧 UI 时使用）--------------
  //
  //   所有坐标基于 design_width×design_height（默认 1920×1080）写死，
  //   ScreenAdapter 在运行时乘以缩放因子换算到实际屏幕坐标。
  //   缺点：需要同时修改 OnAdaptLayout() 以便在跨屏或 DPI 变化时刷新坐标。
  //
  // label_title_ = new QLabel(tr("Screen Adaptation Demo"), this);
  // label_title_->setAlignment(Qt::AlignCenter);
  // label_title_->setFont(QFont(QStringLiteral("Arial"), SFS(22), QFont::Bold));
  // label_title_->setGeometry(SR(30, 20, 900, 60));
  //
  // btn_ok_ = new QPushButton(tr("OK"), this);
  // btn_ok_->setFont(QFont(QStringLiteral("Arial"), SFS(14)));
  // btn_ok_->setGeometry(SR(680, 500, 120, 44));
  //
  // btn_cancel_ = new QPushButton(tr("Cancel"), this);
  // btn_cancel_->setFont(QFont(QStringLiteral("Arial"), SFS(14)));
  // btn_cancel_->setGeometry(SR(820, 500, 120, 44));

  connect(btn_ok_,     &QPushButton::clicked, this, &AdapterScreen::close);
  connect(btn_cancel_, &QPushButton::clicked, this, &AdapterScreen::close);
}

void AdapterScreen::OnAdaptLayout() {
  // 方式 1（布局管理器）：无需重写此函数——布局引擎自动伸缩子控件。
  // 只需让基类处理窗口自身的 geometry（百分比 / 设计坐标）即可。
  AdaptiveMixin<QWidget>::OnAdaptLayout();

  // 若仅更新字体缩放（布局负责位置），只刷新字体：
  if (label_title_) {
    label_title_->setFont(
        QFont(QStringLiteral("Arial"), SFS(22), QFont::Bold));
  }
  if (btn_ok_)     btn_ok_->setFont(QFont(QStringLiteral("Arial"), SFS(14)));
  if (btn_cancel_) btn_cancel_->setFont(QFont(QStringLiteral("Arial"), SFS(14)));

  // 方式 2（绝对坐标）：取消下面的注释，删除上面的布局代码。
  // if (label_title_) label_title_->setGeometry(SR(30, 20, 900, 60));
  // if (btn_ok_)      btn_ok_->setGeometry(SR(680, 500, 120, 44));
  // if (btn_cancel_)  btn_cancel_->setGeometry(SR(820, 500, 120, 44));
}
