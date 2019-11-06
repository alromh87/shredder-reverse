#ifndef POS3_H
#define POS3_H

#include <Bounce2.h>

class Pos3
{
public:
   Pos3(int upPin, int downPin);

   int setup();
   int loop();

protected:
   int upPin;
   int downPin;

   int last_switch = -1; // Track last switch position
   int switch_pos = -1;  // Current switch position

   Bounce debouncerUp;
   Bounce debouncerDown;

private:
   int read();
};

#endif
