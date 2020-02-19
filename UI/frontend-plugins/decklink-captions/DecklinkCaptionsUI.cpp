#import "DecklinkCaptionsUI.h"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/dstr.hpp>
#include <util/platform.h>

DecklinkCaptionsUI::DecklinkCaptionsUI(QWidget *parent)
        : QDialog(parent), ui(new Ui_CaptionsDialog)
{
    ui->setupUi(this);

    setSizeGripEnabled(true);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto cb = [this](obs_source_t *source) {
        uint32_t caps = obs_source_get_output_flags(source);
        QString name = obs_source_get_name(source);

        if (caps & OBS_SOURCE_CEA_708)
            ui->source->addItem(name);
        return true;
    };

    using cb_t = decltype(cb);

    ui->source->blockSignals(true);
    ui->source->addItem(QStringLiteral(""));
    ui->source->setCurrentIndex(0);
    obs_enum_sources(
            [](void *data, obs_source_t *source) {
                return (*static_cast<cb_t *>(data))(source);
            },
            &cb);
    ui->source->blockSignals(false);
}