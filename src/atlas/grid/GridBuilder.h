#ifndef atlas_grid_builder_H
#define atlas_grid_builder_H
/*
 * (C) Copyright 1996-2014 ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

#include <cstddef>
#include <vector>
#include <cmath>

#include "grib_api.h"

#include "eckit/exception/Exceptions.h"
#include "eckit/memory/NonCopyable.h"
#include "eckit/memory/ScopedPtr.h"
#include "eckit/filesystem/PathName.h"
#include "eckit/io/StdFile.h"

#include "eckit/geometry/Point2.h"
#include "atlas/grid/RegularLatLonGrid.h"
#include "atlas/grid/ReducedLatLonGrid.h"
#include "atlas/grid/ReducedGaussianGrid.h"
#include "atlas/grid/RegularGaussianGrid.h"

//------------------------------------------------------------------------------------------------------

namespace atlas {
namespace grid {

class RegularLatLonGrid;
class RegularGaussianGrid;
class ReducedGaussianGrid;

//------------------------------------------------------------------------------------------------------

/// Abstract base class for building Grid* objects
/// The Grid objects themselves should be independent of building mechanism
/// Currently we are building the Grid's from GRIB, however in the future
/// this could be from NetCDF or from reading a file
class GridBuilder : private eckit::NonCopyable {
public: // methods

   GridBuilder();
   virtual ~GridBuilder();

   /// @returns a shared ptr to a Grid. This will open and interrogate the file
   /// Currently we are assuming the file is a GRIB file
   virtual Grid::Ptr build(const eckit::PathName& pathname) const = 0;

   //
   // virtual Grid* build(eckit::DataHandle) = 0;
};

// =============================================================================

/// Singleton derivative of GridBuilder.
/// This will create Grid* by reading from a Grib file
/// If we come across Grids that we can not handle, we will create the Unstructured* Grid
class GRIBGridBuilder : public GridBuilder {
private:
   GRIBGridBuilder();
public:

   virtual ~GRIBGridBuilder();

   /// This method will open the file, get the grib handle and then call
   /// build_grid_from_grib_handle
   virtual Grid::Ptr build(const eckit::PathName& pathname) const;

   /// This function has been separated out to aid testing
   Grid::Ptr build_grid_from_grib_handle(grib_handle* h ) const;

   /// @returns the singleton instance of this class
   static GRIBGridBuilder& instance();

   /// Returns the list of all known grib grid types.
   /// Allow better error handling during Grid construction.
   /// This really belongs in grib_api,
   static void known_grid_types(std::set<std::string>&);
};

// =============================================================================

/// Base helper class for creating Grid derivatives from GRIB files.
class GribGridBuilderHelper : private eckit::NonCopyable {
public:
   virtual ~GribGridBuilderHelper();

   /// @returns unique hash for this grid
   std::string hash() const { return hash_;}

   /// The derivatives will create the Grid, based on GRIB contents
   virtual Grid::Ptr build() = 0;

protected:
   GribGridBuilderHelper( grib_handle* h );

   /// grib edition 1 - milli-degrees
   /// grib edition 2 - micro-degrees or could be defined by the keys: "subdivisionsOfBasicAngle" and "basicAngleOfTheInitialProductionDomain"
   /// Therefore the client needs access to this when dealing with double based comparisons (for tolerances)
   double epsilon() const { return epsilon_; }

   /// It Appears as IFS does not honour longitude of last grid point for GRIB2 Guassian grids
   /// It should be 360 - 90/N
   /// However they appear to be using GRIB1 precision for GRIB2 files.
   /// Hence our check for globalness will be incorrect.
   double globalness_epsilon() const { return 0.001;}

   static int scanningMode(long iScansNegatively, long jScansPositively);
   static void comparePointList(const std::vector<Grid::Point>& points,  double epsilon, grib_handle* handle);

   Grid::BoundBox boundingBox() const;

protected:
   grib_handle* handle_;             ///< No not delete
   long   editionNumber_;           ///< Grib 1 or Grib 2
   double north_;                   ///< In degrees
   double south_;                   ///< In degrees
   double west_;                    ///< In degrees
   double east_;                    ///< In degrees
   double epsilon_;                 ///< Grib 1 or Grib 2
   long   numberOfDataPoints_;
   long   iScansNegatively_;
   long   jScansPositively_;
   std::string hash_;
};

// =============================================================================

/// To avoid copying data, we placed the data directly into GRIB
/// via use of friendship
class GribReducedGaussianGrid : public GribGridBuilderHelper {
public:
   GribReducedGaussianGrid( grib_handle* h );
   virtual ~GribReducedGaussianGrid();

   virtual Grid::Ptr build();

private:
   void add_point(int lat_index);
   bool isGlobalNorthSouth() const;
   bool isGlobalWestEast() const;

private:
   long   nj_;                      ///< No of points along Y axes
   eckit::ScopedPtr<ReducedGaussianGrid> the_grid_;
};

// =============================================================================

/// To avoid copying data, we placed the data directly into GRIB
/// via use of friendshi
class GribRegularGaussianGrid : public GribGridBuilderHelper {
public:
   GribRegularGaussianGrid( grib_handle* h );
   virtual ~GribRegularGaussianGrid();

   virtual Grid::Ptr build();

private:
   bool isGlobalNorthSouth() const;
   bool isGlobalWestEast() const;

private:
   long   nj_;                      /// No of points along Y axes
   eckit::ScopedPtr<RegularGaussianGrid> the_grid_;
};

// =============================================================================

/// To avoid copying data, we placed the data directly into GRIB
/// via use of friendship
class GribRegularLatLonGrid : public GribGridBuilderHelper {
public:
   GribRegularLatLonGrid( grib_handle* h );
   virtual ~GribRegularLatLonGrid();

   virtual Grid::Ptr build();

private:
   // Functions specific to Regular Lat long grids
   long rows() const;
   long cols() const;
   double incLat() const;
   double incLon() const;

   // for verification/checks
   double computeIncLat() const ;
   double computeIncLon() const ;
   long computeRows(double north, double south, double west, double east) const;
   long computeCols(double west, double east) const;

private:
   eckit::ScopedPtr<RegularLatLonGrid> the_grid_;
};


/// To avoid copying data, we placed the data directly into GRIB
/// via use of friendship
class GribReducedLatLonGrid : public GribGridBuilderHelper {
public:
   GribReducedLatLonGrid( grib_handle* h );
   virtual ~GribReducedLatLonGrid();

   virtual Grid::Ptr build();

private:
   // Functions specific to Regular Lat long grids
   long rows() const;

   // for verification/checks
   double computeIncLat() const ;

   bool isGlobalNorthSouth() const;
   bool isGlobalWestEast() const;

private:
   eckit::ScopedPtr<ReducedLatLonGrid> the_grid_;
};


// Ensures grib handle is always deleted, in presence of exceptions
class GribFile {
public:
   GribFile(const std::string& the_file_path);
   GribFile(const eckit::PathName& pathname);
   ~GribFile();

   grib_handle* handle() const { return handle_;}

private:
   eckit::StdFile theGribFile_;
   grib_handle* handle_;

private:
   void init(const std::string& the_file_path);
   GribFile(const GribFile&);            // prevent copies
   GribFile& operator=(const GribFile&); // prevent assignment
};

//------------------------------------------------------------------------------------------------------

} // namespace grid
} // namespace atlas

#endif
