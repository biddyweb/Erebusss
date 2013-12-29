#include <string>
using std::string;

#include <queue>
using std::priority_queue;

#ifdef _DEBUG
#include <cassert>
#endif

#include "utils.h"

#include "../logiface.h"

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

bool Rect2D::intersect(const Rect2D &rect) const {
    if( rect.bottom_right.x >= this->top_left.x - E_TOL_LINEAR && rect.bottom_right.y >= this->top_left.y - E_TOL_LINEAR &&
        rect.top_left.x <= this->bottom_right.x + E_TOL_LINEAR && rect.top_left.y <= this->bottom_right.y + E_TOL_LINEAR ) {
        return true;
    }
    return false;
}

bool Rect2D::overlaps(const Rect2D &rect) const {
    if( rect.bottom_right.x >= this->top_left.x + E_TOL_LINEAR && rect.bottom_right.y >= this->top_left.y + E_TOL_LINEAR &&
        rect.top_left.x <= this->bottom_right.x - E_TOL_LINEAR && rect.top_left.y <= this->bottom_right.y - E_TOL_LINEAR ) {
        return true;
    }
    return false;
}

void Polygon2D::addPoint(Vector2D point) {
    this->points.push_back(point);
    if( this->points.size() == 1 ) {
        this->top_left = point;
        this->bottom_right = point;
    }
    else {
        this->top_left.x = std::min(this->top_left.x, point.x);
        this->top_left.y = std::min(this->top_left.y, point.y);
        this->bottom_right.x = std::max(this->bottom_right.x, point.x);
        this->bottom_right.y = std::max(this->bottom_right.y, point.y);
    }
}

void Polygon2D::insertPoint(size_t indx, Vector2D point) {
    if( indx == 0 || indx > points.size() ) {
        LOG("Polygon2D::insertPoint invalid indx %d\n", indx);
        throw string("Polygon2D::insertPoint invalid indx");
    }
    //LOG("insert point at %d: %f, %f\n", indx, point.x, point.y);
    this->points.insert( this->points.begin() + indx, point );
    if( this->points.size() == 1 ) {
        this->top_left = point;
        this->bottom_right = point;
    }
    else {
        this->top_left.x = std::min(this->top_left.x, point.x);
        this->top_left.y = std::min(this->top_left.y, point.y);
        this->bottom_right.x = std::max(this->bottom_right.x, point.x);
        this->bottom_right.y = std::max(this->bottom_right.y, point.y);
    }
}

Vector2D Polygon2D::offsetInwards(size_t indx, float dist) const {
    int n_points = this->getNPoints();
    Vector2D p_point = this->getPoint((indx+n_points-1) % n_points);
    Vector2D point = this->getPoint(indx);
    Vector2D n_point = this->getPoint((indx+1) % n_points);
    Vector2D d0 = point - p_point;
    Vector2D d1 = n_point - point;
    if( d0.magnitude() < E_TOL_LINEAR || d1.magnitude() < E_TOL_LINEAR ) {
        LOG("p_point: %f, %f\n", p_point.x, p_point.y);
        LOG("point: %f, %f\n", point.x, point.y);
        LOG("n_point: %f, %f\n", n_point.x, n_point.y);
        throw string("coi polygon points not allowed");
    }
    d0.normalise();
    d1.normalise();
    // these vectors must point away from the wall
    Vector2D normal_from_wall0 = d0.perpendicularYToX();
    Vector2D normal_from_wall1 = d1.perpendicularYToX();
    Vector2D inwards = normal_from_wall0 + normal_from_wall1;
    if( inwards.magnitude() > E_TOL_MACHINE ) {
        float angle = acos(normal_from_wall0 % normal_from_wall1);
        float ratio = cos( 0.5f * angle );
        inwards.normalise();
        point += inwards * dist;
    }
    else {
        throw string("knife edge polygon not allowed");
    }
    return point;
}

Vector2D Polygon2D::findCentre() const {
    Vector2D centre;
    for(size_t j=0;j<this->getNPoints();j++) {
        Vector2D pos = this->getPoint(j);
        centre += pos;
    }
    centre /= this->getNPoints();
    return centre;
}

bool Polygon2D::pointInside(Vector2D pvec) const {
    if( pvec.x < this->top_left.x || pvec.x > this->bottom_right.x || pvec.y < this->top_left.y || pvec.y > this->bottom_right.y ) {
        return false;
    }
    if( this->getNPoints() == 0 ) {
        return false;
    }
    bool c = false;
    for(size_t i=0,j=this->getNPoints()-1;i<this->getNPoints();j=i++) {
        Vector2D pi = points.at(i);
        Vector2D pj = points.at(j);
        // first exclude case where the pvec lies on the polygon boundary
        if( (pvec.x-pi.x)*(pj.y-pi.y) == (pj.x-pi.x)*(pvec.y-pi.y) ) {
            // lies on line, but need to test extent
            if( (pvec.x >= pi.x && pvec.x <= pj.x) || (pvec.x >= pj.x && pvec.x <= pi.x) ) {
                if( (pvec.y >= pi.y && pvec.y <= pj.y) || (pvec.y >= pj.y && pvec.y <= pi.y) ) {
                    c = true;
                    break;
                }
            }
        }
        if( ( (pi.y > pvec.y) != (pj.y > pvec.y) ) && (pvec.x < (pj.x-pi.x) * (pvec.y-pi.y) / (pj.y-pi.y) + pi.x) ) {
            c = !c;
        }
    }
    return c;

    /*
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
        else if( this_sign != sign ) {
            if( c ) {
                for(size_t i=0;i<this->getNPoints();i++) {
                    qDebug("%d: %f, %f", i, this->points.at(i).x, this->points.at(i).y);
                }
                qDebug("test: %f, %f", pvec.x, pvec.y);
            }
            ASSERT_LOGGER( !c );
            return false;
        }
    }

    if( !c ) {
        for(size_t i=0;i<this->getNPoints();i++) {
            qDebug("%d: %f, %f", i, this->points.at(i).x, this->points.at(i).y);
        }
        qDebug("test: %f, %f", pvec.x, pvec.y);
    }
    ASSERT_LOGGER( c );
    return true;
    */
}

float Polygon2D::distanceFrom(Vector2D pvec) const {
    if( this->getNPoints() == 0 ) {
        return 0.0f;
    }
    if( this->pointInside(pvec) ) {
        return 0.0f;
    }

    bool set_min_dist = false;
    float min_dist = 0.0f;
    for(size_t i=0;i<this->getNPoints();i++) {
        Vector2D p0 = points.at(i);
        Vector2D p1 = points.at((i+1) % this->getNPoints());
        float dist_from_p0 = (p0 - pvec).magnitude();
        // distance from vertex
        if( !set_min_dist || dist_from_p0 < min_dist - E_TOL_LINEAR ) {
            set_min_dist = true;
            min_dist = dist_from_p0;
        }
        // distance from line segment
        Vector2D dir = p1 - p0;
        float edge_len = dir.magnitude();
        if( edge_len > E_TOL_LINEAR ) {
            dir /= edge_len; // normalies dir
            Vector2D proj_pvec = pvec;
            proj_pvec.dropOnLine(p0, dir);
            float edge_dist = (proj_pvec - p0).magnitude();
            if( edge_dist > -E_TOL_LINEAR & edge_dist < edge_len + E_TOL_LINEAR ) {
                // projects onto line
                float perp_dist = (pvec - proj_pvec).magnitude();
                if( perp_dist < min_dist - E_TOL_LINEAR ) {
                    min_dist = perp_dist;
                }
            }
        }
    }
    return min_dist;
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

/*float distFromBox2D(const Vector2D &centre, float width, float height, const Vector2D &pos) {
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
}*/

/* Return probability (as a proportion of RAND_MAX) that at least one poisson event
* occurred within the time_interval, given the mean number of time units per event.
*/
int poisson(int mean_ticks_per_event, int time_interval) {
        if( mean_ticks_per_event == 0 )
                return RAND_MAX;
        ASSERT_LOGGER( mean_ticks_per_event > 0 );
        int prob = (int)(RAND_MAX * ( 1.0 - exp( - ((double)time_interval) / mean_ticks_per_event ) ));
        return prob;
}


// Perlin noise

#define B 0x100
#define BM 0xff

#define N 0x1000
#define NP 12   /* 2^N */
#define NM 0xfff

static int p[B + B + 2];
static float g3[B + B + 2][3];
static float g2[B + B + 2][2];
static float g1[B + B + 2];
static int start = 1;

static void normalize2(float v[2])
{
    float s;

    s = sqrt(v[0] * v[0] + v[1] * v[1]);
    v[0] = v[0] / s;
    v[1] = v[1] / s;
}

static void normalize3(float v[3])
{
    float s;

    s = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    v[0] = v[0] / s;
    v[1] = v[1] / s;
    v[2] = v[2] / s;
}

void initPerlin() {
    start = 0;
    int i, j, k;

    for (i = 0 ; i < B ; i++) {
        p[i] = i;

        g1[i] = (float)((rand() % (B + B)) - B) / B;

        for (j = 0 ; j < 2 ; j++)
            g2[i][j] = (float)((rand() % (B + B)) - B) / B;
        normalize2(g2[i]);

        for (j = 0 ; j < 3 ; j++)
            g3[i][j] = (float)((rand() % (B + B)) - B) / B;
        normalize3(g3[i]);
    }

    while (--i) {
        k = p[i];
        p[i] = p[j = rand() % B];
        p[j] = k;
    }

    for (i = 0 ; i < B + 2 ; i++) {
        p[B + i] = p[i];
        g1[B + i] = g1[i];
        for (j = 0 ; j < 2 ; j++)
            g2[B + i][j] = g2[i][j];
        for (j = 0 ; j < 3 ; j++)
            g3[B + i][j] = g3[i][j];
    }
}

#define s_curve(t) ( t * t * (3.0f - 2.0f * t) )

#define lerp(t, a, b) ( a + t * (b - a) )

#define setup(i,b0,b1,r0,r1)\
    t = vec[i] + N;\
    b0 = ((int)t) & BM;\
    b1 = (b0+1) & BM;\
    r0 = t - (int)t;\
    r1 = r0 - 1.0f;

float perlin_noise2(float vec[2]) {
    int bx0, bx1, by0, by1, b00, b10, b01, b11;
    float rx0, rx1, ry0, ry1, *q, sx, sy, a, b, t, u, v;
    register int i, j;

    if (start) {
        initPerlin();
    }

    setup(0, bx0,bx1, rx0,rx1);
    setup(1, by0,by1, ry0,ry1);

    i = p[ bx0 ];
    j = p[ bx1 ];

    b00 = p[ i + by0 ];
    b10 = p[ j + by0 ];
    b01 = p[ i + by1 ];
    b11 = p[ j + by1 ];

    sx = s_curve(rx0);
    sy = s_curve(ry0);

#define at2(rx,ry) ( rx * q[0] + ry * q[1] )

    q = g2[ b00 ] ; u = at2(rx0,ry0);
    q = g2[ b10 ] ; v = at2(rx1,ry0);
    a = lerp(sx, u, v);

    q = g2[ b01 ] ; u = at2(rx0,ry1);
    q = g2[ b11 ] ; v = at2(rx1,ry1);
    b = lerp(sx, u, v);

    return lerp(sy, a, b);
}
