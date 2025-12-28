# NS32K Utilities and example code

This repo is a companion to https://github.com/drelectro/ns32k-timewarp
It provides some example code and utilites to demonstrate building code to run under TDS.

In addition to this repo you will need a working gcc compiler.
I'm using the pre-built one found here https://wiki.sensi.org/dokuwiki/doku.php?id=ns32ktoolchain

The "bin2tds" file located in the top directory of this repository can be used to convert a flat binary file 
To the format required by the TDS ZI command.

The full sequence to load this to memory in TDS is:-

`
ZISM 10000 1FFFF
<send .tds file line by line>

CMD D000 = D100
CMD D004 = 0
CMD D008 = 10000
CMD D00C = 0
`

Then, to run the loaded program :-
`
CPS = 300
CSP = 3FFFF
CFP = 0
B D000 0
G
`
This sequence must be entered each time you want to start the program.