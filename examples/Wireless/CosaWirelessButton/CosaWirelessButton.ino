/**
 * @file CosaWirelessButton.ino
 * @version 1.0
 *
 * @section License
 * Copyright (C) 2013-2015, Mikael Patel
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * @section Description
 * Demonstration of the Wireless interface and ExternalInterruptPin
 * for wakeup after power down. An ATtiny85-20PU running this example
 * sketch will require 5 uA in power down mode (VWI transmitter),
 * Arduino Pro-Mini with LED removed will require 10 uA with
 * BOD disabled.
 *
 * @section Circuit
 * See Wireless drivers for circuit connections.
 *
 * This file is part of the Arduino Che Cosa project.
 */

#include "Cosa/AnalogPin.hh"
#include "Cosa/ExternalInterrupt.hh"
#include "Cosa/Power.hh"
#include "Cosa/Watchdog.hh"
#include "Cosa/RTC.hh"

// Connect button between ground and pin TinyX4 EXT0/D10, TinyX5 EXT0/D2,
// Mega EXT2/D29 and others to Arduino EXT1 which is Standard/D3 and
// Mighty/D11.
#define NETWORK 0xC05A
#if defined(BOARD_ATMEGA2560)
#define DEVICE 0x52
#define EXT Board::EXT2
#elif defined(BOARD_ATTINY)
#define DEVICE 0x50
#define EXT Board::EXT0
#else
#define DEVICE 0x51
#define EXT Board::EXT1
#endif

// Select Wireless device driver
// #define USE_CC1101
// #define USE_NRF24L01P
// #define USE_RFM69
#define USE_VWI

#if defined(USE_CC1101)
#include "Cosa/Wireless/Driver/CC1101.hh"
CC1101 rf(NETWORK, DEVICE);

#elif defined(USE_NRF24L01P)
#include "Cosa/Wireless/Driver/NRF24L01P.hh"
NRF24L01P rf(NETWORK, DEVICE);

#elif defined(USE_RFM69)
#include "Cosa/Wireless/Driver/RFM69.hh"
RFM69 rf(NETWORK, DEVICE);

#elif defined(USE_VWI)
#include "Cosa/Wireless/Driver/VWI.hh"
// #include "Cosa/Wireless/Driver/VWI/Codec/BitstuffingCodec.hh"
// BitstuffingCodec codec;
// #include "Cosa/Wireless/Driver/VWI/Codec/Block4B4BCodec.hh"
// Block4B4BCodec codec;
#include "Cosa/Wireless/Driver/VWI/Codec/HammingCodec_7_4.hh"
HammingCodec_7_4 codec;
// #include "Cosa/Wireless/Driver/VWI/Codec/HammingCodec_8_4.hh"
// HammingCodec_8_4 codec;
// #include "Cosa/Wireless/Driver/VWI/Codec/ManchesterCodec.hh"
// ManchesterCodec codec;
// #include "Cosa/Wireless/Driver/VWI/Codec/VirtualWireCodec.hh"
// VirtualWireCodec codec;
#define SPEED 4000
#if defined(BOARD_ATTINY)
VWI rf(NETWORK, DEVICE, SPEED, Board::D1, Board::D0, &codec);
#else
VWI rf(NETWORK, DEVICE, SPEED, Board::D7, Board::D8, &codec);
#endif
#endif

class Button : public ExternalInterrupt {
public:
  Button() : ExternalInterrupt(EXT, ON_LOW_LEVEL_MODE, true) {}

  virtual void on_interrupt(uint16_t arg)
  {
    UNUSED(arg);
    disable();
  }
};

// Wakeup button
Button wakeup;

// Analog pins to sample for values to send
AnalogPin luminance(Board::A2);
AnalogPin temperature(Board::A3);
OutputPin led(Board::LED, 0);

// Sample and transmission min period
static const uint32_t MIN_PERIOD = 128;

void setup()
{
  // Start Watchdog, RTC and Wireless device
  Watchdog::begin();
  RTC::begin();
  rf.begin();
}

struct dlt_msg_t {	       // Digital Luminance Temperature message
  uint8_t nr;		       // Message sequence number
  uint16_t luminance;	       // Light level (0..1023 raw value)
  uint16_t temperature;	       // Room temperature (0..1023 raw value)
  uint16_t battery;	       // Battery level (mV)
};
static const uint8_t DIGITAL_LUMINANCE_TEMPERATURE_TYPE = 0x04;

void loop()
{
  // Construct the message with sample values and broadcast
  static uint8_t nr = 0;
  dlt_msg_t msg;
  msg.nr = nr++;
  msg.luminance = luminance.sample();
  msg.temperature = temperature.sample();
  msg.battery = AnalogPin::bandgap();
  led.on();
  rf.broadcast(DIGITAL_LUMINANCE_TEMPERATURE_TYPE, &msg, sizeof(msg));
  rf.powerdown();
  led.off();

  // Put the hardware to sleep
  Power::all_disable();
  uint8_t mode = Power::set(SLEEP_MODE_PWR_DOWN);
  do Watchdog::delay(MIN_PERIOD); while (wakeup.is_clear());
  Watchdog::end();
  RTC::end();
  wakeup.enable();

  // And wait for the wakeup
  while (wakeup.is_set()) yield();

  // Wake up the hardware
  Power::set(mode);
  Watchdog::begin();
  RTC::begin();
  Power::all_enable();
  rf.powerup();
}
