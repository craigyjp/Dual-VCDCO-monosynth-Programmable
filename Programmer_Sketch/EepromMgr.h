#include <EEPROM.h>

#define EEPROM_MIDI_CH 0
#define EEPROM_AFTERTOUCH 1
#define EEPROM_PITCHBEND 2
#define EEPROM_AFTERTOUCH_DEPTH 3
#define EEPROM_ENCODER_DIR 4
#define EEPROM_LAST_PATCH 5
#define EEPROM_FILTERENV 6
#define EEPROM_AMPENV 7
#define EEPROM_NOTEPRIORITY 8
#define EEPROM_FILTERLOOP 9
#define EEPROM_AMPLOOP 10
#define EEPROM_CLOCKSOURCE 11
#define EEPROM_MODWHEEL_DEPTH 12

int getMIDIChannel() {
  byte midiChannel = EEPROM.read(EEPROM_MIDI_CH);
  if (midiChannel < 0 || midiChannel > 16) midiChannel = MIDI_CHANNEL_OMNI;//If EEPROM has no MIDI channel stored
  return midiChannel;
}

void storeMidiChannel(byte channel)
{
  EEPROM.update(EEPROM_MIDI_CH, channel);
}

float getAfterTouch() {
  byte AfterTouchDest = EEPROM.read(EEPROM_AFTERTOUCH);
  if (AfterTouchDest < 0  || AfterTouchDest > 7) return 0;
  return AfterTouchDest; //If EEPROM has no key tracking stored
}

void storeAfterTouch(byte AfterTouchDest)
{
  EEPROM.update(EEPROM_AFTERTOUCH, AfterTouchDest);
}

float getModWheelDepth() {
  int mw = EEPROM.read(EEPROM_MODWHEEL_DEPTH);
  if (mw < 1 || mw > 10) return modWheelDepth; //If EEPROM has no mod wheel depth stored
  return mw;
}

void storeModWheelDepth(float mwDepth)
{
  int mw =  mwDepth;
  EEPROM.update(EEPROM_MODWHEEL_DEPTH, mw);
}

boolean getEncoderDir() {
  byte ed = EEPROM.read(EEPROM_ENCODER_DIR); 
  if (ed < 0 || ed > 1)return true; //If EEPROM has no encoder direction stored
  return ed == 1 ? true : false;
}

void storeEncoderDir(byte encoderDir)
{
  EEPROM.update(EEPROM_ENCODER_DIR, encoderDir);
}

float getNotePriority() {
  byte NotePriority = EEPROM.read(EEPROM_NOTEPRIORITY);
  if (NotePriority < 0  || NotePriority > 2) return 1;
  return NotePriority;
}

void storeNotePriority(byte NotePriority)
{
  EEPROM.update(EEPROM_NOTEPRIORITY, NotePriority);
}

float getFilterLoop() {
  byte FilterLoop = EEPROM.read(EEPROM_FILTERLOOP);
  if (FilterLoop < 0  || FilterLoop > 2) return 0;
  return FilterLoop;
}

void storeFilterLoop(byte FilterLoop)
{
  EEPROM.update(EEPROM_FILTERLOOP, FilterLoop);
}

float getAmpLoop() {
  byte AmpLoop = EEPROM.read(EEPROM_AMPLOOP);
  if (AmpLoop < 0  || AmpLoop > 2) return 0;
  return AmpLoop;
}

void storeAmpLoop(byte AmpLoop)
{
  EEPROM.update(EEPROM_AMPLOOP, AmpLoop);
}

float getClockSource() {
  byte ClockSource = EEPROM.read(EEPROM_CLOCKSOURCE);
  if (ClockSource < 0  || ClockSource > 2) return 0;
  return ClockSource;
}

void storeClockSource(byte ClockSource)
{
  EEPROM.update(EEPROM_CLOCKSOURCE, ClockSource);
}

float getafterTouchDepth() {
  byte afterTouchDepth = EEPROM.read(EEPROM_AFTERTOUCH_DEPTH);
  if (afterTouchDepth < 0 || afterTouchDepth > 10) return 0; //If EEPROM has no mod wheel depth stored
  return afterTouchDepth;
}

void storeafterTouchDepth(byte afterTouchDepth)
{
  EEPROM.update(EEPROM_AFTERTOUCH_DEPTH, afterTouchDepth);
}

boolean getFilterEnv() {
  byte fenv = EEPROM.read(EEPROM_FILTERENV);
  if (fenv < 0 || fenv > 1)return true;
  return fenv == 0 ? false : true;
}

void storeFilterEnv(byte filterLogLin)
{
  EEPROM.update(EEPROM_FILTERENV, filterLogLin);
}

boolean getAmpEnv() {
  byte aenv = EEPROM.read(EEPROM_AMPENV);
  if (aenv < 0 || aenv > 1)return true;
  return aenv == 0 ? false : true;
}

void storeAmpEnv(byte ampLogLin)
{
  EEPROM.update(EEPROM_AMPENV, ampLogLin);
}

int getLastPatch() {
  int lastPatchNumber = EEPROM.read(EEPROM_LAST_PATCH);
  if (lastPatchNumber < 1 || lastPatchNumber > 999) lastPatchNumber = 1;
  return lastPatchNumber;
}

void storeLastPatch(int lastPatchNumber)
{
  EEPROM.update(EEPROM_LAST_PATCH, lastPatchNumber);
}
