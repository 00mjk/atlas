! (C) Copyright 2013-2015 ECMWF.

!------------------------------------------------------------------------------

TYPE, extends(atlas_object) :: atlas_JSON
  character(kind=c_char,len=1), allocatable, private :: string(:)
contains
  procedure :: str => atlas_JSON__str
END TYPE atlas_JSON

interface atlas_JSON
  module procedure atlas_JSON__ctor_str
end interface

!------------------------------------------------------------------------------
