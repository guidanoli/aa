#ifndef yatester_cmd_h
#define yatester_cmd_h

/* Commands */

#include <stddef.h>

/**
 * @brief Command definition
 */
typedef struct
{
	const char *name; /**< Command name */
	size_t argc; /**< Number of arguments */
	void (*handler)(const char** argv); /**< Command handler */
}
yatester_command;

/* Terminates on command with name = NULL */
extern const yatester_command yatester_commands[];

#endif
