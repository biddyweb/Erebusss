#include "utils.h"
#include "../game.h"

#include <queue>
using std::priority_queue;

GraphVertex *GraphVertex::getNeighbour(Graph *graph, float *distance, size_t i) const {
    *distance = distances.at(i);
    size_t id = neighbour_ids.at(i);
    return graph->getVertex(id);
}

const GraphVertex *GraphVertex::getNeighbour(const Graph *graph, float *distance, size_t i) const {
    *distance = distances.at(i);
    size_t id = neighbour_ids.at(i);
    return graph->getVertex(id);
}

GraphVertex *GraphVertex::getNeighbour(Graph *graph, size_t i) const {
    size_t id = neighbour_ids.at(i);
    return graph->getVertex(id);
}

const GraphVertex *GraphVertex::getNeighbour(const Graph *graph, size_t i) const {
    size_t id = neighbour_ids.at(i);
    return graph->getVertex(id);
}

Graph *Graph::clone() const {
    Graph *copy = new Graph();
    for(vector<GraphVertex>::const_iterator iter = vertices.begin(); iter != vertices.end(); ++iter) {
        GraphVertex vertex = *iter;
        copy->addVertex(vertex);
    }
    return copy;
}

class DistanceComparison {
public:
    bool operator() (const GraphVertex *lhs, const GraphVertex *rhs) const {
        // return iff rhs < lhs
        return rhs->getValue() < lhs->getValue();
    }
};

vector<GraphVertex *> Graph::shortestPath(size_t start, size_t end) {
    LOG("Graph::shortestPath(%d, %d)\n", start, end);
    for(vector<GraphVertex>::iterator iter = vertices.begin(); iter != vertices.end(); ++iter) {
        GraphVertex *vertex = &*iter;
        vertex->initPathFinding();
    }

    priority_queue<GraphVertex *, vector<GraphVertex *>, DistanceComparison> queue;
    GraphVertex *start_vertex = this->getVertex(start);
    start_vertex->setValue(0.0f, NULL);
    queue.push(start_vertex);

    GraphVertex *end_vertex = this->getVertex(end);
    bool found_dest = false;

    while( !queue.empty() && !found_dest ) {
        // find current cheapest
        GraphVertex *c_vertex = queue.top();
        queue.pop();

        // check that we've got the "cheapest" version
        if( !c_vertex->isVisited() ) {
            c_vertex->setVisited();

            float value = c_vertex->getValue();
            LOG("    check vertex at %f, %f value %f\n", c_vertex->getPos().x, c_vertex->getPos().y, c_vertex->getValue());
            for(int i=0;i<c_vertex->getNNeighbours();i++) {
                float dist = 0.0f;
                GraphVertex *n_vertex = c_vertex->getNeighbour(this, &dist, i);
                float n_value = value + dist;
                bool changed = false;
                if( n_vertex->getValue() < 0.0f || n_value < n_vertex->getValue() ) {
                    n_vertex->setValue(n_value, c_vertex);
                    changed = true;
                }
                if( changed ) {
                    // update with cheaper version
                    queue.push(n_vertex);
                }
            }

            if( c_vertex == end_vertex ) {
                // reached destination
                found_dest = true;
            }
        }
    }

    vector<GraphVertex *> path;
    if( found_dest ) {
        // n.b., don't include start_vertex in path
        vector<GraphVertex *> path_backwards;
        GraphVertex *c_vertex = end_vertex;
        while( c_vertex != start_vertex ) {
            path_backwards.push_back(c_vertex);
            c_vertex = c_vertex->getPathTraceback();
            if( c_vertex == NULL ) {
                LOG("error tracing back path: start %d end %d\n", start, end);
                throw string("error tracing back path");
            }
        }
        // reverse path
        for(vector<GraphVertex *>::const_reverse_iterator iter = path_backwards.rbegin();iter != path_backwards.rend(); ++iter) {
            path.push_back( *iter );
        }
    }
    LOG("    calculated shortest path, length: %d\n", path.size());
    return path;
}
