/*=====================================================================
 * Open Vega+ Emulator. Handheld ESP32 based hardware with
 * ZX Spectrum emulation
 *
 * (C) 2019 Alvaro Alea Fernandez
 *
 * This program is free software; redistributable under the terms of
 * the GNU General Public License Version 2
 *
 * Based on the Aspectrum emulator Copyright (c) 2000 Santiago Romero Iglesias
 * Use Adafruit's IL9341 and GFX libraries.
 * Compile as ESP32 Wrover Module
 *======================================================================
 */
#include <WiFi.h>
#include <TFT_eSPI.h>
#include "SPIFFS.h"
#include "SPI.h"
#include "gui.h"
#include "driver/timer.h"
#include "AudioOutput/I2SOutput.h"
#include "z80/hardware.h"
#include "z80/snaps.h"
#include "z80/spectrum.h"
#include "z80/z80.h"

TFT_eSPI tft = TFT_eSPI();
AudioOutput *audioOutput = NULL;

/* Definimos unos objetos
 *  spectrumZ80 es el procesador en si
 *  mem es la memoria ROM/RAM del ordenador
 *  hwopt es la configuracion de la maquina a emular esto se guarda en los snapshots
 *  emuopt es la configuracion del emulador en si, no se guarda con el snapshot
 */
Z80Regs spectrumZ80;
tipo_mem mem;
tipo_hwopt hwopt;
tipo_emuopt emuopt;
uint16_t *frameBuffer = NULL;

xQueueHandle cpuRunTimerQueue;
xQueueHandle frameRenderTimerQueue;

const int screenWidth = TFT_HEIGHT;
const int screenHeight = TFT_WIDTH;
const int borderWidth = (screenWidth - 256) / 2;
const int borderHeight = (screenHeight - 192) / 2;

const int specpal565[16] = {
    0x0000, 0x1B00, 0x00B8, 0x17B8, 0xE005, 0xF705, 0xE0BD, 0x18C6, 0x0000, 0x1F00, 0x00F8, 0x1FF8, 0xE007, 0xFF07, 0xE0FF, 0xFFFF};

uint32_t c = 0;
uint32_t runs = 0;

void drawDisplay(void *pvParameters)
{
  uint32_t frames = 0;
  int32_t frame_millis = 0;
  uint16_t lastBorderColor = 0;
  tft.startWrite();
  tft.fillScreen(TFT_BLACK);
  tft.endWrite();
  while (1)
  {
    uint32_t evt;
    if (xQueueReceive(frameRenderTimerQueue, &evt, portMAX_DELAY))
    {
      if (frame_millis == 0)
      {
        frame_millis = millis();
      }
      tft.startWrite();
      tft.dmaWait();
      // do the border
      uint8_t borderColor = hwopt.BorderColor & B00000111;
      // TODO - can the border color be bright?
      uint16_t tftColor = specpal565[borderColor];
      if (tftColor != lastBorderColor)
      {
        // do the border with some simple rects - no need to push pixels for a solid color
        tft.fillRect(0, 0, screenWidth, borderHeight, tftColor);
        tft.fillRect(0, screenHeight - borderHeight, screenWidth, borderHeight, tftColor);
        tft.fillRect(0, borderHeight, borderWidth, screenHeight - borderHeight, tftColor);
        tft.fillRect(screenWidth - borderWidth, borderHeight, borderWidth, screenHeight - borderHeight, tftColor);
        lastBorderColor = tftColor;
      }
      // do the pixels
      uint8_t *attrBase = mem.p + mem.vo[hwopt.videopage] + 0x1800;
      uint8_t *pixelBase = mem.p + mem.vo[hwopt.videopage];
      for (int attrY = 0; attrY < 192 / 8; attrY++)
      {
        bool dirty = false;
        for (int attrX = 0; attrX < 256 / 8; attrX++)
        {
          // read the value of the attribute
          uint8_t attr = *(attrBase + 32 * attrY + attrX);
          uint8_t inkColor = attr & B00000111;
          uint8_t paperColor = (attr & B00111000) >> 3;
          if ((attr & B01000000) != 0)
          {
            inkColor = inkColor + 8;
            paperColor = paperColor + 8;
          }
          uint16_t tftInkColor = specpal565[inkColor];
          uint16_t tftPaperColor = specpal565[paperColor];
          for (int y = attrY * 8; y < attrY * 8 + 8; y++)
          {
            // read the value of the pixels
            int col = (attrX * 8 & B11111000) >> 3;
            int scan = (y & B11000000) + ((y & B111) << 3) + ((y & B111000) >> 3);
            uint8_t row = *(pixelBase + 32 * scan + col);
            for (int x = attrX * 8; x < attrX * 8 + 8; x++)
            {
              uint16_t *pixelAddress = frameBuffer + x + 256 * y;
              if (row & (1 << (7 - (x & 7))))
              {
                if (tftInkColor != *pixelAddress)
                {
                  *pixelAddress = tftInkColor;
                  dirty = true;
                }
              }
              else
              {
                if (tftPaperColor != *pixelAddress)
                {
                  *pixelAddress = tftPaperColor;
                  dirty = true;
                }
              }
            }
          }
        }
        if (dirty)
        {
          // push out this block of pixels 256 * 8
          tft.dmaWait();
          tft.setWindow(borderWidth, borderHeight + attrY * 8, borderWidth + 255, borderHeight + attrY * 8 + 7);
          tft.pushPixelsDMA(frameBuffer + attrY * 8 * 256, 256 * 8);
        }
      }
      tft.endWrite();
      frames++;
      if (millis() - frame_millis > 1000)
      {
        vTaskDelay(1);
        Serial.printf("Frame rate=%d\n", frames);
        frames = 0;
        frame_millis = millis();
        Serial.printf("Executed at %d cycles %d runs\n", c, runs);
        c = 0;
        runs = 0;
      }
    }
  }
}

void z80Runner(void *pvParameter)
{
  int8_t audioBuffer[256];
  int audioPos = 0;
  int lastMillis = 0;
  while (1)
  {
    uint32_t evt;
    size_t bytes_written = 0;
    if (xQueueReceive(cpuRunTimerQueue, &evt, portMAX_DELAY))
    {
      runs++;
      c += 175;
      Z80Run(&spectrumZ80, 175);
      if (hwopt.SoundBits != 0)
      {
        audioBuffer[audioPos] = 20;
      }
      else
      {
        audioBuffer[audioPos] = -20;
      }
      audioPos++;
      if (audioPos == 256)
      {
        audioPos = 0;
        // write the audio buffer to the I2S device (blocking for 1 second if necessary
        audioOutput->write(audioBuffer, 256);
      }
    }
  }
}

IRAM_ATTR void onTimerCallback(void *param)
{
  static uint32_t timerCount = 0;
  timerCount++;
  timer_spinlock_take(TIMER_GROUP_0);
  timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);
  timer_group_enable_alarm_in_isr(TIMER_GROUP_0, TIMER_0);
  timer_spinlock_give(TIMER_GROUP_0);
  uint32_t value = 0;
  BaseType_t high_task_awoken = pdFALSE;
  xQueueSendFromISR(cpuRunTimerQueue, &value, &high_task_awoken);
  if (timerCount < 350)
  {
    hwopt.portFF = 0;
  }
  else
  {
    hwopt.portFF = 0xFF;
  }
  // is it time to render a frame? (50 times per second)
  if (timerCount == 400)
  {
    timerCount = 0;
    xQueueSendFromISR(frameRenderTimerQueue, &value, &high_task_awoken);
  }
  if (high_task_awoken == pdTRUE)
  {
    portYIELD_FROM_ISR();
  }

  // return high_task_awoken == pdTRUE; // return whether we need to yield at the end of ISR
}

void setup(void)
{
  Serial.begin(115200);

  // turn on the TFT
  pinMode(TFT_POWER, OUTPUT);
  digitalWrite(TFT_POWER, LOW);

  for (int i = 0; i < 5; i++)
  {
    delay(1000);
    AS_printf("Waiting %i\n", i);
  }
  AS_printf("OpenVega+ Boot!\n");
  pinMode(SPK_MODE, OUTPUT);
  digitalWrite(SPK_MODE, HIGH);
  i2s_pin_config_t i2s_speaker_pins = {
      .bck_io_num = I2S_SPEAKER_SERIAL_CLOCK,
      .ws_io_num = I2S_SPEAKER_LEFT_RIGHT_CLOCK,
      .data_out_num = I2S_SPEAKER_SERIAL_DATA,
      .data_in_num = I2S_PIN_NO_CHANGE};

  audioOutput = new I2SOutput(I2S_NUM_1, i2s_speaker_pins);
  audioOutput->start(20000);
  int8_t dummyAudio[256] = {0};
  for(int i = 0; i<10; i++){
    audioOutput->write(dummyAudio, 256);
  }

  keypad_i2c_init();
  // Initialize SPIFFS
  if (!SPIFFS.begin(true))
  {
    AS_printf("An Error has occurred while mounting SPIFFS\n");
    return;
  }

  Serial.println("Border Width: " + String(borderWidth));
  Serial.println("Border Height: " + String(borderHeight));

  // This is supous that provide por CPU to the program.
  // WiFi.mode(WIFI_OFF);
  // btStop();

  tft.begin();
  // DMA - should work with ESP32, STM32F2xx/F4xx/F7xx processors  >>>>>> DMA IS FOR SPI DISPLAYS ONLY <<<<<<
  tft.initDMA(); // Initialise the DMA engine
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  AS_printf("Total heap: ");
  AS_print(ESP.getHeapSize());
  AS_printf("\n");
  AS_printf("Free heap: ");
  AS_print(ESP.getFreeHeap());
  AS_printf("\n");
  AS_printf("Total PSRAM: ");
  AS_print(ESP.getPsramSize());
  AS_printf("\n");
  AS_printf("Free PSRAM: ");
  AS_print(ESP.getFreePsram());
  AS_printf("\n");

  frameBuffer = (uint16_t *)malloc(256 * 192 * sizeof(uint16_t));
  if (frameBuffer == NULL)
  {
    AS_printf("Error! memory not allocated for screenbuffer.\n");
    delay(10000);
  }
  memset(frameBuffer, 0, 256 * 192 * sizeof(uint16_t));

  AS_printf("\nDespues\n");
  AS_printf("Total heap: ");
  AS_print(ESP.getHeapSize());
  AS_printf("\n");
  AS_printf("Free heap: ");
  AS_print(ESP.getFreeHeap());
  AS_printf("\n");
  AS_printf("Total PSRAM: ");
  AS_print(ESP.getPsramSize());
  AS_printf("\n");
  AS_printf("Free PSRAM: ");
  AS_print(ESP.getFreePsram());
  AS_printf("\n");

  // FIXME porque 69888?? ¿quiza un frame, para que pinte la pantalla una vez?
  Z80Reset(&spectrumZ80, 69888);
  Z80FlagTables();
  AS_printf("Z80 Initialization completed\n");

  init_spectrum(SPECMDL_48K, "/48.rom");
  reset_spectrum(&spectrumZ80);
  Load_SNA(&spectrumZ80, "/manic.sna");
  //  Load_SNA(&spectrumZ80,"/t1.sna");

  delay(2000);
  AS_printf("Entrando en el loop\n");
  cpuRunTimerQueue = xQueueCreate(10, sizeof(uint32_t));
  frameRenderTimerQueue = xQueueCreate(10, sizeof(uint32_t));
  // create a timer that will fire at the sample rate
  timer_config_t timer_config = {
      .alarm_en = TIMER_ALARM_EN,
      .counter_en = TIMER_PAUSE,
      .intr_type = TIMER_INTR_LEVEL,
      .counter_dir = TIMER_COUNT_UP,
      .auto_reload = TIMER_AUTORELOAD_EN,
      .divider = 80};
  esp_err_t result = timer_init(TIMER_GROUP_0, TIMER_0, &timer_config);
  if (result != ESP_OK)
  {
    Serial.printf("Error initializing timer: %d\n", result);
  }
  result = timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);
  if (result != ESP_OK)
  {
    Serial.printf("Error setting timer counter value: %d\n", result);
  }
  result = timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 1000000 / 20000);
  if (result != ESP_OK)
  {
    Serial.printf("Error setting timer alarm value: %d\n", result);
  }
  // timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 1000);
  result = timer_enable_intr(TIMER_GROUP_0, TIMER_0);
  if (result != ESP_OK)
  {
    Serial.printf("Error enabling timer interrupt: %d\n", result);
  }
  result = timer_isr_register(TIMER_GROUP_0, TIMER_0, &onTimerCallback, NULL, ESP_INTR_FLAG_IRAM, NULL);
  if (result != ESP_OK)
  {
    Serial.printf("Error registering timer interrupt: %d\n", result);
    // print the string error
    const char *err = esp_err_to_name(result);
    Serial.printf("Error: %s\n", err);
  }
  result = timer_start(TIMER_GROUP_0, TIMER_0);
  xTaskCreatePinnedToCore(drawDisplay, "drawDisplay", 16384, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(z80Runner, "z80Runner", 16384, NULL, 5, NULL, 0);
}

void loop(void)
{
  vTaskDelay(1000);
}
