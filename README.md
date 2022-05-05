# rocotest
Rotary Compression Tester

This repository contains code, models, and instructions for a DIY compression tester for rotary engines.

**Why is a special compression tester needed for a rotary engine?**

Because each rotor has 3 faces, any one of which can have differing compression numbers from the other two, and there is only one hole to measure compression from. This means that we need a compression tester that is able to figure out which face each compression pulse is coming from and separate them.

**Why cant I use a regular compression tester?**

You certainly can, but it doenst show you the whole picture. If the compression on one face of a rotor is bad, that will not show up on a regular tester, because the pulses from the other two faces are higher.

**What components do I need for this?**

1. A set of 3d printed parts for the case.
2. a screen: https://www.amazon.ca/gp/product/B07YNP2L95
3. an arduino nano: https://www.amazon.ca/gp/product/B071NMD14Y
4. a 200 psi pressure transducer: https://www.amazon.ca/gp/product/B0748CVN3F
5. a 1/4 to 1/8 npt adapter https://www.amazon.ca/gp/product/B0044F8YXW
6. a 14mm long-rech spark plug non-fouler https://www.amazon.ca/gp/product/B000BYGJQY
7. some hookup wire (get the small stuff, nothing here needs high amperage, and its easier to fit into small spaces)
8. at least 9 heat set inserts, M2 X D4.0 X L3.0: https://www.aliexpress.com/item/4000232858343.html
9. at least 9 screws, m2 x 5mm long: https://www.aliexpress.com/item/4000282581926.html
10. a soldering iron
11. a usb cable
12. jbweld

**How do I assemble this?**

1. Print the parts
2. heat set all of the inserts into the parts (2 in the top of the case, and 7 in the bottom)
3. drop the arduino into the slot, and use the small tab to fix it down
4. insert your selected sensor cable into the hole, fix it down with the clamp (if the clamp is too loose, use layers of heatshrink to enlarge it until there is a snug fit.
5. solder the wires from the sensor to the board, power to the 5v pin, ground to the GND pin, and signal to the A0 pin
6. remove any headers from the screen, you will not be able to fit them in
7. solder wires from the back of the screen to the arduino, power to the 5v pin (2 wires there, annoying but doable), ground to the other GND pin, SCL to A5, SDA to A4.
8. use the strap and 2 screws to fix the screen to the front panel. These screens are fragile so be careful. You should not need to trim anything, but it is possible.
9. plug your usb cable in and into a computer
10. load up arduino and load the sketch
11. make sure you have the Adafruit_SSD1306 library and the Adafruit_GFX library in your installed arduino libraries
12. load the firmware onto the board
13. jbweld the npt adapter into the spark plug non fouler
14. teflon tape the sensor into the npt adapter
15. put some big heatshrink or tape around the whole thing if you want some extra grip or damage resistance
16. probably a good idea to put a rubber oring onto the non fouler, makes a better seal

**How do I use this?**

1. disconnect the sensor from the harness
2. screw it into a spark plu hole
3. plug it back in
4. plug the tester into a usb power bank, or a pc
5. wait until it says you can crank the engine
6. do so
