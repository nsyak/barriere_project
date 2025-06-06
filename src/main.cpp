#include "lvgl.h"
#include <HardwareTimer.h>

HardwareTimer *MyTim = new HardwareTimer(TIM2);
lv_obj_t *barriereObj = nullptr; // Objet visuel de la barrière

#define CAPTEUR_TOR1_PIN PB_4

static void event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        LV_LOG_USER("Clicked");
    }
    else if(code == LV_EVENT_VALUE_CHANGED) {
        LV_LOG_USER("Toggled");
    }
}

void testLvgl()
{
  // Création visuelle de la barrière
barriereObj = lv_obj_create(lv_screen_active());
lv_obj_set_size(barriereObj, 10, 100); // Petite barre verticale
lv_obj_set_style_bg_color(barriereObj, lv_palette_main(LV_PALETTE_RED), 0);
lv_obj_align(barriereObj, LV_ALIGN_CENTER, 50, -50); // Position sur l'écran
lv_obj_set_style_transform_pivot_x(barriereObj, 0, 0);
lv_obj_set_style_transform_pivot_y(barriereObj, 100, 0); // pivot en bas pour simuler une barrière
lv_obj_set_style_transform_angle(barriereObj, -900, 0); // 90° = horizontal
}

void animerBarriere(int angleCible)
{
    if (barriereObj == nullptr) return;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, barriereObj);
    lv_anim_set_values(&a,
                       lv_obj_get_style_transform_angle(barriereObj, 0),  // angle actuel
                       angleCible * 10);                                  // angle cible en dixièmes de degré
    lv_anim_set_time(&a, 500); // durée en ms
    lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
        lv_obj_set_style_transform_angle(static_cast<lv_obj_t*>(obj), v, 0);
    });

    lv_anim_start(&a);
}

#ifdef ARDUINO

#include "lvglDrivers.h"
#include <HardwareSerial.h>
#include <Arduino.h>
// à décommenter pour tester la démo
// #include "demos/lv_demos.h"

#define brochePwmChoisie PinName::PH_6                     // On choisit d’émettre sur la broche D6

void mySetup()
{
  // à décommenter pour tester la démo
  // lv_demo_widgets();
  // Initialisations générales
  // Génération du signal PWM
  pinMode(D4, INPUT);
  pinMode(D5, INPUT);
  Serial.begin(115200);
  MyTim->setPWM(1, PA_15, 50, 10);
  testLvgl();
}
void loop()
{
  // Inactif (pour mise en veille du processeur)
}

void myTask(void *pvParameters)
{
  // Init
  TickType_t xLastWakeTime;
  // Lecture du nombre de ticks quand la tâche débute
  xLastWakeTime = xTaskGetTickCount();
  int angle = 1000;
  MyTim->setCaptureCompare(1, angle, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
  while (1)
  {
    // Loop
    // Endort la tâche pendant le temps restant par rapport au réveil,
    // ici 200ms, donc la tâche s'effectue toutes les 200ms
     int etatCapteurAvantPassage = digitalRead(D4);
     int etatCapteurApresPassage = digitalRead(D5);
     if(etatCapteurAvantPassage == LOW && etatCapteurApresPassage != LOW) {
      Serial.println("La barrière s'ouvre de l'avant");
      angle = 2000;
      MyTim->setCaptureCompare(1, angle, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
      animerBarriere(-180);
      vTaskDelay(100);
      while (digitalRead(D4) == LOW || digitalRead(D5) == LOW) vTaskDelay(100);
      angle = 1000;
      MyTim->setCaptureCompare(1, angle, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
      animerBarriere(-90);
      vTaskDelay(500);
      }

     else if(etatCapteurApresPassage == LOW && etatCapteurAvantPassage != LOW) {
      Serial.println("La barrière s'ouvre de l'arrière");
      angle = 2000;
      MyTim->setCaptureCompare(1, angle, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
      animerBarriere(-180);
      vTaskDelay(100);
      while (digitalRead(D4) == LOW || digitalRead(D5) == LOW) vTaskDelay(100);   
      angle = 1000;
      MyTim->setCaptureCompare(1, angle, TimerCompareFormat_t::MICROSEC_COMPARE_FORMAT);
      animerBarriere(-90);
      vTaskDelay(500);
      }
      else Serial.println("pas obstacle");
      vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
  }
}

#else

#include "lvgl.h"
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
