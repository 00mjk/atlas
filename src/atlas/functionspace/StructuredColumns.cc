/*
 * (C) Copyright 1996-2017 ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

#include "atlas/functionspace/StructuredColumns.h"

#include "eckit/utils/MD5.h"

#include "atlas/mesh/Mesh.h"
#include "atlas/field/FieldSet.h"
#include "atlas/util/Checksum.h"
#include "atlas/runtime/ErrorHandling.h"
#include "atlas/parallel/mpi/mpi.h"
#include "atlas/array/MakeView.h"

#ifdef ATLAS_HAVE_TRANS
#include "atlas/trans/Trans.h"
#endif

namespace atlas {
namespace functionspace {
namespace detail {

namespace {
void set_field_metadata(const eckit::Parametrisation& config, Field& field)
{
  bool global(false);
  if( config.get("global",global) )
  {
    if( global )
    {
      size_t owner(0);
      config.get("owner",owner);
      field.metadata().set("owner",owner);
    }
  }
  field.metadata().set("global",global);
}
}

size_t StructuredColumns::config_size(const eckit::Parametrisation& config) const
{
  size_t size = this->size();
  bool global(false);
  if( config.get("global",global) )
  {
    if( global )
    {
      size_t owner(0);
      config.get("owner",owner);
      size = (parallel::mpi::comm().rank() == owner ? grid_.size() : 0);
    }
  }
  return size;
}


// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
StructuredColumns::StructuredColumns(const Grid& grid) :
  grid_(grid)
{
    if ( not grid_ )
    {
      throw eckit::BadCast("Grid is not a grid::Structured type. "
                           "Cannot partition using IFS trans", Here());
    }

#ifdef ATLAS_HAVE_TRANS
    trans_ = new trans::Trans(grid_);

    npts_ = trans_->ngptot();

    // Number of latitude bands
    int n_regions_NS = trans_->n_regions_NS();

    // Number of partitions per latitude band
    array::LocalView<int,1> n_regions = trans_->n_regions();

    // First latitude of latitude band
    array::LocalView<int,1> nfrstlat = trans_->nfrstlat();

    // First latitude of latitude band
    array::LocalView<int,1> nlstlat = trans_->nlstlat();

    // Index of latitude partition (note that if a partition
    // has two regions on a latitude - the index increases
    // by one (2 numbers)
    array::LocalView<int,1> nptrfrstlat = trans_->nptrfrstlat();

    // Starting longitudinal point per given latitude (ja)
    // Note that it is associated to nptrfrstlat
    array::LocalView<int,2> nsta = trans_->nsta();

    // Number of longitudinal points per given latitude (ja)
    // Note that it is associated to nptrfrstlat
    array::LocalView<int,2> nonl = trans_->nonl();

    size_t proc(0);
    // Loop over number of latitude bands (ja)
    for (int ja = 0; ja < n_regions_NS; ++ja)
    {
        // Loop over number of longitude bands (jb)
        for (int jb = 0; jb < n_regions(ja); ++jb)
        {
            if (proc == parallel::mpi::comm().rank())
            {
                nlat_ = nlstlat(ja) - nfrstlat(ja) + 1;
                nlon_.resize(nlat_);
                first_lon_.resize(nlat_);
                first_lat_ = nfrstlat(ja)-1;

                // Loop over latitude points of lat band (ja) and lon band (jb)
                size_t ilat = 0;
                for (int jglat = first_lat_; jglat < nlstlat(ja); ++jglat)
                {
                    size_t igl = nptrfrstlat(ja) + jglat - nfrstlat(ja);
                    nlon_[ilat] = nonl(jb,igl);
                    first_lon_[ilat] = nsta(jb,igl);
                    ilat++;
                }
                goto exit_outer_loop;
            }
            ++proc;
        }
    }
    exit_outer_loop: ;


#if 0
    // COMPILED OUT
    {
        int localLatID;
        int localLonID;
        proc = 0;

        // Loop over number of latitude bands (ja)
        for (size_t ja = 0; ja < n_regions_NS; ++ja)
        {
            // Loop over number of longitude bands (jb)
            for (size_t jb = 0; jb < n_regions[ja]; ++jb)
            {
                if (proc == parallel::mpi::comm().rank())
                {
                    // Loop over latitude points of lat band (ja) and lon band (jb)
                    for (int jglat = nfrstlat[ja]-1; jglat < nlstlat[ja]; ++jglat)
                    {
                        int globalLatID = localLatID + nfrstlat[ja];
                        size_t igl = nptrfrstlat[ja] + jglat - nfrstlat[ja];

                        // Loop over longitude points of given latitude point
                        // of lat band (ja) and lon band (jb) and
                        for (int jglon = nsta(jb,igl)-1;
                             jglon < nsta(jb,igl)+nonl(jb,igl)-1; ++jglon)
                        {
                            int globalLonID = nsta(jb,igl) + localLonID;
                        }
                    }
                }
                ++proc;
            }
        }
    }
#endif
#else
    npts_ = grid_.size();
    nlat_ = grid_.ny();
    nlon_.resize(nlat_);
    for( size_t jlat=0; jlat<nlat_; ++jlat )
      nlon_[jlat] = grid_.nx(jlat);
    first_lat_ = 0;
    first_lon_.resize(nlat_,0);
#endif
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
StructuredColumns::~StructuredColumns()
{
#ifdef ATLAS_HAVE_TRANS
    delete trans_;
#endif
}
// ----------------------------------------------------------------------------


size_t StructuredColumns::footprint() const {
  size_t size = sizeof(*this);
  // TODO
  return size;
}

// ----------------------------------------------------------------------------
// Create Field
// ----------------------------------------------------------------------------
Field StructuredColumns::createField(const std::string& name, array::DataType datatype, const eckit::Parametrisation& options ) const
{
#ifdef ATLAS_HAVE_TRANS
    size_t npts = config_size(options);
    Field field = Field(name, datatype, array::make_shape(npts));
    field.set_functionspace(this);
    set_field_metadata(options,field);
    return field;
#else
    if( parallel::mpi::comm().size() > 1 )
    {
        throw eckit::NotImplemented(
          "StructuredColumns::createField currently relies"
          " on ATLAS_HAVE_TRANS for parallel fields", Here());
    }
    Field field = Field(name, datatype, array::make_shape(grid_.size()) );
    field.set_functionspace(this);
    set_field_metadata(options,field);
    return field;
#endif
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// Create Field with vertical levels
// ----------------------------------------------------------------------------
Field StructuredColumns::createField(
    const std::string& name, array::DataType datatype,
    size_t levels, const eckit::Parametrisation& options) const
{
#ifdef ATLAS_HAVE_TRANS
    size_t npts = config_size(options);
    Field field = Field(
                    name, datatype, array::make_shape(npts, levels));

    field.set_functionspace(this);
    field.set_levels(levels);
    set_field_metadata(options,field);
    return field;
#else
    if( parallel::mpi::comm().size() > 1 )
    {
        throw eckit::NotImplemented(
          "StructuredColumns::createField currently relies"
          " on ATLAS_HAVE_TRANS for parallel fields", Here());
    }
    Field field = Field(
                    name, datatype, array::make_shape(grid_.size(), levels));

    field.set_functionspace(this);
    set_field_metadata(options,field);
    return field;
#endif
}
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Gather FieldSet
// ----------------------------------------------------------------------------
void StructuredColumns::gather(
    const FieldSet& local_fieldset,
    FieldSet& global_fieldset ) const
{
#ifdef ATLAS_HAVE_TRANS
    ASSERT(local_fieldset.size() == global_fieldset.size());

    for( size_t f=0; f<local_fieldset.size(); ++f )
    {
        const Field& loc = local_fieldset[f];
        Field& glb = global_fieldset[f];
        if( loc.datatype() != array::DataType::str<double>() )
        {
            std::stringstream err;
            err << "Cannot gather Structured field " << loc.name()
                << " of datatype " << loc.datatype().str() << ".";
            err << "Only " << array::DataType::str<double>() << " supported.";
            throw eckit::BadValue(err.str());
        }

        size_t root(0);
        glb.metadata().get("owner",root);
        std::vector<int> nto(1,root+1);
        if( loc.rank() > 1 )
        {
            nto.resize(loc.stride(0));
            for( size_t i=0; i<nto.size(); ++i )
            {
                nto[i] = root+1;
            }
        }
        trans_->gathgrid(nto.size(), nto.data(),
                         array::make_storageview<double>(loc).data(),
                         array::make_storageview<double>(glb).data());
    }

#else
    eckit::NotImplemented("StructuredColumns::gather currently relies "
                          "on ATLAS_HAVE_TRANS", Here());
#endif
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// Gather Field
// ----------------------------------------------------------------------------
void StructuredColumns::gather(
    const Field& local,
    Field& global) const
{
    FieldSet local_fields;
    FieldSet global_fields;
    local_fields.add(local);
    global_fields.add(global);
    gather(local_fields,global_fields);
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// Scatter FieldSet
// ----------------------------------------------------------------------------
void StructuredColumns::scatter(
    const FieldSet& global_fieldset,
    FieldSet& local_fieldset) const
{
#ifdef ATLAS_HAVE_TRANS
    ASSERT(local_fieldset.size() == global_fieldset.size());

    for( size_t f=0; f<local_fieldset.size(); ++f )
    {
        const Field& glb = global_fieldset[f];
        Field& loc = local_fieldset[f];
        if( loc.datatype() != array::DataType::str<double>() )
        {
            std::stringstream err;
            err << "Cannot scatter Structured field " << glb.name()
                << " of datatype " << glb.datatype().str() << ".";
            err << "Only " << array::DataType::str<double>() << " supported.";
            throw eckit::BadValue(err.str());
        }

        size_t root(0);
        glb.metadata().get("owner",root);
        std::vector<int> nfrom(1,root+1);
        if( loc.rank() > 1 )
        {
            nfrom.resize(loc.stride(0));
            for( size_t i=0; i<nfrom.size(); ++i)
            {
                nfrom[i] = root+1;
            }
        }
        trans_->distgrid(nfrom.size(), nfrom.data(),
                         array::make_storageview<double>(glb).data(),
                         array::make_storageview<double>(loc).data());
        glb.metadata().broadcast(loc.metadata(),root);
        loc.metadata().set("global",false);
  }
#else
    eckit::NotImplemented("StructuredColumns::scatter currently relies "
                          "on ATLAS_HAVE_TRANS", Here());
#endif
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// Scatter Field
// ----------------------------------------------------------------------------
void StructuredColumns::scatter(
    const Field& global,
    Field& local) const
{
    FieldSet global_fields;
    FieldSet local_fields;
    global_fields.add(global);
    local_fields.add(local);
    scatter(global_fields, local_fields);
}
// ----------------------------------------------------------------------------






// ----------------------------------------------------------------------------
// Retrieve Global index from Local one
// ----------------------------------------------------------------------------
double StructuredColumns::y(
    size_t j) const
{
  return grid_.y(j+first_lat_);
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// Retrieve Global index from Local one
// ----------------------------------------------------------------------------
double StructuredColumns::x(
    size_t i,
    size_t j) const
{
  return grid_.x(i+first_lon_[j],j+first_lat_);
}
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Checksum FieldSet
// ----------------------------------------------------------------------------
std::string StructuredColumns::checksum(
    const FieldSet& fieldset) const
{
    eckit::MD5 md5;
    NOTIMP;
    return md5;
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// Checksum Field
// ----------------------------------------------------------------------------
std::string StructuredColumns::checksum(
    const Field& field) const
{
    FieldSet fieldset;
    fieldset.add(field);
    return checksum(fieldset);
}
// ----------------------------------------------------------------------------

} // namespace detail

// ----------------------------------------------------------------------------

StructuredColumns::StructuredColumns() :
  FunctionSpace(),
  functionspace_(nullptr) {
}

StructuredColumns::StructuredColumns( const FunctionSpace& functionspace ) :
  FunctionSpace( functionspace ),
  functionspace_( dynamic_cast<const detail::StructuredColumns*>( get() ) ) {
}

StructuredColumns::StructuredColumns( const Grid& grid ) :
  FunctionSpace( new detail::StructuredColumns(grid) ),
  functionspace_( dynamic_cast<const detail::StructuredColumns*>( get() ) ) {
}

Field StructuredColumns::createField(const std::string& name, array::DataType datatype, const eckit::Parametrisation& options ) const
{
  return functionspace_->createField(name,datatype,options);
}

Field StructuredColumns::createField(
    const std::string& name, array::DataType datatype,
    size_t levels, const eckit::Parametrisation& options) const
{
  return functionspace_->createField(name,datatype,levels,options);
}

void StructuredColumns::gather( const FieldSet& local, FieldSet& global ) const {
  functionspace_->gather(local,global);
}

void StructuredColumns::gather( const Field& local, Field& global ) const {
  functionspace_->gather(local,global);
}

void StructuredColumns::scatter( const FieldSet& global, FieldSet& local ) const {
  functionspace_->scatter(global,local);
}

void StructuredColumns::scatter( const Field& global, Field& local ) const {
  functionspace_->scatter(global,local);
}

std::string StructuredColumns::checksum( const FieldSet& fieldset ) const {
  return functionspace_->checksum(fieldset);
}

std::string StructuredColumns::checksum( const Field& field) const {
  return functionspace_->checksum(field);
}


// ----------------------------------------------------------------------------
// Fortran interfaces
// ----------------------------------------------------------------------------
extern "C"
{

const detail::StructuredColumns* atlas__functionspace__StructuredColumns__new__grid (const Grid::Implementation* grid)
{
  ATLAS_ERROR_HANDLING(
    return new detail::StructuredColumns( Grid(grid) );
  );
  return 0;
}

void atlas__functionspace__StructuredColumns__delete (detail::StructuredColumns* This)
{
  ATLAS_ERROR_HANDLING(
    ASSERT(This);
    delete This;
  );
}

field::FieldImpl* atlas__fs__StructuredColumns__create_field_name_kind (const detail::StructuredColumns* This, const char* name, int kind, const eckit::Parametrisation* options)
{
  ATLAS_ERROR_HANDLING(
    ASSERT(This);
    field::FieldImpl* field;
    {
      Field f = This->createField(std::string(name),array::DataType(kind),*options);
      field = f.get();
      field->attach();
    }
    field->detach();
    return field;
  );
  return 0;
}

field::FieldImpl* atlas__fs__StructuredColumns__create_field_name_kind_lev (const detail::StructuredColumns* This, const char* name, int kind, int levels, const eckit::Parametrisation* options)
{
  ATLAS_ERROR_HANDLING(
    ASSERT(This);
    field::FieldImpl* field;
    {
      Field f = This->createField(std::string(name),array::DataType(kind),levels,*options);
      field = f.get();
      field->attach();
    }
    field->detach();
    return field;
  );
  return 0;
}

void atlas__functionspace__StructuredColumns__gather (const detail::StructuredColumns* This, const field::FieldImpl* local, field::FieldImpl* global)
{
  ATLAS_ERROR_HANDLING(
    ASSERT(This);
    ASSERT(global);
    ASSERT(local);
    const Field l(local);
    Field g(global);
    This->gather(l,g);
  );
}

void atlas__functionspace__StructuredColumns__scatter (const detail::StructuredColumns* This, const field::FieldImpl* global, field::FieldImpl* local)
{
  ATLAS_ERROR_HANDLING(
    ASSERT(This);
    ASSERT(global);
    ASSERT(local);
    const Field g(global);
    Field l(local);
    This->scatter(g,l);
  );
}

void atlas__fs__StructuredColumns__checksum_fieldset(const detail::StructuredColumns* This, const field::FieldSetImpl* fieldset, char* &checksum, int &size, int &allocated)
{
  ASSERT(This);
  ASSERT(fieldset);
  ATLAS_ERROR_HANDLING(
    std::string checksum_str (This->checksum(fieldset));
    size = checksum_str.size();
    checksum = new char[size+1]; allocated = true;
    strcpy(checksum,checksum_str.c_str());
  );
}

void atlas__fs__StructuredColumns__checksum_field(const detail::StructuredColumns* This, const field::FieldImpl* field, char* &checksum, int &size, int &allocated)
{
  ASSERT(This);
  ASSERT(field);
  ATLAS_ERROR_HANDLING(
    std::string checksum_str (This->checksum(field));
    size = checksum_str.size();
    checksum = new char[size+1]; allocated = true;
    strcpy(checksum,checksum_str.c_str());
  );
}

}
// ----------------------------------------------------------------------------

} // namespace functionspace
} // namespace atlas
