#include <X11/extensions/XTest.h>

#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

typedef struct
{
	int click_button;
	int trigger_button;
	int device_id;
	uint32_t delay_ms;

	// Alternate modes
	bool calibrate_mode;
	bool list_mode;
} opts_t;

void msleep(uint32_t ms)
{
    struct timespec ts;
    int res;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;

    do 
	{
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);
}

/**
 * Generate one synthetic mouse click.
 */
void do_click(Display* display, int button)
{
	XTestFakeButtonEvent(display, button, 1, CurrentTime);
	XFlush(display);
	XTestFakeButtonEvent(display, button, 0, CurrentTime);
	XFlush(display);
}

/**
 * Find and display a list of pointer devices.
 */
void find_mouse_device(Display* display)
{
	XDeviceInfo* info;
    int num_devices;

    info = XListInputDevices(display, &num_devices);

    for (int i = 0; i < num_devices; ++i) 
	{
        if (info[i].use == IsXPointer ||
		    info[i].use == IsXExtensionPointer)
		{
			printf("Found pointing device (%d): %s -> %d\n",
					info[i].use,
					info[i].name,
					(int)info[i].id);
		}
    }

	XFreeDeviceList(info);
}

/**
 * Check the given device to determine if the given button is pressed.
 */
bool check_button_state(Display* display, XDevice* device, int button)
{
	bool ret = false;
	XDeviceState* st = XQueryDeviceState(display, device);
	
	if (!st)
	{
		fprintf(stderr, "Cannot query device state\n");
		goto done;
	}

	XInputClass* ic = st->data;
	if (ic->class != ButtonClass)
	{
		fprintf(stderr, "Specified device has no buttons\n");
		goto done;
	}

	XButtonState* bstate = (XButtonState*)ic;
	ret = bstate->buttons[button / 8] & (1 << button % 8);

done:
	XFreeDeviceState(st);
	return ret;
}

/**
 * Check each button on the device to determine if it's pressed.
 */
int find_pressed_button(Display* display, XDevice* device, int num_buttons)
{
	// Walk through all available buttons until we find one that's pressed
	for (int i = 1; i < num_buttons; ++i)
	{
		if (check_button_state(display, device, i))
		{
			return i;
		}
	}
	return -1;
}

/**
 * Help the user figure out what the desired device ID and button ID is.
 */
void do_calibrate(Display* display)
{
	bool found = false;
	printf("Press the mouse button you want to identify\n");
	int button = 0;
	Window root = RootWindow(display, 0);
	XGrabPointer(display, 
	             root, 
				 False, 
				 ButtonPressMask | ButtonReleaseMask,
				 GrabModeAsync,
				 GrabModeAsync,
				 root,
				 None,
				 CurrentTime);

	while (!found)
	{
		// Walk the list of mouse devices until we find a pressed button
		XDeviceInfo* info;
		int num_devices;

		// Get the list of devices each time in case it changes
		info = XListInputDevices(display, &num_devices);

		for (int i = 0; i < num_devices; ++i) 
		{
			int num_buttons = 0;

			if (info[i].use != IsXExtensionPointer)
			{
				continue;
			}

			XAnyClassPtr generic_info = info[i].inputclassinfo;
			if (generic_info->class == ButtonClass)
			{
				XButtonInfoPtr button_info = (XButtonInfoPtr)generic_info;
				num_buttons = button_info->num_buttons;
			}
			else
			{
				continue;
			}
			
			// Get an XDevice from the device info
			XDevice* device = XOpenDevice(display, info[i].id);
			button = find_pressed_button(display, device, num_buttons);
			if (button > 0)
			{
				printf("Found button: %s -> device %d button %d\n",
						info[i].name,
						(int)info[i].id,
						button);
				found = true;

			}
		}

		XFreeDeviceList(info);
	}
	XUngrabPointer(display, CurrentTime);
}

bool read_opts(int argc, char** argv, opts_t* opts)
{
	// Set defaults
	opts->click_button = 1;
	opts->trigger_button = -1;
	opts->delay_ms = 50;
	opts->device_id = -1;
	opts->calibrate_mode = false;
	opts->list_mode = false;

	for (int i = 1; i < argc; ++i)
	{
		if (argv[i][0] == '-')
		{
			// Ensure no reading off the end of the array
			switch (argv[i][1])
			{
			case 'd':
			case 'b':
			case 't':
			case 'v':
				if (i == argc - 1)
				{
					fprintf(stderr, "Parameter for %s missing\n", argv[i]);
					return false;
				}
				break;
			}
			// Parse flag
			switch (argv[i][1])
			{
			case 'd': // Delay
				opts->delay_ms = strtoul(argv[++i], NULL, 10);
				break;
			case 'b': // Button
				opts->click_button = strtol(argv[++i], NULL, 10);
				break;
			case 't': // Trigger
				opts->trigger_button = strtol(argv[++i], NULL, 10);
				break;
			case 'i': // Device ID
				opts->device_id = strtol(argv[++i], NULL, 10);
				break;
			case '-':
				if (strcmp(argv[i], "--calibrate") == 0)
				{
					opts->calibrate_mode = true;
					// Calibrate mode overrides other options
					return true;
				}
				else if (strcmp(argv[1], "--list") == 0)
				{
					opts->list_mode = true;
					// List mode overrides other options
					return true;
				}
				fprintf(stderr, "Unknown option %s\n", argv[i]);
				return false;
			default:
				fprintf(stderr, "Unknown option %s\n", argv[i]);
				return false;
			}
		}
		else
		{
			fprintf(stderr, "Unknown option %s\n", argv[i]);
			return false;
		}
	}
	return true;
}

int main(int argc, char** argv)
{
	Display* display = XOpenDisplay(NULL);
	opts_t opts;

	if (!read_opts(argc, argv, &opts) || opts.device_id < 0 || opts.trigger_button < 0)
	{
		printf("Usage: %s [-d delay_ms] [-b click_button] <-t trigger_button> <-i device_id>\n"
		       "       or\n"
			   "       %s --calibrate\n",
			   argv[0],
			   argv[0]);
		return EINVAL;
	}

	// Calibrate mode
	if (opts.calibrate_mode)
	{
		do_calibrate(display);
		return 0;
	}
	
	// List mode
	if (opts.list_mode)
	{
		find_mouse_device(display);
		return 0;
	}

	// Normal operation
	XDevice* device = XOpenDevice(display, opts.device_id);

	while (true)
	{
		if (check_button_state(display, device, opts.trigger_button))
		{
			do_click(display, opts.click_button);
		}
		msleep(opts.delay_ms);
	}


	XCloseDisplay(display);
	return 0;
}

