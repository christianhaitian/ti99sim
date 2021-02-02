//----------------------------------------------------------------------------
//
// File:        device-support.hpp
// Date:        29-Feb-2020
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2020 Marc Rousseau, All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
//
// Revision History:
//
//----------------------------------------------------------------------------

#ifndef DEVICE_SUPPORT_HPP_
#define DEVICE_SUPPORT_HPP_

#include <functional>
#include <string>
#include "idevice.hpp"

struct iComputer;

cRefPtr<iDevice> LoadDevice( const std::string &description, const std::string &folder );
void LoadDevices( iComputer *computer, std::function<bool(const char *)> filter );

#endif
