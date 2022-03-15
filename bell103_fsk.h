/**
  afsk.h - AFSK modulation/demodulation and serial decoding

  Copyright (C) 2018 Costin STROIE <costinstroie@eridu.eu.org>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  Many thanks to:
    Kamal Mostafa   https://github.com/kamalmostafa/minimodem
    Mark Qvist      https://github.com/markqvist/MicroModemGP
*/

#ifndef AFSK_H
#define AFSK_H

// Bell 103 defs
//    The originating station used a mark tone of 1,270 Hz and a space tone of 1,070 Hz.
//    The answering station used a mark tone of 2,225 Hz and a space tone of 2,025 Hz.

// Mark and space bits
enum BIT {SPACE, MARK};
// States in RX and TX finite states machines
enum TXRX_STATE {WAIT, PREAMBLE, START_BIT, DATA_BIT, STOP_BIT, TRAIL, CARRIER, NO_CARRIER, NOP};
// Connection direction
//enum DIRECTION {ORIGINATING, ANSWERING};
// Command and data mode
//enum MODES {COMMAND_MODE, DATA_MODE};
// Flow control
//enum FLOWCONTROL {FC_NONE = 0, FC_RTSCTS = 3, FC_XONXOFF = 4};
// On / Off
enum ONOFF {OFF, ON};

// globals
uint8_t fulBit, hlfBit,qrtBit,octBit;

// Receiving and decoding related data
struct RX_t {
  uint8_t active;        // currently receiving something or not
  uint8_t state;     // RX decoder state (TXRX_STATE enum)
  uint8_t data;        // the received data bits, shift in, LSB first
  uint8_t bits;        // counter of received data bits
  uint8_t stream;        // last 8 decoded bit samples
  uint8_t bitsum;        // sum of the last decoded bit samples
  uint8_t clk;        // samples counter for each bit
  uint8_t carrier;      // incoming carrier detected or not
};

struct RX_t rx;  // receiver data structure

void rxDecoder(uint8_t bit);  //template

// take a sample of audio input and convert it into MARK/SPACE

void rxHandle(int8_t sample) {

static uint8_t bias = 128, first=1;
static uint16_t printed = 0;

if (first) {
  first = 0;

  //define some variables for the RX character handler

  fulBit = F_SAMPLE / BAUD;
  hlfBit = fulBit >> 1;
  qrtBit = hlfBit >> 1;
  octBit = qrtBit >> 1;
  rx.active  = 1;        // currently receiving something or not
  rx.state   = WAIT;     // RX decoder state (TXRX_STATE enum)
  rx.data    = 0;        // the received data bits, shift in, LSB first
  rx.bits    = 0;        // counter of received data bits
  rx.stream  = 0;        // last 8 decoded bit samples
  rx.bitsum  = 0;        // sum of the last decoded bit samples
  rx.clk     = 0;        // samples counter for each bit
  rx.carrier = OFF;      // incoming carrier detected or not
}

    circ_buf[circ_buf_index++] = sample;
    if (circ_buf_index == NPERBAUD) circ_buf_index = 0; //cycle round

    if(printed < NPRINT) {
    printed++;
    printf("%d ",sample);
    }
    uint8_t bit=0;
    int result = demodulate();
    if(result > 0) bit=1;

    rxDecoder(bit);
    }

/**
  The RX data decoder.  Receive the decoded data bit and try
  to figure out the entire received byte.
  simplified from Costin Stroie's decoder, with state machine debugging added  SJR

  @param bt the decoded data bit
*/
void rxDecoder(uint8_t bt) {
  static int printed=0;
  static char state[]={'W','P','S','D','T','.','.','.','0'}; //decoder state
  if(printed < NPRINT) {
    printed++;
    printf("%u %c\n",bt,state[rx.state]); //
  }
  // Update bitsum and bit stream
  rx.bitsum  += bt;
  rx.stream <<= 1;
  rx.stream  |= bt;
  rx.clk++;  // Count the received samples

  // Check the RX decoder state
  switch (rx.state) {
    // Do nothing
    case NOP:
      break;

    // Detect the incoming carrier
    case CARRIER:
       break;

    // Check each sample for a HIGH->LOW transition
    case WAIT:
      // Check the transition
      if ((rx.stream & 0x03) == 0x02) {
        // We have a transition, let's assume the start bit begins here,
        // but we need a validation, which will be done in PREAMBLE state
        rx.state  = PREAMBLE;
        rx.clk    = 0;
        rx.bitsum = 0;
      }
       break;
    // Validate the start bit after half the samples have been collected
    case PREAMBLE:
      // Check if we have collected enough samples
      if (rx.clk >= hlfBit) {
        // Check the average level of decoded samples: less than a quarter
        // of them may be HIGHs; the bitsum must be lesser than octBit
        if (rx.bitsum > octBit) {
          // Too many HIGH, this is not a start bit
          rx.state  = WAIT;
        }
        else
          // Could be a start bit, keep on going and check again at the end
          rx.state  = START_BIT;
      }
      break;
    // Other states than WAIT and PREAMBLE (for each sample)
    default:

      // Check if we have received all the samples required for a bit
      if (rx.clk >= fulBit) {
        // Check the RX decoder state
        switch (rx.state) {

          // We have received a start bit
          case START_BIT:

            // Check the average level of decoded samples: less than a quarter
            // of them may be HIGH, the bitsum must be lesser than qrtBit
            if (rx.bitsum > qrtBit) {
              // Too many HIGHs, this is not a start bit
              rx.state  = WAIT;
            }
            else {
              // This is a start bit, go on to data bits
              rx.state  = DATA_BIT;
              rx.data   = 0;
              rx.clk    = 0;
              rx.bitsum = 0;
              rx.bits   = 0;
            }
            break;

          // We have received a data bit
          case DATA_BIT:
            // Keep the received bits, LSB first, shift right
            rx.data = rx.data >> 1;
            // The received data bit value is the average of all decoded
            // samples.  We count the HIGH samples, threshold at half
            rx.data |= rx.bitsum > hlfBit ? 0x80 : 0x00;

            // Check if we are still receiving the data bits
            if (++rx.bits < 8) {
              // Prepare for a new bit: reset the clock and the bitsum
              rx.clk    = 0;
              rx.bitsum = 0;
            }
            else {
              // Go on with the stop bit, count only half the samples (why? To save time?)
              rx.state  = STOP_BIT;
              rx.clk    = hlfBit;
              rx.bitsum = 0;
 //             printf("STOP BIT\n");
            }
            break;

          // We have received the first half of the stop bit
          case STOP_BIT:

            // Check the average level of decoded samples: at least half
            // of them must be HIGH, the bitsum must be more than qrtBit
            // (remember we have only the first half of the stop bit)  --again,why?

            if (rx.bitsum > qrtBit)
              // Push the data into FIFO
            printf("%c",rx.data);

            // Start over again
            rx.state  = WAIT;
            break;
        }
      }
  }
 // if (printed < NPRINT) printf("-> %c\n",state[rx.state]); //final decoder state
}
#endif
