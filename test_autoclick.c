#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// TEST_BUILD is defined by compiler flag to exclude main() from autoclick.c
#include "autoclick.c"

// Test helper: create a temporary config file
static char* create_temp_config(const char* content)
{
	static char filename[256];
	// Reset template each time since mkstemp modifies it
	snprintf(filename, sizeof(filename), "/tmp/autoclick_test_XXXXXX");
	int fd = mkstemp(filename);
	if (fd == -1)
	{
		return NULL;
	}

	write(fd, content, strlen(content));
	close(fd);
	return filename;
}

// Test helper: cleanup temp file
static void cleanup_temp_config(const char* filename)
{
	if (filename)
	{
		unlink(filename);
	}
}

//
// Tests for get_config_type()
//

static void test_get_config_type_delay(void** state)
{
	(void)state;  // Unused

	const char* line = "delay 100\n";
	size_t pos = 0;
	config_type type = get_config_type(line, strlen(line), &pos);

	assert_int_equal(type, DELAY);
	assert_int_equal(pos, 6);  // Length of "delay " (skip past "delay" and space)
}

static void test_get_config_type_click_button(void** state)
{
	(void)state;

	const char* line = "click_button 1\n";
	size_t pos = 0;
	config_type type = get_config_type(line, strlen(line), &pos);

	assert_int_equal(type, CLICK_BUTTON);
	assert_int_equal(pos, 13);  // Length of "click_button "
}

static void test_get_config_type_trigger_button(void** state)
{
	(void)state;

	const char* line = "trigger_button 9\n";
	size_t pos = 0;
	config_type type = get_config_type(line, strlen(line), &pos);

	assert_int_equal(type, TRIGGER_BUTTON);
	assert_int_equal(pos, 15);  // Length of "trigger_button "
}

static void test_get_config_type_dev_id(void** state)
{
	(void)state;

	const char* line = "dev_id 10\n";
	size_t pos = 0;
	config_type type = get_config_type(line, strlen(line), &pos);

	assert_int_equal(type, DEV_ID);
	assert_int_equal(pos, 7);  // Length of "dev_id "
}

static void test_get_config_type_toggle_button(void** state)
{
	(void)state;

	const char* line = "toggle_button 8\n";
	size_t pos = 0;
	config_type type = get_config_type(line, strlen(line), &pos);

	assert_int_equal(type, TOGGLE_BUTTON);
	assert_int_equal(pos, 14);  // Length of "toggle_button "
}

static void test_get_config_type_dev_name(void** state)
{
	(void)state;

	const char* line = "dev_name Logitech M570\n";
	size_t pos = 0;
	config_type type = get_config_type(line, strlen(line), &pos);

	assert_int_equal(type, DEV_NAME);
	assert_int_equal(pos, 9);  // Length of "dev_name "
}

static void test_get_config_type_comment(void** state)
{
	(void)state;

	const char* line = "# This is a comment\n";
	config_type type = get_config_type(line, strlen(line), NULL);

	assert_int_equal(type, COMMENT);
}

static void test_get_config_type_blank(void** state)
{
	(void)state;

	const char* line = "\n";
	config_type type = get_config_type(line, strlen(line), NULL);

	assert_int_equal(type, BLANK);
}

static void test_get_config_type_blank_with_whitespace(void** state)
{
	(void)state;

	const char* line = "   \t  \n";
	config_type type = get_config_type(line, strlen(line), NULL);

	assert_int_equal(type, BLANK);
}

static void test_get_config_type_invalid(void** state)
{
	(void)state;

	const char* line = "invalid_option: 123\n";
	config_type type = get_config_type(line, strlen(line), NULL);

	assert_int_equal(type, INVALID);
}

//
// Tests for parse_config_file()
//

static void test_parse_config_file_valid(void** state)
{
	(void)state;

	const char* config_content =
		"delay 100\n"
		"click_button 2\n"
		"trigger_button 9\n"
		"dev_id 10\n";

	char* filename = create_temp_config(config_content);
	assert_non_null(filename);

	opts_t opts = {0};
	bool result = parse_config_file(filename, &opts);

	assert_true(result);
	assert_int_equal(opts.delay_ms, 100);
	assert_int_equal(opts.click_button, 2);
	assert_int_equal(opts.trigger_button, 9);
	assert_int_equal(opts.device_id, 10);

	cleanup_temp_config(filename);
}

static void test_parse_config_file_with_comments(void** state)
{
	(void)state;

	const char* config_content =
		"# Configuration file\n"
		"delay 50\n"
		"# This is a comment\n"
		"click_button 1\n"
		"\n"
		"trigger_button 8\n";

	char* filename = create_temp_config(config_content);
	assert_non_null(filename);

	opts_t opts = {0};
	bool result = parse_config_file(filename, &opts);

	assert_true(result);
	assert_int_equal(opts.delay_ms, 50);
	assert_int_equal(opts.click_button, 1);
	assert_int_equal(opts.trigger_button, 8);

	cleanup_temp_config(filename);
}

static void test_parse_config_file_with_device_name(void** state)
{
	(void)state;

	const char* config_content =
		"delay 50\n"
		"dev_name Logitech M570\n"
		"trigger_button 9\n";

	char* filename = create_temp_config(config_content);
	assert_non_null(filename);

	opts_t opts = {0};
	bool result = parse_config_file(filename, &opts);

	assert_true(result);
	assert_int_equal(opts.delay_ms, 50);
	assert_int_equal(opts.trigger_button, 9);
	assert_non_null(opts.device_name);
	assert_string_equal(opts.device_name, "Logitech M570");

	free(opts.device_name);  // Clean up the allocated memory
	cleanup_temp_config(filename);
}

static void test_parse_config_file_nonexistent(void** state)
{
	(void)state;

	opts_t opts = {0};
	bool result = parse_config_file("/tmp/nonexistent_config_file_xyz.conf", &opts);

	assert_false(result);
}

static void test_parse_config_file_with_toggle_button(void** state)
{
	(void)state;

	const char* config_content =
		"delay 75\n"
		"toggle_button 8\n"
		"dev_id 12\n";

	char* filename = create_temp_config(config_content);
	assert_non_null(filename);

	opts_t opts = {0};
	bool result = parse_config_file(filename, &opts);

	assert_true(result);
	assert_int_equal(opts.delay_ms, 75);
	assert_int_equal(opts.toggle_button, 8);
	assert_int_equal(opts.device_id, 12);
	// trigger_button not set in config, so remains 0 from initialization

	cleanup_temp_config(filename);
}

static void test_parse_config_file_with_trigger_and_toggle(void** state)
{
	(void)state;

	const char* config_content =
		"delay 100\n"
		"click_button 2\n"
		"trigger_button 9\n"
		"toggle_button 8\n"
		"dev_id 10\n";

	char* filename = create_temp_config(config_content);
	assert_non_null(filename);

	opts_t opts = {0};
	bool result = parse_config_file(filename, &opts);

	assert_true(result);
	assert_int_equal(opts.delay_ms, 100);
	assert_int_equal(opts.click_button, 2);
	assert_int_equal(opts.trigger_button, 9);
	assert_int_equal(opts.toggle_button, 8);
	assert_int_equal(opts.device_id, 10);

	cleanup_temp_config(filename);
}

//
// Tests for comp()
//

static void test_comp_exact_match(void** state)
{
	(void)state;

	const char* line = "delay: 100";
	bool result = comp(line, "delay", 5);

	assert_true(result);
}

static void test_comp_no_match(void** state)
{
	(void)state;

	const char* line = "click_button: 1";
	bool result = comp(line, "delay", 5);

	assert_false(result);
}

static void test_comp_line_too_short(void** state)
{
	(void)state;

	const char* line = "del";
	bool result = comp(line, "delay", 5);

	assert_false(result);
}

//
// Tests for read_opts()
//

static void test_read_opts_defaults(void** state)
{
	(void)state;

	char* argv[] = {"ac"};
	int argc = 1;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_true(result);
	assert_int_equal(opts.click_button, 1);
	assert_int_equal(opts.delay_ms, 50);
	assert_int_equal(opts.trigger_button, -1);
	assert_int_equal(opts.device_id, -1);
	assert_null(opts.device_name);
	assert_false(opts.calibrate_mode);
	assert_false(opts.list_mode);
}

static void test_read_opts_delay(void** state)
{
	(void)state;

	char* argv[] = {"ac", "-d", "100"};
	int argc = 3;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_true(result);
	assert_int_equal(opts.delay_ms, 100);
}

static void test_read_opts_button(void** state)
{
	(void)state;

	char* argv[] = {"ac", "-b", "2"};
	int argc = 3;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_true(result);
	assert_int_equal(opts.click_button, 2);
}

static void test_read_opts_trigger(void** state)
{
	(void)state;

	char* argv[] = {"ac", "-t", "9"};
	int argc = 3;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_true(result);
	assert_int_equal(opts.trigger_button, 9);
}

static void test_read_opts_device_id(void** state)
{
	(void)state;

	char* argv[] = {"ac", "-i", "10"};
	int argc = 3;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_true(result);
	assert_int_equal(opts.device_id, 10);
}

static void test_read_opts_device_name(void** state)
{
	(void)state;

	char* argv[] = {"ac", "-n", "Logitech M570"};
	int argc = 3;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_true(result);
	assert_string_equal(opts.device_name, "Logitech M570");
}

static void test_read_opts_multiple_options(void** state)
{
	(void)state;

	char* argv[] = {"ac", "-d", "200", "-b", "3", "-t", "8", "-i", "12"};
	int argc = 9;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_true(result);
	assert_int_equal(opts.delay_ms, 200);
	assert_int_equal(opts.click_button, 3);
	assert_int_equal(opts.trigger_button, 8);
	assert_int_equal(opts.device_id, 12);
}

static void test_read_opts_calibrate_mode(void** state)
{
	(void)state;

	char* argv[] = {"ac", "--calibrate"};
	int argc = 2;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_true(result);
	assert_true(opts.calibrate_mode);
}

static void test_read_opts_list_mode(void** state)
{
	(void)state;

	char* argv[] = {"ac", "--list"};
	int argc = 2;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_true(result);
	assert_true(opts.list_mode);
}

static void test_read_opts_config_file(void** state)
{
	(void)state;

	const char* config_content =
		"delay 75\n"
		"click_button 3\n"
		"trigger_button 7\n"
		"dev_id 11\n";

	char* filename = create_temp_config(config_content);
	assert_non_null(filename);

	char* argv[] = {"ac", "-f", filename};
	int argc = 3;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_true(result);
	assert_int_equal(opts.delay_ms, 75);
	assert_int_equal(opts.click_button, 3);
	assert_int_equal(opts.trigger_button, 7);
	assert_int_equal(opts.device_id, 11);

	cleanup_temp_config(filename);
}

static void test_read_opts_invalid_option(void** state)
{
	(void)state;

	char* argv[] = {"ac", "-x"};
	int argc = 2;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_false(result);
}

static void test_read_opts_missing_parameter(void** state)
{
	(void)state;

	char* argv[] = {"ac", "-d"};
	int argc = 2;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_false(result);
}

static void test_read_opts_unknown_long_option(void** state)
{
	(void)state;

	char* argv[] = {"ac", "--unknown"};
	int argc = 2;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_false(result);
}

static void test_read_opts_no_disable_default(void** state)
{
	(void)state;

	char* argv[] = {"ac", "--no-disable-default", "-t", "9", "-i", "10"};
	int argc = 6;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_true(result);
	assert_false(opts.disable_default_action);
}

static void test_read_opts_disable_default_is_default(void** state)
{
	(void)state;

	char* argv[] = {"ac", "-t", "9", "-i", "10"};
	int argc = 5;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_true(result);
	assert_true(opts.disable_default_action);
}

static void test_read_opts_toggle_button(void** state)
{
	(void)state;

	char* argv[] = {"ac", "-g", "8", "-i", "10"};
	int argc = 5;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_true(result);
	assert_int_equal(opts.toggle_button, 8);
	assert_int_equal(opts.trigger_button, -1);
}

static void test_read_opts_trigger_and_toggle(void** state)
{
	(void)state;

	char* argv[] = {"ac", "-t", "9", "-g", "8", "-i", "10"};
	int argc = 7;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_true(result);
	assert_int_equal(opts.trigger_button, 9);
	assert_int_equal(opts.toggle_button, 8);
}

static void test_read_opts_toggle_default(void** state)
{
	(void)state;

	char* argv[] = {"ac"};
	int argc = 1;
	opts_t opts = {0};

	bool result = read_opts(argc, argv, &opts);

	assert_true(result);
	assert_int_equal(opts.toggle_button, -1);
}

//
// Test main
//

int main(void)
{
	const struct CMUnitTest tests[] = {
		// get_config_type tests
		cmocka_unit_test(test_get_config_type_delay),
		cmocka_unit_test(test_get_config_type_click_button),
		cmocka_unit_test(test_get_config_type_trigger_button),
		cmocka_unit_test(test_get_config_type_toggle_button),
		cmocka_unit_test(test_get_config_type_dev_id),
		cmocka_unit_test(test_get_config_type_dev_name),
		cmocka_unit_test(test_get_config_type_comment),
		cmocka_unit_test(test_get_config_type_blank),
		cmocka_unit_test(test_get_config_type_blank_with_whitespace),
		cmocka_unit_test(test_get_config_type_invalid),

		// parse_config_file tests
		cmocka_unit_test(test_parse_config_file_valid),
		cmocka_unit_test(test_parse_config_file_with_comments),
		cmocka_unit_test(test_parse_config_file_with_device_name),
		cmocka_unit_test(test_parse_config_file_nonexistent),
		cmocka_unit_test(test_parse_config_file_with_toggle_button),
		cmocka_unit_test(test_parse_config_file_with_trigger_and_toggle),

		// comp tests
		cmocka_unit_test(test_comp_exact_match),
		cmocka_unit_test(test_comp_no_match),
		cmocka_unit_test(test_comp_line_too_short),

		// read_opts tests
		cmocka_unit_test(test_read_opts_defaults),
		cmocka_unit_test(test_read_opts_delay),
		cmocka_unit_test(test_read_opts_button),
		cmocka_unit_test(test_read_opts_trigger),
		cmocka_unit_test(test_read_opts_device_id),
		cmocka_unit_test(test_read_opts_device_name),
		cmocka_unit_test(test_read_opts_multiple_options),
		cmocka_unit_test(test_read_opts_calibrate_mode),
		cmocka_unit_test(test_read_opts_list_mode),
		cmocka_unit_test(test_read_opts_config_file),
		cmocka_unit_test(test_read_opts_invalid_option),
		cmocka_unit_test(test_read_opts_missing_parameter),
		cmocka_unit_test(test_read_opts_unknown_long_option),
		cmocka_unit_test(test_read_opts_no_disable_default),
		cmocka_unit_test(test_read_opts_disable_default_is_default),
		cmocka_unit_test(test_read_opts_toggle_button),
		cmocka_unit_test(test_read_opts_trigger_and_toggle),
		cmocka_unit_test(test_read_opts_toggle_default),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
