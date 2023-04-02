// This optional setting causes Encoder to use more optimized code,
// It must be defined before Encoder.h is included.
#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>
#include <Bounce.h>
#include <ADC.h>
#include <ADC_util.h>

ADC *adc = new ADC();

//Teensy 3.6 - Mux Pins
#define MUX_0 36
#define MUX_1 35
#define MUX_2 34

#define MUX1_S A0
#define MUX2_S A1
#define MUX3_S A2
#define MUX4_S A3
#define MUX5_S A5
#define MUX6_S A4

#define DEMUX_0 29
#define DEMUX_1 30
#define DEMUX_2 31
#define DEMUX_3 32

#define DEMUX_EN_1 33
#define DEMUX_EN_2 37

#define FRIG5V 1.32
#define FRIG2V 1.65

//Mux 1 Connections
#define MUX1_fmDepth 0
#define MUX1_osc1Tune 1
#define MUX1_LFORate 2
#define MUX1_osc2Tune 3
#define MUX1_LFOWaveform 4
#define MUX1_osc2WaveMod 5
#define MUX1_osc1WaveMod 6
#define MUX1_osc1WaveSelect 7

//Mux 2 Connections
#define MUX2_filterCutoff 0
#define MUX2_osc1level 1
#define MUX2_fmWaveDepth 2
#define MUX2_filterEGlevel 3
#define MUX2_osc2WaveSelect 4
#define MUX2_keyTrack 5 // spare mux output - decouple from DAC
#define MUX2_osc2level 6
#define MUX2_noiseLevel 7 // spare mux output

//Mux 3 Connections
#define MUX3_tmDepth 0
#define MUX3_filterAttack 1
#define MUX3_Resonance 2
#define MUX3_filterDecay 3
#define MUX3_ampAttack 4
#define MUX3_5 5
#define MUX3_ampDecay 6
#define MUX3_7 7

//Mux 4 Connections
#define MUX4_filterSustain 0
#define MUX4_ampRelease 1
#define MUX4_ampSustain 2
#define MUX4_filterRelease 3
#define MUX4_glideTime 4
#define MUX4_5 5
#define MUX4_pbDepth 6
#define MUX4_volumeControl 7

//Switches 
//Mux 5 Connections
#define MUX5_osc2Wave_B 0
#define MUX5_osc2Wave_A 1
#define MUX5_osc1Oct_B 2
#define MUX5_osc2Oct_A 3
#define MUX5_lfoAlt 4
#define MUX5_osc1Oct_A 5 // spare mux output - decouple from DAC
#define MUX5_osc1Wave_A 6
#define MUX5_osc1Wave_B 7 // spare mux output

//Mux 6 Connections
#define MUX6_filter_B 0
#define MUX6_filter_A 1
#define MUX6_osc2Oct_B 2
#define MUX6_filter_C 3
#define MUX6_filterPole 4
#define MUX6_5 5
#define MUX6_EGInvert 6
#define MUX6_7 7

//DeMux 1 Connections
#define DEMUX1_glide      0               // 0-10v
#define DEMUX1_EGDepth 1                  // 0-5v
#define DEMUX1_spare2 2                   // 0-3.3v
#define DEMUX1_spare3 3                   // 0-3.3v
#define DEMUX1_LFORate 4                  // 0-5v
#define DEMUX1_LFOWave 5                  // 0-5v
#define DEMUX1_spare6 6                   // 0-3.3v
#define DEMUX1_spare7 7                   // 0-3.3v
#define DEMUX1_filterAttack 8             // 0-5v
#define DEMUX1_filterDecay 9              // 0-5v
#define DEMUX1_spare10 10                 // 0-3.3v
#define DEMUX1_spare11 11                 // 0-3.3v
#define DEMUX1_filterSustain 12           // 0-5v
#define DEMUX1_filteRelease 13            // 0-5v
#define DEMUX1_spare14 14                 // 0-3.3v
#define DEMUX1_spare15 15                 // 0-3.3v

//DeMux 2 Connections
#define DEMUX2_spare0 0                   // 0-3.3v
#define DEMUX2_spare1 1                   // 0-3.3v
#define DEMUX2_spare2 2                   // 0-3.3v
#define DEMUX2_spare3 3                   // 0-3.3v
#define DEMUX2_filterCutoff 4             // 0-5v
#define DEMUX2_filterRes 5                // 0-5v
#define DEMUX2_spare6 6                   // 0-3.3v
#define DEMUX2_spare7 7                   // 0-3.3v
#define DEMUX2_ampAttack 8                // 0-5v
#define DEMUX2_AmpDecay 9                 // 0-5v
#define DEMUX2_spare10 10                 // 0-3.3v
#define DEMUX2_spare11 11                 // 0-3.3v
#define DEMUX2_ampSustain 12              // 0-5v 
#define DEMUX2_ampRelease 13              // 0-5v 
#define DEMUX2_spare14 14                 // 0-3.3v
#define DEMUX2_spare15 15                 // 0-3.3v

// New 595 outputs

#define MIDI1 0
#define MIDI2 1
#define MIDI4 2
#define MIDI8 3
#define FILTER_LIN_LOG 4
#define AMP_LIN_LOG 5
#define FLOOPBIT1 6
#define FLOOPBIT0 7

#define NOTEPRIORITYA0 8
#define NOTEPRIORITYA2 9
#define ALOOPBIT1 10
#define ALOOPBIT0 11
#define EXTCLOCK 12
#define MIDICLOCK 13
#define AFTERTOUCH_A 14
#define AFTERTOUCH_B 15

#define AFTERTOUCH_C 16
#define FILTER_POLE 17
#define FILTER_EG_INV 18
#define LFO_ALT 19
#define OSC1_OCTA 20  //3.3v
#define OSC1_OCTB 21  //3.3v
#define OSC2_OCTA 22  //3.3v
#define OSC2_OCTB 23  //3.3v

#define OSC1_BANKA 24  //3.3v
#define OSC1_BANKB 25  //3.3v
#define OSC2_BANKA 26  //3.3v
#define OSC2_BANKB 27  //3.3v
#define FILTER_A 28
#define FILTER_B 29
#define FILTER_C 30
//#define SPARE 31

#define OSC1_WAVEA 32  //3.3v
#define OSC1_WAVEB 33  //3.3v
#define OSC1_WAVEC 34  //3.3v
#define OSC2_WAVEA 35  //3.3v
#define OSC2_WAVEB 36  //3.3v
#define OSC2_WAVEC 37  //3.3v
//#define SPARE 38
//#define SPARE 39

// System buttons

#define RECALL_SW 6
#define SAVE_SW 24
#define SETTINGS_SW 12
#define BACK_SW 10

#define ENCODER_PINA 4
#define ENCODER_PINB 5

#define MUXCHANNELS 8
#define DEMUXCHANNELS 16
#define QUANTISE_FACTOR 7

#define DEBOUNCE 30

static byte muxInput = 0;
static byte muxOutput = 0;

static int mux1ValuesPrev[MUXCHANNELS] = {};
static int mux2ValuesPrev[MUXCHANNELS] = {};
static int mux3ValuesPrev[MUXCHANNELS] = {};
static int mux4ValuesPrev[MUXCHANNELS] = {};
static int mux5ValuesPrev[MUXCHANNELS] = {};
static int mux6ValuesPrev[MUXCHANNELS] = {};

static int mux1Read = 0;
static int mux2Read = 0;
static int mux3Read = 0;
static int mux4Read = 0;
static int mux5Read = 0;
static int mux6Read = 0;

static long encPrevious = 0;

Bounce recallButton = Bounce(RECALL_SW, DEBOUNCE); //On encoder
boolean recall = true; //Hack for recall button
Bounce saveButton = Bounce(SAVE_SW, DEBOUNCE);
boolean del = true; //Hack for save button
Bounce settingsButton = Bounce(SETTINGS_SW, DEBOUNCE);
boolean reini = true; //Hack for settings button
Bounce backButton = Bounce(BACK_SW, DEBOUNCE);
boolean panic = true; //Hack for back button
Encoder encoder(ENCODER_PINB, ENCODER_PINA);//This often needs the pins swapping depending on the encoder

void setupHardware()
{
  //Volume Pot is on ADC0
  adc->adc0->setAveraging(16); // set number of averages 0, 4, 8, 16 or 32.
  adc->adc0->setResolution(10); // set bits of resolution  8, 10, 12 or 16 bits.
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_LOW_SPEED); // change the conversion speed
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED); // change the sampling speed

  //MUXs on ADC1
  adc->adc1->setAveraging(16); // set number of averages 0, 4, 8, 16 or 32.
  adc->adc1->setResolution(10); // set bits of resolution  8, 10, 12 or 16 bits.
  adc->adc1->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_LOW_SPEED); // change the conversion speed
  adc->adc1->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED); // change the sampling speed


  //Mux address pins

  pinMode(MUX_0, OUTPUT);
  pinMode(MUX_1, OUTPUT);
  pinMode(MUX_2, OUTPUT);

  pinMode(DEMUX_0, OUTPUT);
  pinMode(DEMUX_1, OUTPUT);
  pinMode(DEMUX_2, OUTPUT);
  pinMode(DEMUX_3, OUTPUT);

  digitalWrite(MUX_0, LOW);
  digitalWrite(MUX_1, LOW);
  digitalWrite(MUX_2, LOW);

  digitalWrite(DEMUX_0, LOW);
  digitalWrite(DEMUX_1, LOW);
  digitalWrite(DEMUX_2, LOW);
  digitalWrite(DEMUX_3, LOW);

  pinMode(DEMUX_EN_1, OUTPUT);
  pinMode(DEMUX_EN_2, OUTPUT);

  digitalWrite(DEMUX_EN_1, HIGH);
  digitalWrite(DEMUX_EN_2, HIGH);

  analogWriteResolution(10);
  analogReadResolution(10);


  //Switches

  pinMode(RECALL_SW, INPUT_PULLUP); //On encoder
  pinMode(SAVE_SW, INPUT_PULLUP);
  pinMode(SETTINGS_SW, INPUT_PULLUP);
  pinMode(BACK_SW, INPUT_PULLUP);

  
}
