#include <yatester/yatester.h>
#include <yatester/status.h>
#include <yatester/cmdhdl.h>
#include <yatester/runner.h>
#include <yatester/parser.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include <argvp/argvp.h>

static jmp_buf env;
static yadsl_ArgvParserHandle *argvp;
static int is_expecting_status;
static yatester_status expected_status;
static const char *input_fname;
static FILE *input_fp;
#ifdef YADSL_DEBUG
static const char *log_fname;
static FILE *log_fp;
#endif

/**
 * @brief Initialize program
 * @param argc argument count
 * @param argv argument vector
 * @return operation status code
 */
static yatester_status initialize_internal(int argc, char** argv)
{
	int nmatches;
#ifdef YADSL_DEBUG
	float malloc_failing_rate;
	size_t malloc_failing_index;
	unsigned int prng_seed;
	const char *log_channel_name;
#endif

	static const yadsl_ArgvKeywordArgumentDef kwargsdef[] =
	{
		{ "--help", 0 },
		{ "--list-commands", 0 },
		{ "--input-file", 1 },
		{ "--expect", 1 },
#ifdef YADSL_DEBUG
		{ "--log-file", 1 },
		{ "--malloc-failing-rate", 1 },
		{ "--malloc-failing-index", 1 },
		{ "--prng-seed", 1 },
		{ "--enable-log-channel", 1 },
#endif
		{ NULL, 0 }, /* End of definitions array */
	};

	argvp = yadsl_argvp_create(argc, argv);

	if (argvp == NULL)
	{
		return yatester_report(YATESTER_NOMEM, "Could not allocate argument vector parser");
	}

	yadsl_argvp_add_keyword_arguments(argvp, kwargsdef);

	nmatches = yadsl_argvp_parse_keyword_argument_value(argvp, "--expect", 0, "%d", &expected_status);

	if (nmatches == 0)
	{
		return yatester_report(YATESTER_ERROR, "Invalid value passed to --expect option");
	}
	else if (nmatches == 1)
	{
		is_expecting_status = 1;
	}

	input_fname = yadsl_argvp_get_keyword_argument_value(argvp, "--input-file", 0);
	
	if (input_fname == NULL)
	{
		input_fp = stdin;
	}
	else
	{
		input_fp = fopen(input_fname, "r");
		if (input_fp == NULL)
		{
			return yatester_report(YATESTER_IOERR, "Could not open file \"%s\" in reading mode\nReason: %s", input_fname, strerror(errno));
		}
	}

#ifdef YADSL_DEBUG
	log_fname = yadsl_argvp_get_keyword_argument_value(argvp, "--log-file", 0);

	if (log_fname == NULL)
	{
		log_fp = NULL;
	}
	else
	{
		log_fp = fopen(log_fname, "w");
		if (log_fp == NULL)
		{
			return yatester_report(YATESTER_IOERR, "Could not open file \"%s\" in writing mode\nReason: %s", log_fname, strerror(errno));
		}
	}

	yadsl_memdb_set_logger(log_fp);

	nmatches = yadsl_argvp_parse_keyword_argument_value(argvp, "--malloc-failing-rate", 0, "%f", &malloc_failing_rate);

	if (nmatches == 1)
	{
		yadsl_memdb_set_fail_rate(malloc_failing_rate);
	}
	else if (nmatches == 0)
	{
		return yatester_report(YATESTER_ERROR, "Invalid value passed to --malloc-failing-rate option");
	}
	else
	{
		yadsl_memdb_set_fail_rate(0.f);
	}

	nmatches = yadsl_argvp_parse_keyword_argument_value(argvp, "--malloc-failing-index", 0, "%zu", &malloc_failing_index);

	if (nmatches == 1)
	{
		yadsl_memdb_set_fail_by_index(true);
		yadsl_memdb_set_fail_index(malloc_failing_index);
	}
	else if (nmatches == 0)
	{
		return yatester_report(YATESTER_ERROR, "Invalid value passed to --malloc-failing-index option");
	}
	else
	{
		yadsl_memdb_set_fail_by_index(false);
	}

	nmatches = yadsl_argvp_parse_keyword_argument_value(argvp, "--prng-seed", 0, "%u", &prng_seed);

	if (nmatches == 1)
	{
		yadsl_memdb_set_prng_seed(prng_seed);
	}
	else if (nmatches == 0)
	{
		return yatester_report(YATESTER_ERROR, "Invalid value passed to --prng-seed option");
	}

	log_channel_name = yadsl_argvp_get_keyword_argument_value(argvp, "--enable-log-channel", 0);

	while (log_channel_name != NULL)
	{
		yadsl_MemDebugLogChannel log_channel_value;

		if (strcmp(log_channel_name, "ALLOCATION") == 0)
		{
			log_channel_value = YADSL_MEMDB_LOG_CHANNEL_ALLOCATION;
		}
		else if (strcmp(log_channel_name, "DEALLOCATION") == 0)
		{
			log_channel_value = YADSL_MEMDB_LOG_CHANNEL_DEALLOCATION;
		}
		else if (strcmp(log_channel_name, "LEAKAGE") == 0)
		{
			log_channel_value = YADSL_MEMDB_LOG_CHANNEL_LEAKAGE;
		}
		else
		{
			return yatester_report(YATESTER_ERROR, "Invalid value \"%s\" passed to --enable-log-channel option", log_channel_name);
		}

		yadsl_memdb_log_channel_set(log_channel_value, true);

		log_channel_name = yadsl_argvp_get_keyword_argument_value(argvp, NULL, 0);
	}
#endif

	return YATESTER_OK;
}

/**
 * @brief Terminate program
 * @param status current status code
 * @return processed status code
 */
static yatester_status terminate_internal(yatester_status status)
{
	/* Terminate modules */
	yatester_terminatecmdhdl();
	yatester_terminateparser();

#ifdef YADSL_DEBUG
	if (log_fp != NULL)
	{
		yadsl_memdb_set_logger(NULL);
		fclose(log_fp);
		log_fp = NULL;
	}
#endif

	if (input_fp != NULL)
	{
		fclose(input_fp);
		input_fp = NULL;
	}

	if (argvp != NULL)
	{
		yadsl_argvp_destroy(argvp);
		argvp = NULL;
	}

#ifdef YADSL_DEBUG
	if (status == YATESTER_OK)
	{
		if (yadsl_memdb_amb_list_size() > 0)
		{
			status = yatester_report(YATESTER_MEMLK, "The memory debugger has detected a leakage");
		}
		else if (yadsl_memdb_error_occurred())
		{
			status = yatester_report(YATESTER_FTLERR, "The memory debugger has detected an error");
		}
	}

	yadsl_memdb_clear_amb_list();

	if (status == YATESTER_NOMEM && yadsl_memdb_fail_occurred())
	{
		status = YATESTER_OK;
	}
#endif

	if (is_expecting_status)
	{
		if (status == expected_status)
		{
			status = YATESTER_OK;
		}
		else
		{
			status = yatester_report(YATESTER_ERROR, "Expected and real status codes differ");
		}
	}

	return status;
}

/**
 * @brief Print command and the number of arguments it expects
 * @param command
 */
static void printcmd_cb(const yatester_command* command)
{
	printf("%s\n", command->name);
}

/**
 * @brief Asserts status is OK
 * @note If not, performs a long jump
 */
static void assert_ok(yatester_status status)
{
	if (status != YATESTER_OK)
	{
		longjmp(env, status);
	}
}

int main(int argc, char** argv)
{
	/* Set jump buffer */
	yatester_status status = setjmp(env);

	/* If long jump was performed */
	if (status != YATESTER_OK)
	{
		goto end;
	}

	/* Initialize program */
	assert_ok(initialize_internal(argc, argv));

	if (yadsl_argvp_has_keyword_argument(argvp, "--help"))
	{
		fprintf(stderr, "Yet Another Tester\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "Usage: %s [options]\n", argv[0]);
		fprintf(stderr, "\n");
		fprintf(stderr, "Options:\n");
		fprintf(stderr, "--help                          Print usage information and exit\n");
		fprintf(stderr, "--list-commands                 List all valid commands and exit\n");
		fprintf(stderr, "--input-file <filepath>         Read commands from file instead of stdin\n");
		fprintf(stderr, "--expect <status>               Expect the tester to return a status code\n");
#ifdef YADSL_DEBUG
		fprintf(stderr, "--log-file <filepath>           Write log to file instead of stderr\n");
		fprintf(stderr, "--malloc-failing-rate <rate>    Set memory allocation failing rate\n");
		fprintf(stderr, "--malloc-failing-index <index>  Set memory allocation failing index\n");
		fprintf(stderr, "--prng-seed <seed>              Set pseudorandom number generator seed\n");
		fprintf(stderr, "--enable-log-channel <channel>  Enable one of the following log channels:\n");
		fprintf(stderr, "                                ALLOCATION, DEALLOCATION, LEAKAGE\n");
#endif
		goto end;
	}

	/* Initialize modules */
	assert_ok(yatester_initializeparser());
	assert_ok(yatester_initializecmdhdl());

	if (yadsl_argvp_has_keyword_argument(argvp, "--list-commands"))
	{
		yatester_itercommands(printcmd_cb);
		goto end;
	}

	/* Parse script */
	assert_ok(yatester_parsescript(input_fp));

end:
	return terminate_internal(status);
}
