/***************************************************************************
 *   Copyright (c) 2015 FreeCAD Developers                                 *
 *   Authors: Michael Hindley <hindlemp@eskom.co.za>                       *
 *            Ruan Olwagen <olwager@eskom.co.za>                           *
 *            Oswald van Ginkel <vginkeo@eskom.co.za>                      *
 *   Based on Force constraint by Jan Rheinländer                          *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"

#include "FemConstraintInitialTemperature.h"


using namespace Fem;

PROPERTY_SOURCE(Fem::ConstraintInitialTemperature, Fem::Constraint)

ConstraintInitialTemperature::ConstraintInitialTemperature()
{
    ADD_PROPERTY(initialTemperature, (300.0));
}

App::DocumentObjectExecReturn* ConstraintInitialTemperature::execute()
{
    return Constraint::execute();
}

const char* ConstraintInitialTemperature::getViewProviderName() const
{
    return "FemGui::ViewProviderFemConstraintInitialTemperature";
}

void ConstraintInitialTemperature::handleChangedPropertyType(Base::XMLReader& reader,
                                                             const char* TypeName,
                                                             App::Property* prop)
{
    // property initialTemperature had App::PropertyFloat, was changed to App::PropertyTemperature
    if (prop == &initialTemperature && strcmp(TypeName, "App::PropertyFloat") == 0) {
        App::PropertyFloat initialTemperatureProperty;
        // restore the PropertyFloat to be able to set its value
        initialTemperatureProperty.Restore(reader);
        initialTemperature.setValue(initialTemperatureProperty.getValue());
    }
    else {
        Constraint::handleChangedPropertyType(reader, TypeName, prop);
    }
}

void ConstraintInitialTemperature::onChanged(const App::Property* prop)
{
    Constraint::onChanged(prop);
}
