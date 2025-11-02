#include <X11/extensions/XTest.h>
#include <X11/extensions/XInput.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

typedef struct
{
	int click_button;
	int trigger_button;
	int toggle_button;
	int device_id;
	char* device_name;
	uint32_t delay_ms;
	const char* config_filename;

	// Alternate modes
	bool calibrate_mode;
	bool list_mode;

	// Button behavior
	bool disable_default_action;
} opts_t;

typedef enum
{
	DELAY,
	CLICK_BUTTON,
	TRIGGER_BUTTON,
	TOGGLE_BUTTON,
	DEV_ID,
	DEV_NAME,
	COMMENT,
	BLANK,
	INVALID
} config_type;


bool read_opts(int argc, char** argv, opts_t* opts);

/**
 * Sleep for the specified number of milliseconds.
 */
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
		if (info[i].use == IsXPointer || info[i].use == IsXExtensionPointer)
		{
			printf("Found pointing device (%d): %s -> %d\n", info[i].use, info[i].name, (int)info[i].id);
		}
	}

	XFreeDeviceList(info);
}

/**
 * Return the device ID for the device with the given name, or -1 if not found.
 */
int get_device_id_from_name(Display* display, const char* name)
{
	XDeviceInfo* info;
	int num_devices;
	int ret = -1;

	info = XListInputDevices(display, &num_devices);

	for (int i = 0; i < num_devices; ++i)
	{
		if (info[i].use == IsXPointer || info[i].use == IsXExtensionPointer)
		{
			if (strcmp(name, info[i].name) == 0)
			{
				ret = (int)info[i].id;
				break;
			}
		}
	}

	XFreeDeviceList(info);

	return ret;
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
 * Disable the default action of a button using XI1 grab.
 * Returns true on success, false on failure.
 */
bool disable_button_default_action(Display* display, XDevice* device, int button)
{
	Window root = DefaultRootWindow(display);

	// Use XI1 XGrabDeviceButton instead of XI2 XIGrabButton
	// This should be more compatible with our XI1 polling approach
	int result = XGrabDeviceButton(display,
	                               device,
	                               button,
	                               AnyModifier,
	                               NULL,           // modifier_device
	                               root,
	                               True,           // owner_events
	                               0,              // event_count (we don't want events)
	                               NULL,           // event_list
	                               GrabModeAsync,  // this_device_mode
	                               GrabModeAsync); // other_devices_mode

	if (result != Success)
	{
		return false;
	}

	return true;
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
				printf("Found button: %s -> device %d button %d\n", info[i].name, (int)info[i].id, button);
				printf("\nTo use this button as a trigger, run one of these commands:\n");
				printf("  ./ac -i %d -t %d\n", (int)info[i].id, button);
				printf("  ./ac -n \"%s\" -t %d\n", info[i].name, button);
				found = true;
				break;
			}
		}

		XFreeDeviceList(info);
	}
	XUngrabPointer(display, CurrentTime);
}

/**
 * Compare a line in the config file with the name of a config parameter.
 *
 * Sort of like strcmp but more casual with \0 terminators.
 */
bool comp(const char* config_line, const char* config_parm, size_t parmsize)
{
	if (strlen(config_line) < parmsize)
	{
		return false;
	}
	return memcmp(config_line, config_parm, parmsize) == 0;
}

/**
 * Compare a key in the config file with the given option type.
 */
#define check_config(_str, _type)          \
	parmsize = strlen(_str);               \
	if (pos) *pos = parmsize + 1;          \
	if (comp(config_line, _str, parmsize)) \
	{                                      \
		return _type;                      \
	}

config_type get_config_type(const char* config_line, size_t line_len, size_t* pos)
{
	bool start = true;
	size_t parmsize = 0;

	if (line_len == 0)
	{
		return BLANK;
	}

	for (size_t i = 0; i < line_len; ++i)
	{
		// Ignore leading whitespace
		if (start && (config_line[i] == ' ' || config_line[i] == '\t'))
		{
			continue;
		}

		// EOL implies blank line
		if (start && config_line[i] == '\n')
		{
			return BLANK;
		}

		// Check if this is a comment line
		if (config_line[i] == '#')
		{
			if (start)
			{
				return COMMENT;
			}
			// I really don't think we should get here
			return INVALID;
		}

		start = false;

		// This switch just optimizes the number of strcmps we need to do
		switch (config_line[i])
		{
		case 'c':
			check_config("click_button", CLICK_BUTTON);
			return INVALID;
		case 't':
			check_config("trigger_button", TRIGGER_BUTTON);
			check_config("toggle_button", TOGGLE_BUTTON);
			return INVALID;
		case 'd':
			check_config("delay", DELAY);
			check_config("dev_id", DEV_ID);
			check_config("dev_name", DEV_NAME);
			return INVALID;
		default:
			return INVALID;
		}
	}
	return INVALID;
}

/**
 * Read an integer from a config file line.
 */
#define read_int(_dest)                      \
	value = atoi(&line[pos]);                \
	if (value > 0)                           \
	{                                        \
		_dest = value;                       \
	}                                        \
	else                                     \
	{                                        \
		fprintf(stderr, "Config error: Couldn't parse line '%s'\n", line); \
		return false;                        \
	}

/**
 * Gross config file parsing logic.
 *
 * Don't read this unless you absolutely have to.
 */
bool parse_config_file(const char* filename, opts_t* opts)
{
	FILE* fp = NULL;
	char* line = NULL;
	size_t line_len = 0;
	ssize_t read_len = 0;
	int value = -1;
	int line_num = 0;

	// Open the file
	fp = fopen(filename, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "Error opening file %s for reading\n", filename);
		return false;
	}

	// Read and process each line
	while ((read_len = getline(&line, &line_len, fp)) != -1)
	{
		++line_num;
		size_t pos;
		config_type t = get_config_type(line, line_len, &pos);

		// Read the value for the parameter
		switch (t)
		{
		case DELAY:
			read_int(opts->delay_ms);
			break;
		case CLICK_BUTTON:
			read_int(opts->click_button);
			break;
		case DEV_ID:
			read_int(opts->device_id);
			break;
		case TRIGGER_BUTTON:
			read_int(opts->trigger_button);
			break;
		case TOGGLE_BUTTON:
			read_int(opts->toggle_button);
			break;
		case DEV_NAME:
		{
			int i = 0;

			// Allocate a buffer for the device name and copy the name from the file into it
			// +1 for null terminator (this gets leaked but it doesn't matter)
			opts->device_name = malloc(strlen(&line[pos]) + 1);
			if (opts->device_name == NULL)
			{
				fprintf(stderr, "Memory allocation failed\n");
				fclose(fp);
				if (line != NULL)
				{
					free(line);
				}
				return false;
			}
			for (char c = line[pos++]; c != '#' && c != '\n' && c != '\0'; c = line[pos++])
			{
				opts->device_name[i++] = c;
			}
			opts->device_name[i] = '\0';
		}
			break;
		case COMMENT:
		case BLANK:
			continue;
		case INVALID:
			fprintf(stderr, "Error reading config file on line %d\n", line_num);
			return false;
		}
	}

	fclose(fp);
	if (line != NULL)
	{
		free(line);
	}
	return true;
}

bool read_opts(int argc, char** argv, opts_t* opts)
{
	// Set defaults
	opts->click_button = 1;
	opts->trigger_button = -1;
	opts->toggle_button = -1;
	opts->delay_ms = 50;
	opts->device_id = -1;
	opts->device_name = NULL;
	opts->calibrate_mode = false;
	opts->list_mode = false;
	opts->disable_default_action = true;

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
			case 'g':
			case 'i':
			case 'n':
			case 'f':
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
			case 'd':  // Delay
				opts->delay_ms = strtoul(argv[++i], NULL, 10);
				break;
			case 'b':  // Button
				opts->click_button = strtol(argv[++i], NULL, 10);
				break;
			case 't':  // Trigger
				opts->trigger_button = strtol(argv[++i], NULL, 10);
				break;
			case 'g':  // Toggle
				opts->toggle_button = strtol(argv[++i], NULL, 10);
				break;
			case 'i':  // Device ID
				opts->device_id = strtol(argv[++i], NULL, 10);
				break;
			case 'n':  // Device name
				opts->device_name = argv[++i];
				break;
			case 'f':  // Config file name
				opts->config_filename = argv[++i];
				return parse_config_file(opts->config_filename, opts);
			case '-':
				if (strcmp(argv[i], "--calibrate") == 0)
				{
					opts->calibrate_mode = true;
					// Calibrate mode overrides other options
					return true;
				}
				else if (strcmp(argv[i], "--list") == 0)
				{
					opts->list_mode = true;
					// List mode overrides other options
					return true;
				}
				else if (strcmp(argv[i], "--no-disable-default") == 0)
				{
					opts->disable_default_action = false;
					break;
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

void usage(const char* prog_name)
{
	printf(
	    "Usage: %s [-d delay_ms] [-b click_button] [--no-disable-default] <-t trigger_button | -g toggle_button> <-i device_id | -n device_name>\n"
	    "       or\n"
	    "       %s <-f path_to_config_file>\n"
	    "       or\n"
	    "       %s --calibrate\n"
	    "       or\n"
	    "       %s --list\n"
	    "\n"
	    "Options:\n"
	    "  -d delay_ms              Delay between clicks in milliseconds (default: 50)\n"
	    "  -b click_button          Button ID to click (default: 1)\n"
	    "  -t trigger_button        Button ID that triggers clicks while held\n"
	    "  -g toggle_button         Button ID that toggles clicking on/off\n"
	    "  -i device_id             Device ID for the pointing device\n"
	    "  -n device_name           Device name for the pointing device\n"
	    "  -f config_file           Path to configuration file\n"
	    "  --no-disable-default     Don't disable button's default action\n"
	    "  --calibrate              Interactive mode to identify button IDs\n"
	    "  --list                   List all pointing devices\n"
	    "\n"
	    "Notes:\n"
	    "  - At least one of -t or -g is required\n"
	    "  - Both -t and -g can be used together (must be different buttons)\n"
	    "  - Trigger button (-t): Clicks while the button is held down\n"
	    "  - Toggle button (-g): First press starts clicking, second press stops\n",
	    prog_name,
	    prog_name,
	    prog_name,
	    prog_name);
}

#ifndef TEST_BUILD
int main(int argc, char** argv)
{
	Display* display = XOpenDisplay(NULL);
	opts_t opts;

	if (display == NULL)
	{
		fprintf(stderr, "Cannot open X display\n");
		return 1;
	}

	if (!read_opts(argc, argv, &opts))
	{
		usage(argv[0]);
		return EINVAL;
	}

	// If device name is specified, convert to device ID
	if (opts.device_name != NULL)
	{
		if (opts.device_id > 0)
		{
			fprintf(stderr, "Cannot specify both device ID and device name\n");
			usage(argv[0]);
			XCloseDisplay(display);
			return EINVAL;
		}
		opts.device_id = get_device_id_from_name(display, opts.device_name);
		if (opts.device_id < 0)
		{
			fprintf(stderr, "Device '%s' not found. Use --list to see available devices.\n", opts.device_name);
			XCloseDisplay(display);
			return EINVAL;
		}
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

	// Normal operation - validate required options
	if (opts.device_id < 0)
	{
		fprintf(stderr, "Error: Device ID or device name is required\n");
		usage(argv[0]);
		return EINVAL;
	}

	if (opts.trigger_button < 0 && opts.toggle_button < 0)
	{
		fprintf(stderr, "Error: At least one of -t (trigger) or -g (toggle) is required\n");
		usage(argv[0]);
		return EINVAL;
	}

	// Validate that trigger and toggle buttons are different if both specified
	if (opts.trigger_button >= 0 && opts.toggle_button >= 0 &&
	    opts.trigger_button == opts.toggle_button)
	{
		fprintf(stderr, "Error: Trigger button (-t) and toggle button (-g) must be different\n");
		return EINVAL;
	}

	//
	// Main program logic
	//
	XDevice* device = XOpenDevice(display, opts.device_id);

	if (device == NULL)
	{
		fprintf(stderr, "Cannot open device with ID %d\n", opts.device_id);
		XCloseDisplay(display);
		return 1;
	}

	// Disable the default action of buttons if requested
	if (opts.disable_default_action)
	{
		if (opts.trigger_button >= 0)
		{
			if (!disable_button_default_action(display, device, opts.trigger_button))
			{
				fprintf(stderr, "Warning: Failed to disable default action for trigger button %d\n", opts.trigger_button);
				fprintf(stderr, "The button will still trigger its normal action.\n");
				fprintf(stderr, "You can suppress this with --no-disable-default\n");
			}
		}
		if (opts.toggle_button >= 0)
		{
			if (!disable_button_default_action(display, device, opts.toggle_button))
			{
				fprintf(stderr, "Warning: Failed to disable default action for toggle button %d\n", opts.toggle_button);
				fprintf(stderr, "The button will still trigger its normal action.\n");
				fprintf(stderr, "You can suppress this with --no-disable-default\n");
			}
		}
	}

	// State tracking for toggle button
	bool toggle_active = false;
	bool toggle_prev_pressed = false;

	while (true)
	{
		bool should_click = false;

		// Check trigger button if specified
		if (opts.trigger_button >= 0)
		{
			if (check_button_state(display, device, opts.trigger_button))
			{
				should_click = true;
			}
		}

		// Check toggle button if specified
		if (opts.toggle_button >= 0)
		{
			bool toggle_pressed = check_button_state(display, device, opts.toggle_button);

			// Detect transition from not-pressed to pressed (button press event)
			if (toggle_pressed && !toggle_prev_pressed)
			{
				toggle_active = !toggle_active;
			}

			toggle_prev_pressed = toggle_pressed;

			// If toggle is active, we should click
			if (toggle_active)
			{
				should_click = true;
			}
		}

		// Perform click if any condition is met
		if (should_click)
		{
			do_click(display, opts.click_button);
		}

		msleep(opts.delay_ms);
	}

	XCloseDisplay(display);
	return 0;
}
#endif  // TEST_BUILD
