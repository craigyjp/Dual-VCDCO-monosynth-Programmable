#define SETTINGSOPTIONSNO 11
#define SETTINGSVALUESNO 17  //Maximum number of settings option values needed
int settingsValueIndex = 0;  //currently selected settings option value index

struct SettingsOption {
  char *option;                   //Settings option string
  char *value[SETTINGSVALUESNO];  //Array of strings of settings option values
  int handler;                    //Function to handle the values for this settings option
  int currentIndex;               //Function to array index of current value for this settings option
};

void settingsMIDICh(char *value);
void settingsEncoderDir(char *value);
void settingsModWheelDepth(char *value);
void settingsAfterTouch(char *value);
void settingsafterTouchDepth(char *value);
void settingsFilterEnv(char *value);
void settingsAmpEnv(char *value);
void settingsNotePriority(char *value);
void settingsFilterLoop(char *value);
void settingsAmpLoop(char *value);
void settingsClockSource(char *value);
void settingsHandler(char *s, void (*f)(char *));

int currentIndexMIDICh();
int currentIndexEncoderDir();
int currentIndexModWheelDepth();
int currentIndexAfterTouch();
int currentIndexafterTouchDepth();
int currentIndexFilterEnv();
int currentIndexAmpEnv();
int currentIndexNotePriority();
int currentIndexFilterLoop();
int currentIndexAmpLoop();
int currentIndexClockSource();
int getCurrentIndex(int (*f)());


void settingsMIDICh(char *value) {
  if (strcmp(value, "ALL") == 0) {
    midiChannel = MIDI_CHANNEL_OMNI;
  } else {
    midiChannel = atoi(value);
  }
  storeMidiChannel(midiChannel);
}

void settingsEncoderDir(char *value) {
  if (strcmp(value, "Type 1") == 0) {
    encCW = true;
  } else {
    encCW = false;
  }
  storeEncoderDir(encCW ? 1 : 0);
}

void settingsAfterTouch(char *value) {
  if (strcmp(value, "Off") == 0) AfterTouchDest = 0;
  if (strcmp(value, "DCO Mod") == 0) AfterTouchDest = 1;
  if (strcmp(value, "CutOff Freq") == 0) AfterTouchDest = 2;
  if (strcmp(value, "VCF Mod") == 0) AfterTouchDest = 3;
  storeAfterTouch(AfterTouchDest);
}

void settingsModWheelDepth(char *value) {
  modWheelDepth = atoi(value);
  storeModWheelDepth(modWheelDepth);
}

void settingsNotePriority(char *value) {
  if (strcmp(value, "Top Note") == 0) NotePriority = 0;
  if (strcmp(value, "Last Note") == 0) NotePriority = 1;
  if (strcmp(value, "Bottom Note") == 0) NotePriority = 2;
  storeNotePriority(NotePriority);
}

void settingsFilterLoop(char *value) {
  if (strcmp(value, "Normal ADSR") == 0) FilterLoop = 0;
  if (strcmp(value, "Gated Looping") == 0) FilterLoop = 1;
  if (strcmp(value, "LFO Looping") == 0) FilterLoop = 2;
  storeFilterLoop(FilterLoop);
}

void settingsAmpLoop(char *value) {
  if (strcmp(value, "Normal ADSR") == 0) AmpLoop = 0;
  if (strcmp(value, "Gated Looping") == 0) AmpLoop = 1;
  if (strcmp(value, "LFO Looping") == 0) AmpLoop = 2;
  storeAmpLoop(AmpLoop);
}

void settingsClockSource(char *value) {
  if (strcmp(value, "Internal") == 0) ClockSource = 0;
  if (strcmp(value, "External") == 0) ClockSource = 1;
  if (strcmp(value, "MIDI Clk") == 0) ClockSource = 2;
  storeClockSource(ClockSource);
}

void settingsafterTouchDepth(char *value) {
  if (strcmp(value, "Off") == 0) {
    afterTouchDepth = 0;
  } else {
    afterTouchDepth = atoi(value);
  }
  storeafterTouchDepth(afterTouchDepth);
}

void settingsFilterEnv(char *value) {
  if (strcmp(value, "Logarithmic") == 0) {
    filterLogLin = true;
  } else {
    filterLogLin = false;
  }
  storeFilterEnv(filterLogLin ? 1 : 0);
}

void settingsAmpEnv(char *value) {
  if (strcmp(value, "Logarithmic") == 0) {
    ampLogLin = true;
  } else {
    ampLogLin = false;
  }
  storeAmpEnv(ampLogLin ? 1 : 0);
}

//Takes a pointer to a specific method for the settings option and invokes it.
void settingsHandler(char *s, void (*f)(char *)) {
  f(s);
}

int currentIndexMIDICh() {
  return getMIDIChannel();
}

int currentIndexEncoderDir() {
  return getEncoderDir() ? 0 : 1;
}

int currentIndexModWheelDepth() {
  return (getModWheelDepth()) - 1;
}

int currentIndexAfterTouch() {
  return getAfterTouch();
}

int currentIndexNotePriority() {
  return getNotePriority();
}

int currentIndexFilterLoop() {
  return getFilterLoop();
}

int currentIndexAmpLoop() {
  return getAmpLoop();
}

int currentIndexClockSource() {
  return getClockSource();
}

int currentIndexafterTouchDepth() {
  return getafterTouchDepth();
}

int currentIndexFilterEnv() {
  return getFilterEnv() ? 0 : 1;
}

int currentIndexAmpEnv() {
  return getAmpEnv() ? 0 : 1;
}


//Takes a pointer to a specific method for the current settings option value and invokes it.
int getCurrentIndex(int (*f)()) {
  return f();
}

CircularBuffer<SettingsOption, SETTINGSOPTIONSNO> settingsOptions;

// add settings to the circular buffer
void setUpSettings() {
  settingsOptions.push(SettingsOption{ "MIDI Ch.", { "All", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", '\0' }, settingsMIDICh, currentIndexMIDICh });
  settingsOptions.push(SettingsOption{ "Encoder", { "Type 1", "Type 2", '\0' }, settingsEncoderDir, currentIndexEncoderDir });
  settingsOptions.push(SettingsOption{ "MW Depth", { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", '\0' }, settingsModWheelDepth, currentIndexModWheelDepth });
  settingsOptions.push(SettingsOption{ "AT Destination", { "Off", "DCO Mod", "CutOff Freq", "VCF Mod", '\0' }, settingsAfterTouch, currentIndexAfterTouch });
  settingsOptions.push(SettingsOption{ "AT Depth", { "Off", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", '\0' }, settingsafterTouchDepth, currentIndexafterTouchDepth });
  settingsOptions.push(SettingsOption{ "Filter Env", { "Logarithmic", "Linear", '\0' }, settingsFilterEnv, currentIndexFilterEnv });
  settingsOptions.push(SettingsOption{ "Amplifier Env", { "Logarithmic", "Linear", '\0' }, settingsAmpEnv, currentIndexAmpEnv });
  settingsOptions.push(SettingsOption{ "Note Priority", { "Top Note", "Last Note", "Bottom Note", '\0' }, settingsNotePriority, currentIndexNotePriority });
  settingsOptions.push(SettingsOption{ "Filter ADSR", { "Normal ADSR", "Gated Looping", "LFO Looping", '\0' }, settingsFilterLoop, currentIndexFilterLoop });
  settingsOptions.push(SettingsOption{ "Amp ADSR", { "Normal ADSR", "Gated Looping", "LFO Looping", '\0' }, settingsAmpLoop, currentIndexAmpLoop });
  settingsOptions.push(SettingsOption{ "LFO Clock", { "Internal", "External", "MIDI Clk", '\0' }, settingsClockSource, currentIndexClockSource });
}