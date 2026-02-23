#ifndef ADAPTER_SCREEN_H
#define ADAPTER_SCREEN_H

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

#include "adaptive_mixin.h"
#include "ui_adapter_screen.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class AdapterScreenClass;
}
QT_END_NAMESPACE

class AdapterScreen : public AdaptiveMixin<QWidget> {
  Q_OBJECT

 public:
  explicit AdapterScreen(QWidget* parent = nullptr);
  ~AdapterScreen();

 protected:
  void OnAdaptLayout() override;

 private:
  void InitUi();

  Ui::AdapterScreenClass* ui_;

  QLabel* label_title_ = nullptr;
  QPushButton* btn_ok_ = nullptr;
  QPushButton* btn_cancel_ = nullptr;
};

#endif  // ADAPTER_SCREEN_H
