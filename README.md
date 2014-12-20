# cs452-flaming-meme

CS452 Real Time Programming

[Course Website](http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/s14)

[Course Advertisement](https://www.youtube.com/watch?v=iQCZ27_Ot2I&t=7m40s)

[Prof. William Cowan](http://www.cgl.uwaterloo.ca/~wmcowan/)


## Background

This µKernel and OS were originally written by
[Nikolas Ilerbrun](https://github.com/unverified/) and myself
([Mark Rada](https://github.com/ferrous26)) as part of our real time
programming course at the University of Waterloo.

The OS is designed for high performance, real time applications. The original
application of the OS was able to automatically navigate and drive multiple
model trains around a course without colliding. The final project included
a small game where a user could manually control a train while being chased
by a computer controlled train. Only the OS remains now, but the code for the
application still exists in the repository history.

While we never thought to make a video of our trains in action, other students
from a previous offering of the course have:
https://www.youtube.com/watch?v=S8HsQTIZ5e0.

## Goal

The current goal of the OS is to be ported to the Raspberry Pi for personal
projects in home automation. There are also certain parts of the OS
that I would like to clean up or improve.


## TODO

- [ ] Fill-in TODO
- [ ] Implement a PiroritySend to put message at front of queue (for clock notifier)
- [ ] More flexible name server
- [ ] Fix clock UI timing (the UI gets temporarily out of sync under load)
- [ ] Fix clock server pq usage (minor)

## License

All code in this repository is owned and copyright by the commit authors as seen in
the repository history, namely Mark Rada and Nikolas Ilerbrun, unless otherwise
explicitly stated. See LICENSE.txt for details.
