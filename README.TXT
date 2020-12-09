PVLAB v. 2.0 (C) 2001-2003 by Remigio Coco

********
* TIPS *
********

When drawing a user curve:

Mouse Left Button - add a new point / move an existing point
Mouse Right Button - delete an existing point
Shift + Mouse Left Button - edit coordinates (click Left Button on the graph to finish)

User curves are saved in .crv files.
These files have the following format:

<number of points (min 2, max 200)>

<x1> <y1>
<x2> <y2>
...
<xN> <yN>

All x,y values MUST be between 0.0 and 1.0 (included).

x coordinates MUST be sorted in ascending order, the first value MUST be 0.0,
and the last MUST be 1.0 .

Example of a .crv file:

4
0.000000		0.000000
0.080000		0.040000
0.080001		0.200000
1.000000		1.000000

This can be useful if the user wants to create his own curves.

--------------------------------------------------------------------------------

******************************
* LIMITATIONS AND KNOWN BUGS *
******************************

In File I/O Dialog, output extension (.WAV) has to be specified
(it's NOT appended automatically by the program).

.WAV files MUST be mono.

ADPCM, ACM, A/mu law and all .WAV files other than PCM are NOT supported.

.WAV files with "strange" header (other than 44 bytes) are not recognized properly
(sometimes PVLAB displays an error message, other times starts analysis creating
"extra-long" output files!!!).

If an error message is displayed, in most cases PVLAB has to be restarted.

In time transformation, user always has to specify the output duration.
This is NOT equal to the input duration, by default (!!!).

Intermediate files (for example pv_out.fft) are not properly clean up.


Please report any other bug or limitation to:

remigio.coco@gmail.it 