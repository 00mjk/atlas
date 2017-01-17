/*
 * (C) Copyright 1996-2016 ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */
#pragma once

#include "atlas/internals/atlas_defines.h"
#ifdef ATLAS_HAVE_GRIDTOOLS_STORAGE
#include "atlas/array/gridtools/GridToolsStorageView.h"
#else

//------------------------------------------------------------------------------------------------------

namespace atlas {
namespace array {

//------------------------------------------------------------------------------------------------------

template< typename Value >
class StorageView {
  typedef void* storage_view_t;
public:
    StorageView(storage_view_t storage_view, size_t size, bool contiguous = true) :
        native_storage_view_(storage_view),
        size_(size),
        contiguous_(contiguous)
    {}

    Value* data() { return (DATA_TYPE*) native_storage_view_; }

    size_t size() { return size_; }

    bool contiguous() const { return contiguous_; }

    void assign(const DATA_TYPE& value) {
        ASSERT( contiguous() );
        DATA_TYPE* raw_data = data();
        for( size_t j=0; j<size_; ++j ) {
            raw_data[j] = value;
        }
    }

private:
    storage_view_t native_storage_view_;
    size_t size_;
    bool contiguous_;
};

//------------------------------------------------------------------------------------------------------

} // namespace array
} // namespace atlas
#endif
