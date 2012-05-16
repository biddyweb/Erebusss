#pragma once

#include <cmath>

#include <vector>
using std::vector;

const float E_TOL_MACHINE = 1.0e-12f;
const float E_TOL_ANGULAR = 1.0e-6f;
const float E_TOL_LINEAR = 1.0e-4f;

class Vector2D {
public:
    float x, y;

    Vector2D() : x(0.0f), y(0.0f) {
    }
    Vector2D(const Vector2D &v) : x(v.x), y(v.y) {
    }
    Vector2D(const float x,const float y) : x(x), y(y) {
    }

    void set(const float x,const float y) {
        this->x = x;
        this->y = y;
    }
    float &operator[] (const int i) {
        return *((&x) + i);
    }
    const float &operator[] (const int i) const {
        return *((&x) + i);
    }
    bool operator== (const Vector2D& v) const {
        return (v.x==x && v.y==y);
    }
    bool operator!= (const Vector2D& v) const {
        return !(v.x==x && v.y==y);
        //return !(v == *this);
    }
    Vector2D operator+ () const {
        return Vector2D(x,y);
        //return (*this);
    }
    Vector2D operator- () const {
        return Vector2D(-x,-y);
    }
    const Vector2D& operator= (const Vector2D& v) {
        x = v.x;
        y = v.y;
        return *this;
    }
    /*const*/ Vector2D& operator+= (const Vector2D& v) {
        x+=v.x;
        y+=v.y;
        return *this;
    }
    /*const*/ Vector2D& operator-= (const Vector2D& v) {
        x-=v.x;
        y-=v.y;
        return *this;
    }
    /*const*/ Vector2D& operator*= (const float& s) {
        x*=s;
        y*=s;
        return *this;
    }
    /*const*/ Vector2D& operator/= (const float& s) {
        const float r = 1 / s;
        x *= r;
        y *= r;
        return *this;
    }
    Vector2D operator+ (const Vector2D& v) const {
        return Vector2D(x + v.x, y + v.y);
    }
    Vector2D operator- (const Vector2D& v) const {
        return Vector2D(x - v.x, y - v.y);
    }
    Vector2D operator* (const float& s) const {
        return Vector2D(x*s,y*s);
    }
    /*friend inline const Vector3D operator* (const float& s,const Vector3D& v) {
    return v * s;
    }*/
    Vector2D operator/ (float s) const {
        s = 1/s;
        return Vector2D(s*x,s*y);
    }

    void scale(const float s) {
        this->x *= s;
        this->y *= s;
    }
    void scale(const float sx,const float sy) {
        this->x*=sx;
        this->y*=sy;
    }
    void translate(const float tx,const float ty) {
        this->x+=tx;
        this->y+=ty;
    }
    void add(const Vector2D &v) {
        this->x+=v.x;
        this->y+=v.y;
    }
    void subtract(const Vector2D &v) {
        this->x-=v.x;
        this->y-=v.y;
    }
    float square() const {
        return (x*x + y*y);
    }
    float magnitude() const {
        return sqrt( x*x + y*y );
    }
    /*float magnitudeChebyshev() const {
        double dist_x = abs(x);
        double dist_y = abs(y);
        double max_dist = dist_x > dist_y ? dist_x : dist_y;
        return max_dist;
    }*/
    float dot(const Vector2D &v) const {
        return ( x * v.x + y * v.y);
    }
    float operator%(const Vector2D& v) const {
        return ( x * v.x + y * v.y);
    }
    void normalise() {
        float mag = magnitude();
        if( mag == 0.0f )
            throw "attempted to normalise zero Vector2D";
        x = x/mag;
        y = y/mag;
    }
    Vector2D unit() const {
        float mag = magnitude();
        return Vector2D(x / mag, y / mag);
        //return (*this) / magnitude();
    }
    bool isZero() const {
        return ( x == 0.0f && y == 0.0f );
    }
    bool isZero(float tol) const {
        float mag = magnitude();
        return mag <= tol;
    }
    bool isEqual(const Vector2D &that, float tol) const {
        Vector2D diff = *this - that;
        float mag = diff.magnitude();
        return mag <= tol;
    }
    Vector2D perpendicularYToX() const {
        return Vector2D(y, -x);
    }
    float getSinAngle(const Vector2D &o) const {
        // returns sin of the signed angle between this and o; effetively a 2D cross product
        return ( this->x * o.y - this->y * o.x );
    }

    void dropOnLine(const Vector2D &o, const Vector2D &n);
    void parallelComp(const Vector2D &n);
    void perpComp(const Vector2D &n);
    float distFromLine(const Vector2D &o, const Vector2D &n) const;
    float distFromLineSq(const Vector2D &o, const Vector2D &n) const;
};

class LineSeg {
public:
    Vector2D start, end;

    LineSeg(Vector2D start, Vector2D end) : start(start), end(end) {
    }
};

class Polygon2D {
protected:
    vector<Vector2D> points; // should be stored anti-clockwise
    int source_type;
    void *source;

public:
    Polygon2D() : source_type(0), source(NULL) {
    }

    Vector2D getPoint(size_t i) const {
        return points.at(i);
    }
    size_t getNPoints() const {
        return points.size();
    }
    virtual void addPoint(Vector2D point) {
        this->points.push_back(point);
    }
    virtual void insertPoint(size_t indx, Vector2D point);
    void setSource(void *source) {
        this->source = source;
    }
    void *getSource() const {
        return this->source;
    }
    void setSourceType(int source_type) {
        this->source_type = source_type;
    }
    int getSourceType() const {
        return this->source_type;
    }

    bool pointInsideConvex(Vector2D pvec) const;
};

class Graph;

class GraphVertex {
    //vector<GraphVertex *> neighbours;
    vector<size_t> neighbour_ids;
    vector<float> distances;
    Vector2D pos;
    void *user_data;

    // for path finding:
    bool visited;
    float value;
    //size_t path_traceback_id; // the id of the neighbouring vertex that is part of the currently shortest path to this vertex
    GraphVertex *path_traceback; // the neighbouring vertex that is part of the currently shortest path to this vertex
public:
    GraphVertex(Vector2D pos, void *user_data) : pos(pos), user_data(user_data), visited(false), value(0) {
    }

    void addNeighbour(size_t neighbour_id, float distance) {
        neighbour_ids.push_back(neighbour_id);
        distances.push_back(distance);
    }
    size_t getNNeighbours() const {
        return neighbour_ids.size();
    }
    GraphVertex *getNeighbour(Graph *graph, float *distance, size_t i) const;
    const GraphVertex *getNeighbour(const Graph *graph, float *distance, size_t i) const;
    GraphVertex *getNeighbour(Graph *graph, size_t i) const;
    const GraphVertex *getNeighbour(const Graph *graph, size_t i) const;
    void *getUserData() const {
        return this->user_data;
    }
    Vector2D getPos() const {
        return this->pos;
    }
    void initPathFinding() {
        this->visited = false;
        this->value = -1.0f; // -ve represents "infinity"
        //this->path_traceback_id = 0;
        this->path_traceback = NULL;
    }
    float getValue() const {
        return this->value;
    }
    /*void setValue(float value, size_t path_traceback_id) {
        this->value = value;
        this->path_traceback_id = path_traceback_id;
    }*/
    void setValue(float value, GraphVertex *path_traceback) {
        this->value = value;
        this->path_traceback = path_traceback;
    }
    GraphVertex *getPathTraceback() const {
        return this->path_traceback;
    }
    bool isVisited() const {
        return this->visited;
    }
    void setVisited() {
        this->visited = true;
    }
};

class Graph {
    vector<GraphVertex> vertices;
public:
    Graph() {
    }

    size_t addVertex(const GraphVertex &vertex) {
        vertices.push_back(vertex);
        return (vertices.size()-1);
    }
    size_t getNVertices() const {
        return vertices.size();
    }
    GraphVertex *getVertex(size_t i) {
        return &vertices.at(i);
    }
    const GraphVertex *getVertex(size_t i) const {
        return &vertices.at(i);
    }

    Graph *clone() const;
    vector<GraphVertex *> shortestPath(size_t start, size_t end);
};

int rollDice(int X, int Y, int Z);

float distFromBox2D(const Vector2D &centre, float width, float height, const Vector2D &pos);
