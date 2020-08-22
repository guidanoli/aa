#include <queue/queue.h>

#include <stdlib.h>

#include <memdb/memdb.h>

struct QueueItem
{
	struct QueueItem *previous;
	void *item;
};

struct Queue
{
	struct QueueItem *entrance;
	struct QueueItem *exit;
	void (*freeItem)(void *item);
};

QueueRet queueCreate(Queue **ppQueue,
	void (*freeItem)(void *item))
{
	Queue *pQueue;
	pQueue = malloc(sizeof(struct Queue));
	if (pQueue == NULL)
		return QUEUE_MEMORY;
	pQueue->freeItem = freeItem;
	pQueue->entrance = NULL;
	pQueue->exit = NULL;
	*ppQueue = pQueue;
	return QUEUE_OK;
}

QueueRet queueQueue(Queue *pQueue,
	void *item)
{
	struct QueueItem *pQueueItem = malloc(sizeof(struct QueueItem));
	if (pQueueItem == NULL)
		return QUEUE_MEMORY;
	pQueueItem->item = item;
	pQueueItem->previous = NULL;
	if (pQueue->entrance != NULL)
		pQueue->entrance->previous = pQueueItem;
	pQueue->entrance = pQueueItem;
	if (pQueue->exit == NULL)
		pQueue->exit = pQueueItem;
	return QUEUE_OK;
}

QueueRet queueDequeue(Queue *pQueue,
	void **pItem)
{
	struct QueueItem *newExit;
	if (pQueue->exit == NULL)
		return QUEUE_EMPTY;
	newExit = pQueue->exit->previous;
	*pItem = pQueue->exit->item;
	free(pQueue->exit);
	pQueue->exit = newExit;
	if (newExit == NULL)
		pQueue->entrance = NULL;
	return QUEUE_OK;
}

QueueRet queueIsEmpty(Queue *pQueue,
	int *pIsEmpty)
{
	*pIsEmpty = pQueue->exit == NULL;
	return QUEUE_OK;
}

void queueDestroy(Queue *pQueue)
{
	struct QueueItem *current;
	if (pQueue == NULL)
		return;
	while (current = pQueue->exit) {
		pQueue->exit = current->previous;
		if (pQueue->freeItem)
			pQueue->freeItem(current->item);
		free(current);
	}
	free(pQueue);
}