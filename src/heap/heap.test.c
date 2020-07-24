#include "heap.h"

#include <string.h>
#include <stdlib.h>

#include "tester.h"

#define matches(a, b) (!strcmp(a,b))

const char *TesterHelpStrings[] = {
    "This is the heap test module",
    "This implements a min heap, meaning that the first item extracted",
    "is the smallest of all in the heap",
    "",
    "/create <size>               create a heap with size <size>",
    "/insert <number>             insert <number> in the heap",
    "/extract <expected number>   extract number from heap",
    "/size <expected size>        get heap size",
    "/resize <new size>           resize heap to size <new size>",
    NULL,
};

static Heap *pHeap;

TesterReturnValue TesterInitCallback()
{
    pHeap = NULL;
    return TESTER_OK;
}

TesterReturnValue convert(HeapRet heapReturnValue)
{
    switch (heapReturnValue) {
    case HEAP_OK:
        return TESTER_OK;
    case HEAP_EMPTY:
        return TesterExternalReturnValue("empty");
    case HEAP_FULL:
        return TesterExternalReturnValue("full");
    case HEAP_SHRINK:
        return TesterExternalReturnValue("shrink");
    case HEAP_MEMORY:
        return TesterExternalReturnValue("memory");
    default:
        return TesterExternalReturnValue("unknown");
    }
}

int cmpObjs(void *a, void *b, void *_unused)
{
    return *((int *) a) < *((int *) b); /* MIN HEAP */
}

TesterReturnValue TesterParseCallback(const char *command)
{
    HeapRet returnId = HEAP_OK;
    if matches(command, "create") {
        size_t size;
        Heap *temp;
        if (TesterParseArguments("z", &size) != 1)
            return TESTER_ARGUMENT;
        if (pHeap != NULL)
            heapDestroy(pHeap);
        returnId = heapCreate(&temp, size, cmpObjs, free, NULL);
        if (!returnId)
            pHeap = temp;
    } else if matches(command, "insert") {
        int obj, *pObj;
        if (TesterParseArguments("i", &obj) != 1)
            return TESTER_ARGUMENT;
        pObj = malloc(sizeof(int));
        if (!pObj)
            return TESTER_MALLOC;
        *pObj = obj;
        returnId = heapInsert(pHeap, pObj);
        if (returnId)
            free(pObj);
    } else if matches(command, "extract") {
        int *pObj, actual, expected;
        if (TesterParseArguments("i", &expected) != 1)
            return TESTER_ARGUMENT;
        returnId = heapExtract(pHeap, &pObj);
        if (!returnId) {
            actual = *pObj;
            free(pObj);
            if (actual != expected)
                return TESTER_RETURN;
        }
    } else if matches(command, "size") {
        size_t actual, expected;
        if (TesterParseArguments("z", &expected) != 1)
            return TESTER_ARGUMENT;
        returnId = heapGetSize(pHeap, &actual);
        if (!returnId && actual != expected)
            return TESTER_RETURN;
    } else if matches(command, "resize") {
        size_t newSize;
        if (TesterParseArguments("z", &newSize) != 1)
            return TESTER_ARGUMENT;
        returnId = heapResize(pHeap, newSize);
    } else {
        return TESTER_COMMAND;
    }
    return convert(returnId);
}

TesterReturnValue TesterExitCallback()
{
    if (pHeap) {
        heapDestroy(pHeap);
        pHeap = NULL;
    }
    return TESTER_OK;
}
