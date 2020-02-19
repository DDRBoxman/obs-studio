#pragma once

#include <QDialog>

#include "ui_captions.h"

class DecklinkCaptionsUI : public QDialog {
        Q_OBJECT
private:

public:
    std::unique_ptr<Ui_CaptionsDialog> ui;
    DecklinkCaptionsUI(QWidget *parent);
};