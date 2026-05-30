/* Showcases the Arduino Due SAM3X8E issue where ADC15 (temp sensor) usage conflicts with using output pin P13 ('blinking LED') */

/* This is the 'smaller footprint' simplified/reduced/cleaned version of ../main/main.ino */

#include "hexdump.h"


// OK, so what follows includes ALL the 'WTF is going on here!' coding activity I did.
//
// Most of the pmc/pio/adc calls are due to reading the SAM3X/SAM3A Atmel datasheet and, when in doubt, 
// trying to adhere **to the letter** of what's written there.
//
// You will, notice that, in particular, the pio_xyz API calls have been replaced by direct PIOB->REGISTER_NAME
// read/write access statements.
//


using SBuf = StringBuffer<200>;






void setup() {
  pmc_enable_periph_clk(ID_PIOB);                                  // To use peripheral, we must enable clock distribution to it
	pmc_enable_periph_clk(ID_PIOA);

  // initialize digital pin LED_BUILTIN as an output.

  // ---------------------------------------------------------------------------------------------------
  // OK, now by the (datasheet) book: making sure **ALL** PIO registers
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
  PIOB->PIO_OWSR |= PIO_PB27B_TIOB0;    // see datasheet section 31.5.5 "synchronous output" + 31.5.7
  PIOB->PIO_ODSR &= ~PIO_PB27B_TIOB0;   // enable direct a.k.a. synchronous output

  PIOB->PIO_PUER &= ~PIO_PB27B_TIOB0;  // pullup DISable
  PIOB->PIO_PUDR |= PIO_PB27B_TIOB0;

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

  // -------------------------------------------------------------------------------------------------------------------

  // Setup all ADC registers
  pmc_enable_periph_clk(ID_ADC);                                  // To use peripheral, we must enable clock distributon to it
  adc_init(ADC, SystemCoreClock, ADC_FREQ_MAX, ADC_STARTUP_FAST); // initialize, set maximum posibble speed
  adc_disable_interrupt(ADC, 0xFFFFFFFF);
  adc_set_resolution(ADC, ADC_12_BITS);
  adc_configure_power_save(ADC, 0, 0);                            // Disable sleep
  adc_configure_timing(ADC, 0, ADC_SETTLING_TIME_3, 1);           // Set timings - standard values
  adc_set_bias_current(ADC, 1);                                   // Bias current - maximum performance over current consumption
  adc_stop_sequencer(ADC);                                        // not using it
  adc_disable_tag(ADC);                                           // it has to do with sequencer, not using it
  adc_enable_ts(ADC);                                             // ENable temperature sensor, see datasheet section 43.5.4
  adc_disable_channel_differential_input(ADC, ADC_TEMPERATURE_SENSOR);
  adc_configure_trigger(ADC, ADC_TRIG_SW, 1);                     // triggering from software, freerunning mode
  adc_disable_all_channel(ADC);
#if 0          // <-- THUNK! BUGGER IT, MILLENIUM HAND & SHRIMP! This *WAS* the original First Contact With The Enemy code line... For this demo, the faulty SAM3 behaviour has been completely moved into the loop() section below.
  adc_enable_channel(ADC, ADC_TEMPERATURE_SENSOR);                // just one channel enabled
#endif
  adc_start(ADC);


  Serial.begin(115200);
  Serial.print("PIN 13 vs Temp.Sensor ADC15 example ");
  Serial.print(__FILE__);
  Serial.println();
  Serial.println();
  Serial.println();
  Serial.println();
}

// Every loop() run is designed to take 1000 msecs each:
// - 100 msecs spent on repeatedly reading the TS/ADC15 channel.
// - ~750 msecs on fast-blinking the P13 LED: the number of light pulses represents our current 'state', which is 'count / 5'
//   as we switch behaviours (see further below) on several 5 second 'edges'.
// - the 'count'-based state machine below resets/cycles after 61 rounds ~ 61 seconds (as loop() is meant to take about a second each)
// - NOTE THE FAULTY SAM3 Pin13 output behaviour during the states when the ADC has been turned ON, whether or not the TSON bit
//   has been set alongside ADC channel 15!
//
void loop() {
  SBuf msgbuf;

  static int count = 0;
  count++;

  bool state_changed = false;                // true: situation's changed, so we'd better reset the ADC value tracker code further below.
  if (count == 5) {
    Serial.print("ADC *ON*\n");
    adc_enable_channel(ADC, ADC_TEMPERATURE_SENSOR);                // just one channel enabled
    state_changed = true;
  }
  if (count == 10) {
    Serial.print("ADC TS *OFF*\n");
    adc_disable_ts(ADC);
    state_changed = true;
  }
  if (count == 15) {
    Serial.print("ADC TS *ON*\n");
    adc_enable_ts(ADC);
    state_changed = true;
  }
  if (count == 20) {
    Serial.print("ADC *OFF*\n");
    adc_disable_channel(ADC, ADC_TEMPERATURE_SENSOR);
    adc_disable_ts(ADC);
    state_changed = true;
  }
  if (count == 61) {
    Serial.print("...RESET CYCLE...\n");
    count = 0;
    //state_changed = true;
  }

    // spend 100 msecs reading the ADC, repeatedly.
    unsigned int max_duration = 100 * 1000;
    auto t0 = micros();
    auto t1 = t0 + max_duration;

    Serial.println();

    while(micros() < t1) {
      // make sure NOT to lock up in this loop when ADC is not activated (yet we still execute this code):
      // timeout and abort the loop on T1 'end of time' expiry!
      while(micros() < t1 && (adc_get_status(ADC) & ADC_ISR_DRDY) != ADC_ISR_DRDY)
        ; // Wait for end of conversion
      // see datasheet section 43.6.4: reading the LCDR register clears the ready/EOC bits!
      auto V = adc_get_latest_value(ADC);

      Serial.print(msgbuf << "ADC =" << V << "@ cycle-counter:" << count << "@ micros dT=" << (micros() - t0));
    }

  // ------------------------------------------------------------------------------------------------------
  // show a light pulse train per state; still take 1000msecs total per round.
  //
  // only do the pulse train with at least 500 msecs separation, so those are nicely 'visually separated'.
  //
  bool led_state = (count % 2 == 0);
  // 'derive' our state ID from count: we switch at the 5,10,15 and 20 marks, so that's 5 states total, there-abouts. ;-)
  int state = std::min(5, count / 5);
  int separation = 1000 - (state + 1) * 150;
  if (separation >= 500 || led_state) {
    int i;
    // state 0 --> 1 pulse, state 1 --> 2 pulses, etc.
    for(i = 0; i <= state; i++) {
      PIO_Set(PIOB,PIO_PB27B_TIOB0);
      delay(75);
      PIO_Clear(PIOB,PIO_PB27B_TIOB0);    
      delay(75);
    }    
    // add surplus delay to arrive at a grand total of 1000 msecs. 
    // Same calculation as for `separation` except we now need to account for the time spent in the ADC scan further above.
    delay(1000 - 100 - i * 150);
  }
  else {
    delay(1000 - 100);
  }
}

