# Bell-103-modem-demodulator
300 Baud FSK tone demodulator (Fourier Transform)

Inspired by Costin Stroie's Arduino code for Bell 103 300 Baud modem at https://github.com/cstroie/Arabell300

The Fourier Transform method posted here has much lower error rate for character decoding than the delay queue/correlation method used above, but is too slow for a 16 MHz Arduino.  On a 16 MHz Arduino, the FT decoder takes about 600 microseconds to decide if a bit is 0 or 1, so the MCU clock rate has to be about 10X higher.

I was unable to upload the audio file used for this project without getting error messages, so it may not work. I created it by using an on-line "youtube to MP3" converter the extract .MP3 audio in this video: https://www.youtube.com/watch?v=bVFem3I9B9w 

I then used Audacity to downsample the MP3 file to 9600 Hz sample rate, mono track, 32 bit float, and exported it as a .WAV file.
