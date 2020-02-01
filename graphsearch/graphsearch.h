#ifndef __GRAPH_SEARCH_H__
#define __GRAPH_SEARCH_H__

#include "graph.h"

/**
* Auxiliary module for searching in Graphs
*/

typedef enum
{
    // SEMANTIC RETURN VALUES

    /* Everything went as excepted */
    GRAPH_SEARCH_RETURN_OK = 0,

    /* Graph does not contain vertex */
    GRAPH_SEARCH_RETURN_DOES_NOT_CONTAIN_VERTEX,

    // ERROR RETURN VALUES

    /* Invalid parameter was provided */
    GRAPH_SEARCH_RETURN_INVALID_PARAMETER,

    /* Could not allocate memory space */
    GRAPH_SEARCH_RETURN_MEMORY,

    /* When an internal error is unrecognized */
    GRAPH_SEARCH_RETURN_UNKNOWN_ERROR,
}
GraphSearchReturnID;

/**
* Visit every neighbour in the graph that can be accessed from
* the initial vertex, in a depth-first-search
* pGraph            pointer to graph
* initialVertex     initial vertex
* visit_cb          function that will be called for every
*                   non-visited vertex in the graph
*/
GraphSearchReturnID graphDFS(Graph *pGraph,
    void *initialVertex,
    void (*visit_cb)(void *vertex));


/**
* Visit every neighbour in the graph that can be accessed from
* the initial vertex, in a breadth-first-search
* pGraph            pointer to graph
* initialVertex     initial vertex
* visit_cb          function that will be called for every
*                   non-visited vertex in the graph
*/
GraphSearchReturnID graphBFS(Graph *pGraph,
    void *initialVertex,
    void (*visit_cb)(void *vertex));

#endif