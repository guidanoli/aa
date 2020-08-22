#include <aa/posixstring.h>
#include <stddef.h>
#include <stdio.h>

#include <tester/tester.h>

#define matches(command, str) (strcmp(command, str) == 0)

char buffer[BUFSIZ];

const char *TesterHelpStrings[] = {
	"/help                  Prints help strings, just like this one",
	"/print [s|i|f] <text>  Prints text formated as string, integer or float",
	"/throw                 Throws custom error",
	NULL,
};

TesterReturnValue TesterInitCallback()
{
	puts("TesterInitCallback called");
	return TESTER_OK;
}

TesterReturnValue TesterParseCallback(const char *command)
{
	if matches(command, "help") {
		TesterPrintHelpStrings();
	} else if matches(command, "print") {
		if (TesterParseArguments("s", buffer) != 1)
			return TESTER_ARGUMENT;
		if (strlen(buffer) != 1)
			return TESTER_ARGUMENT;
		if matches(buffer, "s") {
			if (TesterParseArguments("s", buffer) != 1)
				return TESTER_ARGUMENT;
			printf("String: '%s'\n", buffer);
		} else if matches(buffer, "i") {
			int i;
			if (TesterParseArguments("i", &i) != 1)
				return TESTER_ARGUMENT;
			printf("Integer: '%d'\n", i);
		} else if matches(buffer, "l") {
			long l;
			if (TesterParseArguments("l", &l) != 1)
				return TESTER_RETURN;
			printf("Long: '%ld'\n", l);
		} else if matches(buffer, "f") {
			float f;
			if (TesterParseArguments("f", &f) != 1)
				return TESTER_ARGUMENT;
			printf("Float: '%f'\n", f);
		} else if matches(buffer, "z") {
			size_t z;
			if (TesterParseArguments("z", &z) != 1)
				return TESTER_ARGUMENT;
			printf("Size Type: '%zu'\n", z);
		} else {
			return TESTER_ARGUMENT;
		}
	} else if matches(command, "log") {
		if (TesterParseArguments("s", buffer) != 1)
			return TESTER_ARGUMENT;
		TesterLog(buffer);
	} else if matches(command, "throw") {
		if (TesterParseArguments("s", buffer) != 1)
			return TESTER_ARGUMENT;
		return TesterExternalReturnValue(buffer);
	} else {
		return TESTER_COMMAND;
	}
	return TESTER_OK;
}

TesterReturnValue TesterExitCallback()
{
	puts("TesterExitCallback called");
	return TESTER_OK;
}
