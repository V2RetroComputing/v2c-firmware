# v2c-firmware
Firmware for the RP2040 based Apple IIc VGA adapter

The hardware for this project is still under heavy development, and pin functions will change between board revisions.
This board will NOT be usable on an Apple IIgs, as those systems have Analog RGB with a pinout incompatible with the IIc's video port.

Current pin assignments as of Rev0 prototypes:
```
 0 LED 0
 1 LED 1
 2 LED 2
 3 LED 3
 4 IIc 14.318MHz
 5 IIc GR
 6 IIc Sync
 7 IIc Video Serial Data
 8 IIc SEGB
 9 IIc LDPS
10 IIc D7
11 IIc WNDW
12 Mode Button
13 Select Button
14 Reserved
15 Reserved
16 VGA B 3
17 VGA B 2
18 VGA B 1
19 VGA B 0
20 VGA G 3
21 VGA G 2
22 VGA G 1
23 VGA G 0
24 VGA R 3
25 VGA R 2
26 VGA R 1
27 VGA R 0
28 VGA H-Sync
29 VGA V-Sync
```

Rev1 Prototypes will have the following pinout:
```
 0 LED 0
 1 LED 1
 2 LED 2
 3 LED 3
 4 IIc 14.318MHz
 5 IIc Video Serial Data
 6 IIc D7
 7 IIc Sync
 8 IIc PRAS
 9 IIc SEGB
10 IIc LDPS
11 IIc WNDW
12 IIc GR 
13 IIc TEXT
14 Mode Button
15 Select Button
16 VGA B 0
17 VGA B 1
18 VGA B 2
19 VGA B 3
20 VGA G 0
21 VGA G 1
22 VGA G 2
23 VGA G 3
24 VGA R 0
25 VGA R 1
26 VGA R 2
27 VGA R 3
28 VGA H-Sync
29 VGA V-Sync
```
