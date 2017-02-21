#pragma once

#include <array>
#include "atlas/grid/spacing/Spacing.h"

namespace atlas {
namespace grid {
namespace spacing {

class LinearSpacing: public Spacing {
  
public:

  struct Params {
    double start;
    double end;
    long N;
    double length;
    bool endpoint;
    double step;
    Params(const eckit::Parametrisation& p);
  };

public:

    // constructor
    LinearSpacing( const eckit::Parametrisation& p );

    // LinearSpacing( double centre, double step, long N, bool endpoint=true );

    LinearSpacing( double start, double end, long N, bool endpoint=true );

    // class name
    static std::string className() { return "atlas.LinearSpacing"; }
    static std::string spacing_type_str() {return "linear";}

    double step() const;
    
    bool endpoint() const;

protected:

    // points are equally spaced between xmin and xmax
    // Depending on value of endpoint, the spacing will be different
    void setup(double start, double end, long N, bool endpoint);

};

}  // namespace spacing
}  // namespace grid
}  // namespace atlas
