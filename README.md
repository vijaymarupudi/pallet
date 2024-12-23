# Pallet

An WIP library that makes using the following easier in C++:

* Human interfaces (Mouse, Keyboard, Monome Grid, Midi instruments)
* Midi
* OSC
* Timing
* Graphics
* Audio
* Lua

to make musical and non-musical interfaces easy to develop.

Example use cases include:

* A sequencer using the Monome Grid as an interface
* A window manager macro pad using the Monome Grid or a midi keyboard
* A application that makes tagging images more easier
* An audiovisual application that draws graphics using SDL2 and plays audio using [amy](https://github.com/shorepine/amy)
* And all of the above at the same time

## Current state

Currently only supports Linux (liblo required), but structured in a
way that makes cross platform support easy to support.

A primary goal for this project is to be able to run it on the
Raspberry Pi Pico. For this reason, this **library primarily relies on
static allocation, minimizes its memory footprint, and uses an event
queue instead of threads**.

Most interfaces (Graphics, Grid, Midi, Osc, Audio, Clock, BeatClock)
have been implemented and are functional.

## What's left?

* Refactor and revise the APIs for all the interfaces.
* Write all the lua bindings
* Ensure proper destruction of resources upon application exit 
* Develop the 'killer app', a sequencer that I have in mind

## Why?

* I like C++ and a strong type system
* I want to support multiple scripting languages, especially Guile Scheme 
* I love the monome ecosystem and the monome grid as an interface
* I want to explore asynchoronous programming
* I want to make my dream sequencer mostly from scratch for the fun of
  it
* Eventually, I want to learn the math behind audio processing
  (filters, mostly) and implement it
* I want to learn embedded programming

## Inspired by

* [monome/norns](https://github.com/monome/norns)
* [robbielyman/seamstress](https://github.com/robbielyman/seamstress)
* [SynthstromAudible/DelugeFirmware](https://github.com/SynthstromAudible/DelugeFirmware)

and countless others. Eternally grateful for the community of free and
open source software
