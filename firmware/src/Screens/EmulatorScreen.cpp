#include <Arduino.h>
#include "../TFT/TFTDisplay.h"
#include "../Emulator/spectrum.h"
#include "../Emulator/snaps.h"
#include "../AudioOutput/AudioOutput.h"
#include "../Files/Files.h"
#include "ErrorScreen.h"
#include "EmulatorScreen.h"
#include "AlphabetPicker.h"
#include "GameFilePickerScreen.h"
#include "NavigationStack.h"
#include "SaveSnapshotScreen.h"
#include "EmulatorScreen/Renderer.h"
#include "EmulatorScreen/Machine.h"
#include "EmulatorScreen/GameLoader.h"

const std::vector<std::string> tap_extensions = {".tap", ".tzx"};
const std::vector<std::string> no_sd_card_error = {"No SD Card", "Insert an SD Card", "to load games"};
const std::vector<std::string> no_files_error = {"No games found", "on the SD Card", "add Z80 or SNA files"};

void EmulatorScreen::triggerLoadTape()
{
  if (isLoading) {
    return;
  }
  machine->pause();
  renderer->pause();
  if (!m_files->isAvailable())
  {
    ErrorScreen *errorScreen = new ErrorScreen(
        no_sd_card_error,
        m_tft,
        m_audioOutput);
    m_navigationStack->push(errorScreen);
    return;
  }
  drawBusy();
  FileLetterCountVector fileLetterCounts = m_files->getFileLetters("/", tap_extensions);
  if (fileLetterCounts.size() == 0)
  {
    ErrorScreen *errorScreen = new ErrorScreen(
        no_files_error,
        m_tft,
        m_audioOutput);
    m_navigationStack->push(errorScreen);
    return;
  }
  AlphabetPicker<GameFilePickerScreen> *alphabetPicker = new AlphabetPicker<GameFilePickerScreen>("Select Tape File", m_files, m_tft, m_audioOutput, "/", tap_extensions);
  alphabetPicker->setItems(fileLetterCounts);
  m_navigationStack->push(alphabetPicker);
}

EmulatorScreen::EmulatorScreen(TFTDisplay &tft, AudioOutput *audioOutput, IFiles *files)
    : Screen(tft, audioOutput), m_files(files)
{
  renderer = new Renderer(tft);
  machine = new Machine(renderer, audioOutput, [&]()
                        {
    Serial.println("ROM loading routine hit");
    triggerLoadTape(); });
  gameLoader = new GameLoader(machine, renderer, audioOutput);
}

void EmulatorScreen::run(std::string filename, models_enum model)
{
  m_tft.fillScreen(TFT_BLACK);
  renderer->start();
  auto bl = BusyLight();
  machine->setup(model);
  if (filename.size() > 0)
  {
    // check for tap or tpz files
    std::string ext = filename.substr(filename.find_last_of(".") + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c)
                   { return std::tolower(c); });
    if (ext == "tap" || ext == "tzx")
    {
      machine->startLoading();
      gameLoader->loadTape(filename.c_str());
    }
    else
    {
      // generic loading of z80 and sna files
      Load(machine->getMachine(), filename.c_str());
    }
  }
  // audioFile = fopen("/fs/audio.raw", "wb");
  machine->start(audioFile);
}

void EmulatorScreen::pause()
{
  renderer->pause();
  machine->pause();
}

void EmulatorScreen::resume()
{
  renderer->resume();
  machine->resume();
}

void EmulatorScreen::updateKey(SpecKeys key, uint8_t state)
{
  // TODO audio capture
  // if (key == SPECKEY_0)
  // {
  //   if (audioFile)
  //   {
  //     machine->pause();
  //     vTaskDelay(1000 / portTICK_PERIOD_MS);
  //     fclose(audioFile);
  //     audioFile = NULL;
  //     Serial.printf("Audio file closed\n");
  //   }
  // }
  if (!isInTimeTravelMode)
  {
    machine->updateKey(key, state);
  }
}

void EmulatorScreen::pressKey(SpecKeys key)
{
  if (key == SPECKEY_MENU)
  {
    // TODO show menu for save snapshot, load snapshot, time travel
    if (!isInTimeTravelMode)
    {
      pause();
      isInTimeTravelMode = true;
      machine->startTimeTravel();
      drawTimeTravel();
    }
    // showSaveSnapshotScreen();
  } else if (isInTimeTravelMode)
  {
    // TODO cancel time travel
    if (key == SPECKEY_ENTER)
    {
      machine->stopTimeTravel();
      isInTimeTravelMode = false;
      resume();
    }
    else
    {
      if (key == SPECKEY_5) {
        machine->stepBack();
      }
      if (key == SPECKEY_8) {
        machine->stepForward();
      }
      drawTimeTravel();
    }
  }
}

void EmulatorScreen::showSaveSnapshotScreen()
{
  m_navigationStack->push(new SaveSnapshotScreen(m_tft, m_audioOutput, machine->getMachine()));
}

void EmulatorScreen::loadTape(std::string filename)
{
  isLoading = true;
  renderer->resume();
  renderer->setNeedsRedraw();
  gameLoader->loadTape(filename);
  renderer->setIsLoading(false);
  renderer->setNeedsRedraw();
  machine->resume();
  isLoading = false;
}

void EmulatorScreen::drawTimeTravel() {
    m_tft.fillRect(0, 0, m_tft.width(), 20, TFT_BLACK);

    m_tft.loadFont(GillSans_15_vlw);
    m_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    // Draw the left control "<5"
    m_tft.drawString("<5", 5, 0);
    
    // Draw the center text "Time Travel - Enter=Jump"
    Point centerSize = m_tft.measureString("Time Travel - Enter=Jump");
    int centerX = (m_tft.width() - centerSize.x) / 2;
    m_tft.drawString("Time Travel - Enter=Jump", centerX, 0);
    
    // Draw the right control "8>"
    Point rightSize = m_tft.measureString("8>");
    int rightX = m_tft.width() - rightSize.x - 5;  // 5 pixels from right edge
    m_tft.drawString("8>", rightX, 0);
}
