/* Configuration, you can adjust this via serial */

#define EEPROM_ID "PPS"
#define EEPROM_V 1

#define D_V0A 512                   // Tune this value to Analog read when no current flowing
#define D_GAIN 66                   // Hall efect sensor mVolt to amp relation
#define D_START_SPAN 500            // Time to ignore current spikes due to motor start
#define MAX_CURRENT 12              // Max current in Amps, to detect jams
#define D_MAX_JAMS 3                // Max amount of jams in a set time
#define D_MIN_JAM_TIME 15000        // That time in milliseconds
#define E_UNJAM_REVERSE_T 3000      // Retraction time to unjam

#define A_2_AREAD(c) (runConf.v0A + (c/runConf.AnalogR2A))
#define AREAD_2_A(c) ((c - runConf.v0A)*runConf.AnalogR2A)
#define A_GAIN(g) (5.0 / (1024 * (g/1000.0)))

struct ShredderConf {
  int v0A;
  float AnalogR2A;
  int startSpan;
  int maxCurrent;   // this is the value over which the hall sensor signal will register as a jam.
  int maxJams;
  int minJamTime;
  int unjamReverseT;
};
ShredderConf runConf;
