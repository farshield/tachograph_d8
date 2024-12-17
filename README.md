# tachograph_d8

Tachograph D8 serial output interpreter for `VDO` and `Stoneridge` models. Code was designed for `PIC24` but can be adapted to any controller. Make sure you setup an interrupt-based UART driver which calls the notification functions from the `tacho` module. `FRAM`, `J1939` and `FMI` parts can be removed.

`Stoneridge` specs can be found at this [link](http://files.webyan.com/10552/files/D8/1231_078-990136%2001%20SE5000%20rev%207%20D8%20Serial%20data%20Output.pdf).

I was unable to find specs for the `VDO` tachograph so an attempt at reverse engineering the frame was made.

## VDO frame interpretation

Frame arrival period = ~1 second

### Frame length (bytes)

```
106 - both cards inserted
88 - 1 card
70 - no card
```

### Content

```
----------------------
Frame start
----------------------

Byte[0] - 0x55 - U  # sync byte (for autobaud?)
Byte[1] - 0x44 - D  # device name
Byte[2] - 0x54 - T  # -
Byte[3] - 0x43 - C  # -
Byte[4] - 0x4F - O  # -

Byte[5] - 0x00 - '0' # reserved? (zero)

----------------------
Time info
----------------------

Byte[6] - 0xAC - '172'  # UTC seconds (0.25sec/bit) [0-236]
Byte[7] - 0x1E - '30'  # UTC minutes
Byte[8] - 0x0D - \r  # UTC hours
Byte[9] - 0x08 - '8'  # month
Byte[10] - 0x47 - G '71'  # day (0.25day/bit) - value of 0 is null (day 1 starts at 0.25)
Byte[11] - 0x20 - ' ' '32'  # year (1985 offset)
Byte[12] - 0x7D - } '125'  # local minute offset (1 min/bit and -125 min offset)
Byte[13] - 0x80 - '128'  # local hour offset ( 1 h/bit and -125 h offset)

----------------------
Driver info
----------------------

Byte[14] - 0x0A - \n '10'  # driver 1&2 working state + vehicle motion? (ex. 01 011 011)
Byte[15] - 0x00 - '0'  # overspeed & driver 1 time rel states (0x40) (0x41)
Byte[16] - 0xC0 - '192'  # driver 2 rel states
Byte[17] - 0xC0 - '192'  # system event (first bit)

----------------------
Car info
----------------------

Byte[18] - 0x00 - '0'  # speed LSB (1/256 km/h/bit)
Byte[19] - 0x00 - '0'  # speed MSB (1 km/h/bit)

Byte[20] - 0x9B - '155' # High resolution total vehicle distance (4 bytes)
Byte[21] - 0xB2 - '178' # - 5 m/bit gain
Byte[22] - 0xE1 - '225' # -
Byte[23] - 0x05 - '5'   # - MSB

Byte[24] - 0x03 - '3'   # High resolution trip distance (4 bytes)
Byte[25] - 0x6F - o     # - 5 m/bit
Byte[26] - 0x53 - S     # -
Byte[27] - 0x00 - '0'   # - MSB

Byte[28] - 0x40 - @     # K-factor (2 bytes)
Byte[29] - 0x1F - '31'

Byte[30] - 0xFF - '255'
Byte[31] - 0xFF - '255'

Byte[32] - 0x50 - P
Byte[33] - 0x04 - '4'

----------------------
Vehicle identification
----------------------

Byte[34] - 0x11 - '17' # VIN length (17 bytes)?
Byte[35] - 0x57 - W  # VIN start
Byte[36] - 0x44 - D  # -
Byte[37] - 0x42 - B  # -
Byte[38] - 0x39 - 9  # -
Byte[39] - 0x36 - 6  # -
Byte[40] - 0x33 - 3  # -
Byte[41] - 0x34 - 4  # -
Byte[42] - 0x30 - 0  # -
Byte[43] - 0x33 - 3  # -
Byte[44] - 0x31 - 1  # -
Byte[45] - 0x4C - L  # -
Byte[46] - 0x37 - 7  # -
Byte[47] - 0x31 - 1  # -
Byte[48] - 0x37 - 7  # -
Byte[49] - 0x37 - 7  # -
Byte[50] - 0x32 - 2  # -
Byte[51] - 0x39 - 9  # -

----------------------
Custom string?
----------------------

Byte[52] - 0x0E - '14' # Custom string length (14 bytes)?
Byte[53] - 0x01 - '1' # Custom string start...
Byte[54] - 0x31 - 1
Byte[55] - 0x32 - 2
Byte[56] - 0x33 - 3
Byte[57] - 0x54 - T
Byte[58] - 0x45 - E
Byte[59] - 0x53 - S
Byte[60] - 0x54 - T
Byte[61] - 0x20 - ' '
Byte[62] - 0x20 - ' '
Byte[63] - 0x20 - ' '
Byte[64] - 0x20 - ' '
Byte[65] - 0x20 - ' '
Byte[66] - 0x20 - ' '

-------------------------
1st Driver identification
-------------------------

Byte[67] - 0x12 - '18' # is non-zero if card is available
Byte[68] - 0x04 - '4'  #
Byte[69] - 0x29 - )    # issuing member state (0x29 - Romania)
Byte[70] - 0x30 - 0  # card number start
Byte[71] - 0x30 - 0  #
Byte[72] - 0x30 - 0  #
Byte[73] - 0x30 - 0  #
Byte[74] - 0x30 - 0  #
Byte[75] - 0x30 - 0  #
Byte[76] - 0x30 - 0  #
Byte[77] - 0x30 - 0  #
Byte[78] - 0x30 - 0  #
Byte[79] - 0x30 - 0  #
Byte[80] - 0x38 - 8  #
Byte[81] - 0x36 - 6  #
Byte[82] - 0x48 - H  #
Byte[83] - 0x31 - 1  #
Byte[84] - 0x30 - 0  #
Byte[85] - 0x31 - 1  #

-------------------------
2nd Driver identification
-------------------------

Byte[86] - 0x00 - '0' # 2nd card not present

-------------------------
End of frame
-------------------------

Byte[87] - 0xCC - '204' # CRC-8 simple XOR
```
