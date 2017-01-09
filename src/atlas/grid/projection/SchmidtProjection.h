#ifndef atlas_SchmidtProjection_H
#define atlas_SchmidtProjection_H

#include "atlas/grid/projection/Projection.h"

namespace atlas {
namespace grid {
namespace projection {

class SchmidtProjection: public Projection {
	public:
		// constructor
		SchmidtProjection(const eckit::Parametrisation& p);
		SchmidtProjection() {};
		
		// copy constructor
		SchmidtProjection( const SchmidtProjection& rhs );
		
		// clone method
		virtual SchmidtProjection *clone() const ;
		
		// class name
		static std::string className() { return "atlas.SchmidtProjection"; }
		static std::string projection_type_str() {return "schmidt";}
		virtual std::string virtual_projection_type_str() const {return "schmidt";}

		// projection and inverse projection
		virtual eckit::geometry::LLPoint2 coords2lonlat(eckit::geometry::Point2) const;
		virtual eckit::geometry::Point2 lonlat2coords(eckit::geometry::LLPoint2) const;

		// purely regional? - no!
		bool isRegional() { return false; }	// schmidt is global grid
		
		// specification
		virtual eckit::Properties spec() const;
	
	private:
		double c_;		// stretching factor
};

}  // namespace projection
}  // namespace grid
}  // namespace atlas


#endif
