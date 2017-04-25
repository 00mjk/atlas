/*
 * (C) Copyright 1996-2017 ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

#include <sstream>
#include <algorithm>
#include <iomanip>

#define BOOST_TEST_MODULE TestHalo
#include "ecbuild/boost_test_framework.h"

#include "atlas/parallel/mpi/mpi.h"
#include "atlas/library/config.h"
#include "tests/TestMeshes.h"
#include "atlas/output/Gmsh.h"
#include "atlas/mesh/Mesh.h"
#include "atlas/mesh/Nodes.h"
#include "atlas/array/IndexView.h"
#include "atlas/array/ArrayView.h"
#include "atlas/array.h"
#include "atlas/mesh/actions/BuildHalo.h"
#include "atlas/mesh/actions/BuildParallelFields.h"
#include "atlas/mesh/actions/BuildPeriodicBoundaries.h"
#include "atlas/mesh/actions/BuildEdges.h"
#include "atlas/mesh/actions/BuildDualMesh.h"
#include "atlas/util/CoordinateEnums.h"
#include "atlas/mesh/IsGhostNode.h"

#include "tests/AtlasFixture.h"


using namespace atlas::output;
using namespace atlas::util;
using namespace atlas::meshgenerator;

namespace atlas {
namespace test {

double dual_volume(Mesh& mesh)
{
  mesh::Nodes& nodes = mesh.nodes();
  mesh::IsGhostNode is_ghost_node(nodes);
  int nb_nodes = nodes.size();
  array::ArrayView<double,1> dual_volumes = array::make_view<double,1>( nodes.field("dual_volumes") );
  double area=0;
  for( int node=0; node<nb_nodes; ++node )
  {
    if( ! is_ghost_node(node) )
    {
      area += dual_volumes(node);
    }
  }

  parallel::mpi::comm().allReduceInPlace(area, eckit::mpi::sum());

  return area;
}

BOOST_GLOBAL_FIXTURE( AtlasFixture );

#if 0
BOOST_AUTO_TEST_CASE( test_small )
{
  int nlat = 5;
  int lon[5] = {10, 12, 14, 16, 16};

  Mesh = test::generate_mesh(nlat, lon);

  mesh::actions::build_parallel_fields(*m);
  mesh::actions::build_periodic_boundaries(*m);
  mesh::actions::build_halo(*m,2);


  if( parallel::mpi::comm().size() == 5 )
  {
    IndexView<int,1> ridx ( m->nodes().remote_index() );
    array::array::ArrayView<gidx_t,1> gidx ( m->nodes().global_index() );

    switch( parallel::mpi::comm().rank() ) // with 5 tasks
    {
    case 0:
      BOOST_CHECK_EQUAL( ridx(9),  9  );
      BOOST_CHECK_EQUAL( gidx(9),  10 );
      BOOST_CHECK_EQUAL( ridx(30), 9 );
      BOOST_CHECK_EQUAL( gidx(30), 875430066 ); // hashed unique idx
      break;
    }
  }
  else
  {
    if( parallel::mpi::comm().rank() == 0 )
      std::cout << "skipping tests with 5 mpi tasks!" << std::endl;
  }

  mesh::actions::build_edges(*m);
  mesh::actions::build_median_dual_mesh(*m);

  BOOST_CHECK_CLOSE( test::dual_volume(*m), 2.*M_PI*M_PI, 1e-6 );

  std::stringstream filename; filename << "small_halo_p" << parallel::mpi::comm().rank() << ".msh";
  Gmsh(filename.str()).write(*m);
}
#endif

#if 1
BOOST_AUTO_TEST_CASE( test_t63 )
{
  // Mesh m = test::generate_mesh( T63() );

  Mesh m = test::generate_mesh( {10, 12, 14, 16, 16, 16, 16, 14, 12, 10} );

  mesh::actions::build_nodes_parallel_fields(m.nodes());
  mesh::actions::build_periodic_boundaries(m);
  mesh::actions::build_halo(m,1);
  //mesh::actions::build_edges(m);
  //mesh::actions::build_pole_edges(m);
  //mesh::actions::build_edges_parallel_fields(m.function_space("edges"),m-.nodes());
  //mesh::actions::build_centroid_dual_mesh(m);
  mesh::actions::renumber_nodes_glb_idx(m.nodes());

  std::stringstream filename; filename << "T63_halo.msh";
  Gmsh(filename.str()).write(m);

//  BOOST_CHECK_CLOSE( test::dual_volume(m), 2.*M_PI*M_PI, 1e-6 );

//  Nodes& nodes = m.nodes();
//  FunctionSpace& edges = m.function_space("edges");
//  array::array::ArrayView<double,1> dual_volumes  ( nodes.field( "dual_volumes" ) );
//  array::array::ArrayView<double,2> dual_normals  ( edges.field( "dual_normals" ) );

//  std::string checksum;
//  checksum = nodes.checksum()->execute(dual_volumes);
//  DEBUG("dual_volumes checksum "<<checksum,0);

//  checksum = edges.checksum()->execute(dual_normals);
//  DEBUG("dual_normals checksum "<<checksum,0);

}
#endif

} // namespace test
} // namespace atlas
