#include "utils.h"
#include "../game.h"

#include <queue>
using std::priority_queue;

/* Drops the vector onto a line of direction 'n' that passes through 'o'.
* 'n' must be normalised.
* Formula: nv = ((v - o) DOT n) n + o
*/
void Vector2D::dropOnLine(const Vector2D &o, const Vector2D &n) {
    this->subtract(o);
    float dot = this->x * n.x + this->y * n.y;
    this->x = dot * n.x;
    this->y = dot * n.y;
    this->add(o);
}

void Vector2D::parallelComp(const Vector2D &n) {
    Vector2D zero(0.0f, 0.0f);
    this->dropOnLine(zero, n);
}

void Vector2D::perpComp(const Vector2D &n) {
    Vector2D zero(0.0f, 0.0f);
    Vector2D v = *this;
    v.dropOnLine(zero, n);
    this->subtract(v);
}

float Vector2D::distFromLine(const Vector2D &o, const Vector2D &n) const {
    Vector2D nv(this->x, this->y);
    nv.dropOnLine(o, n);
    nv.subtract(*this);
    float dist = sqrt( nv.square() );
    return dist;
}

float Vector2D::distFromLineSq(const Vector2D &o, const Vector2D &n) const {
    Vector2D nv(this->x, this->y);
    nv.dropOnLine(o, n);
    nv.subtract(*this);
    float dist = nv.square();
    return dist;
}

void Polygon2D::insertPoint(int indx, Vector2D point) {
    if( indx <= 0 || indx > points.size() ) {
        LOG("Polygon2D::insertPoint invalid indx %d\n", indx);
        throw string("Polygon2D::insertPoint invalid indx");
    }
    LOG("insert point at %d: %f, %f\n", indx, point.x, point.y);
    this->points.insert( this->points.begin() + indx, point );
    LOG("    done\n");
}

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
    //LOG("Graph::shortestPath(%d, %d)\n", start, end);
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
            //LOG("    check vertex at %f, %f value %f\n", c_vertex->getPos().x, c_vertex->getPos().y, c_vertex->getValue());
            for(size_t i=0;i<c_vertex->getNNeighbours();i++) {
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
        // n.b., using const_reverse_iterator doesn't compile on Symbian for some reason?
        for(vector<GraphVertex *>::reverse_iterator iter = path_backwards.rbegin();iter != path_backwards.rend(); ++iter) {
            path.push_back( *iter );
        }
    }
    //LOG("    calculated shortest path, length: %d\n", path.size());
    return path;
}

int rollDice(int X, int Y, int Z) {
    // X DY + Z
    int value = Z;
    for(int i=0;i<X;i++) {
        int roll = (rand() % Y) + 1;
        value += roll;
    }
    return value;
}

float distFromBox2D(const Vector2D &centre, float width, float height, const Vector2D &pos) {
    float dist_x = 0.0f, dist_y = 0.0f;

    if( pos.x < centre.x - 0.5f*width ) {
        dist_x = centre.x - 0.5f*width - pos.x;
    }
    else if( pos.x > centre.x + 0.5f*width ) {
        dist_x = pos.x - ( centre.x + 0.5f*width );
    }

    if( pos.y < centre.y - 0.5f*height ) {
        dist_y = centre.y - 0.5f*height - pos.y;
    }
    else if( pos.y > centre.y + 0.5f*height ) {
        dist_y = pos.y - ( centre.y + 0.5f*height );
    }

    return sqrt( dist_x*dist_x + dist_y*dist_y );
}
