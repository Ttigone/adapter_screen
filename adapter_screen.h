#ifndef ADAPTER_SCREEN_H
#define ADAPTER_SCREEN_H

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

#include "adaptive_mixin.h"
#include "ui_adapter_screen.h"

QT_BEGIN_NAMESPACE
namespace Ui { class AdapterScreenClass; }
QT_END_NAMESPACE

/**
 * @brief AdapterScreen — example main window using the screen-adapter library.
 *
 * Inherits AdaptiveMixin<QWidget>, which provides:
 *   - Automatic re-layout when the window moves to a different monitor.
 *   - Automatic re-layout when the system DPI / resolution changes.
 *   - Android immersive full-screen support.
 *
 * If you prefer not to change the inheritance chain, keep QWidget as the base
 * and call the scaling functions directly in the constructor:
 *   resize(SS(1280, 720));
 *   btn->setGeometry(SR(10, 10, 120, 40));
 */
class AdapterScreen : public AdaptiveMixin<QWidget>
{
    Q_OBJECT

public:
    explicit AdapterScreen(QWidget* parent = nullptr);
    ~AdapterScreen();

protected:
    /**
     * @brief Re-position manually-laid-out child widgets after a scale change.
     *        Not needed when using a Qt layout manager (QVBoxLayout, etc.).
     */
    void OnAdaptLayout() override;

private:
    void InitUi();

    Ui::AdapterScreenClass* ui_;

    QLabel*      label_title_ = nullptr;
    QPushButton* btn_ok_      = nullptr;
    QPushButton* btn_cancel_  = nullptr;
};

#endif  // ADAPTER_SCREEN_H
