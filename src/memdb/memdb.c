#define YADSL_MEMDB_DONT_DEFINE_MACROS
#include <memdb/memdb.h>

#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>

#if defined(_MSC_VER)
# pragma warning(disable : 4996)
#endif

/**
 * @brief Return code used internally
*/
typedef enum
{
	YADSL_MEMDB_RET_OK = 0, /**< All went ok */
	YADSL_MEMDB_RET_COPY, /**< Found duplicate node */
	YADSL_MEMDB_RET_NOT_FOUND, /**< Node not found */
	YADSL_MEMDB_RET_MEMORY, /**< Could not allocate memory */
}
yadsl_MemDebugRet;

/**
 * @brief List of allocated memory blocks (AMB)
*/
struct yadsl_MemDebugAMB_s
{
	struct yadsl_MemDebugAMB_s* next; /**< Next node (nullable) */
	const char* funcname; /**< Name of function called to allocate it */
	const char* file; /**< File where function was called */
	int line; /**< Line where function was called, in file */
	size_t size; /**< Size of memory block */
	void* amb; /**< Pointer to memory block (unique) */
};

typedef struct yadsl_MemDebugAMB_s yadsl_MemDebugAMB;

/* Globals */

static yadsl_MemDebugAMB* amb_list_head; /**< AMB list head (nullable) */
static size_t amb_list_size; /**< AMB list size */
static size_t amb_total_count; /**< AMB total count */

static uint8_t log_channels; /**< Log channels bitmap */
static FILE* log_fp; /**< Log file pointer (nullable) */

static bool fail_by_index; /**< Fail by index flag */
static bool error_occurred; /**< Error occurred flag */
static bool fail_occurred; /**< Fail occurred flag */

static float fail_rate; /**< Allocation fail rate */
static size_t fail_index; /**< Allocation fail_by_prng index */
static unsigned int prng_state; /**< PRNG seed */

/* Functions */

static int
yadsl_memdb_prng_internal()
{
	prng_state = ((prng_state * 1103515245) + 12345) & 0x7fffffff;
	return prng_state;
}

bool
yadsl_memdb_log_channel_get(
	yadsl_MemDebugLogChannel log_channel)
{
	return log_channels & (1 << log_channel);
}

void
yadsl_memdb_log_channel_set(
	yadsl_MemDebugLogChannel log_channel,
	bool enable)
{
	if (enable)
		log_channels |= (1 << log_channel);
	else
		log_channels &= ~(1 << log_channel);
}

/**
 * @brief Get label for log channel
 * @param log_channel log channel
 * @return label or NULL if nonexistent
*/
static const char*
yadsl_memdb_log_channel_label_get_internal(
		yadsl_MemDebugLogChannel log_channel)
{
	switch (log_channel) {
	case YADSL_MEMDB_LOG_CHANNEL_ALLOCATION:
		return "ALLOC";
	case YADSL_MEMDB_LOG_CHANNEL_DEALLOCATION:
		return "DEALLOC";
	case YADSL_MEMDB_LOG_CHANNEL_LEAKAGE:
		return "LEAK";
	default:
		return NULL;
	}
}

/**
 * @brief Log message to a specific channel
 * @param log_channel log channel
 * @param format see fprintf function family
 * @param ... see fprintf function family
*/
static void
yadsl_memdb_log_internal(
		yadsl_MemDebugLogChannel log_channel,
		const char* format,
		...)
{
	va_list va;
	const char* label;
	FILE* fp;

	/* Check if log channel is temp */
	if (!yadsl_memdb_log_channel_get(log_channel))
		return;

	/* Use stderr by default if no file pointer is given */
	fp = log_fp ? log_fp : stderr;

	/* Start varadic arguments */
	va_start(va, format);

	/* Get log channel label */
	label = yadsl_memdb_log_channel_label_get_internal(log_channel);

	if (label)
		fprintf(fp, "[MEMDB<<%s] ", label);
	else
		fprintf(fp, "[MEMDB] ");
	
	vfprintf(fp, format, va);

	/* End varadic arguments */
	va_end(va);

	fprintf(fp, "\n");
}

bool
yadsl_memdb_error_occurred()
{
	return error_occurred;
}

bool
yadsl_memdb_fail_occurred()
{
	return fail_occurred;
}

/**
 * @brief Bernoulli trial with probability of fail_rate
 * @return whether allocation should fail (true) or not (false)
*/
static bool
yadsl_memdb_fail_by_prng_internal()
{
	return yadsl_memdb_prng_internal() < (int) (fail_rate * (float) RAND_MAX);
}

/**
 * @brief Checks if AMB total count coincides with failing index and
 *        whether failing by index is enabled
 * @return whether allocation should fail (true) or not (false)
*/
static bool
yadsl_memdb_fail_by_index_internal()
{
	return fail_by_index && (amb_total_count == fail_index);
}

/**
 * @brief Checks if memory allocation should fail
 * @return whether allocation should fail (true) or not (false)
*/
static bool
yadsl_memdb_fail_internal()
{
	bool fail = yadsl_memdb_fail_by_prng_internal() ||
	            yadsl_memdb_fail_by_index_internal();
	fail_occurred = fail_occurred || fail;
	return fail;
}

/**
 * @brief Find node holding AMB
 * @param camb pointer to an AMB
 * @param prev_ptr previous node (NULL = first node)
 * @return
 * * node, and *prev_ptr is updated if not NULL
 * * NULL if node was not found
*/
static yadsl_MemDebugAMB*
yadsl_memdb_find_amb_internal(
		void* amb,
		yadsl_MemDebugAMB** prev_ptr)
{
	yadsl_MemDebugAMB* node = amb_list_head, *prev = NULL;
	for (; node; node = node->next) {
		if (node->amb == amb) {
			if (prev_ptr)
				*prev_ptr = prev;
			return node;
		}
		prev = node;
	}
	return NULL;
}

/**
 * @brief Add node to AMB list
 * @params see yadsl_MemDebugAMB
 * @return
 * * ::YADSL_MEMDB_RET_OK
 * * ::YADSL_MEMDB_RET_COPY
 * * ::YADSL_MEMDB_RET_MEMORY
*/
static yadsl_MemDebugRet
yadsl_memdb_add_amb_internal(
		const char* funcname,
		void* amb,
		size_t size,
		const char* file,
		const int line)
{
	yadsl_MemDebugAMB* node;

	/* Try to find node holding AMB */
	if (node = yadsl_memdb_find_amb_internal(amb, NULL)) {

		/* Log copy error */
		yadsl_memdb_log_internal(YADSL_MEMDB_LOG_CHANNEL_ALLOCATION,
			"Tried to add %p (%zuB @ %s:%d by %s) to the list but found copy "
			" (%zuB @ %s:%d by %s)",
			amb, size, file, line, funcname,
			node->size, node->file, node->line, node->funcname);

		error_occurred = true;

		return YADSL_MEMDB_RET_COPY;
	}

	/* Allocate node */
	node = malloc(sizeof(yadsl_MemDebugAMB));
	if (node) {
		node->amb = amb;
		node->size = size;
		node->file = file;
		node->line = line;
		node->next = amb_list_head;
		node->funcname = funcname;
		
		/* Append node to AMB list */
		amb_list_head = node;
		++amb_list_size;
		++amb_total_count;

		/* Log allocation */
		yadsl_memdb_log_internal(YADSL_MEMDB_LOG_CHANNEL_ALLOCATION,
			"%s(%zu) @ %s:%d -> %p",
			funcname, size, file, line, amb);

		return YADSL_MEMDB_RET_OK;
	} else {
		return YADSL_MEMDB_RET_MEMORY;
	}
}

/**
 * @brief Remove node from AMB list
 * @param camb pointer to AMB
 * @return
 * * ::YADSL_MEMDB_RET_OK
 * * ::YADSL_MEMDB_RET_NOT_FOUND
*/
static yadsl_MemDebugRet
yadsl_memdb_remove_amb_internal(
		void* amb)
{
	yadsl_MemDebugAMB* node, *prev;

	/* Try to find node holding AMB */
	node = yadsl_memdb_find_amb_internal(amb, &prev);

	if (node) {
		/* Remove node */
		if (prev == NULL)
			amb_list_head = node->next;
		else
			prev->next = node->next;

		--amb_list_size;

		/* Log deallocation */
		yadsl_memdb_log_internal(YADSL_MEMDB_LOG_CHANNEL_DEALLOCATION,
			"free(%p) <- %s(%zu) @ %s:%d",
			node->amb, node->funcname, node->size, node->file, node->line);

		/* Deallocate node */
		free(node);

		return YADSL_MEMDB_RET_OK;
	} else {
		/* Log failed deallocation */
		yadsl_memdb_log_internal(YADSL_MEMDB_LOG_CHANNEL_DEALLOCATION,
			"free(%p) <- ?(?) @ ?:?", amb);

		error_occurred = true;

		return YADSL_MEMDB_RET_NOT_FOUND;
	}
}

bool
yadsl_memdb_contains_amb(
		void* amb)
{
	return yadsl_memdb_find_amb_internal(amb, NULL) != NULL;
}

void
yadsl_memdb_set_prng_seed(
		unsigned int seed)
{
	prng_state = seed;
}

void
yadsl_memdb_clear_amb_list()
{
	bool temp;

	/* Check if list is not empty */
	if (amb_list_size) {

		/* Log leakage */
		yadsl_memdb_log_internal(YADSL_MEMDB_LOG_CHANNEL_LEAKAGE,
			"%zu leaks detected:", amb_list_size);
	}

	temp = yadsl_memdb_log_channel_get(YADSL_MEMDB_LOG_CHANNEL_DEALLOCATION);
	yadsl_memdb_log_channel_set(YADSL_MEMDB_LOG_CHANNEL_DEALLOCATION, true);

	while (amb_list_size) {
		void* amb = amb_list_head->amb;
		yadsl_memdb_remove_amb_internal(amb);
		free(amb);
	}

	yadsl_memdb_log_channel_set(YADSL_MEMDB_LOG_CHANNEL_DEALLOCATION, temp);
}

void
yadsl_memdb_clear_amb_list_from_file(const char* file)
{
	bool temp;
	yadsl_MemDebugAMB* node, *next;
	size_t mem_leaks = 0;

	temp = yadsl_memdb_log_channel_get(YADSL_MEMDB_LOG_CHANNEL_DEALLOCATION);
	yadsl_memdb_log_channel_set(YADSL_MEMDB_LOG_CHANNEL_DEALLOCATION, true);

	for (node = amb_list_head; node; node = node->next)
		if (strcmp(node->file, file) == 0)
			++mem_leaks;

	if (mem_leaks) {

		/* Log leakage */
		yadsl_memdb_log_internal(YADSL_MEMDB_LOG_CHANNEL_LEAKAGE,
			"%zu leaks detected:", mem_leaks);
	}

	for (node = amb_list_head; node; node = next) {
		next = node->next;
		if (strcmp(node->file, file) == 0) {
			void* amb = node->amb;
			yadsl_memdb_remove_amb_internal(amb);
			free(amb);
		}
	}

	yadsl_memdb_log_channel_set(YADSL_MEMDB_LOG_CHANNEL_DEALLOCATION, temp);
}

float
yadsl_memdb_get_fail_rate()
{
	return fail_rate;
}

void
yadsl_memdb_set_fail_rate(
		float rate)
{
	if (rate < 0.0f)
		fail_rate = 0.0f;
	else if (rate > 1.0f)
		fail_rate = 1.0f;
	else
		fail_rate = rate;
}

bool
yadsl_memdb_get_fail_by_index()
{
	return fail_by_index;
}

void
yadsl_memdb_set_fail_by_index(
		bool enable)
{
	fail_by_index = enable;
}

size_t
yadsl_memdb_get_fail_index()
{
	return fail_index;
}

void
yadsl_memdb_set_fail_index(
		size_t index)
{
	fail_index = index;
}

size_t
yadsl_memdb_amb_list_size()
{
	return amb_list_size;
}

void
yadsl_memdb_set_logger(
		FILE* fp)
{
	log_fp = fp;
}

void
yadsl_memdb_free(
	void* amb)
{
	/* Remove AMB node from list */
	yadsl_memdb_remove_amb_internal(amb);

	/* Deallocate AMB */
	free(amb);
}

void*
yadsl_memdb_malloc(
		size_t size,
		const char* file,
		const int line)
{
	void* amb = NULL; /* Allocated memory block */

	if (yadsl_memdb_fail_internal()) {
		/* Log allocation error */
		yadsl_memdb_log_internal(YADSL_MEMDB_LOG_CHANNEL_ALLOCATION,
			"malloc(%zu) @ %s:%d -> %p (FAILED ARTIFICIALLY)",
			size, file, line, NULL);
	} else {
		amb = malloc(size);
		if (amb) {
			/* If allocation succeeded, add AMB node */
			if (yadsl_memdb_add_amb_internal("malloc", amb, size, file, line)) {
				/* If node could not be added, deallocate AMB */
				free(amb);
				amb = NULL;
			}
		}
	}

	return amb;
}

void*
yadsl_memdb_realloc(
		void* amb,
		size_t size,
		const char* file,
		const int line)
{
	void* ramb = NULL; /* Reallocated memory block */

	if (yadsl_memdb_fail_internal()) {
		/* Log reallocation error */
		yadsl_memdb_log_internal(YADSL_MEMDB_LOG_CHANNEL_ALLOCATION,
			"realloc(%p, %zu) @ %s:%d -> %p (FAILED ARTIFICIALLY)",
			amb, size, file, line, NULL);
	} else {
		yadsl_MemDebugAMB* node;

		/* Try finding AMB node in list */
		node = yadsl_memdb_find_amb_internal(amb, NULL);
		if (node) {
			/* If found, reallocate memory block */
			ramb = realloc(amb, size);
			if (ramb) {
				/* If reallocation succedded, update AMB node */
				node->file = file;
				node->funcname = "realloc";
				node->line = line;
				node->amb = ramb;
				node->size = size;
			}
			yadsl_memdb_log_internal(YADSL_MEMDB_LOG_CHANNEL_ALLOCATION,
				"realloc(%p, %zu) @ %s:%d -> %p",
				amb, size, file, line, ramb);
		} else {
			/* Log reallocation error */
			yadsl_memdb_log_internal(YADSL_MEMDB_LOG_CHANNEL_ALLOCATION,
				"realloc(%p, %zu) @ %s:%d -> %p (NODE NOT FOUND)",
				amb, size, file, line, NULL);

			error_occurred = true;
		}
	}
	
	return ramb;
}

void*
yadsl_memdb_calloc(
		size_t cnt,
		size_t size,
		const char* file,
		const int line)
{
	void* camb = NULL; /* Cleanly allocated memory block */

	if (yadsl_memdb_fail_internal()) {
		/* Log allocation error */
		yadsl_memdb_log_internal(YADSL_MEMDB_LOG_CHANNEL_ALLOCATION,
			"calloc(%zu, %zu) @ %s:%d -> %p (FAILED ARTIFICIALLY)",
			cnt, size, file, line, NULL);
	} else {
		camb = calloc(cnt, size);
		if (camb) {
			/* If cleanly allocation succeeded, add AMB node */
			if (yadsl_memdb_add_amb_internal("calloc", camb, size, file, line)) {
				/* If node could not be added, deallocate AMB */
				free(camb);
				camb = NULL;
			}
		}
	}

	return camb;
}
