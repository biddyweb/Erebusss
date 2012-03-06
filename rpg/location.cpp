#include "location.h"
#include "character.h"
//#include "../game.h"

#include <qdebug.h>

#include <algorithm>
using std::min;
using std::max;
using std::swap;

FloorRegion *FloorRegion::createRectangle(float x, float y, float w, float h) {
    FloorRegion *floor_regions = new FloorRegion();
    floor_regions->points.push_back(Vector2D(x, y));
    floor_regions->points.push_back(Vector2D(x, y+h));
    floor_regions->points.push_back(Vector2D(x+w, y+h));
    floor_regions->points.push_back(Vector2D(x+w, y));
    return floor_regions;
}

Location::Location(/*float width, float height*/) /*:
    width(width), height(height)*/
{
}

Location::~Location() {
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        delete floor_region;
    }
}

void Location::calculateSize(float *w, float *h) const {
    *w = 0.0f;
    *h = 0.0f;
    for(vector<FloorRegion *>::const_iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        const FloorRegion *floor_region = *iter;
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            Vector2D point = floor_region->getPoint(j);
            *w = max(*w, point.x);
            *h = max(*h, point.y);
        }
    }
}

void Location::addCharacter(Character *character, float xpos, float ypos) {
    character->setPos(xpos, ypos);
    this->characters.insert(character);
}

void Location::intersectSweptSquareWithBoundarySeg(bool *hit, float *hit_dist, bool *done, Vector2D p0, Vector2D p1, Vector2D start, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax) {
    Vector2D saved_p0 = p0; // for debugging
    Vector2D saved_p1 = p1;
    // transform into the space of the swept square
    p0 -= start;
    p0 = Vector2D( p0 % du, p0 % dv );
    p1 -= start;
    p1 = Vector2D( p1 % du, p1 % dv );
    Vector2D dp = p1 - p0;
    float dp_length = dp.magnitude();
    if( dp_length == 0.0f ) {
        return;
    }
    // n.b., dp is kept as non-unit vector
    Vector2D normal_from_wall = dp.perpendicularYToX(); // since walls are defined anti-clockwise, this vector must point away from the wall
    //qDebug("candidate hit wall from %f, %f to %f, %f:", saved_p0.x, saved_p0.y, saved_p1.x, saved_p1.y);
    //qDebug("    transformed to: %f, %f to %f, %f", p0.x, p0.y, p1.x, p1.y);
    //qDebug("    normal_from_wall = %f, %f", normal_from_wall.x, normal_from_wall.y);
    if( normal_from_wall.y >= 0.0f ) {
        return; // okay to move along or away from wall (if there are any intersections, it means we've already started from an intersection point, so best to allow us to move away!)
    }
    // trivial rejections:
    if( p0.x < xmin && p1.x < xmin )
        return;
    if( p0.x > xmax && p1.x > xmax )
        return;
    if( p0.y < ymin && p1.y < ymin )
        return;
    if( p0.y > ymax && p1.y > ymax )
        return;
    // use Liang-Barsky Line Clipping to intersect line segment with axis-aligned 2D box
    // see http://www.cs.helsinki.fi/group/goa/viewing/leikkaus/intro.html
    // and http://www.skytopia.com/project/articles/compsci/clipping.html
    float tmin = 0.0f, tmax = 1.0f;
    const int n_edges_c = 4;
    // left, right, bottom, top
    bool has_t_value[n_edges_c] = {false, false, false, false};
    float t_value[n_edges_c] = {0.0f, 0.0f, 0.0f, 0.0f};
    if( p1.x != p0.x ) {
        has_t_value[0] = true;
        t_value[0] = ( xmin - p0.x ) / ( p1.x - p0.x );
        has_t_value[1] = true;
        t_value[1] = ( xmax - p0.x ) / ( p1.x - p0.x );
    }
    if( p1.y != p0.y ) {
        has_t_value[2] = true;
        t_value[2] = ( ymin - p0.y ) / ( p1.y - p0.y );
        has_t_value[3] = true;
        t_value[3] = ( ymax - p0.y ) / ( p1.y - p0.y );
    }
    Vector2D normals[n_edges_c];
    normals[0] = Vector2D(-1.0f, 0.0f);
    normals[1] = Vector2D(1.0f, 0.0f);
    normals[2] = Vector2D(0.0f, -1.0f);
    normals[3] = Vector2D(0.0f, 1.0f);
    for(int k=0;k<n_edges_c;k++) {
        if( !has_t_value[k] )
            continue;
        /*if( t_value[k] < tmin || t_value[k] > tmax )
            continue;*/
        bool entering = ( dp % normals[k] ) < 0.0f;
        //qDebug("    edge %d: t value %f, is entering? %d", k, t_value[k], entering?1:0);
        // modification from standard algorithm - we only want to clip if it's hit the main box!
        /*Vector2D ix = p0 + dp * t_value[k];
        if( ix.x < xmin || ix.x > xmax || ix.y < ymin || ix.y > ymax ) {
            continue;
        }*/
        //qDebug("        intersects against swept square");
        if( entering ) {
            if( t_value[k] > tmin )
                tmin = t_value[k];
        }
        else {
            if( t_value[k] < tmax )
                tmax = t_value[k];
        }
    }
    if( tmin <= tmax ) {
        //qDebug("hit wall from %f, %f to %f, %f: parms %f, %f", saved_p0.x, saved_p0.y, saved_p1.x, saved_p1.y, tmin, tmax);
        // n.b., intersection positions still in the swept square space
        Vector2D i0 = p0 + dp * tmin;
        Vector2D i1 = p0 + dp * tmax;
        //qDebug("    in swept square space: %f, %f and %f, %f", i0.x, i0.y, i1.x, i1.y);
        float iy = min(i0.y, i1.y);
        iy -= width;
        iy = max(iy, 0.0f);
        if( iy > ymax ) {
            qDebug("intersection is beyond end of swept box");
            throw "intersection is beyond end of swept box";
        }
        if( !(*hit) || iy < *hit_dist ) {
            *hit = true;
            *hit_dist = iy;
            //qDebug("    hit dist is now %f", *hit_dist);
            if( *hit_dist == 0.0f )
                *done = true; // no point doing anymore collisions
        }
    }
}

bool Location::intersectSweptSquareWithBoundaries(const Character *character, Vector2D *hit_pos, Vector2D start, Vector2D end, float width) {
    bool hit = false;
    float hit_dist = 0.0f;

    //qDebug("start: %f, %f", start.x, start.y);

    Vector2D dv = end - start;
    float dv_length = dv.magnitude();
    if( dv_length == 0.0f ) {
        qDebug("Location::intersectSweptSquareWithBoundaries received equal start and end");
        throw "Location::intersectSweptSquareWithBoundaries received equal start and end";
    }
    dv /= dv_length;
    Vector2D du = dv.perpendicularYToX();
    //qDebug("du = %f, %f", du.x, du.y);
    //qDebug("dv = %f, %f", dv.x, dv.y);
    //qDebug("dv_length: %f", dv_length);
    float xmin = - width;
    float xmax = width;
    //float ymin = - width;
    float ymin = 0.0f;
    float ymax = dv_length + width;
    bool done = false;
    for(vector<Polygon2D>::const_iterator iter = this->boundaries.begin(); iter != this->boundaries.end() && !done; ++iter) {
        const Polygon2D *boundary = &*iter;
        for(size_t j=0;j<boundary->getNPoints() && !done;j++) {
            Vector2D p0 = boundary->getPoint(j);
            Vector2D p1 = boundary->getPoint((j+1) % boundary->getNPoints());
            intersectSweptSquareWithBoundarySeg(&hit, &hit_dist, &done, p0, p1, start, du, dv, width, xmin, xmax, ymin, ymax);
#if 0
            Vector2D saved_p0 = p0; // for debugging
            Vector2D saved_p1 = p1;
            // transform into the space of the swept square
            p0 -= start;
            p0 = Vector2D( p0 % du, p0 % dv );
            p1 -= start;
            p1 = Vector2D( p1 % du, p1 % dv );
            Vector2D dp = p1 - p0;
            float dp_length = dp.magnitude();
            if( dp_length == 0.0f )
                continue;
            // n.b., dp is kept as non-unit vector
            Vector2D normal_from_wall = dp.perpendicularYToX(); // since walls are defined anti-clockwise, this vector must point away from the wall
            //qDebug("candidate hit wall from %f, %f to %f, %f:", saved_p0.x, saved_p0.y, saved_p1.x, saved_p1.y);
            //qDebug("    transformed to: %f, %f to %f, %f", p0.x, p0.y, p1.x, p1.y);
            //qDebug("    normal_from_wall = %f, %f", normal_from_wall.x, normal_from_wall.y);
            if( normal_from_wall.y >= 0.0f )
                continue; // okay to move along or away from wall (if there are any intersections, it means we've already started from an intersection point, so best to allow us to move away!)
            // trivial rejections:
            if( p0.x < xmin && p1.x < xmin )
                continue;
            if( p0.x > xmax && p1.x > xmax )
                continue;
            if( p0.y < ymin && p1.y < ymin )
                continue;
            if( p0.y > ymax && p1.y > ymax )
                continue;
            // use Liang-Barsky Line Clipping to intersect line segment with axis-aligned 2D box
            // see http://www.cs.helsinki.fi/group/goa/viewing/leikkaus/intro.html
            // and http://www.skytopia.com/project/articles/compsci/clipping.html
            float tmin = 0.0f, tmax = 1.0f;
            const int n_edges_c = 4;
            // left, right, bottom, top
            bool has_t_value[n_edges_c] = {false, false, false, false};
            float t_value[n_edges_c] = {0.0f, 0.0f, 0.0f, 0.0f};
            if( p1.x != p0.x ) {
                has_t_value[0] = true;
                t_value[0] = ( xmin - p0.x ) / ( p1.x - p0.x );
                has_t_value[1] = true;
                t_value[1] = ( xmax - p0.x ) / ( p1.x - p0.x );
            }
            if( p1.y != p0.y ) {
                has_t_value[2] = true;
                t_value[2] = ( ymin - p0.y ) / ( p1.y - p0.y );
                has_t_value[3] = true;
                t_value[3] = ( ymax - p0.y ) / ( p1.y - p0.y );
            }
            Vector2D normals[n_edges_c];
            normals[0] = Vector2D(-1.0f, 0.0f);
            normals[1] = Vector2D(1.0f, 0.0f);
            normals[2] = Vector2D(0.0f, -1.0f);
            normals[3] = Vector2D(0.0f, 1.0f);
            for(int k=0;k<n_edges_c;k++) {
                if( !has_t_value[k] )
                    continue;
                /*if( t_value[k] < tmin || t_value[k] > tmax )
                    continue;*/
                bool entering = ( dp % normals[k] ) < 0.0f;
                //qDebug("    edge %d: t value %f, is entering? %d", k, t_value[k], entering?1:0);
                // modification from standard algorithm - we only want to clip if it's hit the main box!
                /*Vector2D ix = p0 + dp * t_value[k];
                if( ix.x < xmin || ix.x > xmax || ix.y < ymin || ix.y > ymax ) {
                    continue;
                }*/
                //qDebug("        intersects against swept square");
                if( entering ) {
                    if( t_value[k] > tmin )
                        tmin = t_value[k];
                }
                else {
                    if( t_value[k] < tmax )
                        tmax = t_value[k];
                }
            }
            if( tmin <= tmax ) {
                //qDebug("hit wall from %f, %f to %f, %f: parms %f, %f", saved_p0.x, saved_p0.y, saved_p1.x, saved_p1.y, tmin, tmax);
                // n.b., intersection positions still in the swept square space
                Vector2D i0 = p0 + dp * tmin;
                Vector2D i1 = p0 + dp * tmax;
                //qDebug("    in swept square space: %f, %f and %f, %f", i0.x, i0.y, i1.x, i1.y);
                float iy = min(i0.y, i1.y);
                iy -= width;
                iy = max(iy, 0.0f);
                if( iy > ymax ) {
                    qDebug("intersection is beyond end of swept box");
                    throw "intersection is beyond end of swept box";
                }
                if( !hit || iy < hit_dist ) {
                    hit = true;
                    hit_dist = iy;
                    //qDebug("    hit dist is now %f", hit_dist);
                    if( hit_dist == 0.0f )
                        done = true; // no point doing anymore collisions
                }
            }
#endif
        }
    }
    for(set<Character *>::const_iterator iter = this->characters.begin(); iter != this->characters.end() && !done; ++iter) {
        const Character *npc = *iter;
        //if( character == playing_gamestate->getPlayer() ) {
        if( npc == character ) {
            continue;
        }
        float npc_width = npc->getRadius();
        Vector2D npc_pos = npc->getPos();
        //qDebug("npc at %f, %f", npc_pos.x, npc_pos.y);
        // clockwise, as "inside" should be on the left
        Vector2D p0 = npc_pos; p0.x -= npc_width; p0.y -= npc_width;
        Vector2D p1 = npc_pos; p1.x += npc_width; p1.y -= npc_width;
        Vector2D p2 = npc_pos; p2.x += npc_width; p2.y += npc_width;
        Vector2D p3 = npc_pos; p3.x -= npc_width; p3.y += npc_width;
        intersectSweptSquareWithBoundarySeg(&hit, &hit_dist, &done, p0, p1, start, du, dv, width, xmin, xmax, ymin, ymax);
        if( !done )
            intersectSweptSquareWithBoundarySeg(&hit, &hit_dist, &done, p1, p2, start, du, dv, width, xmin, xmax, ymin, ymax);
        if( !done )
            intersectSweptSquareWithBoundarySeg(&hit, &hit_dist, &done, p2, p3, start, du, dv, width, xmin, xmax, ymin, ymax);
        if( !done )
            intersectSweptSquareWithBoundarySeg(&hit, &hit_dist, &done, p3, p0, start, du, dv, width, xmin, xmax, ymin, ymax);
    }

    if( hit ) {
        *hit_pos = start + dv * hit_dist;
    }
    return hit;
}

#if 0
bool Location::intersectSweptCircleWithBoundaries(Vector2D *hit_pos, Vector2D start, Vector2D end, float radius) {
    bool hit = false;
    float hit_dist = 0.0f;

    //qDebug("start: %f, %f", start.x, start.y);
    float radius2 = radius*radius;
    // first find intersection of lines - http://en.wikipedia.org/wiki/Line-line_intersection
    Vector2D ds = end - start;
    float ds_length = ds.magnitude();
    if( ds_length == 0.0f ) {
        throw "Location::intersectSweptCircleWithBoundaries received equal start and end";
    }
    ds /= ds_length;
    for(vector<Polygon2D>::const_iterator iter = this->boundaries.begin(); iter != this->boundaries.end(); ++iter) {
        const Polygon2D *boundary = &*iter;
        for(size_t j=0;j<boundary->getNPoints();j++) {
            Vector2D p0 = boundary->getPoint(j);
            Vector2D p1 = boundary->getPoint((j+1) % boundary->getNPoints());
            Vector2D dp = p1 - p0;
            float dp_length = dp.magnitude();
            if( dp_length == 0.0f )
                continue;
            dp /= dp_length;
            float det = (start.x - end.x) * (p0.y - p1.y) - (start.y - end.y) * (p0.x - p1.x);
            if( fabs(det) < E_TOL_MACHINE ) {
                qDebug("special case - parallel lines");
                //throw "special case";
                float t0 = (p0 - start) % ds;
                float t1 = (p1 - start) % ds;
                float t = min(t0, t1);
                if( t >= 0.0f && t < ds_length ) {
                    // actually assumes square rather than circle here, but good enough!
                    t -= radius;
                    if( t < 0.0f )
                        t = 0.0f;
                    if( !hit || t < hit_dist ) {
                        hit = true;
                        hit_dist = t;
                    }
                }
            }
            else {
                /*Vector2D this_hit;
                this_hit.x = (start.x * end.y - start.y * end.x) * (p0.x - p1.x) - (start.x - end.x) * (p0.x * p1.y - p0.y * p1.x);
                this_hit.y = (start.x * end.y - start.y * end.x) * (p0.y - p1.y) - (start.y - end.y) * (p0.x * p1.y - p0.y * p1.x);
                this_hit /= det;*/
                // intersect parameter for swept sphere
                float t = (p1.x - p0.x) * (start.y - p0.y) - (p1.y - p0.y) * (start.x - p0.x);
                t *= ds_length;
                t /= det;
                if( t > 0.0f ) {
                    // if intersection point is behind the start, we must be trying to move away from this wall
                    // now find points where sphere starts and ends intersect
                    float signed_cos_theta = ds % dp;
                    float cos_theta = fabs( signed_cos_theta );
                    float theta = acos(cos_theta);
                    // parameters on swept sphere
                    float t_step = radius / sin(theta);
                    float t0 = t - t_step;
                    float t1 = t + t_step;
                    if( t1 >= -E_TOL_LINEAR && t0 <= ds_length + E_TOL_LINEAR ) {
                        // intersect parameter for line
                        float alpha = (end.x - start.x) * (start.y - p0.y) - (end.y - start.y) * (start.x - p0.x);
                        alpha *= dp_length;
                        alpha /= det;
                        // limit by line seg
                        float p0_from_x = 0.0f - alpha;
                        float p0_on_sphere_line = t + p0_from_x / signed_cos_theta;
                        float p1_from_x = dp_length - alpha;
                        float p1_on_sphere_line = t + p1_from_x / signed_cos_theta;
                        //qDebug("candidate hit wall from %f, %f to %f, %f: parms %f, %f", p0.x, p0.y, p1.x, p1.y, t, alpha);
                        //qDebug("t0 = %f, t1 = %f", t0, t1);
                        if( p1_on_sphere_line < p0_on_sphere_line )
                            swap(p0_on_sphere_line, p1_on_sphere_line);
                        //qDebug("p0_on_sphere_line = %f, p1_on_sphere_line = %f", p0_on_sphere_line, p1_on_sphere_line);
                        if( p0_on_sphere_line > t0 )
                            t0 = p0_on_sphere_line;
                        if( p1_on_sphere_line < t1 )
                            t1 = p1_on_sphere_line;
                        //if( t1 >= -E_TOL_LINEAR && t0 <= ds_length + E_TOL_LINEAR ) {
                        if( t1 > 0.0f && t0 < ds_length ) {
                            if( t0 < 0.0f )
                                t0 = 0.0f;
                            float alpha_step = radius / tan(theta);
                            float alpha0 = alpha - alpha_step;
                            float alpha1 = alpha + alpha_step;
                            //qDebug("hit wall from %f, %f to %f, %f: parms %f, %f", p0.x, p0.y, p1.x, p1.y, t, alpha);
                            Vector2D sphere_pos = start + ds * t;
                            Vector2D wall_pos = p0 + dp * alpha;
                            //qDebug("    sphere pos: %f, %f", sphere_pos.x, sphere_pos.y);
                            //qDebug("    wall pos: %f, %f", wall_pos.x, wall_pos.y);
                            //qDebug("    sphere range: %f -> %f", t0, t1);
                            //qDebug("    wall range: %f -> %f", alpha0, alpha1);
                            if( !hit || t0 < hit_dist ) {
                                hit = true;
                                hit_dist = t0;
                            }
                        }
                    }
                }
            }
        }
    }

    if( hit ) {
        *hit_pos = start + ds * hit_dist;
    }
    return hit;
}
#endif

#if 0
bool Location::intersectSweptCircleWithBoundaries(Vector2D *hit_pos, Vector2D start, Vector2D end, float radius) {
    // see http://www.gamasutra.com/view/feature/3383/simple_intersection_tests_for_games.php?page=2
    // also Core Techniques and Algorithms in Game Programming, Daniel Sanchez-Crespo Dalmau, pp. 696+
    bool hit = false;
    float hit_dist = 0.0f;

    Vector2D ds = end - start;
    float ds_length = ds.magnitude();
    if( ds_length == 0.0f ) {
        throw "Location::intersectSweptCircleWithBoundaries received equal start and end";
    }
    ds /= ds_length;
    float radius2 = radius * radius;
    for(vector<Polygon2D>::const_iterator iter = this->boundaries.begin(); iter != this->boundaries.end(); ++iter) {
        const Polygon2D *boundary = &*iter;
        for(size_t j=0;j<boundary->getNPoints();j++) {
            Vector2D p0 = boundary->getPoint(j);
            Vector2D p1 = boundary->getPoint((j+1) % boundary->getNPoints());
            Vector2D dp = p1 - p0;
            float dp_length = dp.magnitude();
            if( dp_length == 0.0f )
                continue;
            dp /= dp_length;
            Vector2D dpos = p0 - start;
            Vector2D dv = dp - ds;
            if( dv.magnitude() < E_TOL_MACHINE ) {
                // special case - parallel lines
                throw "special case";
            }
            else {
                float a = dv.square();
                float b = 2.0f * ( dpos % dv );
                float c = dpos.square() - radius2;
                float det = b*b - 4.0f*a*c;
                if( det > E_TOL_MACHINE ) {
                    float sqrt_det = sqrt(det);
                    float t0 = ( - b - sqrt_det ) / ( 2.0f * a );
                    float t1 = ( - b + sqrt_det ) / ( 2.0f * a );
                }
            }
        }
    }
    return hit;
}
#endif

vector<Vector2D> Location::calculatePathWayPoints() const {
    vector<Vector2D> path_way_points;
    for(vector<Polygon2D>::const_iterator iter = this->boundaries.begin(); iter != this->boundaries.end(); ++iter) {
        const Polygon2D *boundary = &*iter;
        int n_points = boundary->getNPoints();
        for(size_t j=0;j<n_points;j++) {
            Vector2D p_point = boundary->getPoint((j+n_points-1) % n_points);
            Vector2D point = boundary->getPoint(j);
            Vector2D n_point = boundary->getPoint((j+1) % n_points);
            Vector2D d0 = point - p_point;
            Vector2D d1 = n_point - point;
            if( d0.magnitude() < E_TOL_LINEAR || d1.magnitude() < E_TOL_LINEAR ) {
                throw string("coi boundary points not allowed");
            }
            d0.normalise();
            d1.normalise();
            // these vectors must point away from the wall
            Vector2D normal_from_wall0 = d0.perpendicularYToX();
            Vector2D normal_from_wall1 = d1.perpendicularYToX();
            Vector2D inwards = normal_from_wall0 + normal_from_wall1;
            if( inwards.magnitude() > E_TOL_MACHINE ) {
                inwards.normalise();
                Vector2D path_way_point = point + inwards * 0.5f;
                path_way_points.push_back( path_way_point );
            }
        }
    }

    return path_way_points;
}

void Location::precalculate() {
    /*boundaries.clear();

    vector<LineSeg> walls;
    for(vector<FloorRegion *>::const_iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        const FloorRegion *floor_region = *iter;
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            Vector2D p0 = floor_region->getPoint(j);
            Vector2D p1 = floor_region->getPoint( (j+1) % floor_region->getNPoints() );
            LineSeg line_seg(p0, p1);
            Vector2D dir = p1 - p0;
            // TODO: error if zero
            float length = dir.magnitude();
            dir /= length;

            // look for existing coi wall
            bool found_coi = false;
            for(size_t k=0;k<walls.size();k++) {
                LineSeg this_line_seg = walls.at(k);
                Vector2D this_dir = this_line_seg.end - this_line_seg.start;
                // TODO: error if zero
                // TODO: look for crossing intersections too - means error
                this_dir.normalise();
                float cos_angle = fabs(this_dir % dir);
                if( cos_angle > 1.0f - 1.0e-5f ) {
                    float d0 = (this_line_seg.start - p0).magnitude();
                    float d1 = (this_line_seg.end - p0).magnitude();
                    if( d1 < d0 ) {
                        swap(d0, d1);
                        swap(this_line_seg.start, this_line_seg.end);
                    }
                    if( d1 < 0.0f || d0 > length ) {
                        // doesn't intersect
                    }
                    else {
                        found_coi = true;
                        walls.erase(walls.begin() + k);
                        k--;

                        if( d0 < 0.0f || d1 > length ) {
                            // TODO:
                        }
                        else if( d0 == 0.0f && d1 == length ) {
                            // fully coi, wall entirely removed
                        }
                        else if( d0 == 0.0f ) {
                            //LineSeg remaining_wall()
                        }
                    }
                }
            }

            if( !found_coi ) {
                walls.push_back(line_seg);
            }
        }
    }*/

}
