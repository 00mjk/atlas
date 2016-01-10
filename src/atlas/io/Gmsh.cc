/*
 * (C) Copyright 1996-2015 ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */



#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cmath>
#include <limits>

#include "eckit/filesystem/PathName.h"
#include "eckit/exception/Exceptions.h"
#include "atlas/runtime/Log.h"
#include "eckit/config/Resource.h"
#include "eckit/runtime/Context.h"
#include "atlas/io/Gmsh.h"
#include "atlas/mpl/GatherScatter.h"
#include "atlas/Array.h"
#include "atlas/util/ArrayView.h"
#include "atlas/util/IndexView.h"
#include "atlas/Mesh.h"
#include "atlas/FunctionSpace.h"
#include "atlas/Field.h"
#include "atlas/FieldSet.h"
#include "atlas/Parameters.h"
#include "atlas/mesh/Nodes.h"
#include "atlas/mesh/HybridElements.h"
#include "atlas/mesh/ElementType.h"
#include "atlas/mesh/Elements.h"

using namespace eckit;
using atlas::functionspace::Nodes;

namespace atlas {
namespace io {

namespace {

static double deg = Constants::radianToDegrees();
static double rad = Constants::degreesToRadians();

class GmshFile : public std::ofstream {
public:
  GmshFile(const PathName& file_path, std::ios_base::openmode mode, int part = eckit::mpi::rank())
  {
    PathName par_path(file_path);
    if (eckit::mpi::size() == 1 || part == -1) {
      std::ofstream::open(par_path.localPath(), mode);
    } else {
      Translator<int, std::string> to_str;
      if (eckit::mpi::rank() == 0) {
        PathName par_path(file_path);
        std::ofstream par_file(par_path.localPath(), std::ios_base::out);
        for(size_t p = 0; p < eckit::mpi::size(); ++p) {
          PathName loc_path(file_path);
          loc_path = loc_path.baseName(false) + "_p" + to_str(p) + ".msh";
          par_file << "Merge \"" << loc_path << "\";" << std::endl;
        }
        par_file.close();
      }
      PathName path(file_path);
      path = path.dirName() + "/" + path.baseName(false) + "_p" + to_str(part) + ".msh";
      std::ofstream::open(path.localPath(), mode);
    }
  }
};

enum GmshElementTypes { LINE=1, TRIAG=2, QUAD=3, POINT=15 };

void write_header_ascii(std::ostream& out)
{
  out << "$MeshFormat\n";
  out << "2.2 0 "<<sizeof(double)<<"\n";
  out << "$EndMeshFormat\n";
}
void write_header_binary(std::ostream& out)
{
  out << "$MeshFormat\n";
  out << "2.2 1 "<<sizeof(double)<<"\n";
  int one = 1;
  out.write(reinterpret_cast<const char*>(&one),sizeof(int));
  out << "\n$EndMeshFormat\n";
}

template< typename DATATYPE >
void write_field_nodes(const Gmsh& gmsh, const functionspace::Nodes& function_space, const Field& field, std::ostream& out)
{
  Log::info() << "writing field " << field.name() << "..." << std::endl;

  bool gather( gmsh.options.get<bool>("gather") );
  bool binary( !gmsh.options.get<bool>("ascii") );
  int nlev  = field.levels();
  int ndata = field.shape(0);
  int nvars = field.stride(0)/nlev;
  ArrayView<gidx_t,1> gidx ( function_space.nodes().global_index() );
  ArrayView<DATATYPE,2> data ( field.data<DATATYPE>(), make_shape(field.shape(0),field.stride(0)) );
  Field::Ptr gidx_glb;
  Field::Ptr data_glb;
  if( gather )
  {
    gidx_glb.reset( function_space.createGlobalField( "gidx_glb", function_space.nodes().global_index() ) );
    function_space.gather(function_space.nodes().global_index(),*gidx_glb);
    gidx = ArrayView<gidx_t,1>( *gidx_glb );

    data_glb.reset( function_space.createGlobalField( "glb_field",field ) );
    function_space.gather(field,*data_glb);
    data = ArrayView<DATATYPE,2>( data_glb->data<DATATYPE>(), make_shape(data_glb->shape(0),data_glb->stride(0)) );
  }

  std::vector<long> lev;
  std::vector<long> gmsh_levels;
  gmsh.options.get("levels",gmsh_levels);
  if( gmsh_levels.empty() || nlev == 1 )
  {
    lev.resize(nlev);
    for (int ilev=0; ilev<nlev; ++ilev)
      lev[ilev] = ilev;
  }
  else
  {
    lev = gmsh_levels;
  }
  for (int ilev=0; ilev<lev.size(); ++ilev)
  {
    int jlev = lev[ilev];
    if( ( gather && eckit::mpi::rank() == 0 ) || !gather )
    {
      char field_lev[6] = {0, 0, 0, 0, 0, 0};
      if( field.has_levels() )
        std::sprintf(field_lev, "[%03d]",jlev);
      double time = field.metadata().has("time") ? field.metadata().get<double>("time") : 0.;
      int step = field.metadata().has("step") ? field.metadata().get<size_t>("step") : 0 ;
      out << "$NodeData\n";
      out << "1\n";
      out << "\"" << field.name() << field_lev << "\"\n";
      out << "1\n";
      out << time << "\n";
      out << "4\n";
      out << step << "\n";
      if     ( nvars == 1 ) out << nvars << "\n";
      else if( nvars <= 3 ) out << 3     << "\n";
      out << ndata << "\n";
      out << eckit::mpi::rank() << "\n";

      if( binary )
      {
        if( nvars == 1)
        {
          double value;
          for( int n = 0; n < ndata; ++n )
          {
            out.write(reinterpret_cast<const char*>(&gidx(n)),sizeof(int));
            value = data(n,jlev*nvars+0);
            out.write(reinterpret_cast<const char*>(&value),sizeof(double));
          }
        }
        else if( nvars <= 3 )
        {
          double value[3] = {0,0,0};
          for( size_t n = 0; n < ndata; ++n )
          {
            out.write(reinterpret_cast<const char*>(&gidx(n)),sizeof(int));
            for( int v=0; v<nvars; ++v)
              value[v] = data(n,jlev*nvars+v);
            out.write(reinterpret_cast<const char*>(&value),sizeof(double)*3);
          }
        }
        out << "\n";
      }
      else
      {
        if( nvars == 1)
        {
          for( int n = 0; n < ndata; ++n )
          {
            ASSERT( jlev*nvars < data.shape(1) );
            ASSERT( n < gidx.shape(0) );
            out << gidx(n) << " " << data(n,jlev*nvars+0) << "\n";
          }
        }
        else if( nvars <= 3 )
        {
          std::vector<DATATYPE> data_vec(3,0.);
          for( size_t n = 0; n < ndata; ++n )
          {
            out << gidx(n);
            for( int v=0; v<nvars; ++v)
              data_vec[v] = data(n,jlev*nvars+v);
            for( int v=0; v<3; ++v)
              out << " " << data_vec[v];
            out << "\n";
          }
        }

      }
      out << "$EndNodeData\n";
    }
  }
}

template< typename DATA_TYPE >
void write_field_elems(const Gmsh& gmsh, const FunctionSpace& function_space, const Field& field, std::ostream& out)
{
  Log::info() << "writing field " << field.name() << "..." << std::endl;
  bool gather( gmsh.options.get<bool>("gather") );
  bool binary( !gmsh.options.get<bool>("ascii") );
  int nlev = field.metadata().has("nb_levels") ? field.metadata().get<size_t>("nb_levels") : 1;
  int ndata = field.shape(0);
  int nvars = field.shape(1)/nlev;
  ArrayView<gidx_t,1    > gidx ( function_space.field( "glb_idx" ) );
  ArrayView<DATA_TYPE> data ( field );
  ArrayT<DATA_TYPE> field_glb_arr;
  ArrayT<gidx_t   > gidx_glb_arr;
  if( gather )
  {
    mpl::GatherScatter& fullgather = function_space.fullgather();
    ndata = fullgather.glb_dof();
    field_glb_arr.resize(ndata,field.shape(1));
    gidx_glb_arr.resize(ndata);
    ArrayView<DATA_TYPE> data_glb( field_glb_arr );
    ArrayView<gidx_t,1> gidx_glb( gidx_glb_arr );
    fullgather.gather( gidx, gidx_glb );
    fullgather.gather( data, data_glb );
    gidx = ArrayView<gidx_t,1>( gidx_glb_arr );
    data = data_glb;
  }

  double time = field.metadata().has("time") ? field.metadata().get<double>("time") : 0.;
  size_t step = field.metadata().has("step") ? field.metadata().get<size_t>("step") : 0 ;

  int nnodes = IndexView<int,2>( function_space.field("nodes") ).shape(1);

  for (int jlev=0; jlev<nlev; ++jlev)
  {
    char field_lev[6] = {0, 0, 0, 0, 0, 0};
    if( field.metadata().has("nb_levels") )
      std::sprintf(field_lev, "[%03d]",jlev);

    out << "$ElementNodeData\n";
    out << "1\n";
    out << "\"" << field.name() << field_lev << "\"\n";
    out << "1\n";
    out << time << "\n";
    out << "4\n";
    out << step << "\n";
    if     ( nvars == 1 ) out << nvars << "\n";
    else if( nvars <= 3 ) out << 3     << "\n";
    out << ndata << "\n";
    out << eckit::mpi::rank() << "\n";

    if( binary )
    {
      if( nvars == 1)
      {
        double value;
        for (size_t jelem=0; jelem<ndata; ++jelem)
        {
          out.write(reinterpret_cast<const char*>(&gidx(jelem)),sizeof(int));
          out.write(reinterpret_cast<const char*>(&nnodes),sizeof(int));
          for (size_t n=0; n<nnodes; ++n)
          {
            value = data(jelem,jlev);
            out.write(reinterpret_cast<const char*>(&value),sizeof(double));
          }
        }
      }
      else if( nvars <= 3 )
      {
        double value[3] = {0,0,0};
        for (size_t jelem=0; jelem<ndata; ++jelem)
        {
          out << gidx(jelem) << " " << nnodes;
          for (size_t n=0; n<nnodes; ++n)
          {
            for( int v=0; v<nvars; ++v)
              value[v] = data(jelem,jlev*nvars+v);
            out.write(reinterpret_cast<const char*>(&value),sizeof(double)*3);
          }
        }
      }
      out <<"\n";
    }
    else
    {
      if( nvars == 1)
      {
        for (size_t jelem=0; jelem<ndata; ++jelem)
        {
          out << gidx(jelem) << " " << nnodes;
          for (size_t n=0; n<nnodes; ++n)
            out << " " << data(jelem,jlev);
          out <<"\n";
        }
      }
      else if( nvars <= 3 )
      {
        std::vector<DATA_TYPE> data_vec(3,0.);
        for (size_t jelem=0; jelem<ndata; ++jelem)
        {
          out << gidx(jelem) << " " << nnodes;
          for (size_t n=0; n<nnodes; ++n)
          {
            for( int v=0; v<nvars; ++v)
              data_vec[v] = data(jelem,jlev*nvars+v);
            for( int v=0; v<3; ++v)
              out << " " << data_vec[v];
          }
          out <<"\n";
        }
      }
    }
    out << "$EndElementNodeData\n";
  }
}

// Unused private function, in case for big-endian
#if 0
void swap_bytes(char *array, int size, int n)
{
  char *x = new char[size];
  for(int i = 0; i < n; i++) {
    char *a = &array[i * size];
    memcpy(x, a, size);
    for(int c = 0; c < size; c++)
      a[size - 1 - c] = x[c];
  }
  delete [] x;
}
#endif


} // end anonymous namespace

Gmsh::Gmsh()
{
  // which field holds the Nodes
  options.set<std::string>("nodes", Resource<std::string>("atlas.gmsh.nodes", "lonlat"));

  // Gather fields to one proc before writing
  options.set<bool>("gather",  Resource<bool>("atlas.gmsh.gather",  false));

  // Output of ghost nodes / elements
  options.set<bool>("ghost",   Resource<bool>("atlas.gmsh.ghost",   false));

  // ASCII format (true) or binary (false)
  options.set<bool>("ascii",   Resource<bool>("atlas.gmsh.ascii",   true ));

  // Output of elements
  options.set<bool>("elements",Resource<bool>("atlas.gmsh.elements",true ));

  // Output of edges
  options.set<bool>("edges",   Resource<bool>("atlas.gmsh.edges",   true ));

  // Radius of the planet
  options.set<double>("radius",   Resource<bool>("atlas.gmsh.radius", 1.0 ));

  // Levels of fields to use
  options.set< std::vector<long> >("levels", Resource< std::vector<long> >("atlas.gmsh.levels", std::vector<long>() ) );
}

Gmsh::~Gmsh()
{
}

Mesh* Gmsh::read(const PathName& file_path) const
{
  Mesh* mesh = new Mesh();
  Gmsh::read(file_path,*mesh);
  return mesh;
}

void Gmsh::read(const PathName& file_path, Mesh& mesh ) const
{
  std::ifstream file;
  file.open( file_path.localPath() , std::ios::in | std::ios::binary );
  if( !file.is_open() )
    throw CantOpenFile(file_path);

  std::string line;

  while(line != "$MeshFormat")
    std::getline(file,line);
  double version;
  int binary;
  int size_of_real;
  file >> version >> binary >> size_of_real;

  while(line != "$Nodes")
    std::getline(file,line);

  // Create nodes
    size_t nb_nodes;
  file >> nb_nodes;

  std::vector<size_t> extents(2);

  extents[0] = nb_nodes;
  extents[1] = FunctionSpace::UNDEF_VARS;

  mesh.createNodes(nb_nodes);
  mesh.nodes().metadata().set<long>("type",Entity::NODES);

  mesh::Nodes& nodes = mesh.nodes();

  nodes.add( Field::create<double>("xyz",make_shape(nb_nodes,3) ) );

  ArrayView<double,2> coords         ( nodes.field("xyz")    );
  ArrayView<gidx_t,1> glb_idx        ( nodes.global_index()  );
  ArrayView<int,   1> part           ( nodes.partition()     );

  std::map<int,int> glb_to_loc;
  int g;
  double x,y,z;
  double xyz[3];
  double xmax = -std::numeric_limits<double>::max();
  double zmax = -std::numeric_limits<double>::max();
  gidx_t max_glb_idx=0;
  while(binary && file.peek()=='\n') file.get();
  for( size_t n = 0; n < nb_nodes; ++n )
  {
    if( binary )
    {
      file.read(reinterpret_cast<char*>(&g), sizeof(int));
      file.read(reinterpret_cast<char*>(&xyz), sizeof(double)*3);
      x = xyz[XX];
      y = xyz[YY];
      z = xyz[ZZ];
    }
    else
    {
      file >> g >> x >> y >> z;
    }
    glb_idx(n) = g;
    coords(n,XX) = x;
    coords(n,YY) = y;
    coords(n,ZZ) = z;
    glb_to_loc[ g ] = n;
    part(n) = 0;
    max_glb_idx = std::max(max_glb_idx, static_cast<gidx_t>(g));
    xmax = std::max(x,xmax);
    zmax = std::max(z,zmax);
  }
  if( xmax < 4*M_PI && zmax == 0. )
  {
    for( size_t n = 0; n < nb_nodes; ++n )
    {
      coords(n,XX) *= deg;
      coords(n,YY) *= deg;
    }
  }
  for (int i=0; i<3; ++i)
    std::getline(file,line);

  nodes.metadata().set("nb_owned",nb_nodes);

  int nb_elements=0;

  while(line != "$Elements")
    std::getline(file,line);

  file >> nb_elements;

  if( binary )
  {
        while(file.peek()=='\n') file.get();
    int accounted_elems = 0;
    while( accounted_elems < nb_elements )
    {
      int header[3];
      int data[100];
      file.read(reinterpret_cast<char*>(&header),sizeof(int)*3);

      int etype = header[0];
      int netype = header[1];
      int ntags = header[2];
      accounted_elems += netype;
      std::string name;
      int nnodes_per_elem;
      switch( etype ) {
      case(QUAD):
        nnodes_per_elem = 4;
        name = "quads";
        break;
      case(TRIAG):
        nnodes_per_elem = 3;
        name = "triags";
        break;
      case(LINE):
        nnodes_per_elem = 3;
        name = "edges";
        break;
      default:
        std::cout << "etype " << etype << std::endl;
        throw Exception("ERROR: element type not supported",Here());
      }

      extents[0] = netype;

      FunctionSpace& fs = mesh.create_function_space( name, "Lagrange_P1", extents );

      fs.metadata().set<long>("type",static_cast<int>(Entity::ELEMS));

      IndexView<int,2> conn  ( fs.create_field<int>("nodes",         4) );
      ArrayView<gidx_t,1> egidx ( fs.create_field<int>("glb_idx",       1) );
      ArrayView<int,1> epart ( fs.create_field<int>("partition",     1) );

      int dsize = 1+ntags+nnodes_per_elem;
      int part;
      for (int e=0; e<netype; ++e)
      {
        file.read(reinterpret_cast<char*>(&data),sizeof(int)*dsize);
        part = 0;
        egidx(e) = data[0];
        epart(e) = part;
        for(int n=0; n<nnodes_per_elem; ++n)
          conn(e,n) = glb_to_loc[data[1+ntags+n]];
      }
    }
  }
  else
  {
    // Find out which element types are inside
    int position = file.tellg();
    std::vector<int> nb_etype(20,0);
    int elements_max_glb_idx(0);
    int etype;
    for (int e=0; e<nb_elements; ++e)
    {
      file >> g >> etype; std::getline(file,line); // finish line
      ++nb_etype[etype];
      elements_max_glb_idx = std::max(elements_max_glb_idx,g);
    }

    // Allocate data structures for quads, triags, edges

    int nb_quads = nb_etype[QUAD];
    extents[0] = nb_quads;
    FunctionSpace& quads      = mesh.create_function_space( "quads", "Lagrange_P1", extents );
    quads.metadata().set<long>("type",static_cast<int>(Entity::ELEMS));
    IndexView<int,2> quad_nodes          ( quads.create_field<int>("nodes",         4) );
    ArrayView<gidx_t,1> quad_glb_idx        ( quads.create_field<gidx_t>("glb_idx",       1) );
    ArrayView<int,1> quad_part           ( quads.create_field<int>("partition",     1) );

    int nb_triags = nb_etype[TRIAG];
    extents[0] = nb_triags;
    FunctionSpace& triags      = mesh.create_function_space( "triags", "Lagrange_P1", extents );
    triags.metadata().set<long>("type",static_cast<int>(Entity::ELEMS));
    IndexView<int,2> triag_nodes          ( triags.create_field<int>("nodes",         3) );
    ArrayView<gidx_t,1> triag_glb_idx        ( triags.create_field<gidx_t>("glb_idx",       1) );
    ArrayView<int,1> triag_part           ( triags.create_field<int>("partition",     1) );

    int nb_edges = nb_etype[LINE];
    IndexView<int,2> edge_nodes;
    ArrayView<gidx_t,1> edge_glb_idx;
    ArrayView<int,1> edge_part;

    if( nb_edges > 0 )
    {
      extents[0] = nb_edges;
      FunctionSpace& edges      = mesh.create_function_space( "edges", "Lagrange_P1", extents );
      edges.metadata().set<long>("type",static_cast<int>(Entity::FACES));
      edge_nodes   = IndexView<int,2> ( edges.create_field<int>("nodes",         2) );
      edge_glb_idx = ArrayView<gidx_t,1> ( edges.create_field<gidx_t>("glb_idx",       1) );
      edge_part    = ArrayView<int,1> ( edges.create_field<int>("partition",     1) );
    }

    // Now read all elements
    file.seekg(position,std::ios::beg);
    int gn0, gn1, gn2, gn3;
    int quad=0, triag=0, edge=0;
    int ntags, tags[100];
    for (int e=0; e<nb_elements; ++e)
    {
      file >> g >> etype >> ntags;
      for( int t=0; t<ntags; ++t ) file >> tags[t];
      int part=0;
      if( ntags > 3 ) part = std::max( part, *std::max_element(tags+3,tags+ntags-1) ); // one positive, others negative
      switch( etype )
      {
        case(QUAD):
          file >> gn0 >> gn1 >> gn2 >> gn3;
          quad_glb_idx(quad) = g;
          quad_part(quad) = part;
          quad_nodes(quad,0) = glb_to_loc[gn0];
          quad_nodes(quad,1) = glb_to_loc[gn1];
          quad_nodes(quad,2) = glb_to_loc[gn2];
          quad_nodes(quad,3) = glb_to_loc[gn3];
          ++quad;
          break;
        case(TRIAG):
          file >> gn0 >> gn1 >> gn2;
          triag_glb_idx(triag) = g;
          triag_part(triag) = part;
          triag_nodes(triag,0) = glb_to_loc[gn0];
          triag_nodes(triag,1) = glb_to_loc[gn1];
          triag_nodes(triag,2) = glb_to_loc[gn2];
          ++triag;
          break;
        case(LINE):
          file >> gn0 >> gn1;
          edge_glb_idx(edge) = g;
          edge_part(edge) = part;
          edge_nodes(edge,0) = glb_to_loc[gn0];
          edge_nodes(edge,1) = glb_to_loc[gn1];
          ++edge;
          break;
        case(POINT):
          file >> gn0;
          break;
        default:
          std::cout << "etype " << etype << std::endl;
          throw Exception("ERROR: element type not supported",Here());
      }
    }
    quads.metadata().set("nb_owned",nb_etype[QUAD]);
    triags.metadata().set("nb_owned",nb_etype[TRIAG]);

    if( nb_edges > 0 )
    {
      mesh.function_space("edges").metadata().set("nb_owned",nb_etype[LINE]);
    }

  }

  file.close();
}


void Gmsh::write(const Mesh& mesh, const PathName& file_path) const
{
  const_cast<Mesh&>(mesh).cells().rebuild_from_fs();
  const_cast<Mesh&>(mesh).edges().rebuild_from_fs();
  int part = mesh.metadata().has("part") ? mesh.metadata().get<size_t>("part") : eckit::mpi::rank();
  bool include_ghost = options.get<bool>("ghost") && options.get<bool>("elements");

  std::string nodes_field = options.get<std::string>("nodes");

  const mesh::Nodes& nodes    = mesh.nodes();
  ArrayView<double,2> coords  ( nodes.field( nodes_field ) );
  ArrayView<gidx_t,   1> glb_idx ( nodes.global_index() );

  const size_t surfdim = coords.shape(1); // nb of variables in coords

  ASSERT(surfdim == 2 || surfdim == 3);

  Log::info() << "writing mesh to gmsh file " << file_path << std::endl;

  bool binary = !options.get<bool>("ascii");

  openmode mode = std::ios::out;
  if( binary )
    mode = std::ios::out | std::ios::binary;
  GmshFile file(file_path,mode,part);

  // Header
  if( binary )
    write_header_binary(file);
  else
    write_header_ascii(file);

  // Nodes
  const size_t nb_nodes = nodes.size();
  file << "$Nodes\n";
  file << nb_nodes << "\n";
  double xyz[3] = {0.,0.,0.};
  for( size_t n = 0; n < nb_nodes; ++n )
  {
    int g = glb_idx(n);

    for(size_t d = 0; d < surfdim; ++d)
        xyz[d] = coords(n,d);

    if( binary )
    {
        file.write(reinterpret_cast<const char*>(&g), sizeof(int));
        file.write(reinterpret_cast<const char*>(&xyz), sizeof(double)*3 );
    }
    else
    {
        file << g << " " << xyz[XX] << " " << xyz[YY] << " " << xyz[ZZ] << "\n";
    }
  }
  if( binary ) file << "\n";
  file << "$EndNodes\n";

  // Elements
  file << "$Elements\n";
  {
    std::vector<const mesh::HybridElements*> grouped_elements;
    if( options.get<bool>("elements") )
      grouped_elements.push_back(&mesh.cells());
    if( options.get<bool>("edges") )
      grouped_elements.push_back(&mesh.edges());

    size_t nb_elements(0);
    for( size_t jgroup=0; jgroup<grouped_elements.size(); ++jgroup )
    {
      const mesh::HybridElements& hybrid = *grouped_elements[jgroup];
      nb_elements += hybrid.size();
      if( !include_ghost )
      {
        const ArrayView<int,1> hybrid_halo( hybrid.halo() );
        for( size_t e=0; e<hybrid.size(); ++e )
        {
          if( hybrid_halo(e) ) --nb_elements;
        }
      }
    }
    
    file << nb_elements << "\n";
    
    for( size_t jgroup=0; jgroup<grouped_elements.size(); ++jgroup )
    {
      const mesh::HybridElements& hybrid = *grouped_elements[jgroup];
      for( size_t etype=0; etype<hybrid.nb_types(); ++etype )
      {
        const mesh::Elements& elements = hybrid.elements(etype);
        const mesh::ElementType& element_type = elements.element_type();
        int gmsh_elem_type;
        if( element_type.name() == "Line" )
          gmsh_elem_type = 1;
        else if( element_type.name() == "Triangle" )
          gmsh_elem_type = 2;
        else if( element_type.name() == "Quadrilateral" )
          gmsh_elem_type = 3;
        else
          NOTIMP;

        const mesh::Elements::Connectivity& node_connectivity = elements.node_connectivity();
        const ArrayView<gidx_t,1> elems_glb_idx = elements.view<gidx_t,1>( elements.global_index() );
        const ArrayView<int,1> elems_partition = elements.view<int,1>( elements.partition() );
        const ArrayView<int,1> elems_halo = elements.view<int,1>( elements.halo() );
        
        if( binary )
        {
          size_t nb_elems=elements.size();
          if( !include_ghost )
          {
            for(size_t elem=0; elem<elements.size(); ++elem )
            {
              if( elems_halo(elem) ) --nb_elems;
            }
          }
          
          int header[3];
          int data[9];
          header[0] = gmsh_elem_type;
          header[1] = nb_elems;
          header[2] = 4; // nb_tags
          file.write(reinterpret_cast<const char*>(&header), sizeof(int)*3 );
          data[1]=1;
          data[2]=1;
          data[3]=1;
          size_t datasize = sizeof(int)*(5+node_connectivity.cols());
          for(size_t elem=0; elem<elements.size(); ++elem )
          {
            if( include_ghost || !elems_halo(elem) )
            {
              data[0] = elems_glb_idx(elem);
              data[4] = elems_partition(elem);
              for( int n=0; n<node_connectivity.cols(); ++n )
                data[5+n] = glb_idx(node_connectivity(elem,n));
              file.write(reinterpret_cast<const char*>(&data), datasize );
            }
          }
        }
        else
        {
          std::stringstream ss_elem_info; ss_elem_info << " " << gmsh_elem_type << " 4 1 1 1 ";
          std::string elem_info = ss_elem_info.str();
          for(size_t elem=0; elem<elements.size(); ++elem )
          {
            if( include_ghost || !elems_halo(elem) )
            {
              file << elems_glb_idx(elem) << elem_info << elems_partition(+elem);
              for( int n=0; n<node_connectivity.cols(); ++n ) {
                file << " " << glb_idx(node_connectivity(elem,n));
              }
              file << "\n";
            }
          }
        }
      }
    }
  }
  if( binary ) file << "\n";
  file << "$EndElements\n";
  file << std::flush;
  file.close();

  // Optional mesh information file
  if( options.has("info") && options.get<bool>("info") )
  {
    PathName mesh_info(file_path);
    mesh_info = mesh_info.dirName()+"/"+mesh_info.baseName(false)+"_info.msh";

    //[next]  make NodesFunctionSpace accept const mesh
    eckit::SharedPtr<functionspace::Nodes> function_space( new functionspace::Nodes(const_cast<Mesh&>(mesh)) );

    write(nodes.partition(),*function_space,mesh_info,std::ios_base::out);

    if (nodes.has_field("dual_volumes"))
    {
      write(nodes.field("dual_volumes"),*function_space,mesh_info,std::ios_base::app);
    }

    if (nodes.has_field("dual_delta_sph"))
    {
      write(nodes.field("dual_delta_sph"),*function_space,mesh_info,std::ios_base::app);
    }

    //[next] if( mesh.has_function_space("edges") )
    //[next] {
    //[next]   FunctionSpace& edges = mesh.function_space( "edges" );

    //[next]   if (edges.has_field("dual_normals"))
    //[next]   {
    //[next]     write(edges.field("dual_normals"),mesh_info,std::ios_base::app);
    //[next]   }

    //[next]   if (edges.has_field("skewness"))
    //[next]   {
    //[next]     write(edges.field("skewness"),mesh_info,std::ios_base::app);
    //[next]   }

    //[next]   if (edges.has_field("arc_length"))
    //[next]   {
    //[next]     write(edges.field("arc_length"),mesh_info,std::ios_base::app);
    //[next]   }
    //[next] }
  }

}

void Gmsh::write(const FieldSet& fieldset, const functionspace::Nodes& functionspace, const PathName& file_path, openmode mode) const
{
  bool is_new_file = (mode != std::ios_base::app || !file_path.exists() );
  bool binary( !options.get<bool>("ascii") );
  if ( binary ) mode |= std::ios_base::binary;
  bool gather = options.has("gather") ? options.get<bool>("gather") : false;
  GmshFile file(file_path,mode,gather?-1:eckit::mpi::rank());

  // Header
  if( is_new_file )
    write_header_ascii(file);

  // Fields
  for(size_t field_idx = 0; field_idx < fieldset.size(); ++field_idx)
  {
    const Field& field = fieldset[field_idx];
    Log::info() << "writing field " << field.name() << " to gmsh file " << file_path << std::endl;

    //[delete] const FunctionSpace& function_space = field.function_space();

    //[delete]if( !function_space.metadata().has("type") )
    //[delete]{
    //[delete]  throw Exception("function_space "+function_space.name()+" has no type.. ?");
    //[delete]}

    //[delete]if( function_space.metadata().get<long>("type") == Entity::NODES )
    //[delete]{
      if     ( field.datatype() == DataType::int32()  ) {  write_field_nodes<int   >(*this,functionspace,field,file); }
      else if( field.datatype() == DataType::int64()  ) {  write_field_nodes<long  >(*this,functionspace,field,file); }
      else if( field.datatype() == DataType::real32() ) {  write_field_nodes<float >(*this,functionspace,field,file); }
      else if( field.datatype() == DataType::real64() ) {  write_field_nodes<double>(*this,functionspace,field,file); }
    //[delete]
    //[delete]else if( function_space.metadata().get<long>("type") == Entity::ELEMS
    //[delete]      || function_space.metadata().get<long>("type") == Entity::FACES )
    //[delete]{
    //[delete]  if     ( field.datatype() == DataType::int32()  ) {  write_field_elems<int   >(*this,function_space,field,file); }
    //[delete]  else if( field.datatype() == DataType::int64()  ) {  write_field_elems<long  >(*this,function_space,field,file); }
    //[delete]  else if( field.datatype() == DataType::real32() ) {  write_field_elems<float >(*this,function_space,field,file); }
    //[delete]  else if( field.datatype() == DataType::real64() ) {  write_field_elems<double>(*this,function_space,field,file); }
    //[delete]}
    file << std::flush;
  }
  file.close();
}

void Gmsh::write(const Field& field, const functionspace::Nodes& functionspace, const PathName& file_path, openmode mode) const
{
  FieldSet fieldset;
  fieldset.add(field);
  write(fieldset,functionspace,file_path,mode);
}


void Gmsh::write(const Field& field, const PathName& file_path, openmode mode) const
{
  if( !field.functionspace() )
  {
    std::stringstream msg;
    msg << "Field ["<<field.name()<<"] has no functionspace";
    throw eckit::AssertionFailed(msg.str(),Here());
  }
  functionspace::Nodes* functionspace = dynamic_cast<functionspace::Nodes*>(field.functionspace());
  if( !functionspace )
  {
    std::stringstream msg;
    msg << "Field ["<<field.name()<<"] has functionspace ["<<functionspace->name()<<"] but requires a [NodesFunctionSpace]";
    throw eckit::AssertionFailed(msg.str(),Here());
  }
  FieldSet fieldset;
  fieldset.add(field);
  write(fieldset,*functionspace,file_path,mode);
}


// ------------------------------------------------------------------
// C wrapper interfaces to C++ routines

Gmsh* atlas__Gmsh__new () {
  return new Gmsh();
}

void atlas__Gmsh__delete (Gmsh* This) {
  delete This;
}

Mesh* atlas__Gmsh__read (Gmsh* This, char* file_path) {
  return This->read( PathName(file_path) );
}

void atlas__Gmsh__write (Gmsh* This, Mesh* mesh, char* file_path) {
  This->write( *mesh, PathName(file_path) );
}

Mesh* atlas__read_gmsh (char* file_path)
{
  return Gmsh().read(PathName(file_path));
}

void atlas__write_gmsh_mesh (Mesh* mesh, char* file_path) {
  Gmsh writer;
  writer.write( *mesh, PathName(file_path) );
}

void atlas__write_gmsh_fieldset (FieldSet* fieldset, functionspace::Nodes* functionspace, char* file_path, int mode) {
  Gmsh writer;
  writer.write( *fieldset, *functionspace, PathName(file_path) );
}

void atlas__write_gmsh_field (Field* field, functionspace::Nodes* functionspace, char* file_path, int mode) {
  Gmsh writer;
  writer.write( *field, *functionspace, PathName(file_path) );
}

// ------------------------------------------------------------------

} // namespace io
} // namespace atlas

