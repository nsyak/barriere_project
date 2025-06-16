#include "lvgl.h"
#include "lvglDrivers.h"
#include <HardwareTimer.h>

HardwareTimer *MyTim = new HardwareTimer(TIM2);
lv_obj_t *barriereObj = nullptr; // Objet visuel de la barrière
lv_obj_t *loginWindow = nullptr; // Fenêtre login
lv_obj_t *pwdTextarea = nullptr; // Champ mot de passe
static lv_obj_t *keyboard = nullptr; // Clavier global
lv_obj_t *changePwdWindow = nullptr;
static lv_obj_t *oldPwdTA = nullptr;
static lv_obj_t *newPwdTA = nullptr;
lv_obj_t *btnChangePwd = nullptr;
static String currentPassword = "aa"; // Mot de passe par défaut
volatile bool connexionAcpt = false;
int voitureCount = 0;
lv_obj_t *voitureLabel = nullptr;


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
        lv_obj_move_foreground(kb);
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
    // Label compteur de voitures
    voitureLabel = lv_label_create(lv_scr_act());
    lv_label_set_text_fmt(voitureLabel, "Voitures: %d", voitureCount);
    lv_obj_align(voitureLabel, LV_ALIGN_TOP_RIGHT, -10, 10);  // En haut à droite
}

// Handler bouton valider de la fenêtre login
static void btnOk_event_handler(lv_event_t * e)
{
    const char *txt = lv_textarea_get_text(pwdTextarea);
    //Ferme le clavier
    if (keyboard) lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    
    if (strcmp(txt, currentPassword.c_str()) == 0) // Mot de passe OK
    {
        Serial.println("Mot de passe correct, ouverture barrière");
        // Supprimer la fenêtre login
        // lv_obj_del(loginWindow);
        // loginWindow = nullptr;
        // pwdTextarea = nullptr;
        // lv_obj_clear_flag(barriereObj, LV_OBJ_FLAG_HIDDEN); // Afficher
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

void createChangePwdWindow()
{
    if (changePwdWindow) return;

    // Fenêtre plein écran
    changePwdWindow = lv_win_create(lv_scr_act());
    lv_obj_set_size(changePwdWindow, 480, 272);
    lv_obj_align(changePwdWindow, LV_ALIGN_CENTER, 0, 0);

    // Titre dans l'en-tête
    lv_obj_t *title = lv_label_create(lv_win_get_header(changePwdWindow));
    lv_label_set_text(title, "Changer mot de passe");
    lv_obj_center(title);

    // Ancien mot de passe
    oldPwdTA = lv_textarea_create(changePwdWindow);
    lv_obj_set_size(oldPwdTA, 380, 50);
    lv_obj_align(oldPwdTA, LV_ALIGN_TOP_MID, 0, 30);
    lv_textarea_set_password_mode(oldPwdTA, true);
    lv_textarea_set_placeholder_text(oldPwdTA, "Ancien mot de passe");

    // Nouveau mot de passe
    newPwdTA = lv_textarea_create(changePwdWindow);
    lv_obj_set_size(newPwdTA, 380, 50);
    lv_obj_align(newPwdTA, LV_ALIGN_TOP_MID, 0, 100);
    lv_textarea_set_password_mode(newPwdTA, true);
    lv_textarea_set_placeholder_text(newPwdTA, "Nouveau mot de passe");

    // Associer le clavier
    lv_obj_add_event_cb(oldPwdTA, ta_event_cb, LV_EVENT_ALL, keyboard);
    lv_obj_add_event_cb(newPwdTA, ta_event_cb, LV_EVENT_ALL, keyboard);

    // Bouton valider
    lv_obj_t *btnSave = lv_btn_create(changePwdWindow);
    lv_obj_set_size(btnSave, 150, 50);
    lv_obj_align(btnSave, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_t *labelSave = lv_label_create(btnSave);
    lv_label_set_text(labelSave, "Valider");
    lv_obj_center(labelSave);

    // Callback bouton
    lv_obj_add_event_cb(btnSave, [](lv_event_t *e) {
        const char *oldPwd = lv_textarea_get_text(oldPwdTA);
        const char *newPwd = lv_textarea_get_text(newPwdTA);

        if (strcmp(oldPwd, currentPassword.c_str()) == 0 && strlen(newPwd) > 0) {
            currentPassword = String(newPwd);
            Serial.println("Mot de passe modifié avec succès");
            lv_obj_del(changePwdWindow);
            changePwdWindow = nullptr;
        } else {
            Serial.println("Ancien mot de passe incorrect ou nouveau vide");
        }
    }, LV_EVENT_CLICKED, nullptr);
}


// Création de la fenêtre login
void createLoginWindow()
{
    if (loginWindow != nullptr) return;
    lv_obj_add_flag(barriereObj, LV_OBJ_FLAG_HIDDEN); // Masquer
    loginWindow = lv_win_create(lv_scr_act());
    lv_obj_set_size(loginWindow, 480, 272);
    lv_obj_align(loginWindow, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *header = lv_win_get_header(loginWindow);
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Connexion");
    lv_obj_center(title);

    // Champ mot de passe
    pwdTextarea = lv_textarea_create(loginWindow);
    lv_obj_set_size(pwdTextarea, 380, 50);
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
    lv_obj_set_size(btnOk, 150, 50);
    lv_obj_align(btnOk, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_t *labelOk = lv_label_create(btnOk);
    lv_label_set_text(labelOk, "Valider");
    lv_obj_center(labelOk);

    lv_obj_add_event_cb(btnOk, btnOk_event_handler, LV_EVENT_CLICKED, nullptr);

    // Bouton changement mot de passe
    btnChangePwd = lv_btn_create(loginWindow);
    lv_obj_set_size(btnChangePwd, 200, 50);
    lv_obj_align(btnChangePwd, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_t *labelChange = lv_label_create(btnChangePwd);
    lv_label_set_text(labelChange, "Changer de mot de passe");
    lv_obj_center(labelChange);
    lv_obj_add_event_cb(btnChangePwd, [](lv_event_t *e) {
    lv_obj_add_flag(btnChangePwd, LV_OBJ_FLAG_HIDDEN); // <<== On cache le bouton
    createChangePwdWindow();
    }, LV_EVENT_CLICKED, nullptr);

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
    int angle = 2000;
    MyTim->setCaptureCompare(1, angle, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);

    int prevEtatAvant = digitalRead(D4);
    int prevEtatApres = digitalRead(D5);
    bool enAttentePassage = false;// variable qui assure que la page de connexion ne re apparait pas tant que la voiture n'est pas encore passé

    while (1)
    {
        int etatAvant = digitalRead(D4);
        int etatApres = digitalRead(D5);
        bool connexionAcceptee = false;

        // === CAS 1 : Détection entrée ===
        if (etatAvant == LOW && etatApres != LOW && !enAttentePassage)
        {
            enAttentePassage = true;
            Serial.println("Détection entrée");

            if (voitureCount >= 3) {
                Serial.println("Parking plein !");
                lvglLock();
                lv_label_set_text(voitureLabel, "Plus de place !");
                lvglUnlock();
                vTaskDelay(2000);
                continue;
            }

            if (!loginWindow) {
                vTaskDelay(200);
                lvglLock();
                createLoginWindow(); 
                lvglUnlock();
                vTaskDelay(200);
            }

            // Attente login accepté ou timeout
            while (loginWindow != nullptr) {
                // Timeout
                if (digitalRead(D4) == HIGH) {
                    Serial.println("Annulation demande de connexion");
                    lvglLock();
                    lv_obj_del(loginWindow);
                    loginWindow = nullptr;
                    pwdTextarea = nullptr;
                    lv_obj_clear_flag(barriereObj, LV_OBJ_FLAG_HIDDEN);
                    lvglUnlock();
                    vTaskDelay(500);
                    enAttentePassage = false; // Libère la détection future
                    break;
                }

                // Si l'utilisateur a été validé
                if (connexionAcpt) {
                    connexionAcceptee = true;
                    connexionAcpt = false; // réinitialisation
                    lvglLock();
                    lv_obj_del(loginWindow);
                    loginWindow = nullptr;
                    pwdTextarea = nullptr;
                    lv_obj_clear_flag(barriereObj, LV_OBJ_FLAG_HIDDEN);
                    lvglUnlock();
                    break;
                }

                vTaskDelay(pdMS_TO_TICKS(100));
            }

            // Si login réussi, ouverture barrière
            if (connexionAcceptee) {
                voitureCount++;
                if (voitureCount > 10) voitureCount = 10;

                lvglLock();
                lv_label_set_text_fmt(voitureLabel, "Voitures: %d", voitureCount);
                lvglUnlock();

                angle = 1100;
                MyTim->setCaptureCompare(1, angle, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
                animerBarriere(0);
                vTaskDelay(pdMS_TO_TICKS(900));

                while (digitalRead(D4) == LOW || digitalRead(D5) == LOW)
                    vTaskDelay(pdMS_TO_TICKS(1000));

                angle = 2000;
                MyTim->setCaptureCompare(1, angle, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
                animerBarriere(90);
                vTaskDelay(pdMS_TO_TICKS(900));
                Serial.println("Entrée terminée");
                enAttentePassage = false; // Libère la détection future
            }
        }

        // === CAS 2 : Détection sortie ===
        else if (etatApres == LOW && etatAvant != LOW)
        {
            Serial.println("Détection sortie");

            if (voitureCount > 0) voitureCount--;

            lvglLock();
            lv_label_set_text_fmt(voitureLabel, "Voitures: %d", voitureCount);
            lvglUnlock();

            angle = 1100;
            MyTim->setCaptureCompare(1, angle, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
            animerBarriere(0);
            vTaskDelay(pdMS_TO_TICKS(900));

            while (digitalRead(D4) == LOW || digitalRead(D5) == LOW)
                vTaskDelay(pdMS_TO_TICKS(1000));

            angle = 2000;
            MyTim->setCaptureCompare(1, angle, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
            animerBarriere(90);
            vTaskDelay(pdMS_TO_TICKS(900));
            Serial.println("Sortie terminée");
        }

        // Mise à jour états précédents
        prevEtatAvant = etatAvant;
        prevEtatApres = etatApres;

        vTaskDelay(pdMS_TO_TICKS(200));
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
