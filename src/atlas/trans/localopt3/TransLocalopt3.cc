/*
 * (C) Copyright 2013 ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation
 * nor does it submit to any jurisdiction.
 */

#include "atlas/trans/localopt3/TransLocalopt3.h"
#include <math.h>
#include "atlas/array.h"
#include "atlas/option.h"
#include "atlas/parallel/mpi/mpi.h"
#include "atlas/runtime/ErrorHandling.h"
#include "atlas/runtime/Log.h"
#include "atlas/trans/VorDivToUV.h"
#include "atlas/trans/local/LegendrePolynomials.h"
#include "atlas/trans/localopt3/LegendrePolynomialsopt3.h"
#include "atlas/util/Constants.h"
#include "eckit/linalg/LinearAlgebra.h"
#include "eckit/linalg/Matrix.h"
#if ATLAS_HAVE_MKL
#include "mkl.h"
#endif

namespace atlas {
namespace trans {

namespace {
static TransBuilderGrid<TransLocalopt3> builder( "localopt3" );
}

// --------------------------------------------------------------------------------------------------------------------
// Helper functions
// --------------------------------------------------------------------------------------------------------------------
namespace {  // anonymous

size_t legendre_size( const size_t truncation ) {
    return ( truncation + 2 ) * ( truncation + 1 ) / 2;
}

int nlats_northernHemisphere( const int nlats ) {
    return ceil( nlats / 2. );
    // using ceil here should make it possible to have odd number of latitudes (with the centre latitude being the equator)
}

int num_n( const int truncation, const int m, const bool symmetric ) {
    int len = 0;
    if ( symmetric ) { len = ( truncation - m + 2 ) / 2; }
    else {
        len = ( truncation - m + 1 ) / 2;
    }
    return len;
}

void alloc_aligned( double*& ptr, size_t n ) {
#if ATLAS_HAVE_MKL
    int al = 64;
    ptr    = (double*)mkl_malloc( sizeof( double ) * n, al );
#else
    posix_memalign( (void**)&ptr, sizeof( double ) * 64, sizeof( double ) * n );
    //ptr = (double*)malloc( sizeof( double ) * n );
    //ptr = new double[n];
#endif
}

void free_aligned( double*& ptr ) {
#if ATLAS_HAVE_MKL
    mkl_free( ptr );
#else
    free( ptr );
#endif
}

int add_padding( int n ) {
    return std::ceil( n / 8. ) * 8;
}
}  // namespace

// --------------------------------------------------------------------------------------------------------------------
// Class TransLocalopt3
// --------------------------------------------------------------------------------------------------------------------

TransLocalopt3::TransLocalopt3( const Cache& cache, const Grid& grid, const long truncation,
                                const eckit::Configuration& config ) :
    grid_( grid ),
    truncation_( truncation ),
    precompute_( config.getBool( "precompute", true ) ) {
    ATLAS_TRACE( "Precompute legendre opt3" );
#if ATLAS_HAVE_MKL
    eckit::linalg::LinearAlgebra::backend( "mkl" );  // might want to choose backend with this command
#else
    eckit::linalg::LinearAlgebra::backend( "generic" );  // might want to choose backend with this command
#endif
    double fft_threshold = 0.05;  // fraction of latitudes of the full grid up to which FFT is used.
    // This threshold needs to be adjusted depending on the dgemm and FFT performance of the machine
    // on which this code is running!
    int nlats         = 0;
    int nlons         = 0;
    int neqtr         = 0;
    useFFT_           = true;
    dgemmMethod1_     = false;
    unstruct_precomp_ = true;
    nlatsNH_          = 0;
    nlatsSH_          = 0;
    nlatsLeg_         = 0;
    if ( grid::StructuredGrid( grid_ ) && not grid_.projection() ) {
        grid::StructuredGrid g( grid_ );
        nlats = g.ny();
        nlons = g.nxmax();
        for ( size_t j = 0; j < nlats; ++j ) {
            // assumptions: latitudes in g.y(j) are monotone and decreasing
            // no assumption on whether we have 0, 1 or 2 latitudes at the equator
            double lat = g.y( j );
            if ( lat > 0. ) { nlatsNH_++; }
            if ( lat == 0. ) { neqtr++; }
            if ( lat < 0. ) { nlatsSH_++; }
        }
        if ( neqtr > 0 ) {
            nlatsNH_++;
            nlatsSH_++;
        }
        if ( nlatsNH_ >= nlatsSH_ ) { nlatsLeg_ = nlatsNH_; }
        else {
            nlatsLeg_ = nlatsSH_;
        }
        Grid g_global( grid.name() );
        grid::StructuredGrid gs_global( g_global );
        nlonsGlobal_  = gs_global.nxmax();
        jlonMin_      = 0;
        double lonmin = fmod( g.x( 0, 0 ), 360 );
        if ( lonmin < 0. ) { lonmin += 360.; }
        if ( nlons < fft_threshold * nlonsGlobal_ ) { useFFT_ = false; }
        else {
            if ( nlons < nlonsGlobal_ ) {
                // need to use FFT with cropped grid
                for ( size_t j = 0; j < nlonsGlobal_; ++j ) {
                    if ( gs_global.x( j, 0 ) == lonmin ) { jlonMin_ = j; }
                }
            }
        }
        //Log::info() << "nlats=" << g.ny() << " nlatsGlobal=" << gs_global.ny() << std::endl;
        std::vector<double> lats( nlatsLeg_ );
        std::vector<double> lons( nlons );
        if ( nlatsNH_ >= nlatsSH_ ) {
            for ( size_t j = 0; j < nlatsLeg_; ++j ) {
                lats[j] = g.y( j ) * util::Constants::degreesToRadians();
            }
        }
        else {
            for ( size_t j = nlats - 1, idx = 0; idx < nlatsLeg_; --j, ++idx ) {
                lats[idx] = -g.y( j ) * util::Constants::degreesToRadians();
            }
        }
        for ( size_t j = 0; j < nlons; ++j ) {
            lons[j] = g.x( j, 0 ) * util::Constants::degreesToRadians();
        }
        // precomputations for Legendre polynomials:
        {
            ATLAS_TRACE( "opt3 precomp Legendre" );
            int size_sym  = 0;
            int size_asym = 0;
            legendre_sym_begin_.resize( truncation_ + 3 );
            legendre_asym_begin_.resize( truncation_ + 3 );
            legendre_sym_begin_[0]  = 0;
            legendre_asym_begin_[0] = 0;
            for ( int jm = 0; jm <= truncation_ + 1; jm++ ) {
                size_sym += add_padding( num_n( truncation_ + 1, jm, true ) * nlatsLeg_ );
                size_asym += add_padding( num_n( truncation_ + 1, jm, false ) * nlatsLeg_ );
                legendre_sym_begin_[jm + 1]  = size_sym;
                legendre_asym_begin_[jm + 1] = size_asym;
            }
            alloc_aligned( legendre_sym_, size_sym );
            alloc_aligned( legendre_asym_, size_asym );
            FILE* file_leg;
            file_leg = fopen( "legendre.bin", "r" );
            if ( false ) {  //if ( file_leg ) {
                fread( legendre_sym_, sizeof( double ), size_sym, file_leg );
                fread( legendre_asym_, sizeof( double ), size_asym, file_leg );
                fclose( file_leg );
            }
            else {
                compute_legendre_polynomialsopt3( truncation_ + 1, nlatsLeg_, lats.data(), legendre_sym_,
                                                  legendre_asym_, legendre_sym_begin_.data(),
                                                  legendre_asym_begin_.data() );
                /*file_leg = fopen( "legendre.bin", "wb" );
                fwrite( legendre_sym_, sizeof( double ), size_sym, file_leg );
                fwrite( legendre_asym_, sizeof( double ), size_asym, file_leg );
                fclose( file_leg );*/
            }
        }

        // precomputations for Fourier transformations:
        if ( useFFT_ ) {
#if ATLAS_HAVE_FFTW
            {
                ATLAS_TRACE( "opt3 precomp FFTW" );
                int num_complex = ( nlonsGlobal_ / 2 ) + 1;
                fft_in_         = fftw_alloc_complex( nlats * num_complex );
                fft_out_        = fftw_alloc_real( nlats * nlonsGlobal_ );
                plan_ = fftw_plan_many_dft_c2r( 1, &nlonsGlobal_, nlats, fft_in_, NULL, 1, num_complex, fft_out_, NULL,
                                                1, nlonsGlobal_, FFTW_ESTIMATE );
            }
                // other FFT implementations should be added with #elif statements
#else
            useFFT_ = false;                             // no FFT implemented => default to dgemm
#endif
        }
        if ( !useFFT_ ) {
            alloc_aligned( fourier_, 2 * ( truncation_ + 1 ) * nlons );
            if ( dgemmMethod1_ ) {
                {
                    ATLAS_TRACE( "opt3 precomp Fourier" );
                    int idx = 0;
                    for ( int jlon = 0; jlon < nlons; jlon++ ) {
                        double factor = 1.;
                        for ( int jm = 0; jm < truncation_ + 1; jm++ ) {
                            if ( jm > 0 ) { factor = 2.; }
                            fourier_[idx++] = +std::cos( jm * lons[jlon] ) * factor;  // real part
                            fourier_[idx++] = -std::sin( jm * lons[jlon] ) * factor;  // imaginary part
                        }
                    }
                }
            }
            else {
                {
                    ATLAS_TRACE( "opt3 precomp Fourier tp" );
                    int idx = 0;
                    for ( int jm = 0; jm < truncation_ + 1; jm++ ) {
                        double factor = 1.;
                        if ( jm > 0 ) { factor = 2.; }
                        for ( int jlon = 0; jlon < nlons; jlon++ ) {
                            fourier_[idx++] = +std::cos( jm * lons[jlon] ) * factor;  // real part
                        }
                        for ( int jlon = 0; jlon < nlons; jlon++ ) {
                            fourier_[idx++] = -std::sin( jm * lons[jlon] ) * factor;  // imaginary part
                        }
                    }
                }
            }
        }
    }
    else {
        // unstructured grid
        if ( unstruct_precomp_ ) {
            ATLAS_TRACE( "opt3 precomp unstructured" );
            std::vector<double> lats( grid_.size() );
            alloc_aligned( legendre_, legendre_size( truncation_ ) * grid_.size() );
            int j( 0 );
            for ( PointXY p : grid_.xy() ) {
                lats[j++] = p.y() * util::Constants::degreesToRadians();
            }
            compute_legendre_polynomials_allopt3( truncation_, grid_.size(), lats.data(), legendre_ );
        }
    }
}  // namespace trans

// --------------------------------------------------------------------------------------------------------------------

TransLocalopt3::TransLocalopt3( const Grid& grid, const long truncation, const eckit::Configuration& config ) :
    TransLocalopt3( Cache(), grid, truncation, config ) {}

// --------------------------------------------------------------------------------------------------------------------

TransLocalopt3::~TransLocalopt3() {
    if ( grid::StructuredGrid( grid_ ) && not grid_.projection() ) {
        free_aligned( legendre_sym_ );
        free_aligned( legendre_asym_ );
        if ( useFFT_ ) {
#if ATLAS_HAVE_FFTW
            fftw_destroy_plan( plan_ );
            fftw_free( fft_in_ );
            fftw_free( fft_out_ );
#endif
        }
        else {
            free_aligned( fourier_ );
        }
    }
    else {
        if ( unstruct_precomp_ ) { free_aligned( legendre_ ); }
    }
}

// --------------------------------------------------------------------------------------------------------------------

void TransLocalopt3::invtrans( const Field& spfield, Field& gpfield, const eckit::Configuration& config ) const {
    NOTIMP;
}

// --------------------------------------------------------------------------------------------------------------------

void TransLocalopt3::invtrans( const FieldSet& spfields, FieldSet& gpfields,
                               const eckit::Configuration& config ) const {
    NOTIMP;
}

// --------------------------------------------------------------------------------------------------------------------

void TransLocalopt3::invtrans_grad( const Field& spfield, Field& gradfield, const eckit::Configuration& config ) const {
    NOTIMP;
}

// --------------------------------------------------------------------------------------------------------------------

void TransLocalopt3::invtrans_grad( const FieldSet& spfields, FieldSet& gradfields,
                                    const eckit::Configuration& config ) const {
    NOTIMP;
}

// --------------------------------------------------------------------------------------------------------------------

void TransLocalopt3::invtrans_vordiv2wind( const Field& spvor, const Field& spdiv, Field& gpwind,
                                           const eckit::Configuration& config ) const {
    NOTIMP;
}

void TransLocalopt3::invtrans( const int nb_scalar_fields, const double scalar_spectra[], double gp_fields[],
                               const eckit::Configuration& config ) const {
    invtrans_uv( truncation_, nb_scalar_fields, 0, scalar_spectra, gp_fields, config );
}

void gp_transposeopt3( const int nb_size, const int nb_fields, const double gp_tmp[], double gp_fields[] ) {
    for ( int jgp = 0; jgp < nb_size; jgp++ ) {
        for ( int jfld = 0; jfld < nb_fields; jfld++ ) {
            gp_fields[jfld * nb_size + jgp] = gp_tmp[jgp * nb_fields + jfld];
        }
    }
}

int TransLocalopt3::posMethod( const int jfld, const int imag, const int jlat, const int jm, const int nb_fields,
                               const int nlats ) const {
    if ( useFFT_ || !dgemmMethod1_ ) { return imag + 2 * ( jm + ( truncation_ + 1 ) * ( jlat + nlats * jfld ) ); }
    else {
        return jfld + nb_fields * ( jlat + nlats * ( imag + 2 * ( jm ) ) );
    };
};

void TransLocalopt3::invtrans_legendreopt3( const int truncation, const int nlats, const int nb_fields,
                                            const double scalar_spectra[], double scl_fourier[],
                                            const eckit::Configuration& config ) const {
    // Legendre transform:
    {
        ATLAS_TRACE( "opt3 Legendre dgemm" );
        for ( int jm = 0; jm <= truncation_; jm++ ) {
            int size_sym  = num_n( truncation_ + 1, jm, true );
            int size_asym = num_n( truncation_ + 1, jm, false );
            int n_imag    = 2;
            if ( jm == 0 ) { n_imag = 1; }
            int size_fourier = nb_fields * n_imag * nlatsLeg_;
            auto posFourier  = [&]( int jfld, int imag, int jlat, int jm, int nlatsH ) {
                return jfld + nb_fields * ( imag + n_imag * ( nlatsLeg_ - nlatsH + jlat ) );
            };
            double* scalar_sym;
            double* scalar_asym;
            double* scl_fourier_sym;
            double* scl_fourier_asym;
            alloc_aligned( scalar_sym, n_imag * nb_fields * size_sym );
            alloc_aligned( scalar_asym, n_imag * nb_fields * size_asym );
            alloc_aligned( scl_fourier_sym, size_fourier );
            alloc_aligned( scl_fourier_asym, size_fourier );
            {
                //ATLAS_TRACE( "opt3 Legendre split" );
                int idx = 0, is = 0, ia = 0, ioff = ( 2 * truncation + 3 - jm ) * jm / 2 * nb_fields * 2;
                // the choice between the following two code lines determines whether
                // total wavenumbers are summed in an ascending or descending order.
                // The trans library in IFS uses descending order because it should
                // be more accurate (higher wavenumbers have smaller contributions).
                // This also needs to be changed when splitting the spectral data in
                // compute_legendre_polynomialsopt3!
                //for ( int jn = jm; jn <= truncation_ + 1; jn++ ) {
                for ( int jn = truncation_ + 1; jn >= jm; jn-- ) {
                    for ( int imag = 0; imag < n_imag; imag++ ) {
                        for ( int jfld = 0; jfld < nb_fields; jfld++ ) {
                            idx = jfld + nb_fields * ( imag + 2 * ( jn - jm ) );
                            if ( jn <= truncation && jm < truncation ) {
                                if ( ( jn - jm ) % 2 == 0 ) { scalar_sym[is++] = scalar_spectra[idx + ioff]; }
                                else {
                                    scalar_asym[ia++] = scalar_spectra[idx + ioff];
                                }
                            }
                            else {
                                if ( ( jn - jm ) % 2 == 0 ) { scalar_sym[is++] = 0.; }
                                else {
                                    scalar_asym[ia++] = 0.;
                                }
                            }
                        }
                    }
                }
                ASSERT( ia == n_imag * nb_fields * size_asym && is == n_imag * nb_fields * size_sym );
            }
            {
                eckit::linalg::Matrix A( scalar_sym, nb_fields * n_imag, size_sym );
                eckit::linalg::Matrix B( legendre_sym_ + legendre_sym_begin_[jm], size_sym, nlatsLeg_ );
                eckit::linalg::Matrix C( scl_fourier_sym, nb_fields * n_imag, nlatsLeg_ );
                eckit::linalg::LinearAlgebra::backend().gemm( A, B, C );
            }
            if ( size_asym > 0 ) {
                eckit::linalg::Matrix A( scalar_asym, nb_fields * n_imag, size_asym );
                eckit::linalg::Matrix B( legendre_asym_ + legendre_asym_begin_[jm], size_asym, nlatsLeg_ );
                eckit::linalg::Matrix C( scl_fourier_asym, nb_fields * n_imag, nlatsLeg_ );
                eckit::linalg::LinearAlgebra::backend().gemm( A, B, C );
            }
            {
                //ATLAS_TRACE( "opt3 merge spheres" );
                // northern hemisphere:
                for ( int jlat = 0; jlat < nlatsNH_; jlat++ ) {
                    for ( int imag = 0; imag < n_imag; imag++ ) {
                        for ( int jfld = 0; jfld < nb_fields; jfld++ ) {
                            int idx = posFourier( jfld, imag, jlat, jm, nlatsNH_ );
                            scl_fourier[posMethod( jfld, imag, jlat, jm, nb_fields, nlats )] =
                                scl_fourier_sym[idx] + scl_fourier_asym[idx];
                        }
                    }
                }
                // southern hemisphere:
                for ( int jlat = 0; jlat < nlatsSH_; jlat++ ) {
                    for ( int imag = 0; imag < n_imag; imag++ ) {
                        for ( int jfld = 0; jfld < nb_fields; jfld++ ) {
                            int idx   = posFourier( jfld, imag, jlat, jm, nlatsSH_ );
                            int jslat = nlats - jlat - 1;
                            scl_fourier[posMethod( jfld, imag, jslat, jm, nb_fields, nlats )] =
                                scl_fourier_sym[idx] - scl_fourier_asym[idx];
                        }
                    }
                }
            }
            free_aligned( scalar_sym );
            free_aligned( scalar_asym );
            free_aligned( scl_fourier_sym );
            free_aligned( scl_fourier_asym );
        }
    }
}

void TransLocalopt3::invtrans_unstructured_precomp( const int truncation, const int nb_fields,
                                                    const int nb_vordiv_fields, const double scalar_spectra[],
                                                    double gp_fields[], const eckit::Configuration& config ) const {
    ATLAS_TRACE( "invtrans_uv unstructured opt3" );
    grid::UnstructuredGrid gu = grid_;
    int nlats                 = grid_.size();
    int size_fourier          = nb_fields * 2;
    double* legendre;
    double* scl_fourier;
    double* scl_fourier_tp;
    double* fouriertp;
    double* gp_opt;
    alloc_aligned( scl_fourier, size_fourier * (truncation)*nlats );
    alloc_aligned( scl_fourier_tp, size_fourier * ( truncation ) );
    alloc_aligned( fouriertp, 2 * ( truncation ) );
    alloc_aligned( gp_opt, nb_fields );

    {
        ATLAS_TRACE( "opt Legendre dgemm" );
        for ( int jm = 0; jm < truncation; jm++ ) {
            int noff = ( 2 * truncation + 3 - jm ) * jm / 2, ns = truncation - jm + 1;
            eckit::linalg::Matrix A( eckit::linalg::Matrix(
                const_cast<double*>( scalar_spectra ) + nb_fields * 2 * noff, nb_fields * 2, ns ) );
            eckit::linalg::Matrix B( legendre_ + noff * nlats, ns, nlats );
            eckit::linalg::Matrix C( scl_fourier + jm * size_fourier * nlats, nb_fields * 2, nlats );
            eckit::linalg::LinearAlgebra::backend().gemm( A, B, C );
        }
    }

    // loop over all points:
    for ( int ip = 0; ip < grid_.size(); ip++ ) {
        PointXY p  = gu.xy( ip );
        double lon = p.x() * util::Constants::degreesToRadians();
        double lat = p.y() * util::Constants::degreesToRadians();
        {
            //ATLAS_TRACE( "opt transposition in Fourier" );
            for ( int jm = 0; jm < truncation; jm++ ) {
                int idx = nb_fields * 2 * ( ip + nlats * jm );
                for ( int imag = 0; imag < 2; imag++ ) {
                    for ( int jfld = 0; jfld < nb_fields; jfld++ ) {
                        int pos_tp = imag + 2 * ( jm + ( truncation ) * ( jfld ) );
                        //int pos  = jfld + nb_fields * ( imag + 2 * ( jm ) );
                        scl_fourier_tp[pos_tp] = scl_fourier[idx++];  // = scl_fourier[pos]
                    }
                }
            }
        }

        // Fourier transformation:
        int idx          = 0;
        fouriertp[idx++] = 1.;  // real part
        fouriertp[idx++] = 0.;  // imaginary part
        for ( int jm = 1; jm < truncation; jm++ ) {
            fouriertp[idx++] = +2. * std::cos( jm * lon );  // real part
            fouriertp[idx++] = -2. * std::sin( jm * lon );  // imaginary part
        }
        {
            //ATLAS_TRACE( "opt Fourier dgemm" );
            eckit::linalg::Matrix A( fouriertp, 1, (truncation)*2 );
            eckit::linalg::Matrix B( scl_fourier_tp, (truncation)*2, nb_fields );
            eckit::linalg::Matrix C( gp_opt, 1, nb_fields );
            eckit::linalg::LinearAlgebra::backend().gemm( A, B, C );
            for ( int j = 0; j < nb_fields; j++ ) {
                gp_fields[ip + j * grid_.size()] = gp_opt[j];
            }
        }
    }
    free_aligned( scl_fourier );
    free_aligned( scl_fourier_tp );
    free_aligned( fouriertp );
    free_aligned( gp_opt );
}

void TransLocalopt3::invtrans_unstructured( const int truncation, const int nb_fields, const int nb_vordiv_fields,
                                            const double scalar_spectra[], double gp_fields[],
                                            const eckit::Configuration& config ) const {
    ATLAS_TRACE( "invtrans_uv unstructured opt3" );
    grid::UnstructuredGrid gu = grid_;
    double* zfn;
    alloc_aligned( zfn, ( truncation + 1 ) * ( truncation + 1 ) );
    compute_zfnopt3( truncation, zfn );
    int size_fourier = nb_fields * 2;
    double* legendre;
    double* scl_fourier;
    double* scl_fourier_tp;
    double* fouriertp;
    double* gp_opt;
    alloc_aligned( legendre, legendre_size( truncation + 1 ) );
    alloc_aligned( scl_fourier, size_fourier * ( truncation + 1 ) );
    alloc_aligned( scl_fourier_tp, size_fourier * ( truncation + 1 ) );
    alloc_aligned( fouriertp, 2 * ( truncation + 1 ) );
    alloc_aligned( gp_opt, nb_fields );

    // loop over all points:
    for ( int ip = 0; ip < grid_.size(); ip++ ) {
        PointXY p  = gu.xy( ip );
        double lon = p.x() * util::Constants::degreesToRadians();
        double lat = p.y() * util::Constants::degreesToRadians();
        compute_legendre_polynomials_latopt3( truncation, lat, legendre, zfn );
        // Legendre transform:
        {
            //ATLAS_TRACE( "opt Legendre dgemm" );
            for ( int jm = 0; jm <= truncation; jm++ ) {
                int noff = ( 2 * truncation + 3 - jm ) * jm / 2, ns = truncation - jm + 1;
                eckit::linalg::Matrix A( eckit::linalg::Matrix(
                    const_cast<double*>( scalar_spectra ) + nb_fields * 2 * noff, nb_fields * 2, ns ) );
                eckit::linalg::Matrix B( legendre + noff, ns, 1 );
                eckit::linalg::Matrix C( scl_fourier + jm * size_fourier, nb_fields * 2, 1 );
                eckit::linalg::LinearAlgebra::backend().gemm( A, B, C );
            }
        }
        {
            //ATLAS_TRACE( "opt transposition in Fourier" );
            int idx = 0;
            for ( int jm = 0; jm < truncation + 1; jm++ ) {
                for ( int imag = 0; imag < 2; imag++ ) {
                    for ( int jfld = 0; jfld < nb_fields; jfld++ ) {
                        int pos_tp = imag + 2 * ( jm + ( truncation + 1 ) * ( jfld ) );
                        //int pos  = jfld + nb_fields * ( imag + 2 * ( jm ) );
                        scl_fourier_tp[pos_tp] = scl_fourier[idx++];  // = scl_fourier[pos]
                    }
                }
            }
        }

        // Fourier transformation:
        int idx          = 0;
        fouriertp[idx++] = 1.;  // real part
        fouriertp[idx++] = 0.;  // imaginary part
        for ( int jm = 1; jm < truncation + 1; jm++ ) {
            fouriertp[idx++] = +2. * std::cos( jm * lon );  // real part
            fouriertp[idx++] = -2. * std::sin( jm * lon );  // imaginary part
        }
        {
            //ATLAS_TRACE( "opt Fourier dgemm" );
            eckit::linalg::Matrix A( fouriertp, 1, ( truncation + 1 ) * 2 );
            eckit::linalg::Matrix B( scl_fourier_tp, ( truncation + 1 ) * 2, nb_fields );
            eckit::linalg::Matrix C( gp_opt, 1, nb_fields );
            eckit::linalg::LinearAlgebra::backend().gemm( A, B, C );
            for ( int j = 0; j < nb_fields; j++ ) {
                gp_fields[ip + j * grid_.size()] = gp_opt[j];
            }
        }
    }
    free_aligned( legendre );
    free_aligned( scl_fourier );
    free_aligned( scl_fourier_tp );
    free_aligned( fouriertp );
    free_aligned( gp_opt );
}

//-----------------------------------------------------------------------------
// Routine to compute the spectral transform by using a localopt3 Fourier transformation
// for a grid (same latitude for all longitudes, allows to compute Legendre functions
// once for all longitudes). U and v components are divided by cos(latitude) for
// nb_vordiv_fields > 0.
//
// Legendre polynomials are computed up to truncation_+1 to be accurate for vorticity and
// divergence computation. The parameter truncation is the truncation used in storing the
// spectral data scalar_spectra and can be different from truncation_. If truncation is
// larger than truncation_+1 the transform will behave as if the spectral data was truncated
// to truncation_+1.
//
// Author:
// Andreas Mueller *ECMWF*
//
void TransLocalopt3::invtrans_uv( const int truncation, const int nb_scalar_fields, const int nb_vordiv_fields,
                                  const double scalar_spectra[], double gp_fields[],
                                  const eckit::Configuration& config ) const {
    if ( nb_scalar_fields > 0 ) {
        int nb_fields = nb_scalar_fields;

        // Transform
        if ( grid::StructuredGrid g = grid_ ) {
            ATLAS_TRACE( "invtrans_uv structured opt3" );
            int nlats            = g.ny();
            int nlons            = g.nxmax();
            int size_fourier_max = nb_fields * 2 * nlats;
            double* scl_fourier;
            alloc_aligned( scl_fourier, size_fourier_max * ( truncation_ + 1 ) );
            invtrans_legendreopt3( truncation, nlats, nb_scalar_fields, scalar_spectra, scl_fourier, config );

            // Fourier transformation:
            if ( useFFT_ ) {
#if ATLAS_HAVE_FFTW
                {
                    int num_complex = ( nlonsGlobal_ / 2 ) + 1;
                    {
                        ATLAS_TRACE( "opt3 FFTW" );
                        for ( int jfld = 0; jfld < nb_fields; jfld++ ) {
                            int idx = 0;
                            for ( int jlat = 0; jlat < nlats; jlat++ ) {
                                fft_in_[idx++][0] = scl_fourier[posMethod( jfld, 0, jlat, 0, nb_fields, nlats )];
                                for ( int jm = 1; jm < num_complex; jm++, idx++ ) {
                                    for ( int imag = 0; imag < 2; imag++ ) {
                                        if ( jm <= truncation_ ) {
                                            fft_in_[idx][imag] =
                                                scl_fourier[posMethod( jfld, imag, jlat, jm, nb_fields, nlats )];
                                        }
                                        else {
                                            fft_in_[idx][imag] = 0.;
                                        }
                                    }
                                }
                            }
                            fftw_execute_dft_c2r( plan_, fft_in_, fft_out_ );
                            for ( int jlat = 0; jlat < nlats; jlat++ ) {
                                for ( int jlon = 0; jlon < nlons; jlon++ ) {
                                    int j = jlon + jlonMin_;
                                    if ( j >= nlonsGlobal_ ) { j -= nlonsGlobal_; }
                                    gp_fields[jlon + nlons * ( jlat + nlats * jfld )] =
                                        fft_out_[j + nlonsGlobal_ * jlat];
                                }
                            }
                        }
                    }
                }
#endif
            }
            else {
                if ( dgemmMethod1_ ) {
                    // dgemm-method 1
                    // should be faster for small domains or large truncation
                    double* gp_opt3;
                    alloc_aligned( gp_opt3, nb_fields * grid_.size() );
                    {
                        ATLAS_TRACE( "opt3 Fourier dgemm method 1" );
                        eckit::linalg::Matrix A( scl_fourier, nb_fields * nlats, ( truncation_ + 1 ) * 2 );
                        eckit::linalg::Matrix B( fourier_, ( truncation_ + 1 ) * 2, nlons );
                        eckit::linalg::Matrix C( gp_opt3, nb_fields * nlats, nlons );
                        eckit::linalg::LinearAlgebra::backend().gemm( A, B, C );
                    }

                    // Transposition in grid point space:
                    {
                        ATLAS_TRACE( "opt3 transposition in gp-space" );
                        int idx = 0;
                        for ( int jlon = 0; jlon < nlons; jlon++ ) {
                            for ( int jlat = 0; jlat < nlats; jlat++ ) {
                                for ( int jfld = 0; jfld < nb_fields; jfld++ ) {
                                    int pos_tp = jlon + nlons * ( jlat + nlats * ( jfld ) );
                                    //int pos  = jfld + nb_fields * ( jlat + nlats * ( jlon ) );
                                    gp_fields[pos_tp] = gp_opt3[idx++];  // = gp_opt3[pos]
                                }
                            }
                        }
                    }
                    free_aligned( gp_opt3 );
                }
                else {
                    // dgemm-method 2
                    {
                        ATLAS_TRACE( "opt3 Fourier dgemm method 2" );
                        eckit::linalg::Matrix A( fourier_, nlons, ( truncation_ + 1 ) * 2 );
                        eckit::linalg::Matrix B( scl_fourier, ( truncation_ + 1 ) * 2, nb_fields * nlats );
                        eckit::linalg::Matrix C( gp_fields, nlons, nb_fields * nlats );
                        eckit::linalg::LinearAlgebra::backend().gemm( A, B, C );
                    }
                }
            }  // namespace trans
            // Computing u,v from U,V:
            {
                if ( nb_vordiv_fields > 0 ) {
                    ATLAS_TRACE( "opt3 u,v from U,V" );
                    std::vector<double> coslats( nlats );
                    for ( size_t j = 0; j < nlats; ++j ) {
                        coslats[j] = std::cos( g.y( j ) * util::Constants::degreesToRadians() );
                    }
                    int idx = 0;
                    for ( int jfld = 0; jfld < nb_fields; jfld++ ) {
                        for ( int jlat = 0; jlat < g.ny(); jlat++ ) {
                            for ( int jlon = 0; jlon < g.nxmax(); jlon++ ) {
                                gp_fields[idx] /= coslats[jlat];
                                idx++;
                            }
                        }
                    }
                }
            }
            free_aligned( scl_fourier );
        }  // namespace atlas
        else {
            if ( unstruct_precomp_ ) {
                invtrans_unstructured_precomp( truncation, nb_scalar_fields, nb_vordiv_fields, scalar_spectra,
                                               gp_fields, config );
            }
            else {
                invtrans_unstructured( truncation, nb_scalar_fields, nb_vordiv_fields, scalar_spectra, gp_fields,
                                       config );
            }
        }

    }  // namespace trans
}  // namespace atlas

// --------------------------------------------------------------------------------------------------------------------

void TransLocalopt3::invtrans( const int nb_vordiv_fields, const double vorticity_spectra[],
                               const double divergence_spectra[], double gp_fields[],
                               const eckit::Configuration& config ) const {
    invtrans( 0, nullptr, nb_vordiv_fields, vorticity_spectra, divergence_spectra, gp_fields, config );
}

void extend_truncationopt3( const int old_truncation, const int nb_fields, const double old_spectra[],
                            double new_spectra[] ) {
    int k = 0, k_old = 0;
    for ( int m = 0; m <= old_truncation + 1; m++ ) {             // zonal wavenumber
        for ( int n = m; n <= old_truncation + 1; n++ ) {         // total wavenumber
            for ( int imag = 0; imag < 2; imag++ ) {              // imaginary/real part
                for ( int jfld = 0; jfld < nb_fields; jfld++ ) {  // field
                    if ( m == old_truncation + 1 || n == old_truncation + 1 ) { new_spectra[k++] = 0.; }
                    else {
                        new_spectra[k++] = old_spectra[k_old++];
                    }
                }
            }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

void TransLocalopt3::invtrans( const int nb_scalar_fields, const double scalar_spectra[], const int nb_vordiv_fields,
                               const double vorticity_spectra[], const double divergence_spectra[], double gp_fields[],
                               const eckit::Configuration& config ) const {
    ATLAS_TRACE( "TransLocalopt3::invtrans" );
    int nb_gp              = grid_.size();
    int nb_vordiv_spec_ext = 2 * legendre_size( truncation_ + 1 ) * nb_vordiv_fields;
    if ( nb_vordiv_fields > 0 ) {
        std::vector<double> vorticity_spectra_extended( nb_vordiv_spec_ext, 0. );
        std::vector<double> divergence_spectra_extended( nb_vordiv_spec_ext, 0. );
        std::vector<double> U_ext( nb_vordiv_spec_ext, 0. );
        std::vector<double> V_ext( nb_vordiv_spec_ext, 0. );

        {
            ATLAS_TRACE( "opt3 extend vordiv" );
            // increase truncation in vorticity_spectra and divergence_spectra:
            extend_truncationopt3( truncation_, nb_vordiv_fields, vorticity_spectra,
                                   vorticity_spectra_extended.data() );
            extend_truncationopt3( truncation_, nb_vordiv_fields, divergence_spectra,
                                   divergence_spectra_extended.data() );
        }

        {
            ATLAS_TRACE( "vordiv to UV opt3" );
            // call vd2uv to compute u and v in spectral space
            trans::VorDivToUV vordiv_to_UV_ext( truncation_ + 1, option::type( "localopt3" ) );
            vordiv_to_UV_ext.execute( nb_vordiv_spec_ext, nb_vordiv_fields, vorticity_spectra_extended.data(),
                                      divergence_spectra_extended.data(), U_ext.data(), V_ext.data() );
        }

        // perform spectral transform to compute all fields in grid point space
        invtrans_uv( truncation_ + 1, nb_vordiv_fields, nb_vordiv_fields, U_ext.data(), gp_fields, config );
        invtrans_uv( truncation_ + 1, nb_vordiv_fields, nb_vordiv_fields, V_ext.data(),
                     gp_fields + nb_gp * nb_vordiv_fields, config );
    }
    if ( nb_scalar_fields > 0 ) {
        invtrans_uv( truncation_, nb_scalar_fields, 0, scalar_spectra, gp_fields + 2 * nb_gp * nb_vordiv_fields,
                     config );
    }
}

// --------------------------------------------------------------------------------------------------------------------

void TransLocalopt3::dirtrans( const Field& gpfield, Field& spfield, const eckit::Configuration& config ) const {
    NOTIMP;
    // Not implemented and not planned.
    // Use the TransIFS implementation instead.
}

// --------------------------------------------------------------------------------------------------------------------

void TransLocalopt3::dirtrans( const FieldSet& gpfields, FieldSet& spfields,
                               const eckit::Configuration& config ) const {
    NOTIMP;
    // Not implemented and not planned.
    // Use the TransIFS implementation instead.
}

// --------------------------------------------------------------------------------------------------------------------

void TransLocalopt3::dirtrans_wind2vordiv( const Field& gpwind, Field& spvor, Field& spdiv,
                                           const eckit::Configuration& config ) const {
    NOTIMP;
    // Not implemented and not planned.
    // Use the TransIFS implementation instead.
}

// --------------------------------------------------------------------------------------------------------------------

void TransLocalopt3::dirtrans( const int nb_fields, const double scalar_fields[], double scalar_spectra[],
                               const eckit::Configuration& ) const {
    NOTIMP;
    // Not implemented and not planned.
    // Use the TransIFS implementation instead.
}

// --------------------------------------------------------------------------------------------------------------------

void TransLocalopt3::dirtrans( const int nb_fields, const double wind_fields[], double vorticity_spectra[],
                               double divergence_spectra[], const eckit::Configuration& ) const {
    NOTIMP;
    // Not implemented and not planned.
    // Use the TransIFS implementation instead.
}

// --------------------------------------------------------------------------------------------------------------------

}  // namespace trans
}  // namespace atlas
