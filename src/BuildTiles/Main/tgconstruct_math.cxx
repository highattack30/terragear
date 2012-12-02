// tgconstruct_math.cxx -- Implement needed math functions
//
// Written by Curtis Olson, started May 1999.
//
// Copyright (C) 1999  Curtis L. Olson  - http://www.flightgear.org/~curt
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
//
// $Id: construct.cxx,v 1.4 2004-11-19 22:25:49 curt Exp $

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <simgear/debug/logstream.hxx>
#include <Geometry/poly_support.hxx>

#include "tgconstruct.hxx"

//using std::string;

#if 0
double TGConstruct::calc_tri_area( int_list& triangle_nodes ) {
    SGGeod p1 = nodes.get_node( triangle_nodes[0] ).GetPosition();
    SGGeod p2 = nodes.get_node( triangle_nodes[1] ).GetPosition();
    SGGeod p3 = nodes.get_node( triangle_nodes[2] ).GetPosition();

    return triangle_area( p1, p2, p3 );
}
#endif

SGVec3f TGConstruct::calc_normal( double area, const SGVec3d& p1, const SGVec3d& p2, const SGVec3d& p3 ) {
    SGVec3f v1, v2;
    SGVec3f normal;

    // do some sanity checking.  With the introduction of landuse
    // areas, we can get some long skinny triangles that blow up our
    // "normal" calculations here.  Let's check for really small
    // triangle areas and check if one dimension of the triangle
    // coordinates is nearly coincident.  If so, assign the "default"
    // normal of straight up.

    bool degenerate = false;
    const double area_eps = 1.0e-12;
    if ( area < area_eps ) {
        degenerate = true;
    } else if ( fabs(p1.x() - p2.x()) < SG_EPSILON && fabs(p1.x() - p3.x()) < SG_EPSILON ) {
        degenerate = true;
    } else if ( fabs(p1.y() - p2.y()) < SG_EPSILON && fabs(p1.y() - p3.y()) < SG_EPSILON ) {
        degenerate = true;
    } else if ( fabs(p1.z() - p2.z()) < SG_EPSILON && fabs(p1.z() - p3.z()) < SG_EPSILON ) {
        degenerate = true;
    }

    if ( degenerate ) {
        normal = normalize(SGVec3f(p1.x(), p1.y(), p1.z()));
    } else {
        v1[0] = p2.x() - p1.x();
        v1[1] = p2.y() - p1.y();
        v1[2] = p2.z() - p1.z();
        v2[0] = p3.x() - p1.x();
        v2[1] = p3.y() - p1.y();
        v2[2] = p3.z() - p1.z();
        normal = normalize(cross(v1, v2));
    }

    return normal;
}

void TGConstruct::calc_normals( std::vector<SGGeod>& geod_nodes, std::vector<SGVec3d>& wgs84_nodes, tgPolygon& poly ) {
    // for each face in the superpoly, calculate a face normal
    SGVec3f     normal;
    double      area;

    for (unsigned int tri = 0; tri < poly.Triangles(); tri++) {
        SGGeod g1 = geod_nodes[ poly.GetTriIdx( tri, 0 ) ];
        SGGeod g2 = geod_nodes[ poly.GetTriIdx( tri, 1 ) ];
        SGGeod g3 = geod_nodes[ poly.GetTriIdx( tri, 2 ) ];

        SGVec3d v1 = wgs84_nodes[ poly.GetTriIdx( tri, 0 ) ];
        SGVec3d v2 = wgs84_nodes[ poly.GetTriIdx( tri, 1 ) ];
        SGVec3d v3 = wgs84_nodes[ poly.GetTriIdx( tri, 2 ) ];

        area   = triangle_area( g1, g2, g3 );
        normal = calc_normal( area, v1, v2, v3 );

        poly.SetTriFaceArea( tri, area );
        poly.SetTriFaceNormal( tri, normal );
    }
}

void TGConstruct::CalcFaceNormals( void )
{
    // traverse the superpols, and calc normals for each tri within
    std::vector<SGVec3d> wgs84_nodes;
    nodes.get_wgs84_nodes( wgs84_nodes );

    std::vector<SGGeod>  geod_nodes;
    nodes.get_geod_nodes( geod_nodes );

    for (unsigned int area = 0; area < TG_MAX_AREA_TYPES; area++) {
        for (unsigned int shape = 0; shape < polys_clipped.area_size(area); shape++ ) {
            for ( unsigned int segment = 0; segment < polys_clipped.shape_size(area, shape); segment++ ) {
                SG_LOG( SG_CLIPPER, SG_INFO, "Calculating face normals for " << get_area_name( (AreaType)area ) << ":" << shape+1 << "-" << segment << " of " << polys_in.area_size(area) );
                calc_normals( geod_nodes, wgs84_nodes, polys_clipped.get_poly( area, shape, segment ) );
            }
        }
    }
}

void TGConstruct::CalcPointNormals( void )
{
    // traverse triangle structure building the face normal table
    SG_LOG(SG_GENERAL, SG_ALERT, "Calculating point normals: 0%");

    SGVec3d normal;
    double  face_area;

    std::vector<SGVec3d> wgs84_nodes;
    nodes.get_wgs84_nodes( wgs84_nodes );

    unsigned int one_percent = nodes.size() / 100;
    unsigned int cur_percent = 1;

    for ( unsigned int i = 0; i<nodes.size(); i++ ) {
        TGNode node       = nodes.get_node( i );
        TGFaceList faces  = node.GetFaces();
        TGNeighborFaces* neighbor_faces = NULL;
        double total_area = 0.0;

        SGVec3d average( 0.0, 0.0, 0.0 );

        if ( i == one_percent ) {
            SG_LOG(SG_GENERAL, SG_ALERT, "Calculating point normals: " << cur_percent << "%" );
            one_percent += nodes.size() / 100;
            cur_percent += 1;
        }

        // for each triangle that shares this node
        for ( unsigned int j = 0; j < faces.size(); ++j ) {
            unsigned int at      = faces[j].area;
            unsigned int shape   = faces[j].shape;
            unsigned int segment = faces[j].seg;
            unsigned int tri     = faces[j].tri;

            normal     = polys_clipped.get_face_normal( at, shape, segment, tri ).toSGVec3d();
            face_area  = polys_clipped.get_face_area( at, shape, segment, tri );

            normal *= face_area;    // scale normal weight relative to area
            total_area += face_area;
            average += normal;
        }

        // if this node exists in the shared edge db, add the faces from the neighbooring tile
        neighbor_faces = FindNeighborFaces( Point3D::fromSGGeod( node.GetPosition() ) );
        if ( neighbor_faces ) {
            int num_faces = neighbor_faces->face_areas.size();
            for ( int j = 0; j < num_faces; j++ ) {
                normal    = neighbor_faces->face_normals[j].toSGVec3d();
                face_area = neighbor_faces->face_areas[j];

                normal *= face_area;
                total_area += face_area;
                average += normal;
            }
        }

        average /= total_area;
        SGVec3f n = SGVec3f( average.x(), average.y(), average.z() );
        nodes.SetNormal( i, n );
    }
}
