#include "lvgl.h"                        // Inclusion de la bibliothèque LVGL pour l'interface graphique
#include "lvglDrivers.h"                 // Inclusion des drivers LVGL spécifiques au matériel
#include <HardwareTimer.h>               // Timer matériel pour la gestion PWM
#include "timer.h"                       // Fichier d'en-tête pour la gestion du timer

HardwareTimer *MyTim = new HardwareTimer(TIM2); // Création d'un objet timer matériel sur TIM2

// Déclaration des objets LVGL globaux
lv_obj_t *barriereObj = nullptr;         // Objet visuel du bras de la barrière
lv_obj_t *barriereContainer = nullptr;   // Conteneur principal de la barrière (bras + socle)
lv_obj_t *loginWindow = nullptr;         // Fenêtre de connexion
lv_obj_t *pwdTextarea = nullptr;         // Champ de saisie du mot de passe
static lv_obj_t *keyboard = nullptr;     // Clavier virtuel global
lv_obj_t *changePwdWindow = nullptr;     // Fenêtre de changement de mot de passe
static lv_obj_t *oldPwdTA = nullptr;     // Champ ancien mot de passe
static lv_obj_t *newPwdTA = nullptr;     // Champ nouveau mot de passe
lv_obj_t *btnChangePwd = nullptr;        // Bouton pour changer le mot de passe

static String currentPassword = "aa";    // Mot de passe par défaut
volatile bool connexionAcpt = false;     // Flag d'acceptation de la connexion
int voitureCount = 0;                    // Compteur de voitures dans le parking

lv_obj_t *voitureLabel = nullptr;        // Label affichant le nombre de voitures
lv_obj_t *heureLabel = nullptr;          // Label affichant l'heure simulée
lv_obj_t *etatLabel = nullptr;           // Label affichant l'état de la barrière
lv_obj_t *horaireLabel = nullptr;        // Label affichant l'état horaire (code requis ou non)

// Met à jour le label de l'heure simulée
void updateSimulatedTimeLabel(time_t fakeTime)
{
    struct tm *heure = localtime(&fakeTime); // Conversion en structure tm
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", heure->tm_hour, heure->tm_min, heure->tm_sec); // Formatage
    lv_lock(); // Protection LVGL
    lv_label_set_text(heureLabel, buf); // Mise à jour du label
    lv_unlock();
}

// Anime la barrière (rotation du bras)
void animerBarriere(int angleCible)
{
    if (barriereObj == nullptr) return; // Sécurité

    lv_anim_t a;
    lv_anim_init(&a); // Initialisation de l'animation
    lv_anim_set_var(&a, barriereObj); // Cible : bras de la barrière
    lv_anim_set_values(&a,
                       lv_obj_get_style_transform_angle(barriereObj, 0), // Angle actuel
                       angleCible * 10);                                // Angle cible (dixième de degré)
    lv_anim_set_time(&a, 500); // Durée de l'animation en ms
    lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
        lv_obj_set_style_transform_angle(static_cast<lv_obj_t*>(obj), v, 0); // Applique l'angle
    });
    lv_anim_start(&a); // Démarre l'animation
}

// Callback pour la gestion du clavier virtuel sur les textareas
static void ta_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e); // Type d'événement LVGL
    lv_obj_t * ta = (lv_obj_t *)lv_event_get_target(e); // Objet cible (textarea)
    lv_obj_t * kb = (lv_obj_t *)lv_event_get_user_data(e); // Clavier associé

    if(code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(kb, ta); // Associe le clavier à la textarea
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN); // Affiche le clavier
        lv_obj_move_foreground(kb); // Met le clavier au premier plan
    }

    if(code == LV_EVENT_DEFOCUSED) {
        lv_keyboard_set_textarea(kb, NULL); // Détache le clavier
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN); // Cache le clavier
    }
}

// Création de la barrière visuelle (socle + bras + rayures)
void testLvgl()
{
    // Taille du conteneur principal
    int largeur = 200;
    int hauteur = 180;

    // Création du conteneur centré
    barriereContainer = lv_obj_create(lv_scr_act());
    lv_obj_set_size(barriereContainer, largeur, hauteur);
    lv_obj_align(barriereContainer, LV_ALIGN_CENTER, 0, 20); // Décalé vers le bas pour ne pas chevaucher l'heure
    lv_obj_clear_flag(barriereContainer, LV_OBJ_FLAG_SCROLLABLE); // Pas de scroll

    // Création du socle (base de la barrière)
    lv_obj_t *socle = lv_obj_create(barriereContainer);
    lv_obj_set_size(socle, 40, 40);
    lv_obj_align(socle, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(socle, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_radius(socle, 8, 0);

    // Création du bras mobile (parent des rayures)
    barriereObj = lv_obj_create(barriereContainer);
    lv_obj_set_size(barriereObj, 12, 120);
    lv_obj_align_to(barriereObj, socle, LV_ALIGN_OUT_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(barriereObj, lv_color_white(), 0); // Fond blanc
    lv_obj_set_style_radius(barriereObj, 4, 0);
    lv_obj_set_style_pad_all(barriereObj, 0, 0);

    // Définition du pivot de rotation à la base du bras
    lv_obj_set_style_transform_pivot_x(barriereObj, 6, 0); // Centre en largeur
    lv_obj_set_style_transform_pivot_y(barriereObj, 120, 0); // Base du bras

    lv_obj_set_style_transform_angle(barriereObj, 900, 0); // Barrière fermée (90°)

    // Ajout des rayures rouges et blanches sur le bras
    int nb_rayures = 8;
    int hauteur_r = 120 / nb_rayures;
    for (int i = 0; i < nb_rayures; ++i) {
        lv_obj_t *rayure = lv_obj_create(barriereObj);
        lv_obj_set_size(rayure, 12, hauteur_r);
        lv_obj_align(rayure, LV_ALIGN_TOP_MID, 0, i * hauteur_r);
        if (i % 2 == 0)
            lv_obj_set_style_bg_color(rayure, lv_palette_main(LV_PALETTE_RED), 0); // Rouge
        else
            lv_obj_set_style_bg_color(rayure, lv_color_white(), 0); // Blanc
        lv_obj_set_style_border_width(rayure, 0, 0);
        lv_obj_set_style_radius(rayure, 0, 0);
        lv_obj_set_style_pad_all(rayure, 0, 0);
    }

    // Création du label compteur de voitures (en haut à droite)
    voitureLabel = lv_label_create(lv_scr_act());
    lv_label_set_text_fmt(voitureLabel, "Voitures: %d", voitureCount);
    lv_obj_align(voitureLabel, LV_ALIGN_TOP_RIGHT, -10, 10);

    // Création du label heure simulée (en haut à gauche)
    heureLabel = lv_label_create(lv_scr_act());
    lv_label_set_text(heureLabel, "00:00:00");
    lv_obj_align(heureLabel, LV_ALIGN_TOP_LEFT, 10, 10);

    // Création du label d'état de la barrière (en bas au centre)
    etatLabel = lv_label_create(lv_scr_act());
    lv_label_set_text(etatLabel, "");
    lv_obj_align(etatLabel, LV_ALIGN_BOTTOM_MID, 0, -10);

    // Création du label horaire automatique sous l'heure simulée
    horaireLabel = lv_label_create(lv_scr_act());
    lv_label_set_text(horaireLabel, "");
    lv_obj_align_to(horaireLabel, heureLabel, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
}

// Handler du bouton "Valider" de la fenêtre login
static void btnOk_event_handler(lv_event_t * e)
{
    if (!pwdTextarea) return; // Sécurité
    const char *txt = lv_textarea_get_text(pwdTextarea); // Récupère le texte saisi

    // Ferme le clavier si ouvert
    if (keyboard) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(keyboard, NULL);
    }

    // Vérifie le mot de passe
    if (strcmp(txt, currentPassword.c_str()) == 0) // Mot de passe OK
    {
        Serial.println("Mot de passe correct, ouverture barrière");
        connexionAcpt = true; // Flag pour la tâche
    }
    else
    {
        Serial.println("Mot de passe incorrect");
        lv_textarea_set_text(pwdTextarea, ""); // Réinitialise le champ
        connexionAcpt = false;
    }
}

// Création de la fenêtre de changement de mot de passe
void createChangePwdWindow()
{
    if (changePwdWindow) return; // Si déjà ouverte, ne rien faire

    // Fenêtre plein écran
    changePwdWindow = lv_win_create(lv_scr_act());
    lv_obj_set_size(changePwdWindow, 480, 272);
    lv_obj_align(changePwdWindow, LV_ALIGN_CENTER, 0, 0);

    // Titre dans l'en-tête
    lv_obj_t *title = lv_label_create(lv_win_get_header(changePwdWindow));
    lv_label_set_text(title, "Changer mot de passe");
    lv_obj_center(title);

    // Champ ancien mot de passe
    oldPwdTA = lv_textarea_create(changePwdWindow);
    lv_obj_set_size(oldPwdTA, 380, 50);
    lv_obj_align(oldPwdTA, LV_ALIGN_TOP_MID, 0, 30);
    lv_textarea_set_password_mode(oldPwdTA, true);
    lv_textarea_set_placeholder_text(oldPwdTA, "Ancien mot de passe");

    // Champ nouveau mot de passe
    newPwdTA = lv_textarea_create(changePwdWindow);
    lv_obj_set_size(newPwdTA, 380, 50);
    lv_obj_align(newPwdTA, LV_ALIGN_TOP_MID, 0, 100);
    lv_textarea_set_password_mode(newPwdTA, true);
    lv_textarea_set_placeholder_text(newPwdTA, "Nouveau mot de passe");

    // Associe le clavier aux deux champs
    lv_obj_add_event_cb(oldPwdTA, ta_event_cb, LV_EVENT_ALL, keyboard);
    lv_obj_add_event_cb(newPwdTA, ta_event_cb, LV_EVENT_ALL, keyboard);

    // Bouton valider
    lv_obj_t *btnSave = lv_btn_create(changePwdWindow);
    lv_obj_set_size(btnSave, 150, 50);
    lv_obj_align(btnSave, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_t *labelSave = lv_label_create(btnSave);
    lv_label_set_text(labelSave, "Valider");
    lv_obj_center(labelSave);

    // Callback du bouton valider
    lv_obj_add_event_cb(btnSave, [](lv_event_t *e) {
        const char *oldPwd = lv_textarea_get_text(oldPwdTA);
        const char *newPwd = lv_textarea_get_text(newPwdTA);

        // Vérifie l'ancien mot de passe et que le nouveau n'est pas vide
        if (strcmp(oldPwd, currentPassword.c_str()) == 0 && strlen(newPwd) > 0) {
            currentPassword = String(newPwd); // Met à jour le mot de passe
            Serial.println("Mot de passe modifie avec succès");
            lv_obj_del(changePwdWindow); // Ferme la fenêtre
            changePwdWindow = nullptr;
            lv_obj_clear_flag(btnChangePwd, LV_OBJ_FLAG_HIDDEN); // Réaffiche le bouton de changement
        } else {
            Serial.println("Ancien mot de passe incorrect ou nouveau vide");
        }
    }, LV_EVENT_CLICKED, nullptr);
}

// Création de la fenêtre login
void createLoginWindow()
{
    if (loginWindow != nullptr) {
        // Si déjà créée, on la réaffiche et on reset le champ
        lv_obj_clear_flag(loginWindow, LV_OBJ_FLAG_HIDDEN);
        if (pwdTextarea) lv_textarea_set_text(pwdTextarea, "");
        if (keyboard) {
            lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
            lv_keyboard_set_textarea(keyboard, NULL);
        }
        return;
    }
    lv_obj_add_flag(barriereContainer, LV_OBJ_FLAG_HIDDEN); // Masque la barrière
    loginWindow = lv_win_create(lv_scr_act());
    lv_obj_set_size(loginWindow, 480, 272);
    lv_obj_align(loginWindow, LV_ALIGN_CENTER, 0, 0);

    // Titre
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

    // Clavier numérique (créé une seule fois)
    if (keyboard == nullptr) {
        keyboard = lv_keyboard_create(lv_scr_act());
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN); // Cacher par défaut
        lv_obj_set_size(keyboard, 480, 100); // Taille personnalisée
        lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0); // Aligné en bas
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER); // Mode texte
    }

    // Associe le callback de focus à la zone texte
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
        lv_obj_add_flag(btnChangePwd, LV_OBJ_FLAG_HIDDEN); // Cache le bouton
        createChangePwdWindow(); // Ouvre la fenêtre de changement
    }, LV_EVENT_CLICKED, nullptr);
}

#ifdef ARDUINO

#include <HardwareSerial.h>
#include <Arduino.h>

#define brochePwmChoisie PinName::PH_6 // Définition de la broche PWM

// Fonction d'initialisation
void mySetup()
{
    pinMode(D4, INPUT); // Capteur entrée
    pinMode(D5, INPUT); // Capteur sortie
    Serial.begin(115200); // Initialisation du port série

    // PWM initial (barrière fermée)
    MyTim->setPWM(1, PA_15, 50, 10);

    testLvgl(); // Création de l'interface graphique
}

// Boucle principale Arduino (vide, tout est géré dans la tâche)
void loop()
{
    // Vide, car la gestion est dans la tâche FreeRTOS
}

// Tâche principale FreeRTOS
void myTask(void *pvParameters)
{
    static time_t fakeTime = 16 * 3600 + 59 * 60; // Heure simulée de départ : 16:59:00
    int prevEtatAvant = HIGH; // État précédent capteur entrée
    int prevEtatApres = HIGH; // État précédent capteur sortie
    bool enAttenteConnexion = false; // Flag attente login
    bool connexionAcceptee = false;  // Flag connexion acceptée

    while (1)
    {
        // Mise à jour heure simulée toutes les secondes
        static TickType_t dernierUpdateHeure = 0;
        TickType_t now = xTaskGetTickCount();
        if (now - dernierUpdateHeure >= pdMS_TO_TICKS(1000))
        {
            dernierUpdateHeure = now;
            fakeTime++;
            updateSimulatedTimeLabel(fakeTime);

            // Mise à jour du label horaire à chaque seconde
            if (localtime(&fakeTime)->tm_hour == 17) {
                lv_lock();
                lv_label_set_text(horaireLabel, "Entree automatique : pas de code requis");
                lv_unlock();
            } else {
                lv_lock();
                lv_label_set_text(horaireLabel, "Entree avec mot de passe requis");
                lv_unlock();
            }
        }

        int etatAvant = digitalRead(D4); // Lecture capteur entrée
        int etatApres = digitalRead(D5); // Lecture capteur sortie

        if (!enAttenteConnexion)
        {
            // Détection entrée voiture (front descendant sur D4)
            if (etatAvant == LOW && prevEtatAvant == HIGH && etatApres != LOW)
            {
                Serial.println("Détection entrée");

                if (voitureCount >= 3)
                {
                    Serial.println("Parking plein !");
                    lv_lock();
                    lv_label_set_text(voitureLabel, "Plus de place !");
                    lv_unlock();
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                else if (localtime(&fakeTime)->tm_hour == 17) // entrée automatique à 17h
                {
                    voitureCount++;
                    if (voitureCount > 3) voitureCount = 3;

                    lv_lock();
                    lv_label_set_text_fmt(voitureLabel, "Voitures: %d", voitureCount);
                    lv_obj_clear_flag(barriereContainer, LV_OBJ_FLAG_HIDDEN);
                    lv_unlock();

                    // Ouvre la barrière
                    lv_lock();
                    lv_obj_clear_flag(etatLabel, LV_OBJ_FLAG_HIDDEN); // Affiche le label d'état
                    lv_label_set_text(etatLabel, "Barriere ouverte");
                    lv_unlock();
                    MyTim->setCaptureCompare(1, 1100, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
                    animerBarriere(0);
                    vTaskDelay(pdMS_TO_TICKS(900));

                    // Attente passage voiture
                    while (digitalRead(D4) == LOW || digitalRead(D5) == LOW) {
                        fakeTime++;
                        updateSimulatedTimeLabel(fakeTime);
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }

                    // Ferme la barrière
                    lv_lock();
                    lv_obj_clear_flag(etatLabel, LV_OBJ_FLAG_HIDDEN); // Affiche le label d'état
                    lv_label_set_text(etatLabel, "Barriere fermee");
                    lv_unlock();
                    MyTim->setCaptureCompare(1, 2000, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
                    animerBarriere(90);
                    vTaskDelay(pdMS_TO_TICKS(900));
                    Serial.println("Entree automatique (17h)");
                }
                else
                {
                    // Crée la fenêtre login
                    lv_lock();
                    createLoginWindow(); // Affiche la fenêtre login
                    lv_obj_add_flag(barriereContainer, LV_OBJ_FLAG_HIDDEN); // Cache la barrière
                    lv_obj_add_flag(etatLabel, LV_OBJ_FLAG_HIDDEN); // Cache le label d'état
                    lv_unlock();
                    enAttenteConnexion = true;
                    connexionAcceptee = false;
                }
            }
            // Détection sortie voiture (front descendant sur D5)
            else if (etatApres == LOW && prevEtatApres == HIGH && etatAvant != LOW)
            {
                Serial.println("Détection sortie");

                if (voitureCount > 0) voitureCount--;

                lv_lock();
                lv_label_set_text_fmt(voitureLabel, "Voitures: %d", voitureCount);
                lv_unlock();

                // Ouvre la barrière
                MyTim->setCaptureCompare(1, 1100, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
                lv_lock();
                lv_obj_clear_flag(etatLabel, LV_OBJ_FLAG_HIDDEN); // Affiche le label d'état
                lv_label_set_text(etatLabel, "Barriere ouverte");
                lv_unlock();
                animerBarriere(0);
                vTaskDelay(pdMS_TO_TICKS(900));

                // Attente passage voiture
                while (digitalRead(D4) == LOW || digitalRead(D5) == LOW) {
                    fakeTime++;
                    updateSimulatedTimeLabel(fakeTime);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }

                // Ferme la barrière
                MyTim->setCaptureCompare(1, 2000, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
                lv_lock();
                lv_obj_clear_flag(etatLabel, LV_OBJ_FLAG_HIDDEN); // Affiche le label d'état
                lv_label_set_text(etatLabel, "Barriere fermee");
                lv_unlock();
                animerBarriere(90);
                vTaskDelay(pdMS_TO_TICKS(900));
                Serial.println("Sortie terminée");
            }
        }
        else
        {
            // En attente connexion dans fenêtre login
            if (connexionAcpt)
            {
                connexionAcceptee = true;
                connexionAcpt = false;
            }
            else if (digitalRead(D4) == HIGH)
            {
                // Annule la demande si plus de détection
                connexionAcceptee = false;
            }

            if (connexionAcceptee || digitalRead(D4) == HIGH)
            {
                Serial.println("On va cacher la fenêtre login !");
                lv_lock();
                if (keyboard) {
                    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
                    lv_keyboard_set_textarea(keyboard, NULL);
                }
                if (loginWindow) {
                    lv_obj_add_flag(loginWindow, LV_OBJ_FLAG_HIDDEN);
                }
                lv_obj_clear_flag(barriereContainer, LV_OBJ_FLAG_HIDDEN); // Réaffiche la barrière
                lv_unlock();

                enAttenteConnexion = false;

                if (connexionAcceptee)
                {
                    voitureCount++;
                    if (voitureCount > 3) voitureCount = 3;

                    lv_lock();
                    lv_label_set_text_fmt(voitureLabel, "Voitures: %d", voitureCount);
                    lv_unlock();

                    // Ouvre la barrière
                    lv_lock();
                    lv_obj_clear_flag(etatLabel, LV_OBJ_FLAG_HIDDEN); // Affiche le label d'état
                    lv_label_set_text(etatLabel, "Barriere ouverte");
                    lv_unlock();
                    MyTim->setCaptureCompare(1, 1100, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
                    animerBarriere(0);
                    vTaskDelay(pdMS_TO_TICKS(900));

                    // Attente passage voiture
                    while (digitalRead(D4) == LOW || digitalRead(D5) == LOW) {
                        fakeTime++;
                        updateSimulatedTimeLabel(fakeTime);
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }

                    lv_lock();
                    lv_obj_clear_flag(etatLabel, LV_OBJ_FLAG_HIDDEN); // Affiche le label d'état
                    lv_label_set_text(etatLabel, "Barriere fermee");
                    lv_unlock();

                    // Ferme la barrière
                    MyTim->setCaptureCompare(1, 2000, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
                    animerBarriere(90);
                    vTaskDelay(pdMS_TO_TICKS(900));
                    Serial.println("Entrée terminée");
                }
            }
        }

        prevEtatAvant = etatAvant; // Mémorise l'état précédent entrée
        prevEtatApres = etatApres; // Mémorise l'état précédent sortie

        vTaskDelay(pdMS_TO_TICKS(10)); // Petite pause pour la boucle
    }
}




#else

#include "app_hal.h"
#include <cstdio>

// Point d'entrée pour la version simulateur
int main(void)
{
    printf("LVGL Simulator\n");
    fflush(stdout);

    lv_init();      // Initialisation LVGL
    hal_setup();    // Initialisation HAL

    testLvgl();     // Création de l'interface graphique

    hal_loop();     // Boucle principale simulateur
    return 0;
}

#endif