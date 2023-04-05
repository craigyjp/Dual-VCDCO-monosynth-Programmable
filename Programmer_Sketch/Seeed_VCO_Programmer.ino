/*
  Seeed VCDCO programmer- Firmware Rev 1.2

  Includes code by:
    Dave Benn - Handling MUXs, a few other bits and original inspiration  https://www.notesandvolts.com/2019/01/teensy-synth-part-10-hardware.html

  Arduino IDE
  Tools Settings:
  Board: "Teensy3.6"
  USB Type: "Serial + MIDI + Audio"
  CPU Speed: "180"
  Optimize: "Fastest"

  Additional libraries:
    Agileware CircularBuffer available in Arduino libraries manager
    Replacement files are in the Modified Libraries folder and need to be placed in the teensy Audio folder.
*/

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <MIDI.h>
#include <USBHost_t36.h>
#include "MidiCC.h"
#include "Constants.h"
#include "Parameters.h"
#include "PatchMgr.h"
#include "HWControls.h"
#include "EepromMgr.h"
#include "Settings.h"
#include <ShiftRegister74HC595.h>

#define PARAMETER 0      //The main page for displaying the current patch and control (parameter) changes
#define RECALL 1         //Patches list
#define SAVE 2           //Save patch page
#define REINITIALISE 3   // Reinitialise message
#define PATCH 4          // Show current patch bypassing PARAMETER
#define PATCHNAMING 5    // Patch naming page
#define DELETE 6         //Delete patch page
#define DELETEMSG 7      //Delete patch message page
#define SETTINGS 8       //Settings page
#define SETTINGSVALUE 9  //Settings page

unsigned int state = PARAMETER;

#include "ST7735Display.h"

boolean cardStatus = false;

//USB HOST MIDI Class Compliant
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
MIDIDevice midi1(myusb);


//MIDI 5 Pin DIN
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);  //RX - Pin 0


int count = 0;  //For MIDI Clk Sync
int DelayForSH3 = 50;
int patchNo = 0;
int voiceToReturn = -1;        //Initialise
long earliestTime = millis();  //For voice allocation - initialise to now
unsigned long buttonDebounce = 0;

ShiftRegister74HC595<5> sr(26, 28, 27);

void setup() {
  SPI.begin();
  setupDisplay();
  setUpSettings();
  setupHardware();

  cardStatus = SD.begin(BUILTIN_SDCARD);
  if (cardStatus) {
    Serial.println("SD card is connected");
    //Get patch numbers and names from SD card
    loadPatches();
    if (patches.size() == 0) {
      //save an initialised patch to SD card
      savePatch("1", INITPATCH);
      loadPatches();
    }
  } else {
    Serial.println("SD card is not connected or unusable");
    reinitialiseToPanel();
    showPatchPage("No SD", "conn'd / usable");
  }

  //Read MIDI Channel from EEPROM
  midiChannel = getMIDIChannel();
  oldmidiChannel = midiChannel;
  switch (midiChannel) {
    case 0:
      sr.set(MIDI1, LOW);
      sr.set(MIDI2, LOW);
      sr.set(MIDI4, LOW);
      sr.set(MIDI8, LOW);
      break;

    case 1:
      sr.set(MIDI1, HIGH);
      sr.set(MIDI2, LOW);
      sr.set(MIDI4, LOW);
      sr.set(MIDI8, LOW);
      break;

    case 2:
      sr.set(MIDI1, LOW);
      sr.set(MIDI2, HIGH);
      sr.set(MIDI4, LOW);
      sr.set(MIDI8, LOW);
      break;

    case 3:
      sr.set(MIDI1, HIGH);
      sr.set(MIDI2, HIGH);
      sr.set(MIDI4, LOW);
      sr.set(MIDI8, LOW);
      break;

    case 4:
      sr.set(MIDI1, LOW);
      sr.set(MIDI2, LOW);
      sr.set(MIDI4, HIGH);
      sr.set(MIDI8, LOW);
      break;

    case 5:
      sr.set(MIDI1, HIGH);
      sr.set(MIDI2, LOW);
      sr.set(MIDI4, HIGH);
      sr.set(MIDI8, LOW);
      break;

    case 6:
      sr.set(MIDI1, LOW);
      sr.set(MIDI2, HIGH);
      sr.set(MIDI4, HIGH);
      sr.set(MIDI8, LOW);
      break;

    case 7:
      sr.set(MIDI1, HIGH);
      sr.set(MIDI2, HIGH);
      sr.set(MIDI4, HIGH);
      sr.set(MIDI8, LOW);
      break;

    case 8:
      sr.set(MIDI1, LOW);
      sr.set(MIDI2, LOW);
      sr.set(MIDI4, LOW);
      sr.set(MIDI8, HIGH);
      break;

    case 9:
      sr.set(MIDI1, HIGH);
      sr.set(MIDI2, LOW);
      sr.set(MIDI4, LOW);
      sr.set(MIDI8, HIGH);
      break;

    case 10:
      sr.set(MIDI1, LOW);
      sr.set(MIDI2, HIGH);
      sr.set(MIDI4, LOW);
      sr.set(MIDI8, HIGH);
      break;

    case 11:
      sr.set(MIDI1, HIGH);
      sr.set(MIDI2, HIGH);
      sr.set(MIDI4, LOW);
      sr.set(MIDI8, HIGH);
      break;

    case 12:
      sr.set(MIDI1, LOW);
      sr.set(MIDI2, LOW);
      sr.set(MIDI4, HIGH);
      sr.set(MIDI8, HIGH);
      break;

    case 13:
      sr.set(MIDI1, HIGH);
      sr.set(MIDI2, LOW);
      sr.set(MIDI4, HIGH);
      sr.set(MIDI8, HIGH);
      break;

    case 14:
      sr.set(MIDI1, LOW);
      sr.set(MIDI2, HIGH);
      sr.set(MIDI4, HIGH);
      sr.set(MIDI8, HIGH);
      break;

    case 15:
      sr.set(MIDI1, HIGH);
      sr.set(MIDI2, HIGH);
      sr.set(MIDI4, HIGH);
      sr.set(MIDI8, HIGH);
      break;
  }

  Serial.println("MIDI Ch:" + String(midiChannel) + " (0 is Omni On)");

  //USB Client MIDI
  usbMIDI.setHandleControlChange(myupliftControlChange);
  usbMIDI.setHandleProgramChange(myProgramChange);
  Serial.println("USB Client MIDI Listening");

  //MIDI 5 Pin DIN
  MIDI.begin();
  MIDI.setHandleControlChange(myupliftControlChange);
  MIDI.setHandleProgramChange(myProgramChange);
  MIDI.turnThruOn(midi::Thru::Mode::Off);
  Serial.println("MIDI In DIN Listening");


  //Read Aftertouch from EEPROM, this can be set individually by each patch.
  AfterTouchDest = getAfterTouch();
  if (AfterTouchDest < 0 || AfterTouchDest > 3) {
    storeAfterTouch(0);
  }
  oldAfterTouchDest = AfterTouchDest;
  switch (AfterTouchDest) {
    case 0:
      sr.set(AFTERTOUCH_A, LOW);
      sr.set(AFTERTOUCH_B, LOW);
      sr.set(AFTERTOUCH_C, LOW);
      break;

    case 1:
      sr.set(AFTERTOUCH_A, HIGH);
      sr.set(AFTERTOUCH_B, LOW);
      sr.set(AFTERTOUCH_C, LOW);
      break;

    case 2:
      sr.set(AFTERTOUCH_A, LOW);
      sr.set(AFTERTOUCH_B, HIGH);
      sr.set(AFTERTOUCH_C, LOW);
      break;

    case 3:
      sr.set(AFTERTOUCH_A, HIGH);
      sr.set(AFTERTOUCH_B, HIGH);
      sr.set(AFTERTOUCH_C, LOW);
      break;
  }

  // Read NotePriority from EEPROM

  NotePriority = getNotePriority();
  if (NotePriority < 0 || NotePriority > 2) {
    storeNotePriority(1);
  }
  oldNotePriority = NotePriority;
  switch (NotePriority) {
    case 0:
      sr.set(NOTEPRIORITYA0, HIGH);
      sr.set(NOTEPRIORITYA2, LOW);
      break;

    case 1:
      sr.set(NOTEPRIORITYA0, LOW);
      sr.set(NOTEPRIORITYA2, LOW);
      break;

    case 2:
      sr.set(NOTEPRIORITYA0, LOW);
      sr.set(NOTEPRIORITYA2, HIGH);
      break;
  }

  // Read FilterLoop from EEPROM

  FilterLoop = getFilterLoop();
  if (FilterLoop < 0 || FilterLoop > 2) {
    storeFilterLoop(0);
  }
  oldFilterLoop = FilterLoop;
  switch (FilterLoop) {
    case 0:
      sr.set(FLOOPBIT0, LOW);
      sr.set(FLOOPBIT1, LOW);
      break;

    case 1:
      sr.set(FLOOPBIT0, HIGH);
      sr.set(FLOOPBIT1, LOW);
      break;

    case 2:
      sr.set(FLOOPBIT0, HIGH);
      sr.set(FLOOPBIT1, HIGH);
      break;
  }

  // Read FilterLoop from EEPROM

  AmpLoop = getAmpLoop();
  if (AmpLoop < 0 || AmpLoop > 2) {
    storeAmpLoop(0);
  }
  oldAmpLoop = AmpLoop;
  switch (AmpLoop) {
    case 0:
      sr.set(ALOOPBIT0, LOW);
      sr.set(ALOOPBIT1, LOW);
      break;

    case 1:
      sr.set(ALOOPBIT0, HIGH);
      sr.set(ALOOPBIT1, LOW);
      break;

    case 2:
      sr.set(ALOOPBIT0, HIGH);
      sr.set(ALOOPBIT1, HIGH);
      break;
  }

  // Read ClockSource from EEPROM

  ClockSource = getClockSource();
  if (ClockSource < 0 || ClockSource > 2) {
    storeClockSource(0);
  }
  oldClockSource = ClockSource;
  switch (ClockSource) {
    case 0:
      sr.set(EXTCLOCK, LOW);
      sr.set(MIDICLOCK, LOW);
      break;

    case 1:
      sr.set(EXTCLOCK, HIGH);
      sr.set(MIDICLOCK, LOW);
      break;

    case 2:
      sr.set(EXTCLOCK, LOW);
      sr.set(MIDICLOCK, HIGH);
      break;
  }

  //Read aftertouch Depth from EEPROM
  afterTouchDepth = getafterTouchDepth();
  if (afterTouchDepth < 0 || afterTouchDepth > 10) {
    storeafterTouchDepth(0);
  }
  oldafterTouchDepth = afterTouchDepth;
  switch (afterTouchDepth) {
    case 0:
      afterTouch = 0;
      break;

    case 1:
      afterTouch = 164;
      break;

    case 2:
      afterTouch = 328;
      break;

    case 3:
      afterTouch = 492;
      break;

    case 4:
      afterTouch = 656;
      break;

    case 5:
      afterTouch = 840;
      break;

    case 6:
      afterTouch = 1024;
      break;

    case 7:
      afterTouch = 1188;
      break;

    case 8:
      afterTouch = 1352;

    case 9:
      afterTouch = 1536;
      break;

    case 10:
      afterTouch = 1720;
      break;
  }

  filterLogLin = getFilterEnv();
  if (filterLogLin < 0 || filterLogLin > 1) {
    storeFilterEnv(0);
  }
  oldfilterLogLin = filterLogLin;
  switch (filterLogLin) {
    case 0:
      sr.set(FILTER_LIN_LOG, LOW);
      break;
    case 1:
      sr.set(FILTER_LIN_LOG, HIGH);
      break;
  }

  ampLogLin = getAmpEnv();
  if (ampLogLin < 0 || ampLogLin > 1) {
    storeAmpEnv(0);
  } 
  oldampLogLin = ampLogLin;
  switch (ampLogLin) {
    case 0:
      sr.set(AMP_LIN_LOG, LOW);
      break;
    case 1:
      sr.set(AMP_LIN_LOG, HIGH);
      break;
  }

  //Read Pitch Bend Range from EEPROM
  //pitchBendRange = getPitchBendRange();

  //Read Mod Wheel Depth from EEPROM
  modWheelDepth = getModWheelDepth();

  //Read Encoder Direction from EEPROM
  encCW = getEncoderDir();
  patchNo = getLastPatch();
  //Serial.println("Recalling initial patch ");
  recallPatch(1);  //Load first patch

  //  reinitialiseToPanel();
}

void allNotesOff() {
}

void updatefmWaveDepth() {
  showCurrentParameterPage("Wave Mod", int(fmWaveDepthstr));
}

void updatefmDepth() {
  showCurrentParameterPage("FM Depth", int(fmDepthstr));
}

void updateosc1Tune() {
  showCurrentParameterPage("OSC1 Tune", String(osc1Tunestr));
}

void updateosc2Tune() {
  showCurrentParameterPage("OSC2 Tune", String(osc2Tunestr));
}

void updateosc1WaveMod() {
  showCurrentParameterPage("OSC1 Mod", String(osc1WaveModstr));
}

void updateosc2WaveMod() {
  showCurrentParameterPage("OSC2 Mod", String(osc2WaveModstr));
}

void updateosc1Range() {
  if (oct1A > 511 && oct1B > 511) {
    showCurrentParameterPage("Osc1 Range", String("0"));
    sr.set(OSC1_OCTA, HIGH);
    sr.set(OSC1_OCTB, HIGH);
  }
  if (oct1A < 511 && oct1B > 511) {
    showCurrentParameterPage("Osc1 Range", String("-1"));
    sr.set(OSC1_OCTA, HIGH);
    sr.set(OSC1_OCTB, LOW);
  }
  if (oct1A > 511 && oct1B < 511) {
    showCurrentParameterPage("Osc1 Range", String("+1"));
    sr.set(OSC1_OCTA, LOW);
    sr.set(OSC1_OCTB, HIGH);
  }
}

void updateosc2Range() {
  if (oct2A > 511 && oct2B > 511) {
    showCurrentParameterPage("Osc2 Range", String("0"));
    sr.set(OSC2_OCTA, HIGH);
    sr.set(OSC2_OCTB, HIGH);
  }
  if (oct2A < 511 && oct2B > 511) {
    showCurrentParameterPage("Osc2 Range", String("-1"));
    sr.set(OSC2_OCTA, HIGH);
    sr.set(OSC2_OCTB, LOW);
  }
  if (oct2A > 511 && oct2B < 511) {
    showCurrentParameterPage("Osc2 Range", String("+1"));
    sr.set(OSC2_OCTA, LOW);
    sr.set(OSC2_OCTB, HIGH);
  }
}

void updateosc1Bank() {
  if (osc1BankA > 511 && osc1BankB > 511) {
    showCurrentParameterPage("Osc1 Bank", String("FM"));
    sr.set(OSC1_BANKA, HIGH);
    sr.set(OSC1_BANKB, HIGH);
  }
  if (osc1BankA < 511 && osc1BankB > 511) {
    showCurrentParameterPage("Osc1 Bank", String("Fold"));
    sr.set(OSC1_BANKA, HIGH);
    sr.set(OSC1_BANKB, LOW);
  }
  if (osc1BankA > 511 && osc1BankB < 511) {
    showCurrentParameterPage("Osc1 Bank", String("AM"));
    sr.set(OSC1_BANKA, LOW);
    sr.set(OSC1_BANKB, HIGH);
  }
}

void updateosc2Bank() {
  if (osc2BankA > 511 && osc2BankB > 511) {
    showCurrentParameterPage("Osc2 Bank", String("FM"));
    sr.set(OSC2_BANKA, HIGH);
    sr.set(OSC2_BANKB, HIGH);
  }
  if (osc2BankA < 511 && osc2BankB > 511) {
    showCurrentParameterPage("Osc2 Bank", String("Fold"));
    sr.set(OSC2_BANKA, HIGH);
    sr.set(OSC2_BANKB, LOW);
  }
  if (osc2BankA > 511 && osc2BankB < 511) {
    showCurrentParameterPage("Osc2 Bank", String("AM"));
    sr.set(OSC2_BANKA, LOW);
    sr.set(OSC2_BANKB, HIGH);
  }
}

void updateglideTime() {
  showCurrentParameterPage("Glide Time", String(glideTimestr * 10) + " Seconds");
}

void updateosc1WaveSelect() {
  if (osc1WaveSelect >= 0 && osc1WaveSelect < 127) {
    Oscillator1Waveform = "Wave 1";
    sr.set(OSC1_WAVEA, LOW);
    sr.set(OSC1_WAVEB, LOW);
    sr.set(OSC1_WAVEC, LOW);
  } else if (osc1WaveSelect >= 128 && osc1WaveSelect < 255) {
    Oscillator1Waveform = "Wave 2";
    sr.set(OSC1_WAVEA, HIGH);
    sr.set(OSC1_WAVEB, LOW);
    sr.set(OSC1_WAVEC, LOW);
  } else if (osc1WaveSelect >= 256 && osc1WaveSelect < 383) {
    Oscillator1Waveform = "Wave 3";
    sr.set(OSC1_WAVEA, LOW);
    sr.set(OSC1_WAVEB, HIGH);
    sr.set(OSC1_WAVEC, LOW);
  } else if (osc1WaveSelect >= 384 && osc1WaveSelect < 511) {
    Oscillator1Waveform = "Wave 4";
    sr.set(OSC1_WAVEA, HIGH);
    sr.set(OSC1_WAVEB, HIGH);
    sr.set(OSC1_WAVEC, LOW);
  } else if (osc1WaveSelect >= 512 && osc1WaveSelect < 639) {
    Oscillator1Waveform = "Wave 5";
    sr.set(OSC1_WAVEA, LOW);
    sr.set(OSC1_WAVEB, LOW);
    sr.set(OSC1_WAVEC, HIGH);
  } else if (osc1WaveSelect >= 640 && osc1WaveSelect < 767) {
    Oscillator1Waveform = "Wave 6";
    sr.set(OSC1_WAVEA, HIGH);
    sr.set(OSC1_WAVEB, LOW);
    sr.set(OSC1_WAVEC, HIGH);
  } else if (osc1WaveSelect >= 768 && osc1WaveSelect < 895) {
    Oscillator1Waveform = "Wave 7";
    sr.set(OSC1_WAVEA, LOW);
    sr.set(OSC1_WAVEB, HIGH);
    sr.set(OSC1_WAVEC, HIGH);
  } else if (osc1WaveSelect >= 896 && osc1WaveSelect < 1024) {
    Oscillator1Waveform = "Wave 8";
    sr.set(OSC1_WAVEA, HIGH);
    sr.set(OSC1_WAVEB, HIGH);
    sr.set(OSC1_WAVEC, HIGH);
  }
  showCurrentParameterPage("OSC1 Wave", Oscillator1Waveform);
}

void updateosc2WaveSelect() {
  if (osc2WaveSelect >= 0 && osc2WaveSelect < 127) {
    Oscillator2Waveform = "Wave 1";
    sr.set(OSC2_WAVEA, LOW);
    sr.set(OSC2_WAVEB, LOW);
    sr.set(OSC2_WAVEC, LOW);
  } else if (osc2WaveSelect >= 128 && osc2WaveSelect < 255) {
    Oscillator2Waveform = "Wave 2";
    sr.set(OSC2_WAVEA, HIGH);
    sr.set(OSC2_WAVEB, LOW);
    sr.set(OSC2_WAVEC, LOW);
  } else if (osc2WaveSelect >= 256 && osc2WaveSelect < 383) {
    Oscillator2Waveform = "Wave 3";
    sr.set(OSC2_WAVEA, LOW);
    sr.set(OSC2_WAVEB, HIGH);
    sr.set(OSC2_WAVEC, LOW);
  } else if (osc2WaveSelect >= 384 && osc2WaveSelect < 511) {
    Oscillator2Waveform = "Wave 4";
    sr.set(OSC2_WAVEA, HIGH);
    sr.set(OSC2_WAVEB, HIGH);
    sr.set(OSC2_WAVEC, LOW);
  } else if (osc2WaveSelect >= 512 && osc2WaveSelect < 639) {
    Oscillator2Waveform = "Wave 5";
    sr.set(OSC2_WAVEA, LOW);
    sr.set(OSC2_WAVEB, LOW);
    sr.set(OSC2_WAVEC, HIGH);
  } else if (osc2WaveSelect >= 640 && osc2WaveSelect < 767) {
    Oscillator2Waveform = "Wave 6";
    sr.set(OSC2_WAVEA, HIGH);
    sr.set(OSC2_WAVEB, LOW);
    sr.set(OSC2_WAVEC, HIGH);
  } else if (osc2WaveSelect >= 768 && osc2WaveSelect < 895) {
    Oscillator2Waveform = "Wave 7";
    sr.set(OSC2_WAVEA, LOW);
    sr.set(OSC2_WAVEB, HIGH);
    sr.set(OSC2_WAVEC, HIGH);
  } else if (osc2WaveSelect >= 896 && osc2WaveSelect < 1024) {
    Oscillator2Waveform = "Wave 8";
    sr.set(OSC2_WAVEA, HIGH);
    sr.set(OSC2_WAVEB, HIGH);
    sr.set(OSC2_WAVEC, HIGH);
  }
  showCurrentParameterPage("OSC2 Wave", Oscillator2Waveform);
}

void updatenoiseLevel() {
  showCurrentParameterPage("Noise Level", String(noiseLevelstr));
}

void updateOsc2Level() {
  showCurrentParameterPage("OSC2 Level", int(osc2Levelstr));
}

void updateOsc1Level() {
  showCurrentParameterPage("OSC1 Level", int(osc1Levelstr));
}

void updateKeyTrack() {
  showCurrentParameterPage("KeyTrack Lev", int(keyTrackstr));
}

void updateFilterCutoff() {
  showCurrentParameterPage("Cutoff", String(filterCutoffstr) + " Hz");
}

void updatefilterLFO() {
  showCurrentParameterPage("TM depth", int(filterLFOstr));
}

void updatefilterRes() {
  showCurrentParameterPage("Resonance", int(filterResstr));
}

void updateFilterType() {
  if (filterA > 511 && filterB > 511 && filterC > 511) {
    if (filterPoleSW < 511) {
      showCurrentParameterPage("Filter Type", String("3P LowPass"));
    } else {
      showCurrentParameterPage("Filter Type", String("4P LowPass"));
    }
    sr.set(FILTER_A, LOW);
    sr.set(FILTER_B, LOW);
    sr.set(FILTER_C, LOW);

  } else if (filterA < 511 && filterB > 511 && filterC > 511) {
    if (filterPoleSW < 511) {
      showCurrentParameterPage("Filter Type", String("1P LowPass"));
    } else {
      showCurrentParameterPage("Filter Type", String("2P LowPass"));
    }
    sr.set(FILTER_A, HIGH);
    sr.set(FILTER_B, LOW);
    sr.set(FILTER_C, LOW);

  } else if (filterA > 511 && filterB < 511 && filterC > 511) {
    if (filterPoleSW < 511) {
      showCurrentParameterPage("Filter Type", String("3P HP + 1P LP"));
    } else {
      showCurrentParameterPage("Filter Type", String("4P HighPass"));
    }
    sr.set(FILTER_A, LOW);
    sr.set(FILTER_B, HIGH);
    sr.set(FILTER_C, LOW);

  } else if (filterA < 511 && filterB < 511 && filterC > 511) {
    if (filterPoleSW < 511) {
      showCurrentParameterPage("Filter Type", String("1P HP + 1P LP"));
    } else {
      showCurrentParameterPage("Filter Type", String("2P HighPass"));
    }
    sr.set(FILTER_A, HIGH);
    sr.set(FILTER_B, HIGH);
    sr.set(FILTER_C, LOW);

  } else if (filterA > 511 && filterB > 511 && filterC < 511) {
    if (filterPoleSW < 511) {
      showCurrentParameterPage("Filter Type", String("2P HP + 1P LP"));
    } else {
      showCurrentParameterPage("Filter Type", String("4P BandPass"));
    }
    sr.set(FILTER_A, LOW);
    sr.set(FILTER_B, LOW);
    sr.set(FILTER_C, HIGH);

  } else if (filterA < 511 && filterB > 511 && filterC < 511) {
    if (filterPoleSW < 511) {
      showCurrentParameterPage("Filter Type", String("2P BP + 1P LP"));
    } else {
      showCurrentParameterPage("Filter Type", String("2P BandPass"));
    }
    sr.set(FILTER_A, HIGH);
    sr.set(FILTER_B, LOW);
    sr.set(FILTER_C, HIGH);

  } else if (filterA > 511 && filterB < 511 && filterC < 511) {
    if (filterPoleSW < 511) {
      showCurrentParameterPage("Filter Type", String("3P AP + 1P LP"));
    } else {
      showCurrentParameterPage("Filter Type", String("3P AllPass"));
    }
    sr.set(FILTER_A, LOW);
    sr.set(FILTER_B, HIGH);
    sr.set(FILTER_C, HIGH);

  } else if (filterA < 511 && filterB < 511 && filterC < 511) {
    if (filterPoleSW < 511) {
      showCurrentParameterPage("Filter Type", String("2P Notch + LP"));
    } else {
      showCurrentParameterPage("Filter Type", String("Notch"));
    }
    sr.set(FILTER_A, HIGH);
    sr.set(FILTER_B, HIGH);
    sr.set(FILTER_C, HIGH);
  }
}

void updatefilterEGlevel() {
  showCurrentParameterPage("EG Depth", int(filterEGlevelstr));
}

void updateLFORate() {
  showCurrentParameterPage("LFO Rate", String(LFORatestr) + " Hz");
}

void updateStratusLFOWaveform() {
  if (lfoAlt < 511) {
    if (LFOWaveform >= 0 && LFOWaveform < 127) {
      StratusLFOWaveform = "Saw +Oct";
    } else if (LFOWaveform >= 128 && LFOWaveform < 255) {
      StratusLFOWaveform = "Quad Saw";
    } else if (LFOWaveform >= 256 && LFOWaveform < 383) {
      StratusLFOWaveform = "Quad Pulse";
    } else if (LFOWaveform >= 384 && LFOWaveform < 511) {
      StratusLFOWaveform = "Tri Step";
    } else if (LFOWaveform >= 512 && LFOWaveform < 639) {
      StratusLFOWaveform = "Sine +Oct";
    } else if (LFOWaveform >= 640 && LFOWaveform < 767) {
      StratusLFOWaveform = "Sine +3rd";
    } else if (LFOWaveform >= 768 && LFOWaveform < 895) {
      StratusLFOWaveform = "Sine +4th";
    } else if (LFOWaveform >= 896 && LFOWaveform < 1024) {
      StratusLFOWaveform = "Rand Slopes";
    }
  } else {
    if (LFOWaveform >= 0 && LFOWaveform < 127) {
      StratusLFOWaveform = "Sawtooth Up";
    } else if (LFOWaveform >= 128 && LFOWaveform < 255) {
      StratusLFOWaveform = "Sawtooth Down";
    } else if (LFOWaveform >= 256 && LFOWaveform < 383) {
      StratusLFOWaveform = "Squarewave";
    } else if (LFOWaveform >= 384 && LFOWaveform < 511) {
      StratusLFOWaveform = "Triangle";
    } else if (LFOWaveform >= 512 && LFOWaveform < 639) {
      StratusLFOWaveform = "Sinewave";
    } else if (LFOWaveform >= 640 && LFOWaveform < 767) {
      StratusLFOWaveform = "Sweeps";
    } else if (LFOWaveform >= 768 && LFOWaveform < 895) {
      StratusLFOWaveform = "Lumps";
    } else if (LFOWaveform >= 896 && LFOWaveform < 1024) {
      StratusLFOWaveform = "Sample & Hold";
    }
  }
  showCurrentParameterPage("LFO Wave", StratusLFOWaveform);
}

void updatefilterAttack() {
  if (filterAttackstr < 1000) {
    showCurrentParameterPage("VCF Attack", String(int(filterAttackstr)) + " ms", FILTER_ENV);
  } else {
    showCurrentParameterPage("VCF Attack", String(filterAttackstr * 0.001) + " s", FILTER_ENV);
  }
}

void updatefilterDecay() {
  if (filterDecaystr < 1000) {
    showCurrentParameterPage("VCF Decay", String(int(filterDecaystr)) + " ms", FILTER_ENV);
  } else {
    showCurrentParameterPage("VCF Decay", String(filterDecaystr * 0.001) + " s", FILTER_ENV);
  }
}

void updatefilterSustain() {
  showCurrentParameterPage("VCF Sustain", String(filterSustainstr), FILTER_ENV);
}

void updatefilterRelease() {
  if (filterReleasestr < 1000) {
    showCurrentParameterPage("VCF Release", String(int(filterReleasestr)) + " ms", FILTER_ENV);
  } else {
    showCurrentParameterPage("VCF Release", String(filterReleasestr * 0.001) + " s", FILTER_ENV);
  }
}

void updateampAttack() {
  if (ampAttackstr < 1000) {
    showCurrentParameterPage("VCA Attack", String(int(ampAttackstr)) + " ms", AMP_ENV);
  } else {
    showCurrentParameterPage("VCA Attack", String(ampAttackstr * 0.001) + " s", AMP_ENV);
  }
}

void updateampDecay() {
  if (ampDecaystr < 1000) {
    showCurrentParameterPage("VCA Decay", String(int(ampDecaystr)) + " ms", AMP_ENV);
  } else {
    showCurrentParameterPage("VCA Decay", String(ampDecaystr * 0.001) + " s", AMP_ENV);
  }
}

void updateampSustain() {
  showCurrentParameterPage("VCA Sustain", String(ampSustainstr), AMP_ENV);
}

void updateampRelease() {
  if (ampReleasestr < 1000) {
    showCurrentParameterPage("VCA Release", String(int(ampReleasestr)) + " ms", AMP_ENV);
  } else {
    showCurrentParameterPage("VCA Release", String(ampReleasestr * 0.001) + " s", AMP_ENV);
  }
}

void updatevolumeControl() {
  showCurrentParameterPage("Volume", int(volumeControlstr));
}

////////////////////////////////////////////////////////////////

void updatefilterPoleSwitch() {
  if (filterPoleSW < 511) {
    //showCurrentParameterPage("VCF Pole", "On");
    sr.set(FILTER_POLE, HIGH);
    midiCCOut(CCfilterPoleSW, 127);
    updateFilterType();
  } else {
    //showCurrentParameterPage("VCF Pole", "Off");
    sr.set(FILTER_POLE, LOW);
    midiCCOut(CCfilterPoleSW, 0);
    updateFilterType();
  }
}

void updatefilterLoop() {
  switch (FilterLoop) {
    case 0:
      sr.set(FLOOPBIT0, LOW);
      sr.set(FLOOPBIT1, LOW);
      break;

    case 1:
      sr.set(FLOOPBIT0, HIGH);
      sr.set(FLOOPBIT1, LOW);
      break;

    case 2:
      sr.set(FLOOPBIT0, HIGH);
      sr.set(FLOOPBIT1, HIGH);
      break;
  }
  oldFilterLoop = FilterLoop;
}

void updatefilterEGinv() {
  if (filterEGinv > 511) {
    showCurrentParameterPage("Filter Env", "Positive");
    sr.set(FILTER_EG_INV, LOW);
    midiCCOut(CCfilterEGinv, 0);
  } else {
    showCurrentParameterPage("Filter Env", "Negative");
    sr.set(FILTER_EG_INV, HIGH);
    midiCCOut(CCfilterEGinv, 127);
  }
}

void updatevcaLoop() {
  switch (AmpLoop) {
    case 0:
      sr.set(ALOOPBIT0, LOW);
      sr.set(ALOOPBIT1, LOW);
      break;

    case 1:
      sr.set(ALOOPBIT0, HIGH);
      sr.set(ALOOPBIT1, LOW);
      break;

    case 2:
      sr.set(ALOOPBIT0, HIGH);
      sr.set(ALOOPBIT1, HIGH);
      break;
  }
  oldAmpLoop = AmpLoop;
}

void updatelfoAlt() {
  if (lfoAlt > 511) {
    showCurrentParameterPage("LFO Waveform", String("Original"));
    sr.set(LFO_ALT, HIGH);
    midiCCOut(CClfoAlt, 0);
  } else {
    showCurrentParameterPage("LFO Waveform", String("Alternate"));
    sr.set(LFO_ALT, LOW);
    midiCCOut(CClfoAlt, 127);
  }
}

void updateFilterEnv() {
  switch (filterLogLin) {
    case 0:
      sr.set(FILTER_LIN_LOG, LOW);
      break;
    case 1:
      sr.set(FILTER_LIN_LOG, HIGH);
      break;
  }
  oldfilterLogLin = filterLogLin;
}

void updateAmpEnv() {
  switch (ampLogLin) {
    case 0:
      sr.set(AMP_LIN_LOG, LOW);
      break;
    case 1:
      sr.set(AMP_LIN_LOG, HIGH);
      break;
  }
  oldampLogLin = ampLogLin;
}

void updatePitchBend() {
  showCurrentParameterPage("Bender Range", int(PitchBendLevelstr));
}

void updatemodWheel() {
  showCurrentParameterPage("Mod Range", int(modWheelLevelstr));
}

void updatePatchname() {
  showPatchPage(String(patchNo), patchName);
}

void updateAfterTouchDest() {
  switch (AfterTouchDest) {
    case 0:
      sr.set(AFTERTOUCH_A, LOW);
      sr.set(AFTERTOUCH_B, LOW);
      sr.set(AFTERTOUCH_C, LOW);
      break;

    case 1:
      sr.set(AFTERTOUCH_A, HIGH);
      sr.set(AFTERTOUCH_B, LOW);
      sr.set(AFTERTOUCH_C, LOW);
      break;

    case 2:
      sr.set(AFTERTOUCH_A, LOW);
      sr.set(AFTERTOUCH_B, HIGH);
      sr.set(AFTERTOUCH_C, LOW);
      break;

    case 3:
      sr.set(AFTERTOUCH_A, HIGH);
      sr.set(AFTERTOUCH_B, HIGH);
      sr.set(AFTERTOUCH_C, LOW);
      break;
  }
  oldAfterTouchDest = AfterTouchDest;
}

void myupliftControlChange(byte channel, byte control, int value) {
     value = value << 3;
     channel = channel;
     control = control;
     myControlChange(channel, control, value);
}

void myControlChange(byte channel, byte control, int value) {

  //  Serial.println("MIDI: " + String(control) + " : " + String(value));
  switch (control) {

    case CCosc1Tune:
      osc1Tune = value;
      osc1Tunestr = PITCH[value / 8];  // for display
      updateosc1Tune();
      break;

    case CCfmDepth:
      fmDepth = value;
      fmDepthstr = value / 8;
      updatefmDepth();
      break;

    case CCosc2Tune:
      osc2Tune = value;
      osc2Tunestr = PITCH[value / 8];  // for display
      updateosc2Tune();
      break;

    case CCosc1WaveMod:
      osc1WaveMod = value;
      osc1WaveModstr = (value / 8);
      updateosc1WaveMod();
      break;

    case CCosc2WaveMod:
      osc2WaveMod = value;
      osc2WaveModstr = (value / 8);
      updateosc2WaveMod();
      break;

    case CCosc1WaveSelect:
      osc1WaveSelect = value;
      osc1WaveSelectstr = int(value / 8);
      updateosc1WaveSelect();
      break;

    case CCosc2WaveSelect:
      osc2WaveSelect = value;
      osc2WaveSelectstr = int(value / 8);
      updateosc2WaveSelect();
      break;

    case CCglideTime:
      glideTimestr = LINEAR[value / 8];
      glideTime = value;
      updateglideTime();
      break;

    case CCfmWaveDepth:
      fmWaveDepthstr = (value / 8);
      fmWaveDepth = value;
      updatefmWaveDepth();
      break;

    case CCnoiseLevel:
      noiseLevel = value;
      noiseLevelstr = LINEARCENTREZERO[value / 8];
      updatenoiseLevel();
      break;

    case CCosc2Level:
      osc2Level = value;
      osc2Levelstr = value / 8;  // for display
      updateOsc2Level();
      break;

    case CCosc1Level:
      osc1Level = value;
      osc1Levelstr = value / 8;  // for display
      updateOsc1Level();
      break;

    case CCKeyTrack:
      keyTrack = value;
      keyTrackstr = value / 8;  // for display
      updateKeyTrack();
      break;

    case CCfilterCutoff:
      filterCutoff = value;
      filterCutoffstr = FILTERCUTOFF[value / 8];
      updateFilterCutoff();
      break;

    case CCfilterLFO:
      filterLFO = value;
      filterLFOstr = value / 8;
      updatefilterLFO();
      break;

    case CCfilterRes:
      filterRes = value;
      filterResstr = int(value / 8);
      updatefilterRes();
      break;

    case CCfilterType:
      filterType = value;
      updateFilterType();
      break;

    case CCfilterEGlevel:
      filterEGlevel = value;
      filterEGlevelstr = int(value / 8);
      updatefilterEGlevel();
      break;

    case CCLFORate:
      LFORatestr = LFOTEMPO[value / 8];  // for display
      LFORate = value;
      updateLFORate();
      break;

    case CCLFOWaveform:
      LFOWaveform = value;
      updateStratusLFOWaveform();
      break;

    case CCfilterAttack:
      filterAttack = value;
      filterAttackstr = ENVTIMES[value / 8];
      updatefilterAttack();
      break;

    case CCfilterDecay:
      filterDecay = value;
      filterDecaystr = ENVTIMES[value / 8];
      updatefilterDecay();
      break;

    case CCfilterSustain:
      filterSustain = value;
      filterSustainstr = LINEAR_FILTERMIXERSTR[value / 8];
      updatefilterSustain();
      break;

    case CCfilterRelease:
      filterRelease = value;
      filterReleasestr = ENVTIMES[value / 8];
      updatefilterRelease();
      break;

    case CCampAttack:
      ampAttack = value;
      ampAttackstr = ENVTIMES[value / 8];
      updateampAttack();
      break;

    case CCampDecay:
      ampDecay = value;
      ampDecaystr = ENVTIMES[value / 8];
      updateampDecay();
      break;

    case CCampSustain:
      ampSustain = value;
      ampSustainstr = LINEAR_FILTERMIXERSTR[value / 8];
      updateampSustain();
      break;

    case CCampRelease:
      ampRelease = value;
      ampReleasestr = ENVTIMES[value / 8];
      updateampRelease();
      break;

    case CCvolumeControl:
      volumeControl = value;
      volumeControlstr = value / 8;
      updatevolumeControl();
      break;

    case CCPitchBend:
      PitchBendLevel = value;
      PitchBendLevelstr = PITCHBEND[value / 8];  // for display
      updatePitchBend();
      break;

    case CCmodwheel:
      switch (modWheelDepth) {
        case 1:
          modWheelLevel = ((value * 8) / 5);
          fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 2:
          modWheelLevel = ((value * 8) / 4);
          fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 3:
          modWheelLevel = ((value * 8) / 3.5);
          fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 4:
          modWheelLevel = ((value * 8) / 3);
          fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 5:
          modWheelLevel = ((value * 8) / 2.5);
          fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 6:
          modWheelLevel = ((value * 8) / 2);
          fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 7:
          modWheelLevel = ((value * 8) / 1.75);
          fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 8:
          modWheelLevel = ((value * 8) / 1.5);
          fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 9:
          modWheelLevel = ((value * 8) / 1.25);
          fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 10:
          modWheelLevel = (value * 8);
          fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;
      }
      break;

      ////////////////////////////////////////////////

    case CCfilterPoleSW:
      filterPoleSW = value;
      updatefilterPoleSwitch();
      break;

    case CCfilterEGinv:
      filterEGinv = value;
      updatefilterEGinv();
      break;

    case CCosc1BankA:
      osc1BankA = value;
      updateosc1Bank();
      break;

    case CCosc1BankB:
      osc1BankB = value;
      updateosc1Bank();
      break;

    case CCosc2BankA:
      osc2BankA = value;
      updateosc2Bank();
      break;

    case CCosc2BankB:
      osc2BankB = value;
      updateosc2Bank();
      break;

    case CCosc1OctA:
      oct1A = value;
      updateosc1Range();
      break;

    case CCosc1OctB:
      oct1B = value;
      updateosc1Range();
      break;

    case CCosc2OctA:
      oct2A = value;
      updateosc2Range();
      break;

    case CCosc2OctB:
      oct2B = value;
      updateosc2Range();
      break;

    case CClfoAlt:
      lfoAlt = value;
      updatelfoAlt();
      break;

    case CCfilterA:
      filterA = value;
      updateFilterType();
      break;

    case CCfilterB:
      filterB = value;
      updateFilterType();
      break;

    case CCfilterC:
      filterC = value;
      updateFilterType();
      break;

    case CCallnotesoff:
      allNotesOff();
      break;
  }
}

void myProgramChange(byte channel, byte program) {
  state = PATCH;
  patchNo = program + 1;
  recallPatch(patchNo);
  Serial.print("MIDI Pgm Change:");
  Serial.println(patchNo);
  state = PARAMETER;
}

void recallPatch(int patchNo) {
  allNotesOff();
  File patchFile = SD.open(String(patchNo).c_str());
  if (!patchFile) {
    Serial.println("File not found");
  } else {
    String data[NO_OF_PARAMS];  //Array of data read in
    recallPatchData(patchFile, data);
    setCurrentPatchData(data);
    patchFile.close();
    storeLastPatch(patchNo);
  }
}

void setCurrentPatchData(String data[]) {
  patchName = data[0];
  osc1Tune = data[1].toFloat();
  fmDepth = data[2].toFloat();
  osc2Tune = data[3].toFloat();
  osc1WaveMod = data[4].toFloat();
  osc2WaveMod = data[5].toFloat();
  osc1WaveSelect = data[6].toFloat();
  osc2WaveSelect = data[7].toFloat();
  glideTime = data[8].toFloat();
  filterLogLin = data[9].toInt();
  noiseLevel = data[10].toFloat();
  osc2Level = data[11].toFloat();
  osc1Level = data[12].toFloat();
  fmWaveDepth = data[13].toFloat();
  ampLogLin = data[14].toInt();
  filterCutoff = data[15].toFloat();
  filterLFO = data[16].toFloat();
  filterRes = data[17].toFloat();
  filterType = data[18].toFloat();
  filterA = data[19].toFloat();
  filterB = data[20].toFloat();
  filterC = data[21].toFloat();
  filterEGlevel = data[22].toFloat();
  LFORate = data[23].toFloat();
  LFOWaveform = data[24].toFloat();
  filterAttack = data[25].toFloat();
  filterDecay = data[26].toFloat();
  filterSustain = data[27].toFloat();
  filterRelease = data[28].toFloat();
  ampAttack = data[29].toFloat();
  ampDecay = data[30].toFloat();
  ampSustain = data[31].toFloat();
  ampRelease = data[32].toFloat();
  volumeControl = data[33].toFloat();
  osc1BankA = data[34].toFloat();
  keyTrack = data[35].toFloat();
  filterPoleSW = data[36].toInt();
  FilterLoop = data[37].toInt();
  filterEGinv = data[38].toInt();
  osc1BankB = data[39].toFloat();
  AmpLoop = data[40].toInt();
  osc2BankA = data[41].toFloat();
  osc2BankB = data[42].toFloat();
  lfoAlt = data[43].toInt();
  osc1WaveA = data[44].toInt();
  osc1WaveB = data[45].toInt();
  osc1WaveC = data[46].toInt();
  modWheelLevel = data[47].toFloat();
  PitchBendLevel = data[48].toFloat();
  oct1A = data[49].toFloat();
  oct1B = data[50].toFloat();
  oct2A = data[51].toFloat();
  oct2B = data[52].toFloat();
  osc2WaveA = data[53].toInt();
  osc2WaveB = data[54].toInt();
  osc2WaveC = data[55].toInt();
  AfterTouchDest = data[56].toInt();

  oldfilterCutoff = filterCutoff;

  //Switches

  updatefilterPoleSwitch();
  updatefilterEGinv();
  updatelfoAlt();
  updateosc1Range();
  updateosc2Range();
  updateosc1Bank();
  updateosc2Bank();
  updateosc1WaveSelect();
  updateosc2WaveSelect();
  updateFilterType();
  updateFilterEnv();
  updateAmpEnv();
  updatefilterLoop();
  updatevcaLoop();
  updateAfterTouchDest();

  //Patchname
  updatePatchname();

  Serial.print("Set Patch: ");
  Serial.println(patchName);
}

String getCurrentPatchData() {
  return patchName + "," + String(osc1Tune) + "," + String(fmDepth) + "," + String(osc2Tune) + "," + String(osc1WaveMod) + "," + String(osc2WaveMod) + "," + String(osc1WaveSelect) + "," + String(osc2WaveSelect) + "," + String(glideTime) + "," + String(filterLogLin) + "," + String(noiseLevel) + "," + String(osc2Level) + "," + String(osc1Level) + "," + String(fmWaveDepth) + "," + String(ampLogLin) + "," + String(filterCutoff) + "," + String(filterLFO) + "," + String(filterRes) + "," + String(filterType) + "," + String(filterA) + "," + String(filterB) + "," + String(filterC) + "," + String(filterEGlevel) + "," + String(LFORate) + "," + String(LFOWaveform) + "," + String(filterAttack) + "," + String(filterDecay) + "," + String(filterSustain) + "," + String(filterRelease) + "," + String(ampAttack) + "," + String(ampDecay) + "," + String(ampSustain) + "," + String(ampRelease) + "," + String(volumeControl) + "," + String(osc1BankA) + "," + String(keyTrack) + "," + String(filterPoleSW) + "," + String(FilterLoop) + "," + String(filterEGinv) + "," + String(osc1BankB) + "," + String(AmpLoop) + "," + String(osc2BankA) + "," + String(osc2BankB) + "," + String(lfoAlt) + "," + String(osc1WaveA) + "," + String(osc1WaveB) + "," + String(osc1WaveC) + "," + String(modWheelLevel) + "," + String(PitchBendLevel) + "," + String(oct1A) + "," + String(oct1B) + "," + String(oct2A) + "," + String(oct2B) + "," + String(osc2WaveA) + "," + String(osc2WaveB) + "," + String(osc2WaveC) + "," + String(AfterTouchDest);
}

void checkMux() {

  mux1Read = adc->adc0->analogRead(MUX1_S);
  mux2Read = adc->adc0->analogRead(MUX2_S);
  mux3Read = adc->adc0->analogRead(MUX3_S);
  mux4Read = adc->adc0->analogRead(MUX4_S);
  mux5Read = adc->adc0->analogRead(MUX5_S);
  mux6Read = adc->adc0->analogRead(MUX6_S);

  if (mux1Read > (mux1ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux1Read < (mux1ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux1ValuesPrev[muxInput] = mux1Read;

    switch (muxInput) {
      case MUX1_fmDepth:
        midiCCOut(CCfmDepth, int(mux1Read / 8));
        myControlChange(midiChannel, CCfmDepth, mux1Read);
        break;
      case MUX1_osc1Tune:
        midiCCOut(CCosc1Tune, int(mux1Read / 8));
        myControlChange(midiChannel, CCosc1Tune, mux1Read);
        break;
      case MUX1_LFORate:
        midiCCOut(CCLFORate, int(mux1Read / 8));
        myControlChange(midiChannel, CCLFORate, mux1Read);
        break;
      case MUX1_osc2Tune:
        midiCCOut(CCosc2Tune, int(mux1Read / 8));
        myControlChange(midiChannel, CCosc2Tune, mux1Read);
        break;
      case MUX1_LFOWaveform:
        midiCCOut(CCLFOWaveform, int(mux1Read / 8));
        myControlChange(midiChannel, CCLFOWaveform, mux1Read);
        break;
      case MUX1_osc2WaveMod:
        midiCCOut(CCosc2WaveMod, int(mux1Read / 8));
        myControlChange(midiChannel, CCosc2WaveMod, mux1Read);
        break;
      case MUX1_osc1WaveMod:
        midiCCOut(CCosc1WaveMod, int(mux1Read / 8));
        myControlChange(midiChannel, CCosc1WaveMod, mux1Read);
        break;
      case MUX1_osc1WaveSelect:
        midiCCOut(CCosc1WaveSelect, int(mux1Read / 8));
        myControlChange(midiChannel, CCosc1WaveSelect, mux1Read);
        break;
    }
  }

  if (mux2Read > (mux2ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux2Read < (mux2ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux2ValuesPrev[muxInput] = mux2Read;

    switch (muxInput) {
      case MUX2_filterCutoff:
        midiCCOut(CCfilterCutoff, int(mux2Read / 8));
        myControlChange(midiChannel, CCfilterCutoff, mux2Read);
        break;
      case MUX2_osc1level:
        midiCCOut(CCosc1Level, int(mux2Read / 8));
        myControlChange(midiChannel, CCosc1Level, mux2Read);
        break;
      case MUX2_fmWaveDepth:
        midiCCOut(CCfmWaveDepth, int(mux2Read / 8));
        myControlChange(midiChannel, CCfmWaveDepth, mux2Read);
        break;
      case MUX2_filterEGlevel:
        midiCCOut(CCfilterEGlevel, int(mux2Read / 8));
        myControlChange(midiChannel, CCfilterEGlevel, mux2Read);
        break;
      case MUX2_osc2WaveSelect:
        midiCCOut(CCosc2WaveSelect, int(mux2Read / 8));
        myControlChange(midiChannel, CCosc2WaveSelect, mux2Read);
        break;
      case MUX2_keyTrack:
        midiCCOut(CCKeyTrack, int(mux2Read / 8));
        myControlChange(midiChannel, CCKeyTrack, mux2Read);
        break;
      case MUX2_osc2level:
        midiCCOut(CCosc2Level, int(mux2Read / 8));
        myControlChange(midiChannel, CCosc2Level, mux2Read);
        break;
      case MUX2_noiseLevel:
        midiCCOut(CCnoiseLevel, int(mux2Read / 8));
        myControlChange(midiChannel, CCnoiseLevel, mux2Read);
        break;
    }
  }

  if (mux3Read > (mux3ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux3Read < (mux3ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux3ValuesPrev[muxInput] = mux3Read;

    switch (muxInput) {
      case MUX3_tmDepth:
        midiCCOut(CCfilterLFO, int(mux3Read / 8));
        myControlChange(midiChannel, CCfilterLFO, mux3Read);
        break;
      case MUX3_filterAttack:
        midiCCOut(CCfilterAttack, int(mux3Read / 8));
        myControlChange(midiChannel, CCfilterAttack, mux3Read);
        break;
      case MUX3_Resonance:
        midiCCOut(CCfilterRes, int(mux3Read / 8));
        myControlChange(midiChannel, CCfilterRes, mux3Read);
        break;
      case MUX3_filterDecay:
        midiCCOut(CCfilterDecay, int(mux3Read / 8));
        myControlChange(midiChannel, CCfilterDecay, mux3Read);
        break;
      case MUX3_ampAttack:
        midiCCOut(CCampAttack, int(mux3Read / 8));
        myControlChange(midiChannel, CCampAttack, mux3Read);
        break;
      case MUX3_ampDecay:
        midiCCOut(CCampDecay, int(mux3Read / 8));
        myControlChange(midiChannel, CCampDecay, mux3Read);
        break;
    }
  }

  if (mux4Read > (mux4ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux4Read < (mux4ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux4ValuesPrev[muxInput] = mux4Read;

    switch (muxInput) {
      case MUX4_filterSustain:
        midiCCOut(CCfilterSustain, int(mux4Read / 8));
        myControlChange(midiChannel, CCfilterSustain, mux4Read);
        break;
      case MUX4_ampRelease:
        midiCCOut(CCampRelease, int(mux4Read / 8));
        myControlChange(midiChannel, CCampRelease, mux4Read);
        break;
      case MUX4_ampSustain:
        midiCCOut(CCampSustain, int(mux4Read / 8));
        myControlChange(midiChannel, CCampSustain, mux4Read);
        break;
      case MUX4_filterRelease:
        midiCCOut(CCfilterRelease, int(mux4Read / 8));
        myControlChange(midiChannel, CCfilterRelease, mux4Read);
        break;
      case MUX4_glideTime:
        midiCCOut(CCglideTime, int(mux4Read / 8));
        myControlChange(midiChannel, CCglideTime, mux4Read);
        break;
      case MUX4_pbDepth:
        midiCCOut(CCPitchBend, int(mux4Read / 8));
        myControlChange(midiChannel, CCPitchBend, mux4Read);
        break;
      case MUX4_volumeControl:
        midiCCOut(CCvolumeControl, int(mux4Read / 8));
        myControlChange(midiChannel, CCvolumeControl, mux4Read);
        break;
    }
  }

  if (mux5Read > (mux5ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux5Read < (mux5ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux5ValuesPrev[muxInput] = mux5Read;

    switch (muxInput) {
      case MUX5_osc2Wave_B:
        midiCCOut(CCosc2BankB, int(mux5Read / 8));
        myControlChange(midiChannel, CCosc2BankB, mux5Read);
        break;
      case MUX5_osc2Wave_A:
        midiCCOut(CCosc2BankA, int(mux5Read / 8));
        myControlChange(midiChannel, CCosc2BankA, mux5Read);
        break;
      case MUX5_osc1Oct_B:
        midiCCOut(CCosc1OctB, int(mux5Read / 8));
        myControlChange(midiChannel, CCosc1OctB, mux5Read);
        break;
      case MUX5_osc2Oct_A:
        midiCCOut(CCosc2OctA, int(mux5Read / 8));
        myControlChange(midiChannel, CCosc2OctA, mux5Read);
        break;
      case MUX5_lfoAlt:
        midiCCOut(CClfoAlt, int(mux5Read / 8));
        myControlChange(midiChannel, CClfoAlt, mux5Read);
        break;
      case MUX5_osc1Oct_A:
        midiCCOut(CCosc1OctA, int(mux5Read / 8));
        myControlChange(midiChannel, CCosc1OctA, mux5Read);
        break;
      case MUX5_osc1Wave_A:
        midiCCOut(CCosc1BankA, int(mux5Read / 8));
        myControlChange(midiChannel, CCosc1BankA, mux5Read);
        break;
      case MUX5_osc1Wave_B:
        midiCCOut(CCosc1BankB, int(mux5Read / 8));
        myControlChange(midiChannel, CCosc1BankB, mux5Read);
        break;
    }
  }

  if (mux6Read > (mux6ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux6Read < (mux6ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux6ValuesPrev[muxInput] = mux6Read;

    switch (muxInput) {
      case MUX6_filter_B:
        midiCCOut(CCfilterB, int(mux6Read / 8));
        myControlChange(midiChannel, CCfilterB, mux6Read);
        break;
      case MUX6_filter_A:
        midiCCOut(CCfilterA, int(mux6Read / 8));
        myControlChange(midiChannel, CCfilterA, mux6Read);
        break;
      case MUX6_osc2Oct_B:
        midiCCOut(CCosc2OctB, int(mux6Read / 8));
        myControlChange(midiChannel, CCosc2OctB, mux6Read);
        break;
      case MUX6_filter_C:
        midiCCOut(CCfilterC, int(mux6Read / 8));
        myControlChange(midiChannel, CCfilterC, mux6Read);
        break;
      case MUX6_filterPole:
        midiCCOut(CCfilterPoleSW, int(mux6Read / 8));
        myControlChange(midiChannel, CCfilterPoleSW, mux6Read);
        break;
      case MUX6_EGInvert:
        midiCCOut(CCfilterEGinv, int(mux6Read / 8));
        myControlChange(midiChannel, CCfilterEGinv, mux6Read);
        break;
    }
  }

  muxInput++;
  if (muxInput >= MUXCHANNELS)
    muxInput = 0;

  digitalWriteFast(MUX_0, muxInput & B0001);
  digitalWriteFast(MUX_1, muxInput & B0010);
  digitalWriteFast(MUX_2, muxInput & B0100);
}

void midiCCOut(byte cc, byte value) {
  //usbMIDI.sendControlChange(cc, value, midiChannel); //MIDI DIN is set to Out
  //midi1.sendControlChange(cc, value, midiChannel);
  MIDI.sendControlChange(cc, value, midiChannel);  //MIDI DIN is set to Out
}

void writeDemux() {

  switch (muxOutput) {
    case 0:
      analogWrite(A21, int(glideTime));
      analogWrite(A22, int(filterLFO / FRIG2V));
      digitalWriteFast(DEMUX_EN_1, LOW);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
    case 1:
      analogWrite(A21, int(filterEGlevel / FRIG5V));
      analogWrite(A22, int(0));
      digitalWriteFast(DEMUX_EN_1, LOW);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
    case 2:  // 0-3.3v max
      analogWrite(A21, noiseLevel / FRIG2V);
      analogWrite(A22, osc1Level / FRIG5V);
      digitalWriteFast(DEMUX_EN_1, LOW);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
    case 3:  // 0-3.3v max
      analogWrite(A21, volumeControl / FRIG2V);
      analogWrite(A22, osc2Level / FRIG5V);
      digitalWriteFast(DEMUX_EN_1, LOW);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
    case 4:
      analogWrite(A21, int(LFORate / FRIG5V));
      analogWrite(A22, int(filterCutoff / FRIG5V));
      digitalWriteFast(DEMUX_EN_1, LOW);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
    case 5:
      analogWrite(A21, int(LFOWaveform / FRIG5V));
      analogWrite(A22, int(filterRes / FRIG5V));
      digitalWriteFast(DEMUX_EN_1, LOW);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
    case 6:  // 0-3.3v max
      analogWrite(A21, int(keyTrack / FRIG2V));
      analogWrite(A22, int(fmWaveDepth / FRIG2V));
      digitalWriteFast(DEMUX_EN_1, LOW);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
    case 7:  // 0-3.3v max
      analogWrite(A21, int(afterTouch / FRIG2V));
      analogWrite(A22, masterTune);
      digitalWriteFast(DEMUX_EN_1, LOW);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
    case 8:
      analogWrite(A21, int(filterAttack / FRIG5V));
      analogWrite(A22, int(ampAttack / FRIG5V));
      digitalWriteFast(DEMUX_EN_1, LOW);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
    case 9:
      analogWrite(A21, int(filterDecay / FRIG5V));
      analogWrite(A22, int(ampDecay / FRIG5V));
      digitalWriteFast(DEMUX_EN_1, LOW);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
    case 10:  // 0-3.3v max
      analogWrite(A21, int(fmDepth / FRIG2V));
      analogWrite(A22, int(0));
      digitalWriteFast(DEMUX_EN_1, LOW);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
    case 11:  // 0-3.3v max
      analogWrite(A21, int(PitchBendLevel / FRIG2V));
      analogWrite(A22, int(0));
      digitalWriteFast(DEMUX_EN_1, LOW);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
    case 12:
      analogWrite(A21, int(filterSustain / FRIG5V));
      analogWrite(A22, int(ampSustain / FRIG5V));
      digitalWriteFast(DEMUX_EN_1, LOW);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
    case 13:
      analogWrite(A21, int(filterRelease / FRIG5V));
      analogWrite(A22, int(ampRelease / FRIG5V));
      digitalWriteFast(DEMUX_EN_1, LOW);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
    case 14:  // 0-3.3v max
      analogWrite(A21, osc1Tune);
      analogWrite(A22, osc1WaveMod);
      digitalWriteFast(DEMUX_EN_1, LOW);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
    case 15:  // 0-3.3v max
      analogWrite(A21, osc2Tune);
      analogWrite(A22, osc2WaveMod);
      digitalWriteFast(DEMUX_EN_1, LOW);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
  }
  delayMicroseconds(400);
  digitalWriteFast(DEMUX_EN_1, HIGH);
  digitalWriteFast(DEMUX_EN_2, HIGH);

  muxOutput++;
  if (muxOutput >= DEMUXCHANNELS)
    muxOutput = 0;

  digitalWriteFast(DEMUX_0, muxOutput & B0001);
  digitalWriteFast(DEMUX_1, muxOutput & B0010);
  digitalWriteFast(DEMUX_2, muxOutput & B0100);
  digitalWriteFast(DEMUX_3, muxOutput & B1000);
}

void checkSwitches() {

  saveButton.update();
  if (saveButton.read() == LOW && saveButton.duration() > HOLD_DURATION) {
    switch (state) {
      case PARAMETER:
      case PATCH:
        state = DELETE;
        saveButton.write(HIGH);  //Come out of this state
        del = true;              //Hack
        break;
    }
  } else if (saveButton.risingEdge()) {
    if (!del) {
      switch (state) {
        case PARAMETER:
          if (patches.size() < PATCHES_LIMIT) {
            resetPatchesOrdering();  //Reset order of patches from first patch
            patches.push({ patches.size() + 1, INITPATCHNAME });
            state = SAVE;
          }
          break;
        case SAVE:
          //Save as new patch with INITIALPATCH name or overwrite existing keeping name - bypassing patch renaming
          patchName = patches.last().patchName;
          state = PATCH;
          savePatch(String(patches.last().patchNo).c_str(), getCurrentPatchData());
          showPatchPage(patches.last().patchNo, patches.last().patchName);
          patchNo = patches.last().patchNo;
          loadPatches();  //Get rid of pushed patch if it wasn't saved
          setPatchesOrdering(patchNo);
          renamedPatch = "";
          state = PARAMETER;
          break;
        case PATCHNAMING:
          if (renamedPatch.length() > 0) patchName = renamedPatch;  //Prevent empty strings
          state = PATCH;
          savePatch(String(patches.last().patchNo).c_str(), getCurrentPatchData());
          showPatchPage(patches.last().patchNo, patchName);
          patchNo = patches.last().patchNo;
          loadPatches();  //Get rid of pushed patch if it wasn't saved
          setPatchesOrdering(patchNo);
          renamedPatch = "";
          state = PARAMETER;
          break;
      }
    } else {
      del = false;
    }
  }

  settingsButton.update();
  if (settingsButton.read() == LOW && settingsButton.duration() > HOLD_DURATION) {
    //If recall held, set current patch to match current hardware state
    //Reinitialise all hardware values to force them to be re-read if different
    state = REINITIALISE;
    reinitialiseToPanel();
    settingsButton.write(HIGH);              //Come out of this state
    reini = true;                            //Hack
  } else if (settingsButton.risingEdge()) {  //cannot be fallingEdge because holding button won't work
    if (!reini) {
      switch (state) {
        case PARAMETER:
          settingsValueIndex = getCurrentIndex(settingsOptions.first().currentIndex);
          showSettingsPage(settingsOptions.first().option, settingsOptions.first().value[settingsValueIndex], SETTINGS);
          state = SETTINGS;
          break;
        case SETTINGS:
          settingsOptions.push(settingsOptions.shift());
          settingsValueIndex = getCurrentIndex(settingsOptions.first().currentIndex);
          showSettingsPage(settingsOptions.first().option, settingsOptions.first().value[settingsValueIndex], SETTINGS);
        case SETTINGSVALUE:
          //Same as pushing Recall - store current settings item and go back to options
          settingsHandler(settingsOptions.first().value[settingsValueIndex], settingsOptions.first().handler);
          showSettingsPage(settingsOptions.first().option, settingsOptions.first().value[settingsValueIndex], SETTINGS);
          state = SETTINGS;
          break;
      }
    } else {
      reini = false;
    }
  }

  backButton.update();
  if (backButton.read() == LOW && backButton.duration() > HOLD_DURATION) {
    //If Back button held, Panic - all notes off
    allNotesOff();
    backButton.write(HIGH);              //Come out of this state
    panic = true;                        //Hack
  } else if (backButton.risingEdge()) {  //cannot be fallingEdge because holding button won't work
    if (!panic) {
      switch (state) {
        case RECALL:
          setPatchesOrdering(patchNo);
          state = PARAMETER;
          break;
        case SAVE:
          renamedPatch = "";
          state = PARAMETER;
          loadPatches();  //Remove patch that was to be saved
          setPatchesOrdering(patchNo);
          break;
        case PATCHNAMING:
          charIndex = 0;
          renamedPatch = "";
          state = SAVE;
          break;
        case DELETE:
          setPatchesOrdering(patchNo);
          state = PARAMETER;
          break;
        case SETTINGS:
          state = PARAMETER;
          break;
        case SETTINGSVALUE:
          settingsValueIndex = getCurrentIndex(settingsOptions.first().currentIndex);
          showSettingsPage(settingsOptions.first().option, settingsOptions.first().value[settingsValueIndex], SETTINGS);
          state = SETTINGS;
          break;
      }
    } else {
      panic = false;
    }
  }

  //Encoder switch
  recallButton.update();
  if (recallButton.read() == LOW && recallButton.duration() > HOLD_DURATION) {
    //If Recall button held, return to current patch setting
    //which clears any changes made
    state = PATCH;
    //Recall the current patch
    patchNo = patches.first().patchNo;
    recallPatch(patchNo);
    state = PARAMETER;
    recallButton.write(HIGH);  //Come out of this state
    recall = true;             //Hack
  } else if (recallButton.risingEdge()) {
    if (!recall) {
      switch (state) {
        case PARAMETER:
          state = RECALL;  //show patch list
          break;
        case RECALL:
          state = PATCH;
          //Recall the current patch
          patchNo = patches.first().patchNo;
          recallPatch(patchNo);
          state = PARAMETER;
          break;
        case SAVE:
          showRenamingPage(patches.last().patchName);
          patchName = patches.last().patchName;
          state = PATCHNAMING;
          break;
        case PATCHNAMING:
          if (renamedPatch.length() < 13) {
            renamedPatch.concat(String(currentCharacter));
            charIndex = 0;
            currentCharacter = CHARACTERS[charIndex];
            showRenamingPage(renamedPatch);
          }
          break;
        case DELETE:
          //Don't delete final patch
          if (patches.size() > 1) {
            state = DELETEMSG;
            patchNo = patches.first().patchNo;     //PatchNo to delete from SD card
            patches.shift();                       //Remove patch from circular buffer
            deletePatch(String(patchNo).c_str());  //Delete from SD card
            loadPatches();                         //Repopulate circular buffer to start from lowest Patch No
            renumberPatchesOnSD();
            loadPatches();                      //Repopulate circular buffer again after delete
            patchNo = patches.first().patchNo;  //Go back to 1
            recallPatch(patchNo);               //Load first patch
          }
          state = PARAMETER;
          break;
        case SETTINGS:
          //Choose this option and allow value choice
          settingsValueIndex = getCurrentIndex(settingsOptions.first().currentIndex);
          showSettingsPage(settingsOptions.first().option, settingsOptions.first().value[settingsValueIndex], SETTINGSVALUE);
          state = SETTINGSVALUE;
          break;
        case SETTINGSVALUE:
          //Store current settings item and go back to options
          settingsHandler(settingsOptions.first().value[settingsValueIndex], settingsOptions.first().handler);
          showSettingsPage(settingsOptions.first().option, settingsOptions.first().value[settingsValueIndex], SETTINGS);
          state = SETTINGS;
          break;
      }
    } else {
      recall = false;
    }
  }
}

void reinitialiseToPanel() {
  //This sets the current patch to be the same as the current hardware panel state - all the pots
  //The four button controls stay the same state
  //This reinialises the previous hardware values to force a re-read
  muxInput = 0;
  for (int i = 0; i < MUXCHANNELS; i++) {
    mux1ValuesPrev[i] = RE_READ;
    mux2ValuesPrev[i] = RE_READ;
    mux3ValuesPrev[i] = RE_READ;
    mux4ValuesPrev[i] = RE_READ;
    mux5ValuesPrev[i] = RE_READ;
    mux6ValuesPrev[i] = RE_READ;
  }
  patchName = INITPATCHNAME;
  showPatchPage("Initial", "Panel Settings");
}

void checkEncoder() {
  //Encoder works with relative inc and dec values
  //Detent encoder goes up in 4 steps, hence +/-3

  long encRead = encoder.read();
  if ((encCW && encRead > encPrevious + 3) || (!encCW && encRead < encPrevious - 3)) {
    switch (state) {
      case PARAMETER:
        state = PATCH;
        patches.push(patches.shift());
        patchNo = patches.first().patchNo;
        recallPatch(patchNo);
        state = PARAMETER;
        break;
      case RECALL:
        patches.push(patches.shift());
        break;
      case SAVE:
        patches.push(patches.shift());
        break;
      case PATCHNAMING:
        if (charIndex == TOTALCHARS) charIndex = 0;  //Wrap around
        currentCharacter = CHARACTERS[charIndex++];
        showRenamingPage(renamedPatch + currentCharacter);
        break;
      case DELETE:
        patches.push(patches.shift());
        break;
      case SETTINGS:
        settingsOptions.push(settingsOptions.shift());
        settingsValueIndex = getCurrentIndex(settingsOptions.first().currentIndex);
        showSettingsPage(settingsOptions.first().option, settingsOptions.first().value[settingsValueIndex], SETTINGS);
        break;
      case SETTINGSVALUE:
        if (settingsOptions.first().value[settingsValueIndex + 1] != '\0')
          showSettingsPage(settingsOptions.first().option, settingsOptions.first().value[++settingsValueIndex], SETTINGSVALUE);
        break;
    }
    encPrevious = encRead;
  } else if ((encCW && encRead < encPrevious - 3) || (!encCW && encRead > encPrevious + 3)) {
    switch (state) {
      case PARAMETER:
        state = PATCH;
        patches.unshift(patches.pop());
        patchNo = patches.first().patchNo;
        recallPatch(patchNo);
        state = PARAMETER;
        break;
      case RECALL:
        patches.unshift(patches.pop());
        break;
      case SAVE:
        patches.unshift(patches.pop());
        break;
      case PATCHNAMING:
        if (charIndex == -1)
          charIndex = TOTALCHARS - 1;
        currentCharacter = CHARACTERS[charIndex--];
        showRenamingPage(renamedPatch + currentCharacter);
        break;
      case DELETE:
        patches.unshift(patches.pop());
        break;
      case SETTINGS:
        settingsOptions.unshift(settingsOptions.pop());
        settingsValueIndex = getCurrentIndex(settingsOptions.first().currentIndex);
        showSettingsPage(settingsOptions.first().option, settingsOptions.first().value[settingsValueIndex], SETTINGS);
        break;
      case SETTINGSVALUE:
        if (settingsValueIndex > 0)
          showSettingsPage(settingsOptions.first().option, settingsOptions.first().value[--settingsValueIndex], SETTINGSVALUE);
        break;
    }
    encPrevious = encRead;
  }
}

void checkForChanges() {

  if (filterLogLin != oldfilterLogLin) {
    if (filterLogLin == 1) {
      sr.set(FILTER_LIN_LOG, HIGH);
    } else {
      sr.set(FILTER_LIN_LOG, LOW);
    }
    oldfilterLogLin = filterLogLin;
  }

  if (ampLogLin != oldampLogLin) {
    if (ampLogLin == 1) {
      sr.set(AMP_LIN_LOG, HIGH);
    } else {
      sr.set(AMP_LIN_LOG, LOW);
    }
    oldampLogLin = ampLogLin;
  }

  if (midiChannel != oldmidiChannel) {
    switch (midiChannel) {
      case 0:
        sr.set(MIDI1, LOW);
        sr.set(MIDI2, LOW);
        sr.set(MIDI4, LOW);
        sr.set(MIDI8, LOW);
        break;

      case 1:
        sr.set(MIDI1, HIGH);
        sr.set(MIDI2, LOW);
        sr.set(MIDI4, LOW);
        sr.set(MIDI8, LOW);
        break;

      case 2:
        sr.set(MIDI1, LOW);
        sr.set(MIDI2, HIGH);
        sr.set(MIDI4, LOW);
        sr.set(MIDI8, LOW);
        break;

      case 3:
        sr.set(MIDI1, HIGH);
        sr.set(MIDI2, HIGH);
        sr.set(MIDI4, LOW);
        sr.set(MIDI8, LOW);
        break;

      case 4:
        sr.set(MIDI1, LOW);
        sr.set(MIDI2, LOW);
        sr.set(MIDI4, HIGH);
        sr.set(MIDI8, LOW);
        break;

      case 5:
        sr.set(MIDI1, HIGH);
        sr.set(MIDI2, LOW);
        sr.set(MIDI4, HIGH);
        sr.set(MIDI8, LOW);
        break;

      case 6:
        sr.set(MIDI1, LOW);
        sr.set(MIDI2, HIGH);
        sr.set(MIDI4, HIGH);
        sr.set(MIDI8, LOW);
        break;

      case 7:
        sr.set(MIDI1, HIGH);
        sr.set(MIDI2, HIGH);
        sr.set(MIDI4, HIGH);
        sr.set(MIDI8, LOW);
        break;

      case 8:
        sr.set(MIDI1, LOW);
        sr.set(MIDI2, LOW);
        sr.set(MIDI4, LOW);
        sr.set(MIDI8, HIGH);
        break;

      case 9:
        sr.set(MIDI1, HIGH);
        sr.set(MIDI2, LOW);
        sr.set(MIDI4, LOW);
        sr.set(MIDI8, HIGH);
        break;

      case 10:
        sr.set(MIDI1, LOW);
        sr.set(MIDI2, HIGH);
        sr.set(MIDI4, LOW);
        sr.set(MIDI8, HIGH);
        break;

      case 11:
        sr.set(MIDI1, HIGH);
        sr.set(MIDI2, HIGH);
        sr.set(MIDI4, LOW);
        sr.set(MIDI8, HIGH);
        break;

      case 12:
        sr.set(MIDI1, LOW);
        sr.set(MIDI2, LOW);
        sr.set(MIDI4, HIGH);
        sr.set(MIDI8, HIGH);
        break;

      case 13:
        sr.set(MIDI1, HIGH);
        sr.set(MIDI2, LOW);
        sr.set(MIDI4, HIGH);
        sr.set(MIDI8, HIGH);
        break;

      case 14:
        sr.set(MIDI1, LOW);
        sr.set(MIDI2, HIGH);
        sr.set(MIDI4, HIGH);
        sr.set(MIDI8, HIGH);
        break;

      case 15:
        sr.set(MIDI1, HIGH);
        sr.set(MIDI2, HIGH);
        sr.set(MIDI4, HIGH);
        sr.set(MIDI8, HIGH);
        break;
    }
    oldmidiChannel = midiChannel;
  }

  if (AfterTouchDest != oldAfterTouchDest) {
    switch (AfterTouchDest) {
      case 0:
        sr.set(AFTERTOUCH_A, LOW);
        sr.set(AFTERTOUCH_B, LOW);
        sr.set(AFTERTOUCH_C, LOW);
        break;

      case 1:
        sr.set(AFTERTOUCH_A, HIGH);
        sr.set(AFTERTOUCH_B, LOW);
        sr.set(AFTERTOUCH_C, LOW);
        break;

      case 2:
        sr.set(AFTERTOUCH_A, LOW);
        sr.set(AFTERTOUCH_B, HIGH);
        sr.set(AFTERTOUCH_C, LOW);
        break;

      case 3:
        sr.set(AFTERTOUCH_A, HIGH);
        sr.set(AFTERTOUCH_B, HIGH);
        sr.set(AFTERTOUCH_C, LOW);
        break;
    }
    oldAfterTouchDest = AfterTouchDest;
  }

  if (NotePriority != oldNotePriority) {
    switch (NotePriority) {
      case 0:
        sr.set(NOTEPRIORITYA0, HIGH);
        sr.set(NOTEPRIORITYA2, LOW);
        break;

      case 1:
        sr.set(NOTEPRIORITYA0, LOW);
        sr.set(NOTEPRIORITYA2, LOW);
        break;

      case 2:
        sr.set(NOTEPRIORITYA0, LOW);
        sr.set(NOTEPRIORITYA2, HIGH);
        break;
    }
    oldNotePriority = NotePriority;
  }

  if (FilterLoop != oldFilterLoop) {
    switch (FilterLoop) {
      case 0:
        sr.set(FLOOPBIT0, LOW);
        sr.set(FLOOPBIT1, LOW);
        break;

      case 1:
        sr.set(FLOOPBIT0, HIGH);
        sr.set(FLOOPBIT1, LOW);
        break;

      case 2:
        sr.set(FLOOPBIT0, HIGH);
        sr.set(FLOOPBIT1, HIGH);
        break;
    }
    oldFilterLoop = FilterLoop;
  }

  // Read FilterLoop from EEPROM

  if (AmpLoop != oldAmpLoop) {
    switch (AmpLoop) {
      case 0:
        sr.set(ALOOPBIT0, LOW);
        sr.set(ALOOPBIT1, LOW);
        break;

      case 1:
        sr.set(ALOOPBIT0, HIGH);
        sr.set(ALOOPBIT1, LOW);
        break;

      case 2:
        sr.set(ALOOPBIT0, HIGH);
        sr.set(ALOOPBIT1, HIGH);
        break;
    }
    oldAmpLoop = AmpLoop;
  }

  if (ClockSource != oldClockSource) {
    switch (ClockSource) {
      case 0:
        sr.set(EXTCLOCK, LOW);
        sr.set(MIDICLOCK, LOW);
        break;

      case 1:
        sr.set(EXTCLOCK, HIGH);
        sr.set(MIDICLOCK, LOW);
        break;

      case 2:
        sr.set(EXTCLOCK, LOW);
        sr.set(MIDICLOCK, HIGH);
        break;
    }
    oldClockSource = ClockSource;
  }

  if (afterTouchDepth != oldafterTouchDepth) {
    switch (afterTouchDepth) {
      case 0:
        afterTouch = 0;
        break;

      case 1:
        afterTouch = 164;
        break;

      case 2:
        afterTouch = 328;
        break;

      case 3:
        afterTouch = 492;
        break;

      case 4:
        afterTouch = 656;
        break;

      case 5:
        afterTouch = 840;
        break;

      case 6:
        afterTouch = 1024;
        break;

      case 7:
        afterTouch = 1188;
        break;

      case 8:
        afterTouch = 1352;

      case 9:
        afterTouch = 1536;
        break;

      case 10:
        afterTouch = 1720;
        break;
    }
    oldafterTouchDepth = afterTouchDepth;
  }
}

void loop() {
  checkSwitches();
  checkForChanges();
  writeDemux();
  checkMux();
  checkEncoder();
  MIDI.read(midiChannel);
  usbMIDI.read(midiChannel);
}