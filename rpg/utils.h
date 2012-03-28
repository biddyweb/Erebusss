#pragma once

#include <cmath>

#include <vector>
using std::vector;

class Vector2D {
public:
    float x, y;

    Vector2D() : x(0.0f), y(0.0f) {
    }
    Vector2D(const Vector2D *v) : x(v->x), y(v->y) {
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
    const bool operator== (const Vector2D& v) const {
        return (v.x==x && v.y==y);
    }
    const bool operator!= (const Vector2D& v) const {
        return !(v.x==x && v.y==y);
        //return !(v == *this);
    }
    /*const*/ Vector2D operator+ () const {
        return Vector2D(x,y);
        //return (*this);
    }
    /*const*/ Vector2D operator- () const {
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
    /*const*/ Vector2D operator+ (const Vector2D& v) const {
        return Vector2D(x + v.x, y + v.y);
    }
    /*const*/ Vector2D operator- (const Vector2D& v) const {
        return Vector2D(x - v.x, y - v.y);
    }
    /*const*/ Vector2D operator* (const float& s) const {
        return Vector2D(x*s,y*s);
    }
    /*friend inline const Vector3D operator* (const float& s,const Vector3D& v) {
    return v * s;
    }*/
    /*const*/ Vector2D operator/ (float s) const {
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
    void add(const Vector2D *v) {
        this->x+=v->x;
        this->y+=v->y;
    }
    void subtract(const Vector2D *v) {
        this->x-=v->x;
        this->y-=v->y;
    }
    const float square() const {
        return (x*x + y*y);
    }
    const float magnitude() const {
        return sqrt( x*x + y*y );
    }
    /*const float magnitudeChebyshev() const {
        double dist_x = abs(x);
        double dist_y = abs(y);
        double max_dist = dist_x > dist_y ? dist_x : dist_y;
        return max_dist;
    }*/
    const float dot(const Vector2D *v) const {
        return ( x * v->x + y * v->y);
    }
    const float operator%(const Vector2D& v) const {
        return ( x * v.x + y * v.y);
    }
    void normalise() {
        float mag = magnitude();
        if( mag == 0.0f )
            throw "attempted to normalise zero Vector2D";
        x = x/mag;
        y = y/mag;
    }
    /*const*/ Vector2D unit() const {
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
    bool isEqual(const Vector2D *that, float tol) const {
        Vector2D diff = *this - *that;
        float mag = diff.magnitude();
        return mag <= tol;
    }
    Vector2D perpendicularYToX() const {
        return Vector2D(y, -x);
    }
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

public:
    Polygon2D() {
    }

    Vector2D getPoint(size_t i) const {
        return points.at(i);
    }
    size_t getNPoints() const {
        return points.size();
    }
    void addPoint(Vector2D point) {
        this->points.push_back(point);
    }
};

const float E_TOL_MACHINE = 1.0e-12f;
const float E_TOL_ANGULAR = 1.0e-6f;
const float E_TOL_LINEAR = 1.0e-4f;
