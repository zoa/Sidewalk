#include "SPI.h"
#include "Zoa_WS2801.h"
#include "Sine_generator.h"
#include "MsTimer2.h"
#include "Waveform_utilities.h"
#include "Routine_switcher.h"


//////// Globals //////////

#define dataPin 2  // Yellow wire on Adafruit Pixels
#define clockPin 3   // Green wire on Adafruit Pixels
#define switchPin 4
#define stripLen 20

const byte update_frequency = 30; // how often to update the LEDs
volatile unsigned long int interrupt_counter; // updates every time the interrupt timer overflows
unsigned long int prev_interrupt_counter; // the main loop uses this to detect when the interrupt counter has changed 

// track button-presses
unsigned long int last_button_press;
const unsigned long int DEBOUNCE_INTERVAL = 0;//1000;
const byte MULTIPLIER = 3; // how much to speed up the button when pressed (not currently in use)

unsigned long int switch_after; // swap routines after this many milliseconds
unsigned int active_routine; // matches the #s from the switch statement in the main loop
rgbInfo_t (*get_next_color)(); // pointer to current led-updating function within this sketch

// pointer to a function in the Zoa_WS2801 library that takes a color argument. The update functions in this sketch use this
// pointer to decide whether to call pushBack, pushFront or setAll.
void (Zoa_WS2801::* library_update)(rgbInfo_t); 

// Set the first variable to the NUMBER of pixels. 25 = 25 pixels in a row
Zoa_WS2801 strip = Zoa_WS2801(stripLen, dataPin, clockPin, WS2801_GRB);

// Pointers to some waveform objects - currently they're reallocated each time the routine changes
#define WAVES 6
Waveform_generator* waves[WAVES]={};

White_noise_generator twinkles( 255, 255, 5, 8, 0 );

Routine_switcher order;
byte startle_counter;

float fade_fraction; // global variable in [0-1] range used for monitoring where we are in the button fade-in/fade-out
int fade_time; // milliseconds
float fade_step; // fade step size

boolean transitioning = false;

//////// Setup //////////

void setup()
{
  Serial.begin(9600);
  strip.begin();
  strip.setAll(rgbInfo_t(0,0,0));
  
  // initialize button
  pinMode(switchPin, INPUT);
  digitalWrite(switchPin, HIGH); // sets internal pull-up resistor to keep pin high when button is unpressed
  
  switch_after = 180000;
  interrupt_counter = switch_after + 1;
  prev_interrupt_counter = interrupt_counter;
  active_routine = 1;
  get_next_color = update_simple;
  library_update = &Zoa_WS2801::pushBack;
  last_button_press = 0;
  
  // update the interrupt counter (and thus the LEDs) every 30ms. The strip updating takes ~0.1ms 
  // for each LED in the strip, and we are assuming a maximum strip length of 240, plus some extra wiggle room.
  MsTimer2::set( update_frequency, &update_interrupt_counter );
  MsTimer2::start();
  
  fade_fraction = 0;
  fade_time = 1000;
  fade_step = 1.0 / (float(fade_time)/update_frequency);
}



//////// Main loop //////////

void loop()
{  
  // the conditional should be replaced by a real test of whether the button is pressed
  //update_button_status( interrupt_counter >= 900 && interrupt_counter <= 1900 ); 
  
  if ( interrupt_counter > switch_after )
  {
    order.advance();
    byte i = order.active_routine(); //(active_routine+1) % 8;
    if ( i != active_routine )
    {
      deallocate_waveforms();
      
      // Decide which routine to show next
      switch (i)
      {
        case 0:
          // green and blue waves going in and out of phase
          get_next_color = update_simple;
          waves[0] = new Sine_generator( 0, 8, 1, PI/2 );
           // all the /3s are a quick way to get the speed looking right while maintaining prime number ratios
          waves[1] = new Sine_generator( 0, 255, 11/3, 0 );
          waves[2] = new Sine_generator( 0, 255, 17/3, 0 );
          break;
        case 1:
          // green and purple waves, same frequency but out of phase
          get_next_color = update_simple;
          waves[0] = new Sine_generator( 0, 50, 5/3, 0 );
          waves[1] = new Sine_generator( 0, 255, 5/3, PI/2 );
          waves[2] = new Sine_generator( 0, 60, 5/3, 0 );  
          break;
        case 2:
          // two waves multiplied together
          get_next_color = update_convolved; 
          waves[0] = new Sine_generator( 0, 100, 7, PI/2 );
          waves[1] = new Sine_generator( 30, 255, 11/3, PI/2 );
          waves[2] = new Sine_generator( 30, 150, 7/3, 0 );
          waves[3] = new Sine_generator( 0, 100, 7, PI/4 );
          waves[4] = new Sine_generator( 30, 250, 11/12, PI/2 );
          waves[5] = new Sine_generator( 30, 150, 7/12, 0 );
          break;
        case 3:
          // mostly light blue/turquoise/purple with occasional bright green
          get_next_color = update_twinkle_white;
          waves[0] = new Linear_generator( Linear_generator::SAWTOOTH, 0, 30, 1, 75 );
          waves[1] = new Sine_generator( 0, 30, 1, 0 );
          waves[2] = new Linear_generator( Linear_generator::TRIANGLE, 0, 255, 5, 128 );
          waves[3] = new White_noise_generator( 255, 255, 20, 150, 0, 2 );  
          break;
        case 4:
          // moar green
          get_next_color = update_convolved;//simple;
          waves[0] = new Sine_generator( 0, 20, 5/2, PI/2 );//Empty_waveform();
          waves[1] = new Linear_generator( Linear_generator::TRIANGLE, 20, 255, 2 );
          waves[2] = new Sine_generator( 0, 10, 5/2, 0 );//Sine_generator( 5, 20, 3, PI/2 );
          waves[3] = new Constant_waveform(255);
          waves[4] = new Sine_generator( 200, 255, 7/2, 0 );
          waves[5] = new Constant_waveform(255);
          break;
        case 5:
          // purple-blue with bright blue twinkles
          // this could be a startle routine later
          get_next_color = update_simple;
          waves[0] = new Sine_generator( 0, 8, 7/3, PI/2 );
          waves[1] = new Sine_generator( 0, 10, 7/3, 0 );
          waves[2] = new White_noise_generator( 230, 255, 20, 120, 20, 5 );
          break;
        case 6:
          // blue with pink-yellow bits and occasional white twinkles
          get_next_color = update_twinkle_white;
          waves[0] = new Sine_generator( 5, 15, 5, PI/2 );
          waves[1] = new Linear_generator( Linear_generator::TRIANGLE, 0, 30, 1, 30 ); //Sine_generator( 0, 30, PI/2 );//Empty_waveform();//Sine_generator( 0, 255, 5/3, 0 );
          waves[2] = new Sine_generator( 0, 255, 5, 0 );
          waves[3] = new White_noise_generator( 255, 255, 20, 150, 0, 2 );  
          break;
        case 7:
          // blue with some orange
          get_next_color = update_simple;
          waves[0] = new Sine_generator( 0, 140, 3.5, PI/2 );
          waves[1] = new Sine_generator( 20, 120, 3.5, PI/2 );
          waves[2] = new Sine_generator( 0, 210, 3.5, 0 );
          break;
        case 8:
          // dim sine waves with occasional flares of bright colors - could be adapted into a startle routine
          get_next_color = update_scaled_sum;
          waves[0] = new Sine_generator( 0, 5, 7/2, PI/2 );
          waves[1] = new Sine_generator( 0, 10, 7/2, 0 );
          waves[2] = new Sine_generator( 0, 10, 13/2, 0 );
          waves[3] = new Linear_generator( Linear_generator::TRIANGLE, 0, 255, 100, 0, 31 );
          break;
        case 9:
          // purple
          get_next_color = update_simple;
          waves[0] = new Sine_generator( 4, 100, 2 );
          waves[1] = new Sine_generator( 0, 10, 2 );
          waves[2] = new Sine_generator( 10, 200, 2 );
          break;
      }
      active_routine = i;
      if ( interrupt_counter > switch_after )
      {
        interrupt_counter -= switch_after;
      }
      else
      {
        interrupt_counter = 0;
      }
      linear_transition(500);
    }
  }
  // only update once every tick of the timer
  if ( interrupt_counter != prev_interrupt_counter )
  {
    prev_interrupt_counter = interrupt_counter;
    update();
  }
}


//////// LED display routines //////////

// returns true if button is pressed, false otherwise
boolean button()
{
  boolean result = digitalRead(switchPin) == LOW;
  return result;
}

// called before each update to fade in/out depending on button status
float adjust_fade_fraction()
{
  bool button_on = button();
  if ( button_on && fade_fraction < 1 )
  {
    fade_fraction += fade_step;
    if ( fade_fraction > 1 ) fade_fraction = 1;
  }
  if ( !button_on && fade_fraction > 0 )
  {
    fade_fraction -= fade_step;
    if ( fade_fraction < 0 ) fade_fraction = 0;
  }
}

void update()
{
  adjust_fade_fraction();
  Serial.println(fade_fraction);
  rgbInfo_t color = fade_color( get_next_color(), fade_fraction );
  (strip.*library_update)( color );
  if ( !transitioning )
  {
    strip.show();
  }
}

// just show the first 3 waves in the R, G and B channels
rgbInfo_t update_simple()
{
  return get_next_rgb( waves[0], waves[1], waves[2] );
}

// multiply waves[0:2] by waves[3:5]
rgbInfo_t update_convolved()
{
  return rgbInfo_t( next_convolved_value(waves[0],waves[3]), next_convolved_value(waves[1],waves[4]), next_convolved_value(waves[2],waves[5]) );
}

// simply sum the first 3 and next 3 waves (can't remember if this is tested yet)
rgbInfo_t update_summed()
{
  return rgbInfo_t( next_summed_value(waves[0],waves[3]), next_summed_value(waves[1],waves[4]), next_summed_value(waves[2],waves[5]) );
}

// add the 4th wave to the first 3 waves, making sure the library_update function is set to pushBack. Used to
// superimpose white twinkles.
rgbInfo_t update_twinkle_white()
{
  // it's a bit seizure-inducing if you make the whole thing flash white at once
  if ( library_update != &Zoa_WS2801::pushBack )
  {
    library_update = &Zoa_WS2801::pushBack;
  }
  // advance the first three (the base waves) plus the fourth (the white noise)
  for ( byte i = 0; i < 4; ++i )
  {
    waves[i]->next_value();
  }
  // add the twinkles to all 3 base waves
  return rgbInfo_t( summed_value(waves[0], waves[3]), summed_value(waves[1],waves[3]), summed_value(waves[2],waves[3]) );
}

// NOT TESTED
rgbInfo_t update_greyscale()
{
  return next_greyscale_value( waves[0], waves[1], waves[2] );
}

// add waves[3] to waves[0:2], increasing the brightnesses of all 3 waves proportionally
rgbInfo_t update_scaled_sum()
{
  return rgb_scaled_summed_value( waves[0], waves[1], waves[2], waves[3]->next_raw_value() );
}


//////// Transition functions //////////

void linear_transition(uint16_t duration)
{  
  transitioning = true;
  // this is a total hack to get the first value of the next routine without actually displaying it (or having to change the update functions).
  // cache the current first value, update, grab the new first value, then reset the first pixel.
  // this will fall apart if the update routine updates all the pixels and not just the first one!!! check the transitioning flag in all
  // update functions to keep this from happening.
  uint16_t pixel = (library_update == &Zoa_WS2801::pushBack) ? stripLen-1 : 0;
  rgbInfo_t temp_first_value = strip.getPixelRGBColor(pixel);
  update();
  rgbInfo_t next_value = strip.getPixelRGBColor(pixel);
  strip.setPixelColor( pixel, temp_first_value.r, temp_first_value.g, temp_first_value.b );
  transitioning = false;
  linear_transition(temp_first_value,next_value,duration/update_frequency);
}

void linear_transition( const rgbInfo& start_value, const rgbInfo& target_value, byte steps )
{
  for ( byte i = 0; i < steps; ++i )
  {    
    float multiplier = (float)i/steps;
    rgbInfo_t c( 
     interpolated_value( start_value.r, target_value.r, multiplier ),
     interpolated_value( start_value.g, target_value.g, multiplier ),
     interpolated_value( start_value.b, target_value.b, multiplier )
    );
    /// I'm pretty sure there's a bug here, since start_value and target_value have already been affected
    /// by fade_color, so we are fading them twice. However, this did not look atrocious during testing,
    /// and I do not have access to a test rig now so am hesitant to attempt a fix. (I think that dividing the
    /// rgb values of start_value and target_value by the fade fraction before passing 'em in here would fix it, tho.)
    adjust_fade_fraction();
    rgbInfo_t color = fade_color( c, fade_fraction );
    (strip.*library_update)(c);
    strip.show();
    pause_for_interrupt();
  }
}


//////// Utility functions //////////

// Called by the interrupt timer
void update_interrupt_counter()
{
  interrupt_counter += MsTimer2::msecs;
}

// Returns after the next interrupt
void pause_for_interrupt()
{
  while ( interrupt_counter == prev_interrupt_counter ) {}
  prev_interrupt_counter = interrupt_counter;
}

// free the memory in the waves array and sets the update modes to 0
void deallocate_waveforms()
{
  for ( byte i = 0; i < WAVES; ++i )
  {
    if ( waves[i] != NULL )
    {
      delete waves[i];
      waves[i] = NULL;
    }
  }
}

