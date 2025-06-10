#include "lvgl.h"
#include "lvglDrivers.h"
#include <HardwareTimer.h>

HardwareTimer *MyTim = new HardwareTimer(TIM2);
lv_obj_t *barriereObj = nullptr; // Objet visuel de la barrière
lv_obj_t *loginWindow = nullptr; // Fenêtre login
lv_obj_t *pwdTextarea = nullptr; // Champ mot de passe
static lv_obj_t *keyboard = nullptr; // Clavier global

bool connexionAcpt = false;
// Animation de la barrière (angle en degrés * 10)
void animerBarriere(int angleCible)
{
    if (barriereObj == nullptr) return;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, barriereObj);
    lv_anim_set_values(&a,
                       lv_obj_get_style_transform_angle(barriereObj, 0), // angle actuel
                       angleCible * 10);                                   // angle cible
    lv_anim_set_time(&a, 500);
    lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
        lvglLock();
        lv_obj_set_style_transform_angle(static_cast<lv_obj_t*>(obj), v, 0);
        lvglUnlock();
    });
    lv_anim_start(&a);
}

static void ta_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t * kb = (lv_obj_t *)lv_event_get_user_data(e);

    if(code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }

    if(code == LV_EVENT_DEFOCUSED) {
        lv_keyboard_set_textarea(kb, NULL);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

// Création de la barrière visuelle
void testLvgl()
{
    barriereObj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(barriereObj, 10, 100);
    lv_obj_set_style_bg_color(barriereObj, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(barriereObj, LV_ALIGN_CENTER, 50, -50);
    lv_obj_set_style_transform_pivot_x(barriereObj, 0, 0);
    lv_obj_set_style_transform_pivot_y(barriereObj, 100, 0);
    lv_obj_set_style_transform_angle(barriereObj, 900, 0); // Barrière fermée (90°)
}

// Handler bouton valider de la fenêtre login
static void btnOk_event_handler(lv_event_t * e)
{
    const char *txt = lv_textarea_get_text(pwdTextarea);
    //Ferme le clavier
    if (keyboard) lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    
    if (strcmp(txt, "aa") == 0) // Mot de passe OK
    {
        Serial.println("Mot de passe correct, ouverture barrière");
        // Supprimer la fenêtre login
        lv_obj_del(loginWindow);
        loginWindow = nullptr;
        pwdTextarea = nullptr;
        lv_obj_clear_flag(barriereObj, LV_OBJ_FLAG_HIDDEN); // Afficher
        connexionAcpt = true;
    }
    else
    {
        Serial.println("Mot de passe incorrect");
        // Optionnel : message erreur ou reset champ
        lv_textarea_set_text(pwdTextarea, "");
        connexionAcpt = false;
    }
}

// Création de la fenêtre login
void createLoginWindow()
{
    if (loginWindow != nullptr) return;
    lv_obj_add_flag(barriereObj, LV_OBJ_FLAG_HIDDEN); // Masquer
    loginWindow = lv_win_create(lv_scr_act());
    lv_obj_set_size(loginWindow, 240, 160);
    lv_obj_center(loginWindow);

    lv_obj_t *header = lv_win_get_header(loginWindow);
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Connexion");
    lv_obj_center(title);

    // Champ mot de passe
    pwdTextarea = lv_textarea_create(loginWindow);
    lv_obj_set_size(pwdTextarea, 180, 40);
    lv_obj_align(pwdTextarea, LV_ALIGN_TOP_MID, 0, 30);
    lv_textarea_set_password_mode(pwdTextarea, true);
    lv_textarea_set_placeholder_text(pwdTextarea, "Mot de passe");

    // Clavier numérique (une seule instance globale)
    if (keyboard == nullptr) {
        keyboard = lv_keyboard_create(lv_scr_act());
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN); // Cacher par défaut
        lv_obj_set_size(keyboard, 480, 100); // Taille personnalisée
        lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0); // Aligné en bas
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER); // Mode numérique
    }

    // Associer le callback de focus à la zone texte
    lv_obj_add_event_cb(pwdTextarea, ta_event_cb, LV_EVENT_ALL, keyboard);

    // Bouton Valider
    lv_obj_t *btnOk = lv_btn_create(loginWindow);
    lv_obj_set_size(btnOk, 80, 40);
    lv_obj_align(btnOk, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_t *labelOk = lv_label_create(btnOk);
    lv_label_set_text(labelOk, "Valider");
    lv_obj_center(labelOk);

    lv_obj_add_event_cb(btnOk, btnOk_event_handler, LV_EVENT_CLICKED, nullptr);
}

#ifdef ARDUINO

#include <HardwareSerial.h>
#include <Arduino.h>

#define brochePwmChoisie PinName::PH_6

void mySetup()
{
    pinMode(D4, INPUT);
    pinMode(D5, INPUT);
    Serial.begin(115200);

    // PWM initial (barrière fermée)
    MyTim->setPWM(1, PA_15, 50, 10);

    testLvgl();
}

void loop()
{
    // Vide, car la gestion est dans la tâche FreeRTOS
}

void myTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    int angle = 1000;
    MyTim->setCaptureCompare(1, angle, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);

    while (1)
    {
        int etatCapteurAvantPassage = digitalRead(D4);
        int etatCapteurApresPassage = digitalRead(D5);

        if (etatCapteurAvantPassage == LOW && etatCapteurApresPassage != LOW)
        {
            Serial.println("Détection avant passage, demande login");

            // Affiche fenêtre login si pas déjà visible
            if (!loginWindow)
            {
                lvglLock();
                createLoginWindow();
                lvglUnlock();
            }

            // Attend que la fenêtre login soit fermée (mot de passe validé)
            while (loginWindow != nullptr)
            {
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            if(connexionAcpt){
            // Ouvrir la barrière (PWM + animation)
            int angle = 2000;
            MyTim->setCaptureCompare(1, angle, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
            animerBarriere(0);
            vTaskDelay(100); // Barrière ouverte 2s
            while (digitalRead(D4) == LOW || digitalRead(D5) == LOW)
            vTaskDelay(500);
            // Refermer la barrière
            angle = 1000;
            MyTim->setCaptureCompare(1, angle, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
            animerBarriere(90);
            vTaskDelay(500);
            Serial.println("Barrière refermée");
            connexionAcpt = false;
            }
        }
        else if (etatCapteurApresPassage == LOW && etatCapteurAvantPassage != LOW)
        {
            Serial.println("Passage arrière détecté, pas d'action login");

            // Optionnel : traitement passage arrière (existant dans ton code)
            angle = 2000;
            MyTim->setCaptureCompare(1, angle, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
            animerBarriere(0);
            vTaskDelay(100);
            while (digitalRead(D4) == LOW || digitalRead(D5) == LOW)
                vTaskDelay(100);
            angle = 1000;
            MyTim->setCaptureCompare(1, angle, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
            animerBarriere(90);
            vTaskDelay(500);
        }
        else
        {
            Serial.println("pas obstacle");
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
    }
}

#else

#include "app_hal.h"
#include <cstdio>

int main(void)
{
    printf("LVGL Simulator\n");
    fflush(stdout);

    lv_init();
    hal_setup();

    testLvgl();

    hal_loop();
    return 0;
}

#endif
