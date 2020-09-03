# Linux autoclick daemon for X-Windows

## Background / life story

A while ago, I was using a poorly-designed program that needed a lot of clicks on several UI elements. I wasn't sure what would give out first: my mouse, or my finger. I searched for a solution, and found a couple of options:

* xdotool

Using a command line like `xdotool click --repeat 100 --delay 50 1`, I could click the button 100 times. This worked, but it was a bit awkward and fiddly. I would have to position my mouse just right, switch to the terminal, and enter the command. If I made a mistake or if I wanted to stop clicking early, it was kind of a pain to stop the tool before it was done.

* A bash script using xinput and xdotool

I wrote the following script:
```
while true; do
	i=`xinput --query-state 10 | grep -o "button\[9]=down"`
	if [ ! -z "$i" ]; then
		xdotool click 1
	else
		sleep .01
	fi
done
```
This...worked. I had to add the `sleep` in there because otherwise the script would max out one of my CPU cores. And I found I couldn't get clicks quite as fast as I could with `xdotool`, probably because of the overhead of constantly starting the `xinput` and `xdotool` binaries and then parsing the output of `xinput` with `grep`. Although it did the job, it was inefficient in a way that vexed me.

Then I thought...`xinput` and `xdotool` are just calling various APIs. Why can't I write a single binary that cuts out the middleman, so to speak, and call the same APIs?

And so I did.

## Building

`autoclickd` has a somewhat reasonable Makefile. Just type `make` and it will build. You have to have `libxtst`, `libx11`, and `libxi` installed. If you don't, it won't work. Bummer.

## Running

`autoclickd` takes a rather arcane series of parameters:

* `-d`:  The number of milliseconds to delay in between clicks (defaults to `50`, which provides 20 clicks/sec)
* `-b`:  The ID of the button to click (defaults to `1`, which should be the left button)
* `-t`:  The ID of the button to trigger the clicks
* `-i`:  The device ID for the pointing device

You are not expected to know the X Windows button IDs or device IDs for your mouse off the top of your head. `autoclickd` can help!

### Calibrate mode

If you run `ac --calibrate`, you are given an interactive prompt where you're asked to click the trigger button. You will get output that looks like this:
```
$ ./ac --calibrate
Press the mouse button you want to identify
Found button: Logitech M570 -> device 10 button 9
```
With this output, you should run `ac -i 10 -t 9` to get the desired results.

### List mode

If you want to see device IDs for all the pointing devices known to X, you can run in list mode:
```
$ ./ac --list
Found pointing device (0): Virtual core pointer -> 2
Found pointing device (4): Virtual core XTEST pointer -> 4
Found pointing device (4): Nuvoton w836x7hg Infrared Remote Transceiver -> 12
Found pointing device (4): Logitech M570 -> 10
Found pointing device (4): Ergonomic Keyboard Consumer Control -> 14
```
