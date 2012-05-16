#include "utils.h"
#include "../game.h"

#include <queue>
using std::priority_queue;

#ifdef _DEBUG
#include <cassert>
#endif

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

void Polygon2D::insertPoint(size_t indx, Vector2D point) {
    if( indx == 0 || indx > points.size() ) {
        LOG("Polygon2D::insertPoint invalid indx %d\n", indx);
        throw string("Polygon2D::insertPoint invalid indx");
    }
    //LOG("insert point at %d: %f, %f\n", indx, point.x, point.y);
    this->points.insert( this->points.begin() + indx, point );
}

bool Polygon2D::pointInsideConvex(Vector2D pvec) const {
    int sign = 0;
    for(size_t j=0;j<this->getNPoints();j++) {
        int p_j = j==0 ? this->getNPoints()-1 : j-1;
        Vector2D p0 = this->getPoint(p_j);
        Vector2D p1 = this->getPoint(j);
        Vector2D dp = p1 - p0;
        float dp_length = dp.magnitude();
        if( dp_length == 0 ) {
            continue;
        }
        dp /= dp_length;
        Vector2D dq = pvec - p0;
        float dq_length = dq.magnitude();
        if( dq_length == 0.0f ) {
            continue;
        }
        dq /= dq_length;
        float sin_angle = dq.getSinAngle(dp);
        if( sin_angle == 0.0f ) {
            continue;
        }
        int this_sign = (sin_angle > 0.0f) ? 1 : -1;
        if( sign == 0 )
            sign = this_sign;
        else if( this_sign != sign )
            return false;
    }
    return true;
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

class GraphDistance {
    GraphVertex *vertex;
    float distance;
public:
    /** We need to store the distance separately, as although we update the
      * value in vertex, priority_queue doesn't allow us to make the sorting
      * order depend on something that can then change.
      */
    GraphDistance(GraphVertex *vertex) : vertex(vertex), distance(vertex->getValue()) {
    }
    float getDistance() const {
        return distance;
    }
    GraphVertex *getVertex() const {
        return vertex;
    }
};

class DistanceComparison {
public:
    bool operator() (const GraphDistance &lhs, const GraphDistance &rhs) const {
        // return iff rhs < lhs
        //LOG("compare distances %d ( %f ) vs %d ( %f )\n", lhs, lhs->getValue(), rhs, rhs->getValue());
        float l_dist = lhs.getDistance();
        float r_dist = rhs.getDistance();
        if( l_dist == r_dist ) {
            // need to have some way to sort consistently!
            return rhs.getVertex() < lhs.getVertex();
        }
        //return r_dist < l_dist;
        bool res = r_dist < l_dist;
        //LOG("    return %d\n", res);
        return res;
    }
};

vector<GraphVertex *> Graph::shortestPath(size_t start, size_t end) {
    //LOG("Graph::shortestPath(%d, %d)\n", start, end);
    //LOG("graph has %d vertices\n", vertices.size());
    for(vector<GraphVertex>::iterator iter = vertices.begin(); iter != vertices.end(); ++iter) {
        GraphVertex *vertex = &*iter;
        vertex->initPathFinding();
    }

    priority_queue<GraphDistance, vector<GraphDistance>, DistanceComparison> queue;
    GraphVertex *start_vertex = this->getVertex(start);
    start_vertex->setValue(0.0f, NULL);
    GraphDistance start_distance(start_vertex);
    queue.push(start_distance);

    GraphVertex *end_vertex = this->getVertex(end);
    bool found_dest = false;

    //LOG("start algorithm\n");
    while( !queue.empty() && !found_dest ) {
        // find current cheapest
        //LOG("start loop, queue size %d\n", queue.size());
        const GraphDistance &c_distance = queue.top();
        GraphVertex *c_vertex = c_distance.getVertex();
        queue.pop();

        // check that we've got the "cheapest" version
        if( !c_vertex->isVisited() ) {
            c_vertex->setVisited();

            float value = c_vertex->getValue();
            //LOG("    check vertex at %f, %f value %f %d neighbours\n", c_vertex->getPos().x, c_vertex->getPos().y, c_vertex->getValue(), c_vertex->getNNeighbours());
            for(size_t i=0;i<c_vertex->getNNeighbours();i++) {
                //LOG("    i = %d\n", i);
                float dist = 0.0f;
                GraphVertex *n_vertex = c_vertex->getNeighbour(this, &dist, i);
                //LOG("    neighbouring vertex at %f, %f value %f dist %f\n", n_vertex->getPos().x, n_vertex->getPos().y, n_vertex->getValue(), dist);
                float n_value = value + dist;
                bool changed = false;
                if( n_vertex->getValue() < 0.0f || n_value < n_vertex->getValue() ) {
                    //LOG("    shorter dist: %f\n", n_value);
                    n_vertex->setValue(n_value, c_vertex);
                    changed = true;
                }
                if( changed ) {
                    // update with cheaper version
                    //LOG("    push %d, value %f, current size: %d\n", n_vertex, n_vertex->getValue(), queue.size());
                    GraphDistance n_distance(n_vertex);
                    queue.push(n_distance);
                    //LOG("    pushed ok\n");
                }
            }
            //LOG("    done\n");

            if( c_vertex == end_vertex ) {
                // reached destination
                //LOG("reached destination\n");
                found_dest = true;
            }
        }
    }

    //LOG("found_dest: %d\n", found_dest);
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
