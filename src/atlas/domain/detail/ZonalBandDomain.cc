#include "atlas/domain/detail/ZonalBandDomain.h"


namespace atlas {
namespace domain {

namespace {

  static bool _is_global(double ymin, double ymax) {
    const double eps = 1.e-12;
    return std::abs( (ymax-ymin) - 180. ) < eps ;
  }

  static std::array<double,2> get_interval_y(const eckit::Parametrisation& params) {
    double ymin, ymax;

    if( ! params.get("ymin",ymin) )
      throw eckit::BadParameter("ymin missing in Params",Here());

    if( ! params.get("ymax",ymax) )
      throw eckit::BadParameter("ymax missing in Params",Here());

    return {ymin,ymax};
  }

  constexpr std::array<double,2> interval_x() {
    return { 0., 360. };
  }
}

constexpr char ZonalBandDomain::units_[];

ZonalBandDomain::ZonalBandDomain(const eckit::Parametrisation& params) :
  ZonalBandDomain( get_interval_y(params) ) {
}

ZonalBandDomain::ZonalBandDomain( const Interval& interval_y ) :
  RectangularDomain( interval_x(), interval_y, units_ ) {
  global_ = _is_global(ymin(),ymax());
  ymin_tol_ = ymin()-1.e-6;
  ymax_tol_ = ymax()+1.e-6;
}


bool ZonalBandDomain::contains(double x, double y) const {
  return contains_y(y);
}

eckit::Properties ZonalBandDomain::spec() const {
  eckit::Properties domain_prop;
  domain_prop.set("type",type());
  domain_prop.set("ymin",ymin());
  domain_prop.set("ymax",ymax());
  return domain_prop;
}

void ZonalBandDomain::print(std::ostream& os) const {
  os << "ZonalBandDomain["
     <<  "ymin=" << ymin()
     << ",ymax=" << ymax()
     << "]";
}

register_BuilderT1(Domain,ZonalBandDomain,ZonalBandDomain::static_type());

}  // namespace domain
}  // namespace atlas
