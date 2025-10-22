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

Available build targets:
* `make` or `make debug` - Build debug version with debugging symbols
* `make release` - Build optimized release version
* `make clean` - Remove built binaries
* `make test` - Build and run unit tests (requires CMocka)

## Testing

`autoclickd` includes a unit test suite using the CMocka testing framework.

### Prerequisites

To run the tests, you need to install CMocka:

**Debian/Ubuntu:**
```bash
sudo apt-get install libcmocka-dev
```

**Arch Linux:**
```bash
sudo pacman -S cmocka
```

**Fedora/RHEL:**
```bash
sudo dnf install libcmocka-devel
```

### Running Tests

```bash
make test
```

The test suite includes 31 tests covering:
* Config file parsing and validation
* Command-line option parsing (including --no-disable-default)
* Error handling for invalid inputs
* Default value initialization

## Running

`autoclickd` takes a rather arcane series of parameters:

* `-d`:  The number of milliseconds to delay in between clicks (defaults to `50`, which provides 20 clicks/sec)
* `-b`:  The ID of the button to click (defaults to `1`, which should be the left button)
* `-t`:  The ID of the button to trigger the clicks (required)
* `-i`:  The device ID for the pointing device
* `-n`:  The device name for the pointing device (specify either `-i` or `-n`, not both!)
* `-f`:  Path to a config file
* `--no-disable-default`:  Don't disable the trigger button's default action (see below)

You are not expected to know the X Windows button IDs or device IDs for your mouse off the top of your head. `autoclickd` can help!

### Disabling the trigger button's default action

By default, `autoclickd` disables the normal action of the trigger button while the program is running. This prevents the button from performing its usual function (e.g., "Back" navigation, special mouse actions) when you're using it as a trigger.

For example, if you use mouse button 9 (typically the "Back" button) as your trigger, `autoclickd` will prevent it from navigating back in your browser while the autoclicker is active.

The button grab is automatically released when the program exits, restoring normal button behavior.

If you want to keep the button's default action (allowing it to trigger clicks AND perform its normal function), use the `--no-disable-default` option:

```bash
./ac -i 10 -t 9 --no-disable-default
```

Note: This feature uses the XInput2 extension to grab the button. If the grab fails, you'll see a warning message, but the autoclicker will still work (the button will just keep its default action).

### Calibrate mode

If you run `ac --calibrate`, you are given an interactive prompt where you're asked to click the trigger button. You will get output that looks like this:
```
$ ./ac --calibrate
Press the mouse button you want to identify
Found button: Logitech M570 -> device 10 button 9

To use this button as a trigger, run one of these commands:
  ./ac -i 10 -t 9
  ./ac -n "Logitech M570" -t 9
```
You can copy and paste either command to start the autoclicker.

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

### Config file

For convenience, it is possible to store the `autoclickd` configuration in a file and pass it in using `-f`.

```
$ ./ac -f autoclick.conf
```

An example configuration file is included in the repository. The configuration syntax is:
```
key_name value
```
For string values, do not use quotation marks (they will be read as part of the value). Comments can be added with `#`.
