#include "location.h"
#include "character.h"
#include "item.h"
#include "../game.h"

#include <qdebug.h>

#include <algorithm>
using std::min;
using std::max;
using std::swap;

Scenery::Scenery(string name, string image_name) :
    location(NULL), name(name), image_name(image_name), user_data_gfx(NULL),
    is_blocking(false), width(1.0f), height(1.0f),
    opened(false)
{
}

void Scenery::addItem(Item *item) {
    this->items.insert(item);
}

void Scenery::removeItem(Item *item) {
    this->items.erase(item);
}

void Scenery::setOpened(bool opened) {
    this->opened = opened;
    if( this->location != NULL ) {
        this->location->updateScenery(this);
    }
}

void FloorRegion::insertPoint(int indx, Vector2D point) {
    Polygon2D::insertPoint(indx, point);
    EdgeType edge_type = edge_types.at(indx-1);
    this->edge_types.insert( this->edge_types.begin() + indx, edge_type );
    this->temp_marks.insert( this->temp_marks.begin() + indx, 0 );
}

FloorRegion *FloorRegion::createRectangle(float x, float y, float w, float h) {
    FloorRegion *floor_regions = new FloorRegion();
    floor_regions->addPoint(Vector2D(x, y));
    floor_regions->addPoint(Vector2D(x, y+h));
    floor_regions->addPoint(Vector2D(x+w, y+h));
    floor_regions->addPoint(Vector2D(x+w, y));
    return floor_regions;
}

Location::Location() :
    listener(NULL), listener_data(NULL),
    distance_graph(NULL)
{
}

Location::~Location() {
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        delete floor_region;
    }
    for(set<Character *>::iterator iter = characters.begin(); iter != characters.end(); ++iter) {
        Character *character = *iter;
        delete character;
    }
    for(set<Item *>::iterator iter = items.begin(); iter != items.end(); ++iter) {
        Item *item = *iter;
        delete item;
    }
    for(set<Scenery *>::iterator iter = scenerys.begin(); iter != scenerys.end(); ++iter) {
        Scenery *scenery = *iter;
        delete scenery;
    }
    if( distance_graph != NULL ) {
        delete distance_graph;
    }
}

void Location::addFloorRegion(FloorRegion *floorRegion) {
    this->floor_regions.push_back(floorRegion);
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
    character->setLocation(this);
    character->setPos(xpos, ypos);
    this->characters.insert(character);
}

void Location::removeCharacter(Character *character) {
    character->setLocation(NULL);
    this->characters.erase(character);
}

bool Location::hasEnemies(const PlayingGamestate *playing_gamestate) const {
    bool has_enemies = false;
    for(set<Character *>::const_iterator iter = this->characters.begin(); iter != this->characters.end() && !has_enemies; ++iter) {
        const Character *character = *iter;
        if( character->isHostile() ) {
            float dist = (character->getPos() - playing_gamestate->getPlayer()->getPos()).magnitude();
            if( dist <= npc_visibility_c ) {
                has_enemies = true;
            }
        }
    }
    return has_enemies;
}

void Location::addItem(Item *item, float xpos, float ypos) {
    item->setPos(xpos, ypos);
    this->items.insert(item);
    if( this->listener != NULL ) {
        this->listener->locationAddItem(this, item);
    }
}

void Location::removeItem(Item *item) {
    this->items.erase(item);
    if( this->listener != NULL ) {
        this->listener->locationRemoveItem(this, item);
    }
}

void Location::addScenery(Scenery *scenery, float xpos, float ypos) {
    scenery->setLocation(this);
    scenery->setPos(xpos, ypos);
    this->scenerys.insert(scenery);
    if( this->listener != NULL ) {
        this->listener->locationAddScenery(this, scenery);
    }
}

// n.b., would require us to remove the corresponding boundary for collision detection too!
/*void Location::removeScenery(Scenery *scenery) {
    scenery->setLocation(NULL);
    this->scenerys.erase(scenery);
    if( this->listener != NULL ) {
        this->listener->locationRemoveScenery(this, scenery);
    }
}*/

void Location::updateScenery(Scenery *scenery) {
    if( this->listener != NULL ) {
        this->listener->locationUpdateScenery(scenery);
    }
}

void Location::createBoundariesForRegions() {
    LOG("Location::createBoundariesForRegions()\n");
    float eps = 1.0e-5;
    // imprint coi vertices
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            Vector2D p0 = floor_region->getPoint(j);
            Vector2D p1 = floor_region->getPoint((j+1) % floor_region->getNPoints());
            LOG("check edge %f, %f to %f, %f\n", p0.x, p0.y, p1.x, p1.y);
            Vector2D dp = p1 - p0;
            float dp_length = dp.magnitude();
            if( dp_length == 0.0f ) {
                continue;
            }
            dp /= dp_length;
            for(vector<FloorRegion *>::const_iterator iter2 = floor_regions.begin(); iter2 != floor_regions.end(); ++iter2) {
                const FloorRegion *floor_region2 = *iter2;
                if( floor_region == floor_region2 ) {
                    continue;
                }
                for(size_t j2=0;j2<floor_region2->getNPoints();j2++) {
                    Vector2D p = floor_region2->getPoint(j2);
                    Vector2D imp_p = p;
                    imp_p.dropOnLine(p0, dp);
                    float dist_p = (imp_p - p).magnitude();
                    //LOG("    %f, %f is dist: %f\n", p.x, p.y, dist_p);
                    if( dist_p <= eps ) {
                        float dot = ( p - p0 ) % dp;
                        if( dot >= eps && dot <= dp_length - eps ) {
                            // imprint coi vertex
                            LOG("imprint between %f, %f to %f, %f at: %f, %f\n", p0.x, p0.y, p1.x, p1.y, imp_p.x, imp_p.y);
                            floor_region->insertPoint(j+1, imp_p);
                            p1 = imp_p;
                            dp_length = dot;
                        }
                    }
                }
            }
        }
    }

    LOG("calculate which boundary edges are internal\n");
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            Vector2D p0 = floor_region->getPoint(j);
            Vector2D p1 = floor_region->getPoint((j+1) % floor_region->getNPoints());
            LOG("check if edge is internal: %f, %f to %f, %f\n", p0.x, p0.y, p1.x, p1.y);
            Vector2D dp = p1 - p0;
            float dp_length = dp.magnitude();
            if( dp_length == 0.0f ) {
                continue;
            }
            dp /= dp_length;
            bool done = false; // only one other edge should be internal!
            for(vector<FloorRegion *>::const_iterator iter2 = floor_regions.begin(); iter2 != floor_regions.end() && !done; ++iter2) {
                FloorRegion *floor_region2 = *iter2;
                if( floor_region == floor_region2 ) {
                    continue;
                }
                for(size_t j2=0;j2<floor_region2->getNPoints() && !done;j2++) {
                    Vector2D p2 = floor_region2->getPoint(j2);
                    Vector2D p3 = floor_region2->getPoint((j2+1) % floor_region2->getNPoints());
                    // we only need to test for opposed, due to the way floor_region polygons should be ordered
                    float dist0 = (p0 - p3).magnitude();
                    float dist1 = (p1 - p2).magnitude();
                    //LOG("    compare to: %f, %f to %f, %f\n", p2.x, p2.y, p3.x, p3.y);
                    //LOG("    dists: %f, %f", dist0, dist1);
                    if( dist0 <= eps && dist1 <= eps ) {
                        LOG("edge is internal with: %f, %f to %f, %f\n", p2.x, p2.y, p3.x, p3.y);
                        floor_region->setEdgeType(j, FloorRegion::EDGETYPE_INTERNAL);
                        floor_region2->setEdgeType(j2, FloorRegion::EDGETYPE_INTERNAL);
                    }
                }
            }
        }
    }

    LOG("find boundaries\n");
    for(;;) {
        LOG("find an unmarked edge\n");
        FloorRegion *c_floor_region = NULL;
        int c_indx = -1;
        for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end() && c_floor_region==NULL; ++iter) {
            FloorRegion *floor_region = *iter;
            for(size_t j=0;j<floor_region->getNPoints() && c_floor_region==NULL;j++) {
                if( floor_region->getTempMark(j) == 0 && floor_region->getEdgeType(j) == FloorRegion::EDGETYPE_EXTERNAL ) {
                    c_floor_region = floor_region;
                    c_indx = j;
                }
            }
        }
        if( c_floor_region == NULL ) {
            LOG("all done\n");
            break;
        }
        FloorRegion *start_floor_region = c_floor_region;
        int start_indx = c_indx;
        Polygon2D boundary;
        // first point actually added as last point at end
        for(;;) {
            int n_indx = (c_indx+1) % c_floor_region->getNPoints();
            Vector2D back_pvec = c_floor_region->getPoint( c_indx );
            Vector2D pvec = c_floor_region->getPoint( n_indx );
            LOG("on edge %f, %f to %f, %f\n", back_pvec.x, back_pvec.y, pvec.x, pvec.y);
            if( c_floor_region->getTempMark(c_indx) != 0 ) {
                LOG("moved onto boundary already marked: %d, %d\n", c_floor_region, c_indx);
                throw string("moved onto boundary already marked");
            }
            boundary.addPoint(pvec);
            c_floor_region->setTempMark(c_indx, 1);
            if( c_floor_region == start_floor_region && n_indx == start_indx ) {
                break;
            }
            LOG("move on to next edge\n");
            if( c_floor_region->getEdgeType(n_indx) == FloorRegion::EDGETYPE_EXTERNAL ) {
                c_indx = n_indx;
            }
            else {
                LOG("need to find a new boundary\n");
                bool found = false;
                for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end() && !found; ++iter) {
                    FloorRegion *floor_region = *iter;
                    for(size_t j=0;j<floor_region->getNPoints() && !found;j++) {
                        if( floor_region->getTempMark(j) == 0 && floor_region->getEdgeType(j) == FloorRegion::EDGETYPE_EXTERNAL ) {
                            // we only need to check the first pvec, as we want the edge that leaves this point
                            Vector2D o_pvec = floor_region->getPoint(j);
                            float dist = (pvec - o_pvec).magnitude();
                            if( dist <= eps ) {
                                c_floor_region = floor_region;
                                c_indx = j;
                                found = true;
                            }
                        }
                    }
                }
                if( !found ) {
                    LOG("can't find new boundary\n");
                    break;
                }
            }
        }
        this->addBoundary(boundary);
    }
    LOG("reset temp marks\n");
    for(vector<FloorRegion *>::iterator iter = floor_regions.begin(); iter != floor_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            floor_region->setTempMark(j, 0);
        }
    }
}

void Location::createBoundariesForScenery() {
    LOG("Location::createBoundariesForScenery()\n");
    for(set<Scenery *>::iterator iter = scenerys.begin(); iter != scenerys.end(); ++iter) {
        Scenery *scenery = *iter;
        if( !scenery->isBlocking() ) {
            continue;
        }
        Polygon2D boundary;
        boundary.setSource(scenery);
        /*Vector2D pos = scenery->getPos();
        // clockwise, as "inside" should be on the left
        boundary.addPoint(pos);
        boundary.addPoint(pos + Vector2D(1.0f, 0.0f));
        boundary.addPoint(pos + Vector2D(1.0f, 1.0f));
        boundary.addPoint(pos + Vector2D(0.0f, 1.0f));*/
        float width = scenery->getWidth(), height = scenery->getHeight();
        Vector2D pos = scenery->getPos();
        //qDebug("scenery at %f, %f", pos.x, pos.y);
        // clockwise, as "inside" should be on the left
        Vector2D p0 = pos; p0.x -= 0.5f*width; p0.y -= 0.5f*height;
        Vector2D p1 = pos; p1.x += 0.5f*width; p1.y -= 0.5f*height;
        Vector2D p2 = pos; p2.x += 0.5f*width; p2.y += 0.5f*height;
        Vector2D p3 = pos; p3.x -= 0.5f*width; p3.y += 0.5f*height;
        boundary.addPoint(p0);
        boundary.addPoint(p1);
        boundary.addPoint(p2);
        boundary.addPoint(p3);
        this->addBoundary(boundary);
    }
    LOG("    done\n");
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
    if( p0.y > ymax + width && p1.y > ymax + width )
        return;

    if( p0.y <= ymax || p1.y <= ymax ) {
        // check for collision with main 2D box

        // use Liang-Barsky Line Clipping to intersect line segment with axis-aligned 2D box
        // see http://www.cs.helsinki.fi/group/goa/viewing/leikkaus/intro.html
        // and http://www.skytopia.com/project/articles/compsci/clipping.html
        double tmin = 0.0f, tmax = 1.0f;
        const int n_edges_c = 4;
        // left, right, bottom, top
        bool has_t_value[n_edges_c] = {false, false, false, false};
        double t_value[n_edges_c] = {0.0f, 0.0f, 0.0f, 0.0f};
        if( p1.x != p0.x ) {
            has_t_value[0] = true;
            t_value[0] = ( xmin - p0.x ) / (double)( p1.x - p0.x );
            has_t_value[1] = true;
            t_value[1] = ( xmax - p0.x ) / (double)( p1.x - p0.x );
        }
        if( p1.y != p0.y ) {
            has_t_value[2] = true;
            t_value[2] = ( ymin - p0.y ) / (double)( p1.y - p0.y );
            has_t_value[3] = true;
            t_value[3] = ( ymax - p0.y ) / (double)( p1.y - p0.y );
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
            // n.b., intersection positions still in the swept square space
            Vector2D i0 = p0 + dp * tmin;
            Vector2D i1 = p0 + dp * tmax;
            /*if( p0.x == 0.250071f && p0.y == 4.749929f && p1.x == 0.249929f && p1.y == 2.750071f )
            {
                qDebug("hit wall from %f, %f to %f, %f: parms %f, %f", saved_p0.x, saved_p0.y, saved_p1.x, saved_p1.y, tmin, tmax);
                qDebug("    in swept square space: %f, %f and %f, %f", i0.x, i0.y, i1.x, i1.y);
            }*/
            float iy = min(i0.y, i1.y);
            iy -= width;
            iy = max(iy, 0.0f);
            if( iy > ymax ) {
                LOG("intersection is beyond end of swept box\n");
                throw string("intersection is beyond end of swept box");
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

    if( !(*done) ) {
        // check for collision with end circle
        Vector2D end(0.0f, ymax);
        Vector2D diff = end - p0;
        float dt = ( diff % dp ) / dp_length;
        if( dt >= 0.0f && dt <= 1.0f ) {
            Vector2D closest_pt = p0 + dp * dt; // point on line seg closest to end
            if( closest_pt.y > ymax ) {
                double dist = (closest_pt - end).magnitude();
                if( dist <= width ) {
                    // we don't bother calculating the exact collision point - just move back far enough
                    float this_hit_dist = ymax - width;
                    this_hit_dist = max(this_hit_dist, 0.0f);
                    if( !(*hit) || this_hit_dist < *hit_dist ) {
                        *hit = true;
                        *hit_dist = this_hit_dist;
                        if( *hit_dist == 0.0f )
                            *done = true; // no point doing anymore collisions
                    }
                }
            }
        }
    }
}

void Location::intersectSweptSquareWithBoundaries(bool *done, bool *hit, float *hit_dist, Vector2D start, Vector2D end, Vector2D du, Vector2D dv, float width, float xmin, float xmax, float ymin, float ymax, const Scenery *ignore_scenery) {
    for(vector<Polygon2D>::const_iterator iter = this->boundaries.begin(); iter != this->boundaries.end() && !(*done); ++iter) {
        const Polygon2D *boundary = &*iter;
        if( ignore_scenery != NULL && boundary->getSource() == ignore_scenery ) {
            continue;
        }
        for(size_t j=0;j<boundary->getNPoints() && !(*done);j++) {
            Vector2D p0 = boundary->getPoint(j);
            Vector2D p1 = boundary->getPoint((j+1) % boundary->getNPoints());
            intersectSweptSquareWithBoundarySeg(hit, hit_dist, done, p0, p1, start, du, dv, width, xmin, xmax, ymin, ymax);
        }
    }
}

bool Location::intersectSweptSquareWithBoundaries(Vector2D *hit_pos, Vector2D start, Vector2D end, float width, const Scenery *ignore_scenery) {
    bool done = false;
    bool hit = false;
    float hit_dist = 0.0f;

    Vector2D dv = end - start;
    float dv_length = dv.magnitude();
    if( dv_length == 0.0f ) {
        LOG("Location::intersectSweptSquareWithBoundaries received equal start and end\n");
        throw string("Location::intersectSweptSquareWithBoundaries received equal start and end");
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
    //float ymax = dv_length + width;
    float ymax = dv_length;

    intersectSweptSquareWithBoundaries(&done, &hit, &hit_dist, start, end, du, dv, width, xmin, xmax, ymin, ymax, ignore_scenery);

    if( hit ) {
        *hit_pos = start + dv * hit_dist;
    }
    return hit;
}

bool Location::intersectSweptSquareWithBoundariesAndNPCs(const Character *character, Vector2D *hit_pos, Vector2D start, Vector2D end, float width) {
    bool done = false;
    bool hit = false;
    float hit_dist = 0.0f;

    Vector2D dv = end - start;
    float dv_length = dv.magnitude();
    if( dv_length == 0.0f ) {
        LOG("Location::intersectSweptSquareWithBoundaries received equal start and end\n");
        throw string("Location::intersectSweptSquareWithBoundaries received equal start and end");
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

    intersectSweptSquareWithBoundaries(&done, &hit, &hit_dist, start, end, du, dv, width, xmin, xmax, ymin, ymax, NULL);

    for(set<Character *>::const_iterator iter = this->characters.begin(); iter != this->characters.end() && !done; ++iter) {
        const Character *npc = *iter;
        //if( character == playing_gamestate->getPlayer() ) {
        if( npc == character ) {
            continue;
        }
        float npc_width = npc_radius_c;
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

bool Location::collideWithTransient(const Character *character, Vector2D pos) const {
    // does collision detection with entities like NPCs, that do not have boundaries due to not having a permanent position (and hence can't be avoided in pathfinding)
    bool hit = false;
    for(set<Character *>::const_iterator iter = this->characters.begin(); iter != this->characters.end() && !hit; ++iter) {
        const Character *npc = *iter;
        //if( character == playing_gamestate->getPlayer() ) {
        if( npc == character ) {
            continue;
        }
        Vector2D npc_pos = npc->getPos();
        float dist = (pos - npc_pos).magnitude();
        if( dist <= 2.0f * npc_radius_c ) {
            hit = true;
        }
    }
    return hit;
}

vector<Vector2D> Location::calculatePathWayPoints() const {
    qDebug("Location::calculatePathWayPoints");
    vector<Vector2D> path_way_points;
    for(vector<Polygon2D>::const_iterator iter = this->boundaries.begin(); iter != this->boundaries.end(); ++iter) {
        const Polygon2D *boundary = &*iter;
        size_t n_points = boundary->getNPoints();
        for(size_t j=0;j<n_points;j++) {
            Vector2D p_point = boundary->getPoint((j+n_points-1) % n_points);
            Vector2D point = boundary->getPoint(j);
            Vector2D n_point = boundary->getPoint((j+1) % n_points);
            Vector2D d0 = point - p_point;
            Vector2D d1 = n_point - point;
            if( d0.magnitude() < E_TOL_LINEAR || d1.magnitude() < E_TOL_LINEAR ) {
                LOG("p_point: %f, %f\n", p_point.x, p_point.y);
                LOG("point: %f, %f\n", point.x, point.y);
                LOG("n_point: %f, %f\n", n_point.x, n_point.y);
                throw string("coi boundary points not allowed");
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
                //const float offset = npc_radius_c/ratio;
                const float offset = npc_radius_c/ratio + E_TOL_LINEAR;
                Vector2D path_way_point = point + inwards * offset;
                path_way_points.push_back( path_way_point );
            }
        }
    }

    return path_way_points;
}

void Location::calculateDistanceGraph() {
    qDebug("Location::calculateDistanceGraph()");
    if( this->distance_graph != NULL ) {
        delete this->distance_graph;
    }
    this->distance_graph = new Graph();

    vector<Vector2D> path_way_points = this->calculatePathWayPoints();

    for(size_t i=0;i<path_way_points.size();i++) {
        Vector2D A = path_way_points.at(i);
        GraphVertex vertex(A, NULL);
        this->distance_graph->addVertex(vertex);
    }

    for(size_t i=0;i<this->distance_graph->getNVertices();i++) {
        GraphVertex *v_A = this->distance_graph->getVertex(i);
        Vector2D A = v_A->getPos();
        for(size_t j=i+1;j<this->distance_graph->getNVertices();j++) {
            GraphVertex *v_B = this->distance_graph->getVertex(j);
            Vector2D B = v_B->getPos();
            Vector2D hit_pos;
            float dist = (A - B).magnitude();
            bool hit = false;
            if( dist <= E_TOL_LINEAR ) {
                // needed for coi way points?
                hit = false;
            }
            else {
                hit = this->intersectSweptSquareWithBoundaries(&hit_pos, A, B, npc_radius_c, NULL);
            }
            if( !hit ) {
                v_A->addNeighbour(j, dist);
                v_B->addNeighbour(i, dist);
            }
        }
    }
}

#if 0
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
#endif

Quest::Quest() {
}

Quest::~Quest() {
    for(vector<Location *>::iterator iter = this->locations.begin(); iter != this->locations.end(); ++iter) {
        Location *location = *iter;
        delete location;
    }
}

void Quest::addLocation(Location *location) {
    this->locations.push_back(location);
}
