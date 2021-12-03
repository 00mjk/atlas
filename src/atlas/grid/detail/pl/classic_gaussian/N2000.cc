/*
 * (C) Copyright 2013 ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation
 * nor does it submit to any jurisdiction.
 */

// TL3999

#include "N.h"

namespace atlas {
namespace grid {
namespace detail {
namespace pl {
namespace classic_gaussian {

DEFINE_POINTS_PER_LATITUDE(
    2000,
    LIST(18, 25, 32, 40, 45, 50, 60, 60, 72, 72, 75, 81, 90, 96, 96, 108, 108, 120, 120, 120, 125, 135, 135, 144, 144,
         150, 160, 160, 180, 180, 180, 180, 192, 192, 192, 200, 216, 216, 216, 225, 240, 240, 243, 250, 256, 270, 270,
         288, 300, 300, 320, 320, 320, 360, 360, 360, 360, 360, 360, 375, 375, 384, 400, 400, 400, 405, 432, 432, 432,
         432, 450, 450, 450, 480, 480, 480, 480, 486, 500, 500, 500, 512, 512, 540, 540, 540, 540, 576, 576, 576, 576,
         576, 600, 600, 600, 600, 625, 625, 625, 625, 640, 640, 640, 648, 675, 675, 675, 675, 720, 720, 720, 720, 720,
         720, 720, 729, 750, 750, 750, 750, 768, 768, 768, 800, 800, 800, 800, 800, 810, 864, 864, 864, 864, 864, 864,
         864, 864, 864, 900, 900, 900, 900, 900, 900, 960, 960, 960, 960, 960, 960, 960, 960, 960, 972, 972, 1000, 1000,
         1000, 1000, 1000, 1024, 1024, 1024, 1080, 1080, 1080, 1080, 1080, 1080, 1080, 1080, 1080, 1125, 1125, 1125,
         1125, 1125, 1125, 1125, 1152, 1152, 1152, 1152, 1152, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1215, 1215,
         1215, 1250, 1250, 1250, 1250, 1250, 1280, 1280, 1280, 1280, 1280, 1296, 1296, 1296, 1350, 1350, 1350, 1350,
         1350, 1350, 1350, 1350, 1440, 1440, 1440, 1440, 1440, 1440, 1440, 1440, 1440, 1440, 1440, 1440, 1440, 1440,
         1440, 1458, 1458, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1536, 1536, 1536, 1536, 1536, 1536, 1600, 1600,
         1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1620, 1620, 1620, 1728, 1728, 1728, 1728, 1728, 1728,
         1728, 1728, 1728, 1728, 1728, 1728, 1728, 1728, 1728, 1728, 1728, 1800, 1800, 1800, 1800, 1800, 1800, 1800,
         1800, 1800, 1800, 1800, 1800, 1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, 1875, 1920,
         1920, 1920, 1920, 1920, 1920, 1920, 1944, 1944, 1944, 1944, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000,
         2000, 2025, 2025, 2025, 2025, 2048, 2048, 2048, 2048, 2160, 2160, 2160, 2160, 2160, 2160, 2160, 2160, 2160,
         2160, 2160, 2160, 2160, 2160, 2160, 2160, 2160, 2160, 2187, 2187, 2187, 2187, 2187, 2250, 2250, 2250, 2250,
         2250, 2250, 2250, 2250, 2250, 2250, 2304, 2304, 2304, 2304, 2304, 2304, 2304, 2304, 2304, 2400, 2400, 2400,
         2400, 2400, 2400, 2400, 2400, 2400, 2400, 2400, 2400, 2400, 2400, 2400, 2400, 2430, 2430, 2430, 2430, 2430,
         2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500, 2560, 2560, 2560, 2560, 2560, 2560,
         2560, 2560, 2560, 2560, 2592, 2592, 2592, 2592, 2592, 2700, 2700, 2700, 2700, 2700, 2700, 2700, 2700, 2700,
         2700, 2700, 2700, 2700, 2700, 2700, 2700, 2700, 2700, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880,
         2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880,
         2880, 2880, 2880, 2916, 2916, 2916, 2916, 2916, 2916, 3000, 3000, 3000, 3000, 3000, 3000, 3000, 3000, 3000,
         3000, 3000, 3000, 3000, 3000, 3000, 3072, 3072, 3072, 3072, 3072, 3072, 3072, 3072, 3072, 3072, 3072, 3072,
         3125, 3125, 3125, 3125, 3125, 3125, 3125, 3125, 3125, 3200, 3200, 3200, 3200, 3200, 3200, 3200, 3200, 3200,
         3200, 3200, 3200, 3200, 3240, 3240, 3240, 3240, 3240, 3240, 3240, 3375, 3375, 3375, 3375, 3375, 3375, 3375,
         3375, 3375, 3375, 3375, 3375, 3375, 3375, 3375, 3375, 3375, 3375, 3375, 3375, 3375, 3375, 3375, 3456, 3456,
         3456, 3456, 3456, 3456, 3456, 3456, 3456, 3456, 3456, 3456, 3456, 3456, 3456, 3600, 3600, 3600, 3600, 3600,
         3600, 3600, 3600, 3600, 3600, 3600, 3600, 3600, 3600, 3600, 3600, 3600, 3600, 3600, 3600, 3600, 3600, 3600,
         3600, 3600, 3645, 3645, 3645, 3645, 3645, 3645, 3645, 3645, 3750, 3750, 3750, 3750, 3750, 3750, 3750, 3750,
         3750, 3750, 3750, 3750, 3750, 3750, 3750, 3750, 3750, 3750, 3750, 3840, 3840, 3840, 3840, 3840, 3840, 3840,
         3840, 3840, 3840, 3840, 3840, 3840, 3840, 3840, 3840, 3888, 3888, 3888, 3888, 3888, 3888, 3888, 3888, 3888,
         4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000,
         4000, 4000, 4050, 4050, 4050, 4050, 4050, 4050, 4050, 4050, 4050, 4096, 4096, 4096, 4096, 4096, 4096, 4096,
         4096, 4096, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320,
         4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320, 4320,
         4320, 4320, 4320, 4320, 4320, 4320, 4320, 4374, 4374, 4374, 4374, 4374, 4374, 4374, 4374, 4374, 4374, 4500,
         4500, 4500, 4500, 4500, 4500, 4500, 4500, 4500, 4500, 4500, 4500, 4500, 4500, 4500, 4500, 4500, 4500, 4500,
         4500, 4500, 4500, 4500, 4500, 4608, 4608, 4608, 4608, 4608, 4608, 4608, 4608, 4608, 4608, 4608, 4608, 4608,
         4608, 4608, 4608, 4608, 4608, 4608, 4608, 4608, 4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800,
         4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800,
         4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800, 4800, 4860, 4860, 4860, 4860, 4860, 4860, 4860, 4860,
         4860, 4860, 4860, 4860, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000,
         5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5000, 5120, 5120, 5120, 5120,
         5120, 5120, 5120, 5120, 5120, 5120, 5120, 5120, 5120, 5120, 5120, 5120, 5120, 5120, 5120, 5120, 5120, 5120,
         5120, 5120, 5184, 5184, 5184, 5184, 5184, 5184, 5184, 5184, 5184, 5184, 5184, 5184, 5184, 5400, 5400, 5400,
         5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400,
         5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400, 5400,
         5400, 5400, 5400, 5400, 5400, 5400, 5400, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625,
         5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625,
         5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625, 5625,
         5625, 5625, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760,
         5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5760, 5832, 5832, 5832,
         5832, 5832, 5832, 5832, 5832, 5832, 5832, 5832, 5832, 5832, 5832, 5832, 5832, 6000, 6000, 6000, 6000, 6000,
         6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000,
         6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6000, 6075, 6075,
         6075, 6075, 6075, 6075, 6075, 6075, 6075, 6075, 6075, 6075, 6075, 6075, 6075, 6075, 6075, 6075, 6075, 6144,
         6144, 6144, 6144, 6144, 6144, 6144, 6144, 6144, 6144, 6144, 6144, 6144, 6144, 6144, 6144, 6250, 6250, 6250,
         6250, 6250, 6250, 6250, 6250, 6250, 6250, 6250, 6250, 6250, 6250, 6250, 6250, 6250, 6250, 6250, 6250, 6250,
         6250, 6250, 6250, 6250, 6250, 6250, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400,
         6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400,
         6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6400, 6480, 6480, 6480, 6480, 6480, 6480, 6480, 6480, 6480,
         6480, 6480, 6480, 6480, 6480, 6480, 6480, 6480, 6480, 6480, 6480, 6480, 6561, 6561, 6561, 6561, 6561, 6561,
         6561, 6561, 6561, 6561, 6561, 6561, 6561, 6561, 6561, 6561, 6561, 6561, 6561, 6561, 6561, 6561, 6750, 6750,
         6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750,
         6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750,
         6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6750, 6912, 6912,
         6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912,
         6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912,
         6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 6912, 7200, 7200, 7200, 7200, 7200, 7200, 7200,
         7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200,
         7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200,
         7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200,
         7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200,
         7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7200, 7290,
         7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290,
         7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7290, 7500, 7500, 7500, 7500,
         7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500,
         7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500,
         7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500,
         7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500,
         7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7500, 7680, 7680, 7680, 7680, 7680, 7680, 7680,
         7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680,
         7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680,
         7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680,
         7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680,
         7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7680, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776,
         7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776,
         7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776,
         7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 7776, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000, 8000,
         8000, 8000, 8000, 8000, 8000, 8000, 8000))

}  // namespace classic_gaussian
}  // namespace pl
}  // namespace detail
}  // namespace grid
}  // namespace atlas
