// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include "Common/RegistLibrary.hpp"

#include "LibUFEM.hpp"

namespace CF {
namespace UFEM {

CF::Common::RegistLibrary<LibUFEM> libUFEM;

////////////////////////////////////////////////////////////////////////////////

void LibUFEM::initiate()
{
}

void LibUFEM::terminate()
{
}

////////////////////////////////////////////////////////////////////////////////

} // UFEM
} // CF
