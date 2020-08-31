#include <graphsearch/graphsearch.h>

#include <assert.h>
#include <stdlib.h>

#include <queue/queue.h>
#include <memdb/memdb.h>

#ifdef _DEBUG
#include <stdio.h>
int nodeRefCount = 0;
#endif

typedef struct
{
	yadsl_GraphVertexObject *parent;
	yadsl_GraphEdgeObject *edge;
	yadsl_GraphVertexObject *child;
}
yadsl_GraphSearchBFSTreeNode;

/* Private functions prototypes */

static yadsl_GraphSearchBFSTreeNode* yadsl_graph_search_allocate_node_internal(
	yadsl_GraphVertexObject* parent,
	yadsl_GraphEdgeObject* edge,
	yadsl_GraphVertexObject* child);

static void yadsl_graph_search_free_node_internal(
	yadsl_GraphSearchBFSTreeNode* node);

static yadsl_GraphSearchRet yadsl_graph_search_dfs_internal(
	yadsl_GraphHandle *graph,
	yadsl_GraphVertexFlag visited_flag,
	yadsl_GraphVertexObject *vertex,
	yadsl_GraphSearchVertexVisitFunc visit_vertex_func,
	yadsl_GraphSearchEdgeVisitFunc visit_edge_func);

static yadsl_GraphSearchRet yadsl_graph_search_bfs_internal(
	yadsl_GraphHandle *graph,
	Queue *bfs_queue,
	yadsl_GraphVertexFlag visited_flag,
	yadsl_GraphVertexObject* vertex,
	yadsl_GraphSearchVertexVisitFunc visit_vertex_func,
	yadsl_GraphSearchEdgeVisitFunc visit_edge_func);

/* Public functions */

yadsl_GraphSearchRet yadsl_graph_search_dfs(
	yadsl_GraphHandle* graph,
	yadsl_GraphVertexObject* initial_vertex,
	yadsl_GraphVertexFlag visited_flag,
	yadsl_GraphSearchVertexVisitFunc visit_vertex_func,
	yadsl_GraphSearchEdgeVisitFunc visit_edge_func)
{
	yadsl_GraphRet graph_ret;
	yadsl_GraphVertexFlag flag;

	if (graph_ret = yadsl_graph_vertex_flag_get(graph, initial_vertex, &flag)) {
		switch (graph_ret) {
		case YADSL_GRAPH_RET_DOES_NOT_CONTAIN_VERTEX:
			return YADSL_GRAPH_SEARCH_RET_DOES_NOT_CONTAIN_VERTEX;
		default:
			assert(0);
		}
	}
	if (flag == visited_flag)
		return YADSL_GRAPH_SEARCH_RET_VERTEX_ALREADY_VISITED;

	return yadsl_graph_search_dfs_internal(graph, visited_flag, initial_vertex, visit_vertex_func, visit_edge_func);
}

yadsl_GraphSearchRet yadsl_graph_search_bfs(
	yadsl_GraphHandle* graph,
	yadsl_GraphVertexObject* initial_vertex,
	yadsl_GraphVertexFlag visited_flag,
	yadsl_GraphSearchVertexVisitFunc visit_vertex_func,
	yadsl_GraphSearchEdgeVisitFunc visit_edge_func)
{
	Queue* bfs_queue;
	QueueRet queue_ret;
	yadsl_GraphRet graph_ret;
	yadsl_GraphSearchRet graph_search_ret;
	yadsl_GraphVertexFlag flag;

	if (graph_ret = yadsl_graph_vertex_flag_get(graph, initial_vertex, &flag)) {
		switch (graph_ret) {
		case YADSL_GRAPH_RET_DOES_NOT_CONTAIN_VERTEX:
			return YADSL_GRAPH_SEARCH_RET_DOES_NOT_CONTAIN_VERTEX;
		default:
			assert(0);
		}
	}
	if (flag == visited_flag)
		return YADSL_GRAPH_SEARCH_RET_VERTEX_ALREADY_VISITED;

	if (queue_ret = queueCreate(&bfs_queue, yadsl_graph_search_free_node_internal)) {
		assert(queue_ret == QUEUE_MEMORY);
		return YADSL_GRAPH_SEARCH_RET_MEMORY;
	}

	graph_search_ret = yadsl_graph_search_bfs_internal(graph, bfs_queue, visited_flag, initial_vertex, visit_vertex_func, visit_edge_func);

	queueDestroy(bfs_queue);
	return graph_search_ret;
}

/* Private functions */

// Run yadsl_graph_search_dfs_internal on unvisited vertex
yadsl_GraphSearchRet yadsl_graph_search_dfs_internal(
	yadsl_GraphHandle* graph,
	yadsl_GraphVertexFlag visited_flag,
	yadsl_GraphVertexObject* vertex,
	yadsl_GraphSearchVertexVisitFunc visit_vertex_func,
	yadsl_GraphSearchEdgeVisitFunc visit_edge_func)
{
	yadsl_GraphSearchRet graph_search_ret;
	yadsl_GraphVertexObject *nb;
	yadsl_GraphEdgeObject *edge;
	size_t degree;
	bool is_directed;
	yadsl_GraphVertexFlag flag;

	if (visit_vertex_func)
		visit_vertex_func(vertex);
	if (yadsl_graph_vertex_flag_set(graph, vertex, visited_flag)) assert(0);
	if (yadsl_graph_is_directed_check(graph, &is_directed)) assert(0);
	if (is_directed) {
		if (yadsl_graph_vertex_degree_get(graph, vertex, YADSL_GRAPH_EDGE_DIR_OUT, &degree)) assert(0);
		while (degree--) {
			if (yadsl_graph_vertex_nb_iter(graph, vertex, YADSL_GRAPH_EDGE_DIR_OUT, YADSL_GRAPH_ITER_DIR_NEXT, &nb, &edge))
				assert(0);
			if (yadsl_graph_vertex_flag_get(graph, nb, &flag)) assert(0);
			if (flag == visited_flag)
				continue;
			if (visit_edge_func)
				visit_edge_func(vertex, edge, nb);
			if (graph_search_ret = yadsl_graph_search_dfs_internal(graph, visited_flag, nb, visit_vertex_func, visit_edge_func))
				return graph_search_ret;
		}
	} else {
		if (yadsl_graph_vertex_degree_get(graph, vertex, YADSL_GRAPH_EDGE_DIR_BOTH, &degree))
			assert(0);
		while (degree--) {
			if (yadsl_graph_vertex_nb_iter(graph, vertex, YADSL_GRAPH_EDGE_DIR_BOTH, YADSL_GRAPH_ITER_DIR_NEXT, &nb, &edge))
				assert(0);
			if (yadsl_graph_vertex_flag_get(graph, nb, &flag)) assert(0);
			if (flag == visited_flag)
				continue;
			if (visit_edge_func)
				visit_edge_func(vertex, edge, nb);
			if (graph_search_ret = yadsl_graph_search_dfs_internal(graph, visited_flag, nb, visit_vertex_func, visit_edge_func))
				return graph_search_ret;
		}
	}
	return YADSL_GRAPH_SEARCH_RET_OK;
}

static yadsl_GraphSearchRet yadsl_graph_search_add_nb_to_queue_internal(
	yadsl_GraphHandle* graph,
	Queue* bfs_queue,
	yadsl_GraphVertexFlag visited_flag,
	yadsl_GraphVertexObject* vertex,
	yadsl_GraphEdgeDirection edge_direction)
{
	yadsl_GraphSearchBFSTreeNode* node;
	QueueRet queue_ret;
	yadsl_GraphVertexObject* nb;
	yadsl_GraphEdgeObject* edge;
	size_t degree;
	yadsl_GraphVertexFlag flag;

	if (yadsl_graph_vertex_degree_get(graph, vertex, edge_direction, &degree)) assert(0);
	while (degree--) {
		if (yadsl_graph_vertex_nb_iter(graph, vertex, edge_direction, YADSL_GRAPH_ITER_DIR_NEXT, &nb, &edge)) assert(0);
		if (yadsl_graph_vertex_flag_get(graph, nb, &flag)) assert(0);
		if (flag == visited_flag)
			continue;
		if (yadsl_graph_vertex_flag_set(graph, nb, visited_flag)) assert(0);
		node = yadsl_graph_search_allocate_node_internal(vertex, edge, nb);
		if (node == NULL)
			return YADSL_GRAPH_SEARCH_RET_MEMORY;
		if (queue_ret = queueQueue(bfs_queue, node)) {
			yadsl_graph_search_free_node_internal(node);
			return YADSL_GRAPH_SEARCH_RET_MEMORY;
		}
	}
	return YADSL_GRAPH_SEARCH_RET_OK;
}

// Run yadsl_graph_search_bfs_internal on unvisited vertex
yadsl_GraphSearchRet yadsl_graph_search_bfs_internal(
	yadsl_GraphHandle* graph,
	Queue* bfs_queue,
	yadsl_GraphVertexFlag visited_flag,
	yadsl_GraphVertexObject* vertex,
	yadsl_GraphSearchVertexVisitFunc visit_vertex_func,
	yadsl_GraphSearchEdgeVisitFunc visit_edge_func)
{
	bool is_directed;
	yadsl_GraphSearchBFSTreeNode *node = NULL;
	yadsl_GraphSearchRet graph_search_ret;
	yadsl_GraphEdgeDirection edge_direction;

	if (yadsl_graph_is_directed_check(graph, &is_directed)) assert(0);
	if (is_directed) {
		edge_direction = YADSL_GRAPH_EDGE_DIR_OUT;
	} else {
		edge_direction = YADSL_GRAPH_EDGE_DIR_BOTH;
	}
	if (yadsl_graph_vertex_flag_set(graph, vertex, visited_flag)) assert(0);
	if (visit_vertex_func)
		visit_vertex_func(vertex);
	if (graph_search_ret = yadsl_graph_search_add_nb_to_queue_internal(graph, bfs_queue, visited_flag, vertex, edge_direction))
		return graph_search_ret;
	while (queueDequeue(bfs_queue, &node) == QUEUE_OK) {
		if (visit_edge_func)
			visit_edge_func(node->parent, node->edge, node->child);
		if (visit_vertex_func)
			visit_vertex_func(node->child);
		vertex = node->child;
		yadsl_graph_search_free_node_internal(node);
		if (graph_search_ret = yadsl_graph_search_add_nb_to_queue_internal(graph, bfs_queue, visited_flag, vertex, edge_direction))
			return graph_search_ret;
	}
	return YADSL_GRAPH_SEARCH_RET_OK;
}

yadsl_GraphSearchBFSTreeNode* yadsl_graph_search_allocate_node_internal(
	yadsl_GraphVertexObject* parent,
	yadsl_GraphEdgeObject* edge,
	yadsl_GraphVertexObject* child)
{
	yadsl_GraphSearchBFSTreeNode *node = malloc(sizeof(*node));
	if (node) {
		node->parent = parent;
		node->edge = edge;
		node->child = child;

#ifdef _DEBUG
		++nodeRefCount;
		printf("Allocating node %p\n", node);
#endif

	}
	return node;
}

void yadsl_graph_search_free_node_internal(yadsl_GraphSearchBFSTreeNode *node)
{

#ifdef _DEBUG
	--nodeRefCount;
	printf("Deallocating node %p\n", node);
#endif

	free(node);
}

#ifdef _DEBUG
int yadsl_graph_search_get_node_ref_count()
{
	return nodeRefCount;
}
#endif
