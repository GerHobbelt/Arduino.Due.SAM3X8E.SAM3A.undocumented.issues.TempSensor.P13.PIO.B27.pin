/* Showcases the Arduino Due SAM3X8E issue where ADC15 (temp sensor) usage conflicts with using output pin P13 ('blinking LED') */

#include <type_traits>

//
// large-ish debug helper code chunk, ripped from another project verbatim.
//

// SAM3 compiler is stuck at the C++/11 age, so no `%b` for me.

static const char *nibble_2_bitpattern[16] = {
    [ 0] = "0000", [ 1] = "0001", [ 2] = "0010", [ 3] = "0011",
    [ 4] = "0100", [ 5] = "0101", [ 6] = "0110", [ 7] = "0111",
    [ 8] = "1000", [ 9] = "1001", [10] = "1010", [11] = "1011",
    [12] = "1100", [13] = "1101", [14] = "1110", [15] = "1111",
};

static inline void to_bitstring(char dst[8 + 1], size_t dstsize, const uint8_t byte)
{
  if (dstsize < 8 + 1) {
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
    }
    return;
  }
  snprintf(dst, dstsize, "%s%s", nibble_2_bitpattern[byte >> 4], nibble_2_bitpattern[byte & 0x0F]);
}
static inline void to_bitstring(char dst[4 *8 + 1], size_t dstsize, const uint32_t word)
{
  if (dstsize < 4 * 8 + 1) {
    if (dstsize == 1) {
      dst[0] = 0;
    } 
    else if (dstsize >= 2) {
      strcpy(dst, "?");
    }
    return;
  }
  to_bitstring(dst, dstsize, word >> 24);
  to_bitstring(dst + 8, dstsize - 8, word >> 16);
  to_bitstring(dst + 16, dstsize - 16, word >> 8);
  to_bitstring(dst + 24, dstsize - 24, word);
}

template <typename T,
          typename = std::enable_if_t<std::is_arithmetic<T>::value || std::is_enum<T>::value>>
String bin(const T ptr) {
  char buf[sizeof(T) * 8 + 3];
  snprintf(buf, sizeof(buf), "0b%s", to_bitstring(ptr));
  return buf;
}

// polymorphic hex value print helpers.
template <typename T>
String hex(const T *ptr) {
  char buf[12 + 3];
  snprintf(buf, sizeof(buf), "0x%p", (const void *)ptr);
  return buf;
}

template <typename T>
String hex(T *ptr) {
  char buf[12 + 3];
  snprintf(buf, sizeof(buf), "0x%p", (const void *)ptr);
  return buf;
}

static String hex(uint32_t v) {
  char buf[12 + 32 + 7];
  snprintf(buf, sizeof(buf), "0x%08X (0b%s)", (unsigned int)v);
  return buf;
}



// OK, so what follows includes ALL the 'WTF is going on here!' coding activity I did.
//
// Most of the pmc/pio/adc calls are due to reading the SAM3X/SAM3A Atmel datasheet and, when in doubt, 
// trying to adhere **to the letter** of what's written there.
//
// You will, notice that, in particular, the pio_xyz API calls have been replaced by direct PIOB->REGISTER_NAME
// read/write access statements: I did this after a while as nothing I tried seemed to lead *anywhere*, so I dove
// into the CMSIS source code itself and discovered that those APIs are written 'efficiently', i.e. trhey are all
// assuming a lot and lazy as Hell: those APIs are implemented in a way that only works when you are careful
// to use them, and only them, and use them *judiciously*. Which to me is not a smell, but a *stink*.
//
// What do I mean by this? Well, the SAM3 hardware has a little wicked design element spreading *everywhere*:
// to turn things off or on, they did this through providing BOTH AN **ENABLE** AND A **DISABLE** MASK REGISTER,
// BOTH OF WHICH WORK TOGETHER TO DECIDE WHETHER A SIGNAL WILL PROPAGATE, OR NOT.
// OK, that a choice... We can argue about it, but it's a fact any way: this is the hardware we got.
// Then, say, I fiddle with the the ENABLE register and set its bit, i.e. tweak the ENABLE MASK. Fine, that turns 
// things ON, right? Nuh-uh-uh, not so fast, buddy boy! How about the DISABLE register mask, eh? If that one has
// been tweaked before by **anybody**, your 'enable' bit fiddling in the ENABLE register is rudely BLACKLISTED
// and nothing will change!
//
// So the thing to do when you want to be in control and **absolutely certain things behave as you say you want
// them to go, via any sort of API call** is to always make sure you patch *both* ENABLE and DISABLE registers
// **every time**. And that isn't happening in the CMSIS codebase, so plenty cause for 'oh shit' moments when
// somebody elsewhere fucked up. And when you hit weird behaviour like your P13 output pin suddenly doing its
// own thing is plenty reason to suspect somebody fucked up, *somewhere*. Possibly multiple somebodies, you 
// fundamentally do not know, until you have narrowed down the problem area to the smallest zone you can
// achieve. And the existing CMSIS codebase certainly doesn't help ascertaining 'guaranteed behaviour' the way
// it has been written.
// So I had to go through the SAM3X datasheet with a fine comb and replace each pio & adc CMSIS API call with
// code I can actually *trust*: direct register addressing (read/write statements) as can be seen in the
// active code below ('active code' meaning the stuff which has NOT been '//'-commented-out).
//
// I also left the `#if 01..#endif` preprocessor stuff in there so you can see my process of drilling down
// on my way to discover which of my code statements produced the actual moment of fuck-uppery -- NOT that
// that statement (or small chunk of statements) will be the actual culprit, but this is useful to find the
// First Contact With The Enemy, from which you can spread out and test/discover which bit of setup or whatever
// else turns out to be ever so much closer to the Root Cause...
//
// FYI: the `#if 01..#endif` stuff is added 'along the way', the basic process being:
//
// - initially identifying that the 'blinking LED' eff-up only happens when you include ANY or ALL of the 
//   ADC setup + usage/access code (I removed those *outer* `#if 0..#endif` lines, sorry!)
// - arriving at a state of your source code where *attempts* at reading the ADC15 (TS) does NOT LOCK UP
//   the runtime: when you DISABLE (`#if 0..#endif`) the `while((adc_get_status(ADC)...` loop, the actual
//   ADC read operations don't trigger any hardware failure but simply cough up bogus zeroes, which is FINE.
// - now you note that simply running the ADC init/setup code chunk in `setup()` produces the problem.
// - You verify your code against others' you can find on the 'Net and check the datasheet: that's where
//   time was spent on adding those explicit PMC clock enable API calls since the datasheet murmurs
//   something about 'not all features in the PIO controller are available when the clock is not enabled'
//   PLUS elsewhere in the many pages of the datasheet you stumble across a tiny blurb about PIO Controller
//   not always being in control, but instead matters are taken over by 'peripheral A or B' and then
//   there's a little hiss elsewhere about 'write protection' of important registers, which, of course,
//   you `grep` for in CMSIS source code tree and find those unlock/deprotect register writes are, ahhh,
//   spread rather thin in there (rather more: non-existent in any places you'ld expect them). Topping
//   it all off is a wee bit of noise about 'Line Locking` (section 31.5.11) where someone makes it
//   clear you can yell at those PIO setup registers all you want, but once the SAM3 says 'locked' 
//   you're dead in the water until Thy Kingdom Come, a.k.a. a hardware reset (`grep` for `PIO_LOCKSR`), 
//   so you have turned into a positively unmedicated certified paranoid and that last sip of coffee
//   didn't help either.
// - which is where you now go and 'drill down' in your setup/init ADC init code chunk, to find out
//   where the shit starts to hit the fan for real.
// - The approach I use is akin to the binary search algorithm: split the suspect code chunk in about 
//   HALF, disable the latter half, check if that makes a lingering lick of sense and compile + fire off 
//   to see where the hardware crashes or otherwise fails. Divide and hack around until you arrive at
//   that elusive Single Line Of Failure. Add some more 'how about this then?' test lines along the way...
//   the bugger that Does Us In(tm) turns out to be: `adc_enable_channel(ADC, ADC_TEMPERATURE_SENSOR);`.
//   Drat! (adding/activating non-exixtent ADC channels 12+ didn't trigger the issue, so it's VERY
//   specific to channel 15!)
// - the FAST way to do this is to drop those `#if 01..#endif` wrapping lines at the start/end/cutting
//   edges in your code and simply 'abuse' the C/C++ ability to understand OCTAL: `#if 01` is `go!/true`
//   while `#if 0` is `ditch!/false` and it only takes a single keystroke to move from the one state to
//   the other while you are 'instrumenting your code' like this. WAY faster and far more legible across
//   IDEs, dev environments, etc., *where-ever you are* -- and, yes, I know 'modern IDEs' have keyboard
//   shortcuts for CommentOut/RemoveComment for selected chunks of code lines: the problem with that is
//
//   a. not all IDEs / editors have this
//   b. WHEN they have those shortcuts, they don't all agree on which one it is *exactly*
//   c. and you're SOL if you want to see where your cutting points were in your codebase, because 
//      oh-so-clever-you now has a *huge* ream of //-commented-out lines and only Ctrl-Z/Ctrl-Y undo/redo
//      as a way to move around without wrecking your brain. Congrats! So many synapses NOT available
//      for hunting down the real goal/issue: getting to the First Point Of Contact and subsequent 
//      Root Cause hunt.
//
//   Littering the code with `#if 01..#endif` and flipping that octal `1` doesn't suffer from these issues
//   and has delivered for me in all C/C++ environments, across the ages and across all development
//   environments, including some wicked mainframe ones and other assorted 'weirdos'. :-)
//
//   I use the octal 1 flip because that ensures I can always dig out my littering *post parum*, i.e.
//   days or weeks later by simply grepping for `#if 0` and getting a nicely reduced set of source files
//   and 'chunks to check and clean up, while I will be spared a metric ton of preprocessor errors in
//   the heat of the search because every `#if` always has a valid and known state -- possibly unintended
//   but always verifiable, because the worst I can do is, when exhausted, bash in something like `#if 011`
//   and that's a sure sign it's time to call it a day, n9o matter how many self-important people I clogging
//   my calls-missed list by now for 'status updates' and what-not.
//   Took a few years to hone it up & down to `#if 01` but that one has never failed me (of course, sometimes
//   you're tired and wonder WTF, then find out there's an `#if 0` where you **meant** ('remembered') putting
//   and `#if 01` instead, but that's about the worst that can happen, you don't throw away anything 
//   that an hour or two later suddenly seems like a Good Idea Anyway, Now WTF Did I Write Exactly(tm), 
//   plus when the number of WTFs goes up instead of down due to *your mistakes*, it's a good indicator
//   you're running out of juice and should reconsider your priorities Right Now. Anyhow... here it is,
//   in all its nasty glory, the blood still dripping...
//
//   Enjoy!
//
//                                                                                  Ger Hobbelt
//                                                                                  CTO At Large
//

void setup() {
  pmc_enable_periph_clk(ID_PIOB);                                  // To use peripheral, we must enable clock distribution to it
	pmc_enable_periph_clk(ID_PIOA);

  // initialize digital pin LED_BUILTIN as an output.
#if 0
  // !!! Does not work when we turn on the ADC init section above !!!
  pinMode(LED_BUILTIN, OUTPUT);
#else
  //PIO_DisableInterrupt(PIOB, PIO_PB27B_TIOB0);
  //PIOB->PIO_IDR |= PIO_PB27B_TIOB0;
  //PIOB->PIO_IER &= ~PIO_PB27B_TIOB0;

  //PIO_PullUp(PIOB, PIO_PB27B_TIOB0, false /* const uint32_t dwPullUpEnable */ );
  //PIOB->PIO_PUER &= ~PIO_PB27B_TIOB0;
  //PIOB->PIO_PUDR |= PIO_PB27B_TIOB0;

  //PIO_SetDebounceFilter(PIOB, PIO_PB27B_TIOB0, 0 /* const uint32_t dwCuttOff */ );

  //PIO_SetPeripheral(PIOB, PIO_OUTPUT_0, PIO_PB27B_TIOB0);
  //PIO_Configure(PIOB, PIO_OUTPUT_0, PIO_PB27B_TIOB0, PIO_DEFAULT);
  ////PIO_SetInput(PIOB, PIO_PB27B_TIOB0, PIO_DEFAULT);
  //PIO_SetOutput(PIOB, PIO_PB27B_TIOB0, 0 /* uint32_t dwDefaultValue */, 0 /* uint32_t dwMultiDriveEnable */, 0 /* uint32_t dwPullUpEnable */ );

  // ---------------------------------------------------------------------------------------------------
  // OK, now the same as above, sort of, but by the (datasheet) book: making sure **ALL** PIO registers
  // are set to a Known State, using Figure 31.3 et al, Section 43, with particular focus on Table 31-3
  // in Section 31.7: copy/paste, then rip and clip.
  // Yes, PIO_IMR says 'ReadOnly' but I'm getting tired and anything that's NOT a Status Register gets
  // a write, because the datasheet at least says 'those are harmless' (phrased differently, but anyhow...)

  PIOB->PIO_IDR |= PIO_PB27B_TIOB0;   // interrupt disable
  PIOB->PIO_IER &= ~PIO_PB27B_TIOB0;
  PIOB->PIO_IMR &= ~PIO_PB27B_TIOB0;

  PIOB->PIO_AIMDR |= PIO_PB27B_TIOB0;   // interrupt disable
  PIOB->PIO_AIMER &= ~PIO_PB27B_TIOB0;
  PIOB->PIO_AIMMR &= ~PIO_PB27B_TIOB0;

  PIOB->PIO_PDR &= ~PIO_PB27B_TIOB0;  // make sure PIO Controller is in control: clear the 'disable' bit for TIOB0.
  PIOB->PIO_PER |= PIO_PB27B_TIOB0;

  PIOB->PIO_ODR &= ~PIO_PB27B_TIOB0;  // output ENable
  PIOB->PIO_OER |= PIO_PB27B_TIOB0;

  PIOB->PIO_IFER &= ~PIO_PB27B_TIOB0;  
  PIOB->PIO_IFDR |= PIO_PB27B_TIOB0;

  PIOB->PIO_SODR &= ~PIO_PB27B_TIOB0;   // enable output
  PIOB->PIO_CODR |= PIO_PB27B_TIOB0;

  PIOB->PIO_OWDR &= ~PIO_PB27B_TIOB0;   // enable synchronous output
  PIOB->PIO_OWER |= PIO_PB27B_TIOB0;
  // +
  PIOB->PIO_OWSR |= PIO_PB27B_TIOB0;    // see datasheet section 31.5.5 "synchronous output"
  PIOB->PIO_ODSR &= ~PIO_PB27B_TIOB0;   // enable direct a.k.a. synchronous output

  PIOB->PIO_PUER &= ~PIO_PB27B_TIOB0;  // pullup DISable
  PIOB->PIO_PUDR |= PIO_PB27B_TIOB0;

  //PIO_SetDebounceFilter(PIOB, PIO_PB27B_TIOB0, 0 /* const uint32_t dwCuttOff */ );
  PIOB->PIO_DIFSR &= ~PIO_PB27B_TIOB0; /* set Debouncing, 0 bit field no effect */
  PIOB->PIO_SCIFSR &= ~PIO_PB27B_TIOB0; /* set Deglitching, 0 bit field no effect */

  PIOB->PIO_MDER &= ~PIO_PB27B_TIOB0; // no open collector a.k.a. 'multi drive'...
  PIOB->PIO_MDDR |= PIO_PB27B_TIOB0;

  volatile auto dummy = PIOB->PIO_ISR;  // dummy read to clear any interrupt glitches during setup

  /*
PIO_PER
PIO_PDR
PIO_PSR

PIO_OER
PIO_ODR
PIO_OSR

PIO_IFER
PIO_IFDR
PIO_IFSR

PIO_SODR
PIO_CODR

2. PIO_ODSR is Read-only or Read/Write depending on PIO_OWSR I/O lines.
PIO_OWER
PIO_OWDR
PIO_ODSR

PIO_PDSR

PIO_IER
PIO_IDR
PIO_IMR
PIO_ISR

PIO_MDER
PIO_MDDR
PIO_MDSR

PIO_PUDR
PIO_PUER
PIO_PUSR

4. PIO_ISR is reset at 0x0. However, the first read of the register may read a different value as input changes may have
occurred.

PIO_ABSR

PIO_SCIFSR

PIO_DIFSR

PIO_IFDGSR

PIO_SCDR

PIO_OWER
PIO_OWDR
PIO_OWSR

PIO_AIMER
PIO_AIMDR
PIO_AIMMR

PIO_ESR
PIO_LSR
PIO_ELSR

PIO_FELLSR
PIO_REHLSR
PIO_FRLHSR

PIO_LOCKSR

PIO_WPMR
PIO_WPSR
  */
  auto pinwpstat = PIOB->PIO_WPSR;

  uint32_t pinstatus = PIO_GetOutputDataStatus(PIOB, PIO_PB27B_TIOB0);

  PIO_Clear(PIOB, PIO_PB27B_TIOB0);
  uint32_t pinval0 = PIO_Get(PIOB, PIO_OUTPUT_0, PIO_PB27B_TIOB0);

  PIO_Set(PIOB, PIO_PB27B_TIOB0);
  uint32_t pinval1 = PIO_Get(PIOB, PIO_OUTPUT_0, PIO_PB27B_TIOB0);
#endif

#if 0
  pinMode(pwm_out_pin,OUTPUT); // sets the pin as output
  analogWrite(pwm_out_pin, 128);
#endif

#if 01
  // Setup all ADC registers
  pmc_enable_periph_clk(ID_ADC);                                  // To use peripheral, we must enable clock distributon to it
  adc_init(ADC, SystemCoreClock, ADC_FREQ_MAX, ADC_STARTUP_FAST); // initialize, set maximum posibble speed
  adc_disable_interrupt(ADC, 0xFFFFFFFF);
  adc_set_resolution(ADC, ADC_12_BITS);
#endif
#if 01
  adc_configure_power_save(ADC, 0, 0);                            // Disable sleep
  adc_configure_timing(ADC, 0, ADC_SETTLING_TIME_3, 1);           // Set timings - standard values
  adc_set_bias_current(ADC, 1);                                   // Bias current - maximum performance over current consumption
#endif
#if 01
  adc_stop_sequencer(ADC);                                        // not using it
  adc_disable_tag(ADC);                                           // it has to do with sequencer, not using it
  adc_enable_ts(ADC);                                             // ENable temperature sensor, see datasheet section 43.5.4
#endif
#if 01
  adc_disable_channel_differential_input(ADC, ADC_TEMPERATURE_SENSOR);
  adc_configure_trigger(ADC, ADC_TRIG_SW, 1);                     // triggering from software, freerunning mode
#endif
#if 01
  adc_disable_all_channel(ADC);
#endif
#if 0
  adc_enable_all_channel(ADC);
#endif
#if 01
  adc_enable_channel(ADC, ADC_CHANNEL_7);
  adc_enable_channel(ADC, ADC_CHANNEL_11);
  adc_enable_channel(ADC, ADC_CHANNEL_12);
  adc_enable_channel(ADC, ADC_CHANNEL_13);
  adc_enable_channel(ADC, ADC_CHANNEL_14);
#endif
#if 0          // <-- THUNK! BUGGER IT, MILLENIUM HAND & SHRIMP!
  adc_enable_channel(ADC, ADC_TEMPERATURE_SENSOR);                // just one channel enabled
#endif
#if 01
  adc_start(ADC);
#endif

#if 0
  // wait for USB serial port to be connected - wait for pc program to open the serial port
  SerialUSB.begin(115200);    // Initialize Native USB port
  while(!SerialUSB);
#endif  

  Serial.begin(115200);
  Serial.print("PIN 13 vs Temp.Sensor ADC15 example ");
  Serial.print(__FILE__);
  Serial.println();
  Serial.print("pin 13: write-protect status register = " + String(hex(pinwpstat).c_str()));
  Serial.println();
  Serial.print("pin 13: pinval.0 = " + String(pinval0));
  Serial.println();
  Serial.print("pin 13: pinval.1 = " + String(pinval1));
  Serial.println();
  Serial.print("pin 13: pinstatus = " + String(pinstatus));
  Serial.println();

  uint32_t srv = PIOB->PIO_PSR;
  Serial.print("PIO_PSR = " + String(hex(srv).c_str()));
  Serial.println();
  srv = PIOB->PIO_OSR;
  Serial.print("PIO_OSR = " + String(hex(srv).c_str()));
  Serial.println();
  srv = PIOB->PIO_IFSR;
  Serial.print("PIO_IFSR = " + String(hex(srv).c_str()));
  Serial.println();
  srv = PIOB->PIO_ODSR;
  Serial.print("PIO_ODSR = " + String(hex(srv).c_str()));
  Serial.println();
  srv = PIOB->PIO_PDSR;
  Serial.print("PIO_PDSR = " + String(hex(srv).c_str()));
  Serial.println();
  srv = PIOB->PIO_ISR;
  Serial.print("PIO_ISR = " + String(hex(srv).c_str()));
  Serial.println();
  srv = PIOB->PIO_MDSR;
  Serial.print("PIO_MDSR = " + String(hex(srv).c_str()));
  Serial.println();
  srv = PIOB->PIO_PUSR;
  Serial.print("PIO_PUSR = " + String(hex(srv).c_str()));
  Serial.println();
  srv = PIOB->PIO_ABSR;
  Serial.print("PIO_ABSR = " + String(hex(srv).c_str()));
  Serial.println();
  srv = PIOB->PIO_SCIFSR;
  Serial.print("PIO_SCIFSR = " + String(hex(srv).c_str()));
  Serial.println();
  srv = PIOB->PIO_DIFSR;
  Serial.print("PIO_DIFSR = " + String(hex(srv).c_str()));
  Serial.println();
  srv = PIOB->PIO_IFDGSR;
  Serial.print("PIO_IFDGSR = " + String(hex(srv).c_str()));
  Serial.println();
  srv = PIOB->PIO_SCDR;
  Serial.print("PIO_SCDR = " + String(hex(srv).c_str()));
  Serial.println();
  srv = PIOB->PIO_OWSR;
  Serial.print("PIO_OWSR = " + String(hex(srv).c_str()));
  Serial.println();
  srv = PIOB->PIO_ELSR;
  Serial.print("PIO_ELSR = " + String(hex(srv).c_str()));
  Serial.println();
  srv = PIOB->PIO_LOCKSR;
  Serial.print("PIO_LOCKSR = " + String(hex(srv).c_str()));
  Serial.println();
  srv = PIOB->PIO_WPSR;
  Serial.print("PIO_WPSR = " + String(hex(srv).c_str()));
  Serial.println();

  Serial.println();
  Serial.println();
  Serial.println();
  Serial.println();
}

void loop() {
  static int count = 0;
  static bool led_state = false;
#if 0
  digitalWrite(LED_BUILTIN, led_state); 
#else
  if (led_state)
    PIO_Set(PIOB,PIO_PB27B_TIOB0);
  else
    PIO_Clear(PIOB,PIO_PB27B_TIOB0);    
#endif
  led_state = !led_state;

  count++;
  if (count == 5) {
    Serial.print("ADC *ON*\n");
    adc_enable_channel(ADC, ADC_TEMPERATURE_SENSOR);                // just one channel enabled
  }
  if (count == 10) {
    Serial.print("ADC TS *OFF*\n");
    adc_disable_ts(ADC);
  }
  if (count == 15) {
    Serial.print("ADC TS *ON*\n");
    adc_enable_ts(ADC);
  }
  if (count == 20) {
    Serial.print("ADC *OFF*\n");
    adc_disable_channel(ADC, ADC_TEMPERATURE_SENSOR);
  }

#if 0           // <-- didn't matter and disabling this ensures the code does not lock up, even IF you haven't set up the ADC module! yay!
    while((adc_get_status(ADC) & ADC_ISR_DRDY) != ADC_ISR_DRDY)
      ; // Wait for end of conversion
#endif
    auto V = adc_get_latest_value(ADC);
    Serial.print("ADC = " + String(V) + "\n");

  delay(1000);
}

