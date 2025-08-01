# SPDX-License-Identifier: LGPL-2.1-or-later

# ***************************************************************************
# *                                                                         *
# *   Copyright (c) 2013 Yorik van Havre <yorik@uncreated.net>              *
# *                                                                         *
# *   This file is part of FreeCAD.                                         *
# *                                                                         *
# *   FreeCAD is free software: you can redistribute it and/or modify it    *
# *   under the terms of the GNU Lesser General Public License as           *
# *   published by the Free Software Foundation, either version 2.1 of the  *
# *   License, or (at your option) any later version.                       *
# *                                                                         *
# *   FreeCAD is distributed in the hope that it will be useful, but        *
# *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
# *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU      *
# *   Lesser General Public License for more details.                       *
# *                                                                         *
# *   You should have received a copy of the GNU Lesser General Public      *
# *   License along with FreeCAD. If not, see                               *
# *   <https://www.gnu.org/licenses/>.                                      *
# *                                                                         *
# ***************************************************************************

__title__= "FreeCAD Arch Space"
__author__ = "Yorik van Havre"
__url__ = "https://www.freecad.org"

## @package ArchSpace
#  \ingroup ARCH
#  \brief The Space object and tools
#
#  This module provides tools to build Space objects.
#  Spaces define an open volume inside or outside a
#  building, ie. a room.

import re

import FreeCAD
import ArchComponent
import ArchCommands
import Draft

from draftutils import params

if FreeCAD.GuiUp:
    from PySide import QtCore, QtGui
    from PySide.QtCore import QT_TRANSLATE_NOOP
    import FreeCADGui
    from draftutils.translate import translate
else:
    # \cond
    def translate(ctxt,txt):
        return txt
    def QT_TRANSLATE_NOOP(ctxt,txt):
        return txt
    # \endcond

SpaceTypes = [
"Undefined",
"Exterior",
"Exterior - Terrace",
"Office",
"Office - Enclosed",
"Office - Open Plan",
"Conference / Meeting / Multipurpose",
"Classroom / Lecture / Training For Penitentiary",
"Lobby",
"Lobby - For Hotel",
"Lobby - For Performing Arts Theater",
"Lobby - For Motion Picture Theater",
"Audience/Seating Area",
"Audience/Seating Area - For Gymnasium",
"Audience/Seating Area - For Exercise Center",
"Audience/Seating Area - For Convention Center",
"Audience/Seating Area - For Penitentiary",
"Audience/Seating Area - For Religious Buildings",
"Audience/Seating Area - For Sports Arena",
"Audience/Seating Area - For Performing Arts Theater",
"Audience/Seating Area - For Motion Picture Theater",
"Audience/Seating Area - For Transportation",
"Atrium",
"Atrium - First Three Floors",
"Atrium - Each Additional Floor",
"Lounge / Recreation",
"Lounge / Recreation - For Hospital",
"Dining Area",
"Dining Area - For Penitentiary",
"Dining Area - For Hotel",
"Dining Area - For Motel",
"Dining Area - For Bar Lounge/Leisure Dining",
"Dining Area - For Family Dining",
"Food Preparation",
"Laboratory",
"Restrooms",
"Dressing / Locker / Fitting",
"Room",
"Corridor / Transition",
"Corridor / Transition - For Hospital",
"Corridor / Transition - For Manufacturing Facility",
"Stairs",
"Active Storage",
"Active Storage - For Hospital",
"Inactive Storage",
"Inactive Storage - For Museum",
"Electrical / Mechanical",
"Gymnasium / Exercise Center",
"Gymnasium / Exercise Center - Playing Area",
"Gymnasium / Exercise Center - Exercise Area",
"Courthouse / Police Station / Penitentiary",
"Courthouse / Police Station / Penitentiary - Courtroom",
"Courthouse / Police Station / Penitentiary - Confinement Cells",
"Courthouse / Police Station / Penitentiary - Judges' Chambers",
"Fire Stations",
"Fire Stations - Engine Room",
"Fire Stations - Sleeping Quarters",
"Post Office - Sorting Area",
"Convention Center - Exhibit Space",
"Library",
"Library - Card File and Cataloging",
"Library - Stacks",
"Library - Reading Area",
"Hospital",
"Hospital - Emergency",
"Hospital - Recovery",
"Hospital - Nurses' Station",
"Hospital - Exam / Treatment",
"Hospital - Pharmacy",
"Hospital - Patient Room",
"Hospital - Operating Room",
"Hospital - Nursery",
"Hospital - Medical Supply",
"Hospital - Physical Therapy",
"Hospital - Radiology",
"Hospital - Laundry-Washing",
"Automotive - Service / Repair",
"Manufacturing",
"Manufacturing - Low Bay (< 7.5m Floor to Ceiling Height)",
"Manufacturing - High Bay (> 7.5m Floor to Ceiling Height)",
"Manufacturing - Detailed Manufacturing",
"Manufacturing - Equipment Room",
"Manufacturing - Control Room",
"Hotel / Motel Guest Rooms",
"Dormitory - Living Quarters",
"Museum",
"Museum - General Exhibition",
"Museum - Restoration",
"Bank / Office - Banking Activity Area",
"Workshop",
"Sales Area",
"Religious Buildings",
"Religious Buildings - Worship Pulpit, Choir",
"Religious Buildings - Fellowship Hall",
"Retail",
"Retail - Sales Area",
"Retail - Mall Concourse",
"Sports Arena",
"Sports Arena - Ring Sports Area",
"Sports Arena - Court Sports Area",
"Sports Arena - Indoor Playing Field Area",
"Warehouse",
"Warehouse - Fine Material Storage",
"Warehouse - Medium / Bulky Material Storage",
"Parking Garage - Garage Area",
"Transportation",
"Transportation - Airport / Concourse",
"Transportation - Air / Train / Bus - Baggage Area",
"Transportation - Terminal - Ticket Counter"
]

ConditioningTypes = [
"Unconditioned",
"Heated",
"Cooled",
"HeatedAndCooled",
"Vented",
"NaturallyVentedOnly"
]

AreaCalculationType = [
    "XY-plane projection",
    "At Center of Mass"
]


class _Space(ArchComponent.Component):

    "A space object"

    def __init__(self,obj):

        ArchComponent.Component.__init__(self,obj)
        self.Type = "Space"
        self.setProperties(obj)
        obj.IfcType = "Space"
        obj.CompositionType = "ELEMENT"

    def setProperties(self,obj):

        pl = obj.PropertiesList
        if not "Boundaries" in pl:
            obj.addProperty("App::PropertyLinkSubList","Boundaries",    "Space",QT_TRANSLATE_NOOP("App::Property","The objects that make the boundaries of this space object"), locked=True)
        if not "Area" in pl:
            obj.addProperty("App::PropertyArea",       "Area",          "Space",QT_TRANSLATE_NOOP("App::Property","Identical to Horizontal Area"), locked=True)
        if not "FinishFloor" in pl:
            obj.addProperty("App::PropertyString",     "FinishFloor",   "Space",QT_TRANSLATE_NOOP("App::Property","The finishing of the floor of this space"), locked=True)
        if not "FinishWalls" in pl:
            obj.addProperty("App::PropertyString",     "FinishWalls",   "Space",QT_TRANSLATE_NOOP("App::Property","The finishing of the walls of this space"), locked=True)
        if not "FinishCeiling" in pl:
            obj.addProperty("App::PropertyString",     "FinishCeiling", "Space",QT_TRANSLATE_NOOP("App::Property","The finishing of the ceiling of this space"), locked=True)
        if not "Group" in pl:
            obj.addProperty("App::PropertyLinkList",   "Group",         "Space",QT_TRANSLATE_NOOP("App::Property","Objects that are included inside this space, such as furniture"), locked=True)
        if not "SpaceType" in pl:
            obj.addProperty("App::PropertyEnumeration","SpaceType",     "Space",QT_TRANSLATE_NOOP("App::Property","The type of this space"), locked=True)
            obj.SpaceType = SpaceTypes
        if not "FloorThickness" in pl:
            obj.addProperty("App::PropertyLength",     "FloorThickness","Space",QT_TRANSLATE_NOOP("App::Property","The thickness of the floor finish"), locked=True)
        if not "NumberOfPeople" in pl:
            obj.addProperty("App::PropertyInteger",    "NumberOfPeople","Space",QT_TRANSLATE_NOOP("App::Property","The number of people who typically occupy this space"), locked=True)
        if not "LightingPower" in pl:
            obj.addProperty("App::PropertyFloat",      "LightingPower", "Space",QT_TRANSLATE_NOOP("App::Property","The electric power needed to light this space in Watts"), locked=True)
        if not "EquipmentPower" in pl:
            obj.addProperty("App::PropertyFloat",      "EquipmentPower","Space",QT_TRANSLATE_NOOP("App::Property","The electric power needed by the equipment of this space in Watts"), locked=True)
        if not "AutoPower" in pl:
            obj.addProperty("App::PropertyBool",       "AutoPower",     "Space",QT_TRANSLATE_NOOP("App::Property","If True, Equipment Power will be automatically filled by the equipment included in this space"), locked=True)
        if not "Conditioning" in pl:
            obj.addProperty("App::PropertyEnumeration","Conditioning",  "Space",QT_TRANSLATE_NOOP("App::Property","The type of air conditioning of this space"), locked=True)
            obj.Conditioning = ConditioningTypes
        if not "Internal" in pl:
            obj.addProperty("App::PropertyBool",       "Internal",      "Space",QT_TRANSLATE_NOOP("App::Property","Specifies if this space is internal or external"), locked=True)
            obj.Internal = True
        if not "AreaCalculationType" in pl:
            obj.addProperty("App::PropertyEnumeration", "AreaCalculationType",  "Space",QT_TRANSLATE_NOOP("App::Property","Defines the calculation type for the horizontal area and its perimeter length"), locked=True)
            obj.AreaCalculationType = AreaCalculationType

    def onDocumentRestored(self,obj):

        ArchComponent.Component.onDocumentRestored(self,obj)
        self.setProperties(obj)

    def loads(self,state):

        self.Type = "Space"

    def execute(self,obj):

        if self.clone(obj):
            return

        # Space can do without Base.  Base validity is tested in getShape() code below.
        # Remarked out ensureBase() below
        #if not self.ensureBase(obj):
        #    return
        self.getShape(obj)

    def onChanged(self,obj,prop):

        if prop == "Group":
            if hasattr(obj,"EquipmentPower"):
                if obj.AutoPower:
                    p = 0
                    for o in Draft.getObjectsOfType(Draft.get_group_contents(obj.Group, addgroups=True),
                                                    "Equipment"):
                        if hasattr(o,"EquipmentPower"):
                            p += o.EquipmentPower
                    if p != obj.EquipmentPower:
                        obj.EquipmentPower = p
        elif prop == "Zone":
            if obj.Zone:
                if obj.Zone.ViewObject:
                    if hasattr(obj.Zone.ViewObject,"Proxy"):
                        if hasattr(obj.Zone.ViewObject.Proxy,"claimChildren"):
                            obj.Zone.ViewObject.Proxy.claimChildren()
        if hasattr(obj,"Area"):
            obj.setEditorMode('Area',1)
        ArchComponent.Component.onChanged(self,obj,prop)

    def addSubobjects(self,obj,subobjects):

        "adds subobjects to this space"
        objs = obj.Boundaries
        for o in subobjects:
            if isinstance(o,tuple) or isinstance(o,list):
                if o[0].Name != obj.Name:
                    objs.append(tuple(o))
            else:
                for el in o.SubElementNames:
                    if "Face" in el:
                        if o.Object.Name != obj.Name:
                            objs.append((o.Object,el))
        obj.Boundaries = objs

    def removeSubobjects(self,obj,subobjects):

        "removes subobjects to this space"
        bounds = obj.Boundaries
        for o in subobjects:
            for b in bounds:
                if o.Name == b[0].Name:
                    bounds.remove(b)
                    break
        obj.Boundaries = bounds

    def addObject(self,obj,child):

        "Adds an object to this Space"

        if not child in obj.Group:
            g = obj.Group
            g.append(child)
            obj.Group = g

    def getShape(self,obj):

        "computes a shape from a base shape and/or boundary faces"
        import Part
        shape = None
        faces = []

        pl = obj.Placement

        #print("starting compute")

        # 1: if we have a base shape, we use it
        # Check if there is obj.Base and its validity to proceed
        if self.ensureBase(obj):
            if obj.Base.Shape.Solids:
                shape = obj.Base.Shape.copy()
                shape = shape.removeSplitter()

        # 2: if not, add all bounding boxes of considered objects and build a first shape
        if shape:
            #print("got shape from base object")
            bb = shape.BoundBox
        else:
            bb = None
            for b in obj.Boundaries:
                if hasattr(b[0],'Shape'):
                    if not bb:
                        bb = b[0].Shape.BoundBox
                    else:
                        bb.add(b[0].Shape.BoundBox)
            if not bb:
                # compute area even if we are not calculating the shape
                if obj.Shape and obj.Shape.Solids:
                    if hasattr(obj.Area,"Value"):
                        a = self.getArea(obj)
                        if obj.Area.Value != a:
                            obj.Area = a
                return
            shape = Part.makeBox(bb.XLength,bb.YLength,bb.ZLength,FreeCAD.Vector(bb.XMin,bb.YMin,bb.ZMin))
            #print("created shape from boundbox")

        # 3: identifying boundary faces
        goodfaces = []
        for b in obj.Boundaries:
            if hasattr(b[0],'Shape'):
                for sub in b[1]:
                    if "Face" in sub:
                        fn = int(sub[4:])-1
                        faces.append(b[0].Shape.Faces[fn])
                        #print("adding face ",fn," of object ",b[0].Name)

        #print("total: ", len(faces), " faces")

        # 4: get cutvolumes from faces
        cutvolumes = []
        for f in faces:
            f = f.copy()
            f.reverse()
            cutface,cutvolume,invcutvolume = ArchCommands.getCutVolume(f,shape)
            if cutvolume:
                #print("generated 1 cutvolume")
                cutvolumes.append(cutvolume.copy())
                #Part.show(cutvolume)
        for v in cutvolumes:
            #print("cutting")
            shape = shape.cut(v)

        # 5: get the final shape
        if shape:
            if shape.Solids:
                #print("setting objects shape")
                shape = shape.Solids[0]
                self.applyShape(obj,shape,pl)
                if hasattr(obj.HorizontalArea,"Value"):
                    if hasattr(obj,"AreaCalculationType"):
                        if obj.AreaCalculationType == "At Center of Mass":
                            a = self.getArea(obj)
                            obj.HorizontalArea = a
                    if hasattr(obj,"Area"):
                        obj.Area = obj.HorizontalArea

                return

        print("Arch: error computing space boundary for",obj.Label)

    def getArea(self,obj,notouch=False):

        "returns the horizontal area at the center of the space"

        self.face = self.getFootprint(obj)
        if self.face:
            if not notouch:
                if hasattr(obj,"PerimeterLength"):
                    if self.face.OuterWire.Length != obj.PerimeterLength.Value:
                        obj.PerimeterLength = self.face.OuterWire.Length
            return self.face.Area
        else:
            return 0

    def getFootprint(self,obj):

        "returns a face that represents the footprint of this space at the center of mass"

        import Part
        import DraftGeomUtils
        if not hasattr(obj.Shape,"CenterOfMass"):
            return None
        try:
            pl = Part.makePlane(1,1)
            pl.translate(obj.Shape.CenterOfMass)
            sh = obj.Shape.copy()
            cutplane,v1,v2 = ArchCommands.getCutVolume(pl,sh)
            e = sh.section(cutplane)
            e = Part.__sortEdges__(e.Edges)
            w = Part.Wire(e)
            dv = FreeCAD.Vector(obj.Shape.CenterOfMass.x,obj.Shape.CenterOfMass.y,obj.Shape.BoundBox.ZMin)
            dv = dv.sub(obj.Shape.CenterOfMass)
            w.translate(dv)
            return Part.Face(w)
        except Part.OCCError:
            return None


class _ViewProviderSpace(ArchComponent.ViewProviderComponent):

    "A View Provider for Section Planes"
    def __init__(self,vobj):

        ArchComponent.ViewProviderComponent.__init__(self,vobj)
        self.setProperties(vobj)
        vobj.Transparency = params.get_param_arch("defaultSpaceTransparency")
        vobj.LineWidth = params.get_param_view("DefaultShapeLineWidth")
        vobj.LineColor = ArchCommands.getDefaultColor("Space")
        vobj.DrawStyle = ["Solid","Dashed","Dotted","Dashdot"][params.get_param_arch("defaultSpaceStyle")]

    def setProperties(self,vobj):

        pl = vobj.PropertiesList
        if not "Text" in pl:
            vobj.addProperty("App::PropertyStringList",    "Text",        "Space",QT_TRANSLATE_NOOP("App::Property","The text to show. Use $area, $label, $longname, $description or any other property name preceded with $ (case insensitive), or $floor, $walls, $ceiling for finishes, to insert the respective data"), locked=True)
            vobj.Text = ["$label","$area"]
        if not "FontName" in pl:
            vobj.addProperty("App::PropertyFont",          "FontName",    "Space",QT_TRANSLATE_NOOP("App::Property","The name of the font"), locked=True)
            vobj.FontName = params.get_param("textfont")
        if not "TextColor" in pl:
            vobj.addProperty("App::PropertyColor",         "TextColor",   "Space",QT_TRANSLATE_NOOP("App::Property","The color of the area text"), locked=True)
            vobj.TextColor = (0.0,0.0,0.0,1.0)
        if not "FontSize" in pl:
            vobj.addProperty("App::PropertyLength",        "FontSize",    "Space",QT_TRANSLATE_NOOP("App::Property","The size of the text font"), locked=True)
            vobj.FontSize = params.get_param("textheight") * params.get_param("DefaultAnnoScaleMultiplier")
        if not "FirstLine" in pl:
            vobj.addProperty("App::PropertyLength",        "FirstLine",   "Space",QT_TRANSLATE_NOOP("App::Property","The size of the first line of text"), locked=True)
            vobj.FirstLine = params.get_param("textheight") * params.get_param("DefaultAnnoScaleMultiplier")
        if not "LineSpacing" in pl:
            vobj.addProperty("App::PropertyFloat",         "LineSpacing", "Space",QT_TRANSLATE_NOOP("App::Property","The space between the lines of text"), locked=True)
            vobj.LineSpacing = 1.0
        if not "TextPosition" in pl:
            vobj.addProperty("App::PropertyVectorDistance","TextPosition","Space",QT_TRANSLATE_NOOP("App::Property","The position of the text. Leave (0,0,0) for automatic position"), locked=True)
        if not "TextAlign" in pl:
            vobj.addProperty("App::PropertyEnumeration",   "TextAlign",   "Space",QT_TRANSLATE_NOOP("App::Property","The justification of the text"), locked=True)
            vobj.TextAlign = ["Left","Center","Right"]
            vobj.TextAlign = "Center"
        if not "Decimals" in pl:
            vobj.addProperty("App::PropertyInteger",       "Decimals",    "Space",QT_TRANSLATE_NOOP("App::Property","The number of decimals to use for calculated texts"), locked=True)
            vobj.Decimals = params.get_param("dimPrecision")
        if not "ShowUnit" in pl:
            vobj.addProperty("App::PropertyBool",          "ShowUnit",    "Space",QT_TRANSLATE_NOOP("App::Property","Show the unit suffix"), locked=True)
            vobj.ShowUnit = params.get_param("showUnit")

    def onDocumentRestored(self,vobj):

        self.setProperties(vobj)

    def getIcon(self):

        import Arch_rc
        if hasattr(self,"Object"):
            if hasattr(self.Object,"CloneOf"):
                if self.Object.CloneOf:
                    return ":/icons/Arch_Space_Clone.svg"
        return ":/icons/Arch_Space_Tree.svg"

    def attach(self,vobj):

        ArchComponent.ViewProviderComponent.attach(self,vobj)
        from pivy import coin
        self.color = coin.SoBaseColor()
        self.font = coin.SoFont()
        self.text1 = coin.SoAsciiText()
        self.text1.string = " "
        self.text1.justification = coin.SoAsciiText.LEFT
        self.text2 = coin.SoAsciiText()
        self.text2.string = " "
        self.text2.justification = coin.SoAsciiText.LEFT
        self.coords = coin.SoTransform()
        self.header = coin.SoTransform()
        self.label = coin.SoSwitch()
        sep = coin.SoSeparator()
        self.label.whichChild = 0
        sep.addChild(self.coords)
        sep.addChild(self.color)
        sep.addChild(self.font)
        sep.addChild(self.text2)
        sep.addChild(self.header)
        sep.addChild(self.text1)
        self.label.addChild(sep)
        vobj.Annotation.addChild(self.label)
        self.onChanged(vobj,"TextColor")
        self.onChanged(vobj,"FontSize")
        self.onChanged(vobj,"FirstLine")
        self.onChanged(vobj,"LineSpacing")
        self.onChanged(vobj,"FontName")
        self.Object = vobj.Object
        # footprint mode
        self.fmat = coin.SoMaterial()
        self.fcoords = coin.SoCoordinate3()
        self.fset = coin.SoIndexedFaceSet()
        fhints = coin.SoShapeHints()
        fhints.vertexOrdering = fhints.COUNTERCLOCKWISE
        sep = coin.SoSeparator()
        sep.addChild(self.fmat)
        sep.addChild(self.fcoords)
        sep.addChild(fhints)
        sep.addChild(self.fset)
        vobj.RootNode.addChild(sep)

    def updateData(self,obj,prop):

        if prop in ["Shape","Label","Tag","Area"]:
            self.onChanged(obj.ViewObject,"Text")
            self.onChanged(obj.ViewObject,"TextPosition")

    def getTextPosition(self,vobj):

        pos = FreeCAD.Vector()
        if hasattr(vobj,"TextPosition"):
            import DraftVecUtils
            if DraftVecUtils.isNull(vobj.TextPosition):
                try:
                    pos = vobj.Object.Shape.CenterOfMass
                    z = vobj.Object.Shape.BoundBox.ZMin
                    pos = FreeCAD.Vector(pos.x,pos.y,z)
                except (AttributeError, RuntimeError):
                    pos = FreeCAD.Vector()
            else:
                pos = vobj.Object.Placement.multVec(vobj.TextPosition)
        # placement's displacement will be already added by the coin node
        pos = vobj.Object.Placement.inverse().multVec(pos)
        return pos

    def onChanged(self,vobj,prop):

        if prop in ["Text","Decimals","ShowUnit"]:
            if hasattr(self,"text1") and hasattr(self,"text2") and hasattr(vobj,"Text"):
                self.text1.string.deleteValues(0)
                self.text2.string.deleteValues(0)
                text1 = []
                text2 = []
                first = True
                for t in vobj.Text:
                    if t:
                        t = t.replace("$label",vobj.Object.Label)
                        if hasattr(vobj.Object,"Area"):
                            from FreeCAD import Units
                            q = Units.Quantity(vobj.Object.Area.Value,Units.Area).getUserPreferred()
                            qt = vobj.Object.Area.Value/q[1]
                            if hasattr(vobj,"Decimals"):
                                if vobj.Decimals == 0:
                                    qt = str(int(qt))
                                else:
                                    f = "%."+str(abs(vobj.Decimals))+"f"
                                    qt = f % qt
                            else:
                                qt = str(qt)
                            if hasattr(vobj,"ShowUnit"):
                                if vobj.ShowUnit:
                                    qt = qt + q[2].replace("^2",u"\xb2") # square symbol
                            t = t.replace("$area",qt)
                        if hasattr(vobj.Object,"FinishFloor"):
                            t = t.replace("$floor",vobj.Object.FinishFloor)
                        if hasattr(vobj.Object,"FinishWalls"):
                            t = t.replace("$walls",vobj.Object.FinishWalls)
                        if hasattr(vobj.Object,"FinishCeiling"):
                            t = t.replace("$ceiling",vobj.Object.FinishCeiling)
                        # replace all other properties
                        props = vobj.Object.PropertiesList
                        lower_props = [p.lower() for p in props]
                        for rtag in re.findall(r"\$\w+", t):
                            lower_rtag = rtag[1:].lower()
                            if lower_rtag in lower_props:
                                prop = props[lower_props.index(lower_rtag)]
                                value = getattr(vobj.Object, prop, "")
                                if hasattr(value, "UserString"):
                                    value = value.UserString
                                elif hasattr(value, "Label"):
                                    value = value.Label
                                elif hasattr(value, "Name"):
                                    value = value.Name
                                t = t.replace(rtag, str(value))
                        if first:
                            text1.append(t)
                        else:
                            text2.append(t)
                    first = False
                if text1:
                    self.text1.string.setValues(text1)
                if text2:
                    self.text2.string.setValues(text2)

        elif prop == "FontName":
            if hasattr(self,"font") and hasattr(vobj,"FontName"):
                self.font.name = str(vobj.FontName)

        elif prop == "FontSize":
            if hasattr(self,"font") and hasattr(vobj,"FontSize"):
                self.font.size = vobj.FontSize.Value
                if hasattr(vobj,"FirstLine"):
                    scale = vobj.FirstLine.Value/vobj.FontSize.Value
                    self.header.scaleFactor.setValue([scale,scale,scale])
                    self.onChanged(vobj, "TextPosition")

        elif prop == "FirstLine":
            if hasattr(self,"header") and hasattr(vobj,"FontSize") and hasattr(vobj,"FirstLine"):
                scale = vobj.FirstLine.Value/vobj.FontSize.Value
                self.header.scaleFactor.setValue([scale,scale,scale])
                self.onChanged(vobj, "TextPosition")

        elif prop == "TextColor":
            if hasattr(self,"color") and hasattr(vobj,"TextColor"):
                c = vobj.TextColor
                self.color.rgb.setValue(c[0],c[1],c[2])

        elif prop == "TextPosition":
            if hasattr(self,"coords") and hasattr(self,"header") and hasattr(vobj,"TextPosition") and hasattr(vobj,"FirstLine"):
                pos = self.getTextPosition(vobj)
                self.coords.translation.setValue([pos.x,pos.y,pos.z+0.01]) # adding small z offset to separate from bottom face
                up = vobj.FirstLine.Value * vobj.LineSpacing
                self.header.translation.setValue([0,up,0])

        elif prop == "LineSpacing":
            if hasattr(self,"text1") and hasattr(self,"text2") and hasattr(vobj,"LineSpacing"):
                self.text1.spacing = vobj.LineSpacing
                self.text2.spacing = vobj.LineSpacing
                self.onChanged(vobj,"TextPosition")

        elif prop == "TextAlign":
            if hasattr(self,"text1") and hasattr(self,"text2") and hasattr(vobj,"TextAlign"):
                from pivy import coin
                if vobj.TextAlign == "Center":
                    self.text1.justification = coin.SoAsciiText.CENTER
                    self.text2.justification = coin.SoAsciiText.CENTER
                elif vobj.TextAlign == "Right":
                    self.text1.justification = coin.SoAsciiText.RIGHT
                    self.text2.justification = coin.SoAsciiText.RIGHT
                else:
                    self.text1.justification = coin.SoAsciiText.LEFT
                    self.text2.justification = coin.SoAsciiText.LEFT

        elif prop == "Visibility":
            if vobj.Visibility:
                self.label.whichChild = 0
            else:
                self.label.whichChild = -1

        elif prop == "Transparency":
            if hasattr(vobj,"DisplayMode"):
                vobj.DisplayMode = "Wireframe" if vobj.Transparency == 100 else "Flat Lines"

    def setEdit(self, vobj, mode):
        if mode != 0:
            return None

        taskd = SpaceTaskPanel()
        taskd.obj = self.Object
        taskd.update()
        taskd.updateBoundaries()
        FreeCADGui.Control.showDialog(taskd)
        return True

    def getDisplayModes(self,vobj):

        modes = ArchComponent.ViewProviderComponent.getDisplayModes(self,vobj)+["Footprint"]
        return modes

    def setDisplayMode(self,mode):

        self.fset.coordIndex.deleteValues(0)
        self.fcoords.point.deleteValues(0)
        if mode == "Footprint":
            if hasattr(self,"Object"):
                face = self.Object.Proxy.getFootprint(self.Object)
                if face:
                    verts = []
                    fdata = []
                    idx = 0
                    tri = face.tessellate(1)
                    for v in tri[0]:
                        verts.append([v.x,v.y,v.z])
                    for f in tri[1]:
                        fdata.extend([f[0]+idx,f[1]+idx,f[2]+idx,-1])
                    idx += len(tri[0])
                    self.fcoords.point.setValues(verts)
                    self.fset.coordIndex.setValues(0,len(fdata),fdata)
            return "Points"
        return ArchComponent.ViewProviderComponent.setDisplayMode(self,mode)


class SpaceTaskPanel(ArchComponent.ComponentTaskPanel):

    "A modified version of the Arch component task panel"

    def __init__(self):

        ArchComponent.ComponentTaskPanel.__init__(self)
        self.editButton = QtGui.QPushButton(self.form)
        self.editButton.setObjectName("editButton")
        self.editButton.setIcon(QtGui.QIcon(":/icons/Draft_Edit.svg"))
        self.grid.addWidget(self.editButton, 4, 0, 1, 2)
        self.editButton.setText(QtGui.QApplication.translate("Arch", "Set text position", None))
        QtCore.QObject.connect(self.editButton, QtCore.SIGNAL("clicked()"), self.setTextPos)
        boundLabel = QtGui.QLabel(self.form)
        self.grid.addWidget(boundLabel, 5, 0, 1, 2)
        boundLabel.setText(QtGui.QApplication.translate("Arch", "Space boundaries", None))
        self.boundList = QtGui.QListWidget(self.form)
        self.grid.addWidget(self.boundList, 6, 0, 1, 2)
        self.addCompButton = QtGui.QPushButton(self.form)
        self.addCompButton.setObjectName("addCompButton")
        self.addCompButton.setIcon(QtGui.QIcon(":/icons/Arch_Add.svg"))
        self.grid.addWidget(self.addCompButton, 7, 0, 1, 1)
        self.addCompButton.setText(QtGui.QApplication.translate("Arch", "Add", None))
        QtCore.QObject.connect(self.addCompButton, QtCore.SIGNAL("clicked()"), self.addBoundary)
        self.delCompButton = QtGui.QPushButton(self.form)
        self.delCompButton.setObjectName("delCompButton")
        self.delCompButton.setIcon(QtGui.QIcon(":/icons/Arch_Remove.svg"))
        self.grid.addWidget(self.delCompButton, 7, 1, 1, 1)
        self.delCompButton.setText(QtGui.QApplication.translate("Arch", "Remove", None))
        QtCore.QObject.connect(self.delCompButton, QtCore.SIGNAL("clicked()"), self.delBoundary)

    def updateBoundaries(self):

        self.boundList.clear()
        if self.obj:
            for b in self.obj.Boundaries:
                s = b[0].Label
                for n in b[1]:
                    s += ", " + n
                it = QtGui.QListWidgetItem(s)
                it.setToolTip(b[0].Name)
                self.boundList.addItem(it)

    def setTextPos(self):

        FreeCADGui.runCommand("Draft_Edit")

    def addBoundary(self):

        if self.obj:
            if FreeCADGui.Selection.getSelectionEx():
                self.obj.Proxy.addSubobjects(self.obj,FreeCADGui.Selection.getSelectionEx())
                self.updateBoundaries()

    def delBoundary(self):

        if self.boundList.currentRow() >= 0:
            it = self.boundList.item(self.boundList.currentRow())
            if it and self.obj:
                on = it.toolTip()
                bounds = self.obj.Boundaries
                for b in bounds:
                    if b[0].Name == on:
                        bounds.remove(b)
                        break
                self.obj.Boundaries = bounds
                self.updateBoundaries()
