#ifndef ROUTINE_SWITCHER_H
#define ROUTINE_SWITCHER_H

// We need to move through the routines in a fixed but variable-looking order
// so that each arduino stays in sync. This encapsulates the order to keep it out
// of the main file.
//
// Currently assumes that there are 7 different routines. 
class Routine_switcher
{
public:
  Routine_switcher() : cnt(0), dir(1) {}
  
  // prints to the serial monitor
  void test()
  {
    for ( int i = 0; i < 200; ++i )
    {
      advance();
      Serial.println( active_routine() );
    }
  }

  // go to the next routine
  void advance()
  {
    cnt += dir;
    if ( cnt == SIZE-1 && dir == 1 )
    {
      dir = -1;
    }
    else if ( cnt == 0 && dir == -1 )
    {
      dir = 1;
    }
  }
  
  // Corresponds to a number in the switch statement in the main loop
  byte active_routine()
  {
    return abs( order[cnt] ) - 1;
  }

  // Is it traveling toward the ground?
  bool traveling_down()
  {
    return cnt < 0;
  }

private:
  static const byte SIZE = 36;
  // sign determines direction of travel
  const signed char order[SIZE] = { 
    -1, -2, 3, 4, -5, 6, 7, -8, -9,
    2, -7, 9, -4, 8, 1, -3, -6, 5, 
    -1, -8, 4, -2, -5, -9, 7, 3, 6,
    5, 9, -3, 1, -6, 8, 2, -4, -7
  };
  signed char cnt;
  signed char dir;
};


#endif
