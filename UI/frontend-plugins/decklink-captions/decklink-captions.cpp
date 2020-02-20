#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QAction>
#include "DecklinkCaptionsUI.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("decklink-captons", "en-US")

static void connect()
{
    obs_output *output = obs_frontend_get_streaming_output();
    if (output) {
        //obs_source_add_caption_callback();

        //obs_output_output_caption_text1(output, text.c_str());
        obs_output_release(output);
    }
}

static void save_decklink_caption_data(obs_data_t *save_data, bool saving, void *)
{
    if (saving) {
        obs_data_t *obj = obs_data_create();

        obs_data_set_string(obj, "source",
                            captions->source_name.c_str());

        obs_data_set_obj(save_data, "decklink_captions", obj);
        obs_data_release(obj);
    } else {
        captions->stop();

        obs_data_t *obj = obs_data_get_obj(save_data, "decklink_captions");
        if (!obj)
            obj = obs_data_create();

        captions->source_name = obs_data_get_string(obj, "source");
        captions->source =
                GetWeakSourceByName(captions->source_name.c_str());
        obs_data_release(obj);

        if (enabled)
            captions->start();
    }
}

void addOutputUI(void)
{
    QAction *action = (QAction *)obs_frontend_add_tools_menu_qaction(
            obs_module_text("Decklink Captions"));

    auto cb = []() {
        obs_frontend_push_ui_translation(obs_module_get_string);

        QWidget *window = (QWidget *)obs_frontend_get_main_window();

        DecklinkCaptionsUI dialog(window);
        dialog.exec();

        obs_frontend_pop_ui_translation();
    };

    obs_frontend_add_save_callback(save_decklink_caption_data, nullptr);

    action->connect(action, &QAction::triggered, cb);
}

bool obs_module_load(void)
{
    addOutputUI();

    return true;
}
