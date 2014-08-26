# Linker Scripts

Though they are essential to building a free standing program, linker
scripts seem to be an arcane art. Fortunately, they are simple to
work with once you understand some of the basics.

The primary purpose of the linker script is to tell the linker how to
organize code and data in the binary. Some of the script instructions
are just passed on to the binary so that the boot loader will load 
things to the desired locations in memory. Other instructions will cause
some functions to be grouped together or apart in memory, depending
on what we want.

More detail about linker scripts can be read from the
[OSDev wiki page on linker scripts](http://wiki.osdev.org/Linker_Scripts).

