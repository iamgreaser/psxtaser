WARNING WARNING WARNING:
IF YOU LET +3.5V OR +7.5V TOUCH GND, YOU WILL BLOW A FUSE IN YOUR PSX AND IT IS LIKELY THAT THE CONTROLLERS AND MEMORY CARDS WILL NO LONGER WORK. MAKE DAMN SURE YOU HAVE THE RIGHT PINOUT ESPECIALLY DEPENDING ON WHETHER YOU ARE PLUGGING THIS INTO A MEMORY CARD PORT OR A CONTROLLER PORT AS THESE TEND TO BE OFFSET BY ONE.

Here's how you wire it up at the moment (a more intuitive way to wire things up may be considered):

From psx-spx: http://problemkaputt.de/psx-spx.htm#pinoutscontrollerportsandmemorycardports

      1 In  JOYDAT Data from joypad/card (data in)    _______________________
      2 Out JOYCMD Data to joypad/card (command out) |       |       |       |
      3 -   +7.5V  +7.5VDC supply (eg. for Rumble)   | 9 7 6 | 5 4 3 |  2 1  | CARD
      4 -   GND    Ground                            |_______|_______|_______|
      5 -   +3.5V  +3.5VDC supply (normal supply)     _______________________
      6 Out /JOYn  Select joypad/card in Slot 1/2    |       |       |       |
      7 Out JOYCLK Data Shift Clock                  | 9 8 7 | 6 5 4 | 3 2 1 | PAD
      8 In  /IRQ10 IRQ10 (Joy only, not mem card)     \______|_______|______/
      9 In  /ACK   IRQ7 Acknowledge (each eight CLKs)
      Shield       Ground (Joypad only, not memory card)

And the specific wiring:

    PD2 = Arduino Pin  2 = a button with the other side wired to ground - this is your "next button sequence" button.
    PD7 = Arduino Pin  7 = 7 PSX
    PB0 = Arduino Pin  8 = 9 PSX
    PB2 = Arduino Pin 10 = 6 PSX
    PB3 = Arduino Pin 11 = 2 PSX
    PB4 = Arduino Pin 12 = 1 PSX
    GND                  = 4 PSX - this is to keep things less out of sync with the PSX.

Yes the pinout is weird. It's based on the Arduino SPI pinout, except I tried using the AVR SPI and the doofus who decided to put an LED on the same pin as the SPI clock is in dire need of a baseball bat to the face, because everything else can interface the PSX just fine EXCEPT for that pin.

On top of that, the onboard SPI wasn't performing well enough once I had a workaround (putting the clock into pin PD7 and then relaying that out using pin PD4) so I switched over to bit-banging the bus.

So the logic of that:

* PB2,3,4,5 are the SPI pins as per AVR. PB5 was moved to PD7.
* PB0 is ACK, which is not a part of SPI. It was a spare pin which was still on Port B and yet wasn't jammed up against the other pins.
* Because of the wiring it's easy to remember which way PB3 and PB4 go: The most inconvinient way. PB4 goes to PSX 1, and PB3 goes to PSX 2.

Anyway, here's how I wire it up:

* I tend to use the memory card ports for this as they all share the same bus yet the wires tend to stay in a bit more easily. On the other hand I use a controller port for GND because it's easier to plug something into the wrong hole with the memory card port.
* I use memory card port 2 for the most part, except for the SPI slave select line which needs to be grabbed from port 1, so I grab that from memory card port 1 and use a longer wire.
* I take ground from controller port 2.
* You can use a USB-A plug to open the memory card flap. Once you've got one wire in it should hold.
* Unplugging the PSX and placing the front of it vertically makes it much easier to plug stuff in.

vim: set sts=2 sw=2 et :
