/*
 * (C) Copyright 1996-2014 ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

#include <limits>
#include <cassert>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <memory>

#include "eckit/exception/Exceptions.h"
#include "eckit/config/Resource.h"
#include "eckit/runtime/Tool.h"
#include "eckit/filesystem/PathName.h"
#include "eckit/parser/Tokenizer.h"
#include "eckit/geometry/Point3.h"
#include "atlas/atlas.h"
#include "atlas/io/Gmsh.h"
#include "atlas/actions/GenerateMesh.h"
#include "atlas/actions/BuildEdges.h"
#include "atlas/actions/BuildPeriodicBoundaries.h"
#include "atlas/actions/BuildHalo.h"
#include "atlas/actions/BuildParallelFields.h"
#include "atlas/actions/BuildDualMesh.h"
#include "atlas/actions/BuildStatistics.h"
#include "atlas/mpi/mpi.h"
#include "atlas/Mesh.h"
#include "atlas/grids/grids.h"
#include "atlas/GridSpec.h"

//------------------------------------------------------------------------------------------------------

using namespace eckit;
using namespace atlas;
using namespace atlas::actions;
using namespace atlas::grids;

//------------------------------------------------------------------------------------------------------

class Meshgen2Gmsh : public eckit::Tool {

  virtual void run();

public:

  Meshgen2Gmsh(int argc,char **argv): eckit::Tool(argc,argv)
  {
    bool help = Resource< bool >("--help",false);

    do_run = true;

    std::string help_str =
        "NAME\n"
        "       atlas-meshgen - Mesh generator for ReducedGrid compatible meshes\n"
        "\n"
        "SYNOPSIS\n"
        "       atlas-meshgen GRID [OPTION]... [--help] \n"
        "\n"
        "DESCRIPTION\n"
        "\n"
        "       GRID: unique identifier for grid \n"
        "           Example values: rgg.N80, rgg.TL159, gg.N40, ll.128x64\n"
        "\n"
        "       -o       Output file for mesh\n"
        "\n"
        "AUTHOR\n"
        "       Written by Willem Deconinck.\n"
        "\n"
        "ECMWF                        November 2014"
        ;
    if( help )
    {
      Log::info() << help_str << std::endl;
      do_run = false;
    }

    if( argc == 1 )
    {
      Log::info() << "usage: atlas-meshgen GRID [OPTION]... [--help]" << std::endl;
      do_run = false;
    }

    atlas_init(argc,argv);

    key = "";
    for( int i=0; i<argc; ++i )
    {
      if( i==1 && argv[i][0] != '-' )
      {
        key = std::string(argv[i]);
      }
    }

    edges      = Resource< bool> ( "--edges", false );
    stats      = Resource< bool> ( "--stats", false );
    info       = Resource< bool> ( "--info", false );
    halo       = Resource< int > ( "--halo", 0 );
    surfdim    = Resource< int > ( "--surfdim", 2 );

    path_out = Resource<std::string> ( "-o", "" );
    if( path_out.asString().empty() && do_run )
      throw UserError(Here(),"missing output filename, parameter -o");

    if( edges )
      halo = std::max(halo,1);

  }

private:

  bool do_run;
  std::string key;
  int halo;
  bool edges;
  bool stats;
  bool info;
  int surfdim;
  std::string identifier;
  std::vector<long> reg_nlon_nlat;
  std::vector<long> fgg_nlon_nlat;
  std::vector<long> rgg_nlon;
  PathName path_out;
};

//------------------------------------------------------------------------------------------------------

void Meshgen2Gmsh::run()
{
  if( !do_run ) return;
  grids::load();

  ReducedGrid::Ptr grid;
  try{ grid = ReducedGrid::Ptr( ReducedGrid::create(key) ); }
  catch( eckit::BadParameter& err ){}

  if( !grid ) return;
  Mesh::Ptr mesh;

  mesh = Mesh::Ptr( generate_mesh(*grid) );

  build_nodes_parallel_fields(mesh->function_space("nodes"));
  build_periodic_boundaries(*mesh);

  if( halo )
  {
    build_halo(*mesh,halo);
    renumber_nodes_glb_idx(mesh->function_space("nodes"));
  }
  mesh->function_space("nodes").parallelise();
  ArrayView<double,2> lonlat( mesh->function_space("nodes").field("lonlat") );
  ArrayView<double,2> xyz( mesh->function_space("nodes").create_field<double>("xyz",3) );
  for( int j=0; j<lonlat.shape(0); ++j )
  {
    eckit::geometry::lonlat_to_3d( lonlat[j].data(), xyz[j].data() );
  }
  
  Log::info() << "  checksum lonlat : " << mesh->function_space("nodes").checksum().execute( lonlat ) << std::endl;
  if( edges )
  {
    build_edges(*mesh);
    build_pole_edges(*mesh);
    build_edges_parallel_fields(mesh->function_space("edges"),mesh->function_space("nodes"));
    build_median_dual_mesh(*mesh);
  }

  if( stats )
    build_statistics(*mesh);

  atlas::io::Gmsh gmsh;
  gmsh.options.set("info",info);
  if( surfdim == 3 )
    gmsh.options.set("nodes",std::string("xyz"));
  gmsh.write( *mesh, path_out );
  atlas_finalize();
}

//------------------------------------------------------------------------------------------------------

int main( int argc, char **argv )
{
  Meshgen2Gmsh tool(argc,argv);
  tool.start();
  return 0;
}
