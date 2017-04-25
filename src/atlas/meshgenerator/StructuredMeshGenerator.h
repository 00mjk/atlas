/*
 * (C) Copyright 1996-2017 ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

#pragma once

#include "atlas/meshgenerator/MeshGenerator.h"
#include "atlas/util/Metadata.h"
#include "atlas/util/Config.h"

namespace eckit {
  class Parametrisation;
  class MD5;
}

namespace atlas {
    class Mesh;
}

namespace atlas {
namespace grid {
    class StructuredGrid;
    class Distribution;
} }


namespace atlas {
namespace meshgenerator {


namespace detail {

struct Region;

//----------------------------------------------------------------------------------------------------------------------

class StructuredMeshGenerator : public MeshGenerator::Implementation {

public:

  StructuredMeshGenerator(const eckit::Parametrisation& = util::NoConfig() );

  virtual void generate(const Grid&, const grid::Distribution&, Mesh&) const;
  virtual void generate(const Grid&, Mesh&) const;

  using MeshGenerator::Implementation::generate;

private:

  virtual void hash(eckit::MD5&) const;

  void configure_defaults();

  void generate_region(
    const grid::StructuredGrid&,
    const std::vector<int>& parts,
    int mypart,
    Region& region) const;

  void generate_mesh_new(
    const grid::StructuredGrid&,
    const std::vector<int>& parts,
    const Region& region,
    Mesh& m) const;

  void generate_mesh(
    const grid::StructuredGrid&,
    const std::vector<int>& parts,
    const Region& region,
    Mesh& m ) const;

private:

  util::Metadata options;

};

}

class StructuredMeshGenerator : public MeshGenerator {
public:
  StructuredMeshGenerator(const eckit::Parametrisation& config = util::NoConfig()) :
      MeshGenerator("structured",config) {
  }
  StructuredMeshGenerator(const MeshGenerator& m ) :
      MeshGenerator(m) {
  }
};

//----------------------------------------------------------------------------------------------------------------------

} // namespace meshgenerator
} // namespace atlas