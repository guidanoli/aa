#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "tester.h"

/**********************************************
* STATIC VARIABLES DECLARATIONS
***********************************************/

static int externalReturnValue = 0; // external return value
static unsigned long line; // line count
static const char *returnValueInfos[TESTER_RETURN_COUNT]; // return value infos
static char buffer[TESTER_BUFFER_SIZE], // file line
            command[TESTER_BUFFER_SIZE], // command string
            argtemp[TESTER_BUFFER_SIZE], // command argument string
            sep[TESTER_BUFFER_SIZE], // separation characters
            temp[TESTER_BUFFER_SIZE]; // temp. variable
static char *cursor; // buffer cursor

/**********************************************
* STATIC FUNCTIONS DECLARATIONS
***********************************************/

static TesterReturnValue _TesterMain(int argc, char **argv);
static void _TesterLoadReturnValueInfos();
static TesterReturnValue _TesterParse(FILE *fp);
static TesterReturnValue _TesterParseCatchCommand(TesterReturnValue ret);
static int _TesterParseArg(const char *format, void *arg, int *inc);
static int _TesterParseStr(char *arg, int *inc);
static void _TesterPrintReturnValueInfo(TesterReturnValue ret);

/**********************************************
* EXTERN FUNCTIONS DEFINITIONS
***********************************************/

/**
* Usage: <program> [script-path]
* If script-path is not provided,
* then help strings are displayed.
*/
int main(int argc, char **argv)
{
    TesterReturnValue exitReturn, ret;
    ret = _TesterMain(argc, argv);
    exitReturn = TesterExitCallback();
    if (ret == TESTER_RETURN_OK)
        ret = exitReturn;
    _TesterPrintReturnValueInfo(ret);
    return ret;
}

int TesterParseArguments(const char *format, ...)
{
    va_list va;
    int inc = 0;
    void *arg;
    char *str;
    int argc = 0;
    if (format == NULL) return -1;
    va_start(va, format);
    for (; *format != '\0'; ++format, ++argc) {
        int parsingError = 0;
        switch (*format) {
        case 'f':
            arg = va_arg(va, float *);
            if ((parsingError = _TesterParseArg("%f", arg, &inc)) &&
                (parsingError = _TesterParseArg("%g", arg, &inc)) &&
                (parsingError = _TesterParseArg("%e", arg, &inc)))
                break;
            break;
        case 'i':
            arg = va_arg(va, int *);
            if ((parsingError = _TesterParseArg("%d", arg, &inc)) &&
                (parsingError = _TesterParseArg("%u", arg, &inc)))
                break;
            break;
        case 's':
            str = va_arg(va, char *);
            parsingError = _TesterParseStr(str, &inc);
            break;
        }
        if (parsingError) {
            va_end(va);
            return -1;
        }
    }
    va_end(va);
    cursor += inc;
    return argc;
}

TesterReturnValue TesterExternalValue(int value, const char *info)
{
    externalReturnValue = value;
    returnValueInfos[TESTER_RETURN_EXTERNAL] = info;
    return TESTER_RETURN_EXTERNAL;
}

void TesterPrintHelpStrings(FILE *fp)
{
    const char **str = TesterHelpStrings;
    for (; str && *str; ++str) puts(*str);
}

const char *TesterGetReturnValueInfo(TesterReturnValue returnValue)
{
    if (returnValue >= TESTER_RETURN_COUNT)
        return "Invalid return value";
    if (returnValue == TESTER_RETURN_EXTERNAL &&
        !returnValueInfos[returnValue])
        return "Undefined external value";
    return returnValueInfos[returnValue];
}

/**********************************************
* STATIC FUNCTIONS DEFINITIONS
***********************************************/

static TesterReturnValue _TesterMain(int argc, char **argv)
{
    FILE *fp;
    TesterReturnValue ret;
    // First, load return value informations
    _TesterLoadReturnValueInfos();
    // If no arguments were passed, then print help strings
    if (argc == 1) {
        TesterPrintHelpStrings(stdout);
        return TESTER_RETURN_OK;
    }
    // Open file whose path was passed as argument
    fp = fopen(argv[1], "r");
    if (fp == NULL)
        return TESTER_RETURN_FILE;
    // Initialize tester
    if (ret = TesterInitCallback())
        return ret;
    // Parse script
    if (ret = _TesterParse(fp))
        return ret;
    return TESTER_RETURN_OK;
}

static TesterReturnValue _TesterParse(FILE *fp)
{
    TesterReturnValue ret = TESTER_RETURN_OK;
    line = 1;
    // Read a line from the file and store it in a buffer
    while (fgets(buffer, sizeof(buffer), fp)) {
        size_t bufflen = strlen(buffer);
        // Make sure it doesn't overflow
        if (bufflen == TESTER_BUFFER_SIZE - 1)
            return TESTER_RETURN_BUFFER_OVERFLOW;
        // Iterate through the buffer with a cursor
        for (cursor = buffer; cursor < buffer + bufflen; ++cursor) {
            if (*cursor == '\t' || *cursor == ' ' || *cursor == '\n')
                continue; /* Ignore spacings */
            if (*cursor == '#')
                break; /* Ignore comments */
            if (*cursor == '/') {
                // Detect and parse commands
                if (sscanf(cursor, "/%[^ \t\n]", command) == 1) {
                    // Move the cursor to after the command
                    cursor += strlen(command) + 1;
                    if (strcmp(command, "catch") == 0) {
                        // Check if an error already occurred
                        if (ret = _TesterParseCatchCommand(ret))
                            return ret;
                    } else {
                        if (ret)
                            return ret;
                        // Call the command parser (can move cursor)
                        ret = TesterParseCallback(command);
                    }
                } else {
                    // Command does not match regex
                    return TESTER_RETURN_PARSING_COMMAND;
                }
            } else {
                // Unexpected character
                return TESTER_RETURN_UNEXPECTED_ARGUMENT;
            }
        }
        // Increase the line counter
        ++line;
    }
    return ret;
}
static void _TesterLoadReturnValueInfos()
{
    struct returnValue {
        TesterReturnValue value;
        const char *info;
    };
    size_t i;
    struct returnValue nativeValues[] = {
        {TESTER_RETURN_OK, "Success"},
        {TESTER_RETURN_FILE, "File error"},
        {TESTER_RETURN_MEMORY_LACK, "Could not allocate memory"},
        {TESTER_RETURN_MEMORY_LEAK, "Memory leak detected"},
        {TESTER_RETURN_BUFFER_OVERFLOW, "Buffer overflow"},
        {TESTER_RETURN_PARSING_COMMAND, "Malformed command"},
        {TESTER_RETURN_PARSING_ARGUMENT, "Malformed argument"},
        {TESTER_RETURN_UNEXPECTED_ARGUMENT, "Unexpected argument"},
        {TESTER_RETURN_UNEXPECTED_RETURN, "Unexpected return"},
        // TESTER_RETURN_EXTERNAL is not meant to be defined
        // See TesterGetReturnValueInfo
    };
    for (i = 0; i < sizeof(nativeValues)/sizeof(nativeValues[0]); ++i) {
        struct returnValue retVal = nativeValues[i];
        returnValueInfos[retVal.value] = retVal.info;
    }
}

static int _TesterParseArg(const char *format, void *arg, int *inc)
{
    if (arg == NULL) return -1;
    if (sscanf(cursor + *inc, "%[ \t\n]%[^ \t\n]", sep, temp) != 2)
        return -1;
    if (sscanf(temp, format, arg) != 1)
        return -1;
    *inc += strlen(sep) + strlen(temp);
    return 0;
}

static int _TesterParseStr(char *arg, int *inc)
{
    if (arg == NULL) return -1;
    if (sscanf(cursor + *inc, "%[ \t\n]\"%[^\"]\"", sep, temp) == 2) {
        *inc += 2; /* First checks if there are quotation marks */
    } else if (sscanf(cursor + *inc, "%[ \t\n]%[^ \t\n]", sep, temp) != 2) {
        return -1; /* Then attempts to parse without them */
    }
    strcpy(arg, temp);
    if (arg == NULL)
        return -1;
    *inc += strlen(sep) + strlen(temp);
    return 0;
}

static TesterReturnValue _TesterParseCatchCommand(TesterReturnValue ret)
{
    int caughtExternValue;
    TesterReturnValue caughtValue;
    if (TesterParseArguments("si", argtemp, &caughtExternValue) == 2 &&
        strcmp(argtemp, "ext") == 0) {
        if (ret != TESTER_RETURN_EXTERNAL)
            return TESTER_RETURN_UNEXPECTED_RETURN;
        if (caughtExternValue != externalReturnValue)
            return TESTER_RETURN_UNEXPECTED_RETURN;
    } else if (TesterParseArguments("i", &caughtValue) == 1) {
        if (caughtValue != ret)
            return TESTER_RETURN_UNEXPECTED_RETURN;
    } else {
        return TESTER_RETURN_PARSING_ARGUMENT;
    }
    return TESTER_RETURN_OK;
}

static void _TesterPrintReturnValueInfo(TesterReturnValue ret)
{
    if (ret) {
        size_t col = cursor - buffer, bufflen = strlen(buffer);
        fprintf(stderr, "Error on line %lu, column %lu: \"%s\"\n",
        line, cursor - buffer + 1, TesterGetReturnValueInfo(ret));
        fprintf(stderr, "%s", buffer);
        if (buffer[bufflen - 1] != '\n') fprintf(stderr, "\n");
        while (col--) fprintf(stderr, " ");
        fprintf(stderr, "^\n");
    } else {
        fprintf(stdout, "%s\n", TesterGetReturnValueInfo(ret));
    }
}