#include <simgear/sg_inlines.h>

#include "tg_polygon.hxx"
#include "tg_shapefile.hxx" 
#include "tg_intersection_edge.hxx"
#include "tg_intersection_node.hxx"
#include "tg_misc.hxx"
#include "tg_euclidean.hxx"
      
#define DEBUG_INTERSECTIONS (0)
#define LOG_INTERSECTION    (SG_DEBUG)

// generate intersection edge in euclidean space
tgIntersectionEdge::tgIntersectionEdge( tgIntersectionNode* s, tgIntersectionNode* e, double w, unsigned int t, const std::string& dr ) 
{
    static unsigned int ge_count = 0;

    start = s;
    end   = e;
    width = w;
    type  = t;
    
    id = ++ge_count;
    
    msbr_set = false;
    msbr_valid = false;
    
    mstr_set = false;
    mstr_valid = false;
    
    mstl_set = false;
    mstl_valid = false;

    msbl_set = false;
    msbl_valid = false;
    
    // we need to add this edge between start and end to handle multiple edges at a node
    s->AddEdge( true, this );
    e->AddEdge( false, this );
    
    double ecourse   = TGEuclidean::courseDeg(start->GetPosition(), end->GetPosition());
    double elcourse  = SGMiscd::normalizePeriodic(0, 360, ecourse - 90);
        
    botLeft   = TGEuclidean::direct( start->GetPosition(), elcourse,  width/2 );    
    botRight  = TGEuclidean::direct( start->GetPosition(), elcourse, -width/2 );

    topLeft   = TGEuclidean::direct( end->GetPosition(), elcourse,  width/2 );
    topRight  = TGEuclidean::direct( end->GetPosition(), elcourse, -width/2 );
    
    // make sides 1 bit longer...
    SGGeod side_bl = TGEuclidean::direct( botLeft, ecourse, -10.0 );
    SGGeod side_tl = TGEuclidean::direct( topLeft, ecourse,  10.0 );
    
    SGGeod side_br = TGEuclidean::direct( botRight, ecourse, -10.0 );
    SGGeod side_tr = TGEuclidean::direct( topRight, ecourse,  10.0 );
        
    side_l    = tgLine( side_bl, side_tl );
    side_r    = tgLine( side_br, side_tr );

    debugRoot = dr;
    sprintf(datasource, "./edge_dbg/%s/edge_%02ld/", debugRoot.c_str(), id );
}
    
double tgIntersectionEdge::GetHeading( bool originating ) const 
{
    if ( originating ) {
        return TGEuclidean::courseDeg( start->GetPosition(), end->GetPosition() );
    } else {
        return TGEuclidean::courseDeg( end->GetPosition(), start->GetPosition() );
    }
}
    
double tgIntersectionEdge::GetLength( void ) const 
{
    return SGGeodesy::distanceM( start->GetPosition(), end->GetPosition() );
}
    
tgIntersectionEdge* tgIntersectionEdge::Split( tgIntersectionNode* newEnd )
{    
    // first - inform the ending node that we aren't associated anymore
    tgIntersectionNode* oldEnd = end;
    oldEnd->DelEdge( false, this );
    
    // now, edit the first half add new end node
    end = newEnd;
    newEnd->AddEdge( false, this );
    
    // then update all the geometery info
    // the courses may change slightly as we snap the nodes
    double ecourse   = TGEuclidean::courseDeg(start->GetPosition(), end->GetPosition());
    double elcourse  = SGMiscd::normalizePeriodic(0, 360, ecourse - 90);
        
    botLeft   = TGEuclidean::direct( start->GetPosition(), elcourse,  width/2 );    
    botRight  = TGEuclidean::direct( start->GetPosition(), elcourse, -width/2 );

    topLeft   = TGEuclidean::direct( end->GetPosition(), elcourse,  width/2 );
    topRight  = TGEuclidean::direct( end->GetPosition(), elcourse, -width/2 );
    
    side_l    = tgLine( botLeft,  topLeft );
    side_r    = tgLine( botRight, topRight );
    
    // now create the new edge with start == new end, end = oldEnd;
    return new tgIntersectionEdge( newEnd, oldEnd, width, type, debugRoot );
}
    
tgRectangle tgIntersectionEdge::GetBoundingBox( void ) const
{
    SGGeod min, max;

    double minx =  SG_MIN2( start->GetPosition().getLongitudeDeg(), end->GetPosition().getLongitudeDeg() );
    double miny =  SG_MIN2( start->GetPosition().getLatitudeDeg(),  end->GetPosition().getLatitudeDeg() );
    double maxx =  SG_MAX2( start->GetPosition().getLongitudeDeg(), end->GetPosition().getLongitudeDeg() );
    double maxy =  SG_MAX2( start->GetPosition().getLatitudeDeg(),  end->GetPosition().getLatitudeDeg() );

    min = SGGeod::fromDeg( minx, miny );
    max = SGGeod::fromDeg( maxx, maxy );

    return tgRectangle( min, max );
}
    
void tgIntersectionEdge::ToShapefile( void ) const
{            
#if DEBUG_INTERSECTIONS    
    char layer[128];
    
    // draw line from start to end
    tgSegment skel = tgSegment( start->GetPosition(), end->GetPosition() );
    sprintf( layer, "%ld_skeleton", id );
    tgShapefile::FromSegment( skel, true, datasource, layer, "edge" );
    
    // just write the array of constraints
    sprintf( layer, "%ld_ends", id );
    tgShapefile::FromRayList( constrain_bl, datasource, layer, "botLeft" );
    tgShapefile::FromRayList( constrain_br, datasource, layer, "botRight" );
    tgShapefile::FromRayList( constrain_tr, datasource, layer, "topRight" );
    tgShapefile::FromRayList( constrain_tl, datasource, layer, "topLeft" );
    
    sprintf( layer, "%ld_sides", id );
    tgShapefile::FromLine( side_l, datasource, layer, "left" );
    tgShapefile::FromLine( side_r, datasource, layer, "right" );
    
    // now any multiseg constraints
    sprintf( layer, "%ld_ms_corners", id );
    DumpConstraint( layer, "botLeft",  constrain_msbl );
    DumpConstraint( layer, "botRight", constrain_msbr );
    DumpConstraint( layer, "topRight", constrain_mstr );
    DumpConstraint( layer, "topLeft",  constrain_mstl );
    
    // then the single segment corners
    sprintf( layer, "%ld_ss_corners", id );
    tgShapefile::FromGeod( conBotLeft,  datasource, layer, "botLeft" );
    tgShapefile::FromGeod( conBotRight, datasource, layer, "botRight" );
    tgShapefile::FromGeod( conTopRight, datasource, layer, "topRight" );
    tgShapefile::FromGeod( conTopLeft,  datasource, layer, "topLeft" );
    
    // finally, the final contours
    sprintf( layer, "%ld_complete", id );
    DumpConstraint( layer, "right", right_contour );
    DumpConstraint( layer, "left",  left_contour );
#endif    
}

tgSegment tgIntersectionEdge::ToSegment( void ) const
{            
    return tgSegment( start->GetPosition(), end->GetPosition() );
}

SGGeod tgIntersectionEdge::IntersectCorner( const SGGeod& pos, tgray_list& constraint1, tgray_list& constraint2, tgLine& side,
                                     const char* c1name, const char* c2name, const char* sname )
{
    // super simple version of this function - intersect the first constraint with the side...
    // TODO : Just move it into IntersectConstraintsAndSides
    SGGeod side_intersect;

    constraint1[0].Intersect( side, side_intersect );
    
    return side_intersect;
}

void tgIntersectionEdge::IntersectConstraintsAndSides(tgIntersectionEdgeInfo* cur)
{        
    bool ce_originating = cur->IsOriginating();
    
    if ( ce_originating ) {        
        if ( constrain_bl.size() == 1 ) {
            conBotLeft = IntersectCorner( start->GetPosition(), constrain_bl, constrain_tl, 
                                          side_l, "BL", "TL", "LS" );            
        } else {
            double dist = SGGeodesy::distanceM(start->GetPosition(), end->GetPosition() );
            SG_LOG(SG_GENERAL, LOG_INTERSECTION, "tgIntersectionEdge::IntersectConstraintsAndSides: cur edge has " << constrain_bl.size() << " bl constraints - expecting 1. Length is " << dist);                    
        }

        if ( constrain_br.size() == 1 ) {
            conBotRight = IntersectCorner( start->GetPosition(), constrain_br, constrain_tr, 
                                           side_r, "BR", "TR", "RS" );
        } else {
            double dist = SGGeodesy::distanceM(start->GetPosition(), end->GetPosition() );
            SG_LOG(SG_GENERAL, LOG_INTERSECTION, "tgIntersectionEdge::IntersectConstraintsAndSides: cur edge has " << constrain_br.size() << " br constraints - expecting 1. length is " << dist );                    
        }
    } else {
        if ( constrain_tl.size() == 1 ) {
            conTopLeft = IntersectCorner( end->GetPosition(), constrain_tl, constrain_bl, 
                                          side_l, "TL", "BL", "LS" );            
        } else {
            double dist = SGGeodesy::distanceM(start->GetPosition(), end->GetPosition() );            
            SG_LOG(SG_GENERAL, LOG_INTERSECTION, "tgIntersectionEdge::IntersectConstraintsAndSides: cur edge has " << constrain_tl.size() << " tl constraints - expecting 1.  length is " << dist );                    
        }

        if ( constrain_tr.size() == 1 ) {
            conTopRight = IntersectCorner( end->GetPosition(), constrain_tr, constrain_br, 
                                           side_r, "TR", "BR", "RS" );
        } else {
            double dist = SGGeodesy::distanceM(start->GetPosition(), end->GetPosition() );
            SG_LOG(SG_GENERAL, LOG_INTERSECTION, "tgIntersectionEdge::IntersectConstraintsAndSides: cur edge has " << constrain_tr.size() << " tr constraints - expecting 1.  Length is " << dist );                    
        }        
    }    
}

void tgIntersectionEdge::SetLeftConstraint( bool originating, const std::list<SGGeod>& cons )
{
    if ( originating ) {
        if (!msbl_valid) {
            constrain_msbl.clear();
        
            // if we are originating, push to front of bottom left constraint
            for ( std::list<SGGeod>::const_iterator it = cons.begin(); it != cons.end(); it ++ ) {
                constrain_msbl.push_front( (*it) );
            }
            msbl_set = true;
        }
    } else {
        if (!mstr_valid) {
            constrain_mstr.clear();

            // otherwise, push to front of top right constraint
            for ( std::list<SGGeod>::const_iterator it = cons.begin(); it != cons.end(); it ++ ) {
                constrain_mstr.push_front( (*it) );
            }
            mstr_set = true;
        }
    }
}

void tgIntersectionEdge::SetRightConstraint( bool originating, const std::list<SGGeod>& cons )
{
    if ( originating ) {
        if (!msbr_valid) {
            constrain_msbr.clear();
        
            // if we are originating, push to back of bottom right constraint
            for ( std::list<SGGeod>::const_iterator it = cons.begin(); it != cons.end(); it ++ ) {
                constrain_msbr.push_back( (*it) );
            }
            msbr_set = true;
        }
    } else {
        if (!mstl_valid) {
            constrain_mstl.clear();

            // otherwise, push to back of top left constraint
            for ( std::list<SGGeod>::const_iterator it = cons.begin(); it != cons.end(); it ++ ) {
                constrain_mstl.push_back( (*it) );
            }
            mstl_set = true;
        }
    }
}

void tgIntersectionEdge::ApplyConstraint( bool apply )
{
    if ( msbr_set ) {
        if ( apply ) {
            msbr_valid = true;
            msbr_set = false;
        } else {
            constrain_msbr.clear();
            msbr_set = false;
        }
    }
    
    if ( mstr_set ) {
        if ( apply ) {
            mstr_valid = true;
            mstr_set = false;
        } else {
            constrain_mstr.clear();
            mstr_set = false;
        }
    }
    
    if ( mstl_set ) {
        if ( apply ) {
            mstl_valid = true;
            mstl_set = false;
        } else {
            constrain_mstl.clear();
            mstl_set = false;
        }
    }
    
    if ( msbl_set ) {
        if ( apply ) {
            msbl_set = true;
            msbl_set = false;
        } else {
            constrain_msbl.clear();
            msbl_set = false;
        }
    }
}

void tgIntersectionEdge::DumpConstraint( const char* layer, const char* label, const std::list<SGGeod>& contour ) const
{
#if DEBUG_INTERSECTIONS    
    std::vector<SGGeod> gl;
    for (std::list<SGGeod>::const_iterator it = contour.begin(); it != contour.end(); it++ ) {
        gl.push_back( (*it) );
    }
    
    tgShapefile::FromGeodList( gl, true, datasource, layer, label );
#endif    
}

tgPolygon tgIntersectionEdge::CreatePolygon( int& id, double& heading, double& dist, double& w, SGGeod& tref, const char* debug_layer )
{
    // just return a poly with the four nodes
    tgPolygon poly;
    
    poly.AddNode( 0, botLeft );
    poly.AddNode( 0, botRight );
    poly.AddNode( 0, topRight );
    poly.AddNode( 0, topLeft );

    heading      = SGGeodesy::courseDeg( start->GetPosition(), end->GetPosition() );
    dist         = SGGeodesy::distanceM( start->GetPosition(), end->GetPosition() );
    id           = type;
    w            = width;
    tref         = botLeft;    
    
    return poly;
}

// TODO : This can be quite complex - interactions between ms corners and sides...
// best to come up with something as general as possible.
void tgIntersectionEdge::Complete( void )
{
    char layer[256];
    
    // build right and left contours
    right_contour.clear();  // TODO : No right_contour until now...                
        
    if ( constrain_msbr.empty() && constrain_mstr.empty() ) {
        // no ms corners - use normal corners
        right_contour.push_back( start->GetPosition() );
        right_contour.push_back( conBotRight );
        right_contour.push_back( conTopRight );
    } else if ( !constrain_msbr.empty() && constrain_mstr.empty() ) {
        // MS on bottom right, but no MS on top right

        // Check if start of msbr is equal to Start. If not, add start first
        if ( !SGGeod_isEqual2D( start->GetPosition(), *constrain_msbr.begin() ) ) {
            right_contour.push_back( start->GetPosition() );
        }

        // add MSBR
        right_contour.insert( right_contour.end(), constrain_msbr.begin(), constrain_msbr.end() );

        // check if MSBR ends on right side
        std::list<SGGeod>::iterator last_it = constrain_msbr.end(); last_it--;

        if ( side_r.isOn( (*last_it ) ) ) {
            // yes, use conTopRight for top right corner
            right_contour.push_back( conTopRight );
            right_contour.push_back( end->GetPosition() );
        } else {
#if DEBUG_INTERSECTIONS                                    
            sprintf( layer, "NOT_ON_RIGHT_SIDE_1" );
            tgShapefile::FromGeod( (*last_it ), GetDatasource(), layer, "pt" );
#endif            
            right_contour.push_back( end->GetPosition() );            
        }
    } else if ( constrain_msbr.empty() && !constrain_mstr.empty() ) {
        // no MS on bottom right, but MS on top right
        right_contour.push_back( start->GetPosition() );
 
        // check if start of MSTR is on right side
        std::list<SGGeod>::iterator first_it = constrain_mstr.begin();

        if ( side_r.isOn( (*first_it ) ) ) {
            // yes, use conBotRight for bottom right corner
            right_contour.push_back( conBotRight );
        } else {
#if DEBUG_INTERSECTIONS                                    
            sprintf( layer, "NOT_ON_RIGHT_SIDE_2" );
            tgShapefile::FromGeod( (*first_it ), GetDatasource(), layer, "pt" );
#endif            
        }            
        
        // Add MSTR
        right_contour.insert( right_contour.end(), constrain_mstr.begin(), constrain_mstr.end() );
    } else {
        // MS on bottom right and top right

        // Check if start of msbr is equal to Start. If not, add start first
        if ( !SGGeod_isEqual2D( start->GetPosition(), *constrain_msbr.begin() ) ) {
            right_contour.push_back( start->GetPosition() );
        }
        
        // add MSBR
        right_contour.insert( right_contour.end(), constrain_msbr.begin(), constrain_msbr.end() );
        
        // add MSTR
        right_contour.insert( right_contour.end(), constrain_mstr.begin(), constrain_mstr.end() );
    }

    left_contour.clear();

    if ( constrain_mstl.empty() && constrain_msbl.empty() ) {
        // no ms corners - use normal corners
        left_contour.push_back( end->GetPosition() );
        left_contour.push_back( conTopLeft );
        left_contour.push_back( conBotLeft );
    } else if ( !constrain_mstl.empty() && constrain_msbl.empty() ) {
        // MS on top left, but no MS on bottom left

        // Check if start of mstl is equal to End. If not, add end first
        if ( !SGGeod_isEqual2D( end->GetPosition(), *constrain_mstl.begin() ) ) {
            left_contour.push_back( end->GetPosition() );
        }

        // add MSTL
        left_contour.insert( left_contour.end(), constrain_mstl.begin(), constrain_mstl.end() );

        // check if MSTL ends on left side
        std::list<SGGeod>::iterator last_it = constrain_mstl.end(); last_it--;

        if ( side_l.isOn( (*last_it ) ) ) {
            // yes, use conBotLeft for bottom left corner
            left_contour.push_back( conBotLeft );
            left_contour.push_back( start->GetPosition() );
        } else {
#if DEBUG_INTERSECTIONS                                    
            sprintf( layer, "NOT_ON_LEFT_SIDE_1" );
            tgShapefile::FromGeod( (*last_it ), GetDatasource(), layer, "pt" );
#endif            
            left_contour.push_back( start->GetPosition() );            
        }
    } else if ( constrain_mstl.empty() && !constrain_msbl.empty() ) {
        // no MS on top left, but MS on bottom left
        left_contour.push_back( end->GetPosition() );
 
        // check if start of MSBL is on left side
        std::list<SGGeod>::iterator first_it = constrain_msbl.begin();

        if ( side_l.isOn( (*first_it ) ) ) {
            // yes, use conTopLeft for top left corner
            left_contour.push_back( conTopLeft );
        } else {
#if DEBUG_INTERSECTIONS                                    
            sprintf( layer, "NOT_ON_LEFT_SIDE_2" );
            tgShapefile::FromGeod( (*first_it ), GetDatasource(), layer, "pt" );
#endif            
        }            
        
        // Add MSBL
        left_contour.insert( left_contour.end(), constrain_msbl.begin(), constrain_msbl.end() );
    } else {
        // MS on top left and bottom left

        // Check if start of mstl is equal to End. If not, add end first
        if ( !SGGeod_isEqual2D( end->GetPosition(), *constrain_mstl.begin() ) ) {
            left_contour.push_back( end->GetPosition() );
        }
        
        // add MSTL
        left_contour.insert( left_contour.end(), constrain_mstl.begin(), constrain_mstl.end() );
        
        // add MSBL
        left_contour.insert( left_contour.end(), constrain_msbl.begin(), constrain_msbl.end() );
    }
    
    double course;    
    course = TGEuclidean::courseDeg( start->GetPosition(), end->GetPosition() ) + 90;
    for ( std::list<SGGeod>::iterator lit = left_contour.begin(); lit != left_contour.end(); lit++ ) {
        // find the opposite edge and see if we can split it
        tgLine perp( (*lit), course );
        
        std::list<SGGeod>::iterator rit_s = right_contour.begin();
        std::list<SGGeod>::iterator rit_e = rit_s; rit_e++;        
        do {
            if ( perp.OrientedSide( (*rit_s) ) != perp.OrientedSide( (*rit_e) ) ) {
                tgSegment seg( (*rit_s), (*rit_e) );
                SGGeod    intersection;
                if ( perp.Intersect( seg, intersection ) ) {
                    rit_s = right_contour.insert( rit_e, intersection );
                }
                break;
            } else {
                rit_s=rit_e;
                rit_e++;
            }
        } while ( rit_e != right_contour.end() );
    }
    
    // TODO : Traverse left contour, and see if we can project ( in relation to segment course ) onto the right.
    course = TGEuclidean::courseDeg( start->GetPosition(), end->GetPosition() ) - 90;    
    for ( std::list<SGGeod>::iterator rit = right_contour.begin(); rit != right_contour.end(); rit++ ) {
        // find the opposite edge and see if we can split it
        tgLine perp( (*rit), course );
        
        std::list<SGGeod>::iterator lit_s = left_contour.begin();
        std::list<SGGeod>::iterator lit_e = lit_s; lit_e++;        
        do {
            if ( perp.OrientedSide( (*lit_s) ) != perp.OrientedSide( (*lit_e) ) ) {
                tgSegment seg( (*lit_s), (*lit_e) );
                SGGeod    intersection;
                if ( perp.Intersect( seg, intersection ) ) {
                    lit_s = left_contour.insert( lit_e, intersection );
                }
                break;
            } else {
                lit_s=lit_e;
                lit_e++;
            }
        } while ( lit_e != left_contour.end() );
    }
}

tgPolygon tgIntersectionEdge::GetPoly(const char* prefix)
{
    int  idx;
    std::list<SGGeod>::iterator i;
    tgPolygon    poly;

    SG_LOG(SG_GENERAL, SG_DEBUG, "tgIntersectionEdge::GetPoly: rc size " << right_contour.size() << " lc size " << left_contour.size() );
    
    for ( i = right_contour.begin(), idx = 1; i != right_contour.end(); i++, idx++) {
        poly.AddNode( 0, *i );
    }
    
    for ( i = left_contour.begin(), idx = 1; i != left_contour.end(); i++, idx++) {
        poly.AddNode( 0, *i );
    }
    
    return poly;
}

tgIntersectionEdgeInfo::tgIntersectionEdgeInfo( bool orig, tgIntersectionEdge* e ) 
{    
    edge              = e;
    originating       = orig;
    
    if ( originating ) {
        heading = TGEuclidean::courseDeg( edge->start->GetPosition(), edge->end->GetPosition() );
    } else {
        heading = TGEuclidean::courseDeg( edge->end->GetPosition(), edge->start->GetPosition() );
    }
}
