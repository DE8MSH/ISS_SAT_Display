ISS / SAT Display

With a little help of ChatGPT. Why? Why not! ;)

Get TLE data from https://celestrak.org/. BTW donate them. They're doing a great job since decades!!!

Then goto 'Current Data (GP)'. Then choose category. Amateur for e.g. Choose Table list. In Latest Data (right side of page) you find the right URL. 

Put URL into this part of the code:

{ Sgp4(), "**https://celestrak.org/NORAD/elements/gp.php?CATNR=27607&FORMAT=tle**", TFT_YELLOW }, // SO50

**Friendly note: You get no warranty, no support, no anythng for this project. It's a proove of concept. Me and ChatGPT are happy that satellites can be displayed on a cheap chinese ESP32-2432S028.**
