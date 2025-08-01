/***************************************************************************
 *   Copyright (c) 2013 Jürgen Riegel <FreeCAD@juergen-riegel.net>         *
 *                                                                         *
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

#ifndef _PreComp_
#include <Inventor/SbVec3f.h>
#include <Inventor/details/SoFaceDetail.h>
#include <Inventor/details/SoLineDetail.h>
#include <Inventor/details/SoPointDetail.h>
#include <Inventor/nodes/SoAnnotation.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoPolygonOffset.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShapeHints.h>

#include <sstream>

#include <SMESHDS_Mesh.hxx>
#include <SMESH_Mesh.hxx>
#endif

#include <App/DocumentObject.h>
#include <Base/BoundBox.h>
#include <Base/Console.h>
#include <Base/TimeInfo.h>
#include <Mod/Fem/App/FemMeshObject.h>

#include "ViewProviderFemMesh.h"
#include "ViewProviderFemMeshPy.h"


using namespace FemGui;

struct FemFace
{
    const SMDS_MeshNode* Nodes[8];
    unsigned long ElementNumber;
    const SMDS_MeshElement* Element;
    unsigned short Size;
    unsigned short FaceNo;
    bool hide;
    Base::Vector3d getFirstNodePoint()
    {
        return Base::Vector3d(Nodes[0]->X(), Nodes[0]->Y(), Nodes[0]->Z());
    }

    Base::Vector3d set(short size,
                       const SMDS_MeshElement* element,
                       unsigned short id,
                       short faceNo,
                       const SMDS_MeshNode* n1,
                       const SMDS_MeshNode* n2,
                       const SMDS_MeshNode* n3,
                       const SMDS_MeshNode* n4 = nullptr,
                       const SMDS_MeshNode* n5 = nullptr,
                       const SMDS_MeshNode* n6 = nullptr,
                       const SMDS_MeshNode* n7 = nullptr,
                       const SMDS_MeshNode* n8 = nullptr);

    bool isSameFace(FemFace& face);
};

Base::Vector3d FemFace::set(short size,
                            const SMDS_MeshElement* element,
                            unsigned short id,
                            short faceNo,
                            const SMDS_MeshNode* n1,
                            const SMDS_MeshNode* n2,
                            const SMDS_MeshNode* n3,
                            const SMDS_MeshNode* n4,
                            const SMDS_MeshNode* n5,
                            const SMDS_MeshNode* n6,
                            const SMDS_MeshNode* n7,
                            const SMDS_MeshNode* n8)
{
    Nodes[0] = n1;
    Nodes[1] = n2;
    Nodes[2] = n3;
    Nodes[3] = n4;
    Nodes[4] = n5;
    Nodes[5] = n6;
    Nodes[6] = n7;
    Nodes[7] = n8;

    Element = element;
    ElementNumber = id;
    Size = size;
    FaceNo = faceNo;
    hide = false;

    // sorting the nodes for later easier comparison (bubble sort)
    int flag = 1;               // set flag to 1 to start first pass
    const SMDS_MeshNode* temp;  // holding variable

    for (int i = 1; (i <= size) && flag; i++) {
        flag = 0;
        for (int j = 0; j < (size - 1); j++) {
            if (Nodes[j + 1] > Nodes[j])  // ascending order simply changes to <
            {
                temp = Nodes[j];  // swap elements
                Nodes[j] = Nodes[j + 1];
                Nodes[j + 1] = temp;
                flag = 1;  // indicates that a swap occurred.
            }
        }
    }

    return Base::Vector3d(Nodes[0]->X(), Nodes[0]->Y(), Nodes[0]->Z());
}

class FemFaceGridItem: public std::vector<FemFace*>
{
public:
    // FemFaceGridItem(void){reserve(200);}
};

bool FemFace::isSameFace(FemFace& face)
{
    // the same element can not have the same face
    if (face.ElementNumber == ElementNumber) {
        return false;
    }
    if (face.Size != Size) {
        return false;
    }
    // if the same face size just compare if the sorted nodes are the same
    if (Nodes[0] == face.Nodes[0] && Nodes[1] == face.Nodes[1] && Nodes[2] == face.Nodes[2]
        && Nodes[3] == face.Nodes[3] && Nodes[4] == face.Nodes[4] && Nodes[5] == face.Nodes[5]
        && Nodes[6] == face.Nodes[6] && Nodes[7] == face.Nodes[7]) {
        hide = true;
        face.hide = true;
        return true;
    }

    return false;
}

// ----------------------------------------------------------------------------

class ViewProviderFemMesh::Private
{
public:
    static const char* dm_face_wire;
    static const char* dm_wire_node;
    static const char* dm_face_wire_node;
    static const char* dm_face;
    static const char* dm_node;
    static const char* dm_wire;
};

const char* ViewProviderFemMesh::Private::dm_face_wire = "Faces & Wireframe";
const char* ViewProviderFemMesh::Private::dm_wire_node = "Wireframe & Nodes";
const char* ViewProviderFemMesh::Private::dm_face_wire_node = "Faces, Wireframe & Nodes";
const char* ViewProviderFemMesh::Private::dm_face = "Faces";
const char* ViewProviderFemMesh::Private::dm_node = "Nodes";
const char* ViewProviderFemMesh::Private::dm_wire = "Wireframe";

PROPERTY_SOURCE(FemGui::ViewProviderFemMesh, Gui::ViewProviderGeometryObject)

App::PropertyFloatConstraint::Constraints ViewProviderFemMesh::floatRange = {1.0, 64.0, 1.0};

const char* ViewProviderFemMesh::colorModeEnum[] = {"Overall", "ByElement", "ByNode", nullptr};

ViewProviderFemMesh::ViewProviderFemMesh()
{
    sPixmap = "fem-femmesh-from-shape";

    ADD_PROPERTY(PointColor, (Base::Color(0.7f, 0.7f, 0.7f)));
    ADD_PROPERTY(PointSize, (5.0f));
    PointSize.setConstraints(&floatRange);
    ADD_PROPERTY(LineWidth, (1.0f));
    LineWidth.setConstraints(&floatRange);

    ShapeAppearance.setDiffuseColor(Base::Color(1.0f, 0.7f, 0.0f));
    Transparency.setValue(0);
    ADD_PROPERTY(BackfaceCulling, (true));
    ADD_PROPERTY(ShowInner, (false));
    ADD_PROPERTY(MaxFacesShowInner, (50000));

    ADD_PROPERTY_TYPE(ColorMode,
                      ("Overall"),
                      "Display Options",
                      App::Prop_None,
                      "Set the color mode");
    ADD_PROPERTY_TYPE(NodeColorArray,
                      (PointColor.getValue()),
                      "Object Style",
                      App::Prop_Hidden,
                      "Node diffuse color array");
    ADD_PROPERTY_TYPE(ElementColorArray,
                      (ShapeAppearance.getDiffuseColor()),
                      "Object Style",
                      App::Prop_Hidden,
                      "Node diffuse color array");

    suppressibleExt.initExtension(this);

    ColorMode.setEnums(colorModeEnum);
    onlyEdges = false;

    pcDrawStyle = new SoDrawStyle();
    pcDrawStyle->ref();
    pcDrawStyle->style = SoDrawStyle::LINES;
    pcDrawStyle->lineWidth = LineWidth.getValue();

    pShapeHints = new SoShapeHints;
    pShapeHints->shapeType = SoShapeHints::SOLID;
    pShapeHints->vertexOrdering = SoShapeHints::COUNTERCLOCKWISE;
    pShapeHints->ref();

    pcMatBinding = new SoMaterialBinding;
    pcMatBinding->value = SoMaterialBinding::OVERALL;
    pcMatBinding->ref();

    pcCoords = new SoCoordinate3();
    pcCoords->ref();

    pcAnoCoords = new SoCoordinate3();
    pcAnoCoords->ref();
    pcAnoCoords->point.setNum(0);

    pcFaces = new SoIndexedFaceSet;
    pcFaces->ref();

    pcLines = new SoIndexedLineSet;
    pcLines->ref();

    pcPointStyle = new SoDrawStyle();
    pcPointStyle->ref();
    pcPointStyle->style = SoDrawStyle::POINTS;
    pcPointStyle->pointSize = PointSize.getValue();

    pcPointMaterial = new SoMaterial;
    pcPointMaterial->ref();
    // PointMaterial.touch();

    DisplacementFactor = 0;
}

ViewProviderFemMesh::~ViewProviderFemMesh()
{
    pcCoords->unref();
    pcDrawStyle->unref();
    pcFaces->unref();
    pcLines->unref();
    pShapeHints->unref();
    pcMatBinding->unref();
    pcPointMaterial->unref();
    pcPointStyle->unref();
    pcAnoCoords->unref();
}

void ViewProviderFemMesh::attach(App::DocumentObject* pcObj)
{
    ViewProviderGeometryObject::attach(pcObj);

    // Move 'coords' before the switch
    // pcRoot->insertChild(pcCoords,pcRoot->findChild(static_cast<const SoNode*>(pcModeSwitch)));

    // Annotation sets
    SoGroup* pcAnotRoot = new SoAnnotation();

    SoDrawStyle* pcAnoStyle = new SoDrawStyle();
    pcAnoStyle->style = SoDrawStyle::POINTS;
    pcAnoStyle->pointSize = 5;

    SoMaterial* pcAnoMaterial = new SoMaterial;
    pcAnoMaterial->diffuseColor.setValue(0, 1, 0);
    pcAnoMaterial->emissiveColor.setValue(0, 1, 0);
    pcAnotRoot->addChild(pcAnoMaterial);
    pcAnotRoot->addChild(pcAnoStyle);
    pcAnotRoot->addChild(pcAnoCoords);
    SoPointSet* pointset = new SoPointSet;
    pcAnotRoot->addChild(pointset);

    // Faces
    SoGroup* pcFlatRoot = new SoGroup();
    pcFlatRoot->addChild(pcCoords);
    pcFlatRoot->addChild(pShapeHints);
    pcFlatRoot->addChild(pcShapeMaterial);
    pcFlatRoot->addChild(pcMatBinding);
    pcFlatRoot->addChild(pcFaces);
    pcFlatRoot->addChild(pcAnotRoot);
    addDisplayMaskMode(pcFlatRoot, Private::dm_face);

    // Wireframe
    SoGroup* pcWireRoot = new SoSeparator();
    SoLightModel* pcLightModel = new SoLightModel();
    pcLightModel->model = SoLightModel::BASE_COLOR;
    pcWireRoot->addChild(pcCoords);
    pcWireRoot->addChild(pcDrawStyle);
    pcWireRoot->addChild(pcLightModel);
    SoBaseColor* color = new SoBaseColor();
    color->rgb.setValue(0.0f, 0.0f, 0.0f);
    pcWireRoot->addChild(color);
    pcWireRoot->addChild(pcLines);
    addDisplayMaskMode(pcWireRoot, Private::dm_wire);

    // Nodes
    SoGroup* pcPointsRoot = new SoSeparator();
    pcPointsRoot->addChild(pcPointMaterial);
    pcPointsRoot->addChild(pcPointStyle);
    pcPointsRoot->addChild(pcCoords);
    pointset = new SoPointSet;
    pcPointsRoot->addChild(pointset);
    addDisplayMaskMode(pcPointsRoot, Private::dm_node);

    // For combined modes make sure to use a Separator instead of a Group
    // because the group affects nodes that are rendered afterwards (#0003769)

    // Faces + Wireframe (Elements)
    SoPolygonOffset* offset = new SoPolygonOffset();

    SoGroup* pcFlatWireRoot = new SoGroup();
    pcFlatWireRoot->addChild(pcWireRoot);
    pcFlatWireRoot->addChild(offset);
    pcFlatWireRoot->addChild(pcFlatRoot);
    addDisplayMaskMode(pcFlatWireRoot, Private::dm_face_wire);

    // Faces + Wireframe + Nodes (Elements&Nodes)
    SoGroup* pcElemNodesRoot = new SoGroup();
    pcElemNodesRoot->addChild(pcPointsRoot);
    pcElemNodesRoot->addChild(pcWireRoot);
    pcElemNodesRoot->addChild(offset);
    pcElemNodesRoot->addChild(pcFlatRoot);
    addDisplayMaskMode(pcElemNodesRoot, Private::dm_face_wire_node);

    // Wireframe + Nodes
    SoGroup* pcWireNodeRoot = new SoGroup();
    pcWireNodeRoot->addChild(pcPointsRoot);
    pcWireNodeRoot->addChild(pcWireRoot);
    addDisplayMaskMode(pcWireNodeRoot, Private::dm_wire_node);
}

void ViewProviderFemMesh::setDisplayMode(const char* ModeName)
{
    setDisplayMaskMode(ModeName);
    ViewProviderGeometryObject::setDisplayMode(ModeName);
}

std::vector<std::string> ViewProviderFemMesh::getDisplayModes() const
{
    std::vector<std::string> StrList;
    StrList.emplace_back(Private::dm_face_wire);
    StrList.emplace_back(Private::dm_face_wire_node);
    StrList.emplace_back(Private::dm_face);
    StrList.emplace_back(Private::dm_wire);
    StrList.emplace_back(Private::dm_node);
    StrList.emplace_back(Private::dm_wire_node);
    return StrList;
}

void ViewProviderFemMesh::updateData(const App::Property* prop)
{
    if (prop->isDerivedFrom<Fem::PropertyFemMesh>()) {
        ViewProviderFEMMeshBuilder builder;
        resetColorByNodeId();
        resetDisplacementByNodeId();
        builder.createMesh(prop,
                           pcCoords,
                           pcFaces,
                           pcLines,
                           vFaceElementIdx,
                           vNodeElementIdx,
                           onlyEdges,
                           ShowInner.getValue(),
                           MaxFacesShowInner.getValue());
    }
    Gui::ViewProviderGeometryObject::updateData(prop);
}

void ViewProviderFemMesh::onChanged(const App::Property* prop)
{
    auto matchTransparency = [&]() {
        if (getObject() && getObject()->testStatus(App::ObjectStatus::TouchOnColorChange)) {
            getObject()->touch(true);
        }
        long value = static_cast<long>(100 * ShapeAppearance.getTransparency() + 0.5);
        if (value != Transparency.getValue()) {
            Transparency.setValue(value);
        }
    };

    if (prop == &PointSize) {
        pcPointStyle->pointSize = PointSize.getValue();
    }
    else if (prop == &PointColor) {
        const Base::Color& c = PointColor.getValue();
        pcPointMaterial->diffuseColor.setValue(c.r, c.g, c.b);
    }
    else if (prop == &BackfaceCulling) {
        if (BackfaceCulling.getValue()) {
            pShapeHints->shapeType = SoShapeHints::SOLID;
            // pShapeHints->vertexOrdering = SoShapeHints::CLOCKWISE;
        }
        else {
            pShapeHints->shapeType = SoShapeHints::UNKNOWN_SHAPE_TYPE;
            // pShapeHints->vertexOrdering = SoShapeHints::CLOCKWISE;
        }
    }
    else if (prop == &ShowInner) {
        // recalc mesh with new settings
        ViewProviderFEMMeshBuilder builder;
        builder.createMesh(&(static_cast<Fem::FemMeshObject*>(this->pcObject)->FemMesh),
                           pcCoords,
                           pcFaces,
                           pcLines,
                           vFaceElementIdx,
                           vNodeElementIdx,
                           onlyEdges,
                           ShowInner.getValue(),
                           MaxFacesShowInner.getValue());
    }
    else if (prop == &LineWidth) {
        pcDrawStyle->lineWidth = LineWidth.getValue();
    }
    else if (prop == &ColorMode) {
        switch (ColorMode.getValue()) {
            case 1:  // ByElement
                setMaterialByColorArray(&ElementColorArray, vFaceElementIdx);
                break;
            case 2:  // ByNode
                setMaterialByColorArray(&NodeColorArray, vNodeElementIdx);
                break;
            default:  // Overall
                setMaterialOverall();
        }
    }
    else if (prop == &ShapeAppearance && ColorMode.getValue() == 0) {
        matchTransparency();
        setMaterialOverall();
    }
    else if ((prop == &ElementColorArray || prop == &ShapeAppearance)
             && ColorMode.getValue() == 1) {
        matchTransparency();
        setMaterialByColorArray(&ElementColorArray, vFaceElementIdx);
    }
    else if ((prop == &NodeColorArray || prop == &ShapeAppearance) && ColorMode.getValue() == 2) {
        matchTransparency();
        setMaterialByColorArray(&NodeColorArray, vNodeElementIdx);
    }
    else {
        ViewProviderGeometryObject::onChanged(prop);
    }
}

std::string ViewProviderFemMesh::getElement(const SoDetail* detail) const
{
    std::stringstream str;
    if (detail) {
        if (detail->getTypeId() == SoFaceDetail::getClassTypeId()) {
            const SoFaceDetail* face_detail = static_cast<const SoFaceDetail*>(detail);
            unsigned long edx = vFaceElementIdx[face_detail->getFaceIndex()];

            str << "Elem" << (edx >> 3) << "F" << (edx & 7) + 1;
        }
        // trigger on edges only if edge only mesh, otherwise you only hit edges and never faces....
        else if (onlyEdges && detail->getTypeId() == SoLineDetail::getClassTypeId()) {
            const SoLineDetail* line_detail = static_cast<const SoLineDetail*>(detail);
            int edge = line_detail->getLineIndex() + 1;
            str << "Edge" << edge;
        }
        else if (detail->getTypeId() == SoPointDetail::getClassTypeId()) {
            const SoPointDetail* point_detail = static_cast<const SoPointDetail*>(detail);
            int idx = point_detail->getCoordinateIndex();
            // first check if the index is part of the highlighted nodes (#0003618)
            if (idx < static_cast<int>(vHighlightedIdx.size())) {
                int vertex = vHighlightedIdx[idx];
                str << "Node" << vertex;
            }
            else if (idx < static_cast<int>(vNodeElementIdx.size())) {
                int vertex = vNodeElementIdx[idx];
                str << "Node" << vertex;
            }
            else {
                return {};
            }
        }
    }

    return str.str();
}

SoDetail* ViewProviderFemMesh::getDetail(const char* subelement) const
{
    std::string element = subelement;
    std::string::size_type pos = element.find_first_of("0123456789");
    int index = -1;
    if (pos != std::string::npos) {
        index = std::atoi(element.substr(pos).c_str());
        element = element.substr(0, pos);
    }

    SoDetail* detail = nullptr;
    if (index < 0) {
        return detail;
    }
    if (element == "Elem") {
        detail = new SoFaceDetail();
        static_cast<SoFaceDetail*>(detail)->setPartIndex(index - 1);
    }
    // else if (element == "Edge") {
    //     detail = new SoLineDetail();
    //     static_cast<SoLineDetail*>(detail)->setLineIndex(index - 1);
    // }
    // else if (element == "Vertex") {
    //     detail = new SoPointDetail();
    //     static_cast<SoPointDetail*>(detail)->setCoordinateIndex(index +
    //     nodeset->startIndex.getValue() - 1);
    // }

    return detail;
}

std::vector<Base::Vector3d> ViewProviderFemMesh::getSelectionShape(const char* /*Element*/) const
{
    return {};
}

std::set<long> ViewProviderFemMesh::getHighlightNodes() const
{
    std::set<long> nodes;
    nodes.insert(vHighlightedIdx.begin(), vHighlightedIdx.end());
    return nodes;
}

void ViewProviderFemMesh::setHighlightNodes(const std::set<long>& HighlightedNodes)
{
    if (!HighlightedNodes.empty()) {
        const SMESHDS_Mesh* data = static_cast<Fem::FemMeshObject*>(this->pcObject)
                                       ->FemMesh.getValue()
                                       .getSMesh()
                                       ->GetMeshDS();

        pcAnoCoords->point.setNum(HighlightedNodes.size());
        SbVec3f* verts = pcAnoCoords->point.startEditing();
        int i = 0;
        for (std::set<long>::const_iterator it = HighlightedNodes.begin();
             it != HighlightedNodes.end();
             ++it, i++) {
            const SMDS_MeshNode* Node = data->FindNode(*it);
            if (Node) {
                verts[i].setValue((float)Node->X(), (float)Node->Y(), (float)Node->Z());
            }
            else {
                verts[i].setValue(0, 0, 0);
            }
        }
        pcAnoCoords->point.finishEditing();

        // save the node ids
        vHighlightedIdx.clear();
        vHighlightedIdx.insert(vHighlightedIdx.end(),
                               HighlightedNodes.begin(),
                               HighlightedNodes.end());
    }
    else {
        pcAnoCoords->point.setNum(0);
        vHighlightedIdx.clear();
    }
}

void ViewProviderFemMesh::resetHighlightNodes()
{
    pcAnoCoords->point.setNum(0);
    vHighlightedIdx.clear();
}

PyObject* ViewProviderFemMesh::getPyObject()
{
    if (!pyViewObject) {
        pyViewObject = new ViewProviderFemMeshPy(this);
    }
    pyViewObject->IncRef();
    return pyViewObject;
}

void ViewProviderFemMesh::setDisplacementByNodeId(const std::map<long, Base::Vector3d>& NodeDispMap)
{
    long startId = NodeDispMap.begin()->first;
    long endId = (--NodeDispMap.end())->first;

    std::vector<Base::Vector3d> vecVec(endId - startId + 2, Base::Vector3d());

    for (const auto& it : NodeDispMap) {
        vecVec[it.first - startId] = it.second;
    }

    setDisplacementByNodeIdHelper(vecVec, startId);
}

void ViewProviderFemMesh::setDisplacementByNodeId(const std::vector<long>& NodeIds,
                                                  const std::vector<Base::Vector3d>& NodeDisps)
{
    long startId = *(std::min_element(NodeIds.begin(), NodeIds.end()));
    long endId = *(std::max_element(NodeIds.begin(), NodeIds.end()));

    std::vector<Base::Vector3d> vecVec(endId - startId + 2, Base::Vector3d());

    long i = 0;
    for (std::vector<long>::const_iterator it = NodeIds.begin(); it != NodeIds.end(); ++it, i++) {
        vecVec[*it - startId] = NodeDisps[i];
    }

    setDisplacementByNodeIdHelper(vecVec, startId);
}

void ViewProviderFemMesh::setDisplacementByNodeIdHelper(
    const std::vector<Base::Vector3d>& DispVector,
    long startId)
{
    DisplacementVector.resize(vNodeElementIdx.size());
    int i = 0;
    for (std::vector<unsigned long>::const_iterator it = vNodeElementIdx.begin();
         it != vNodeElementIdx.end();
         ++it, i++) {
        DisplacementVector[i] = DispVector[*it - startId];
    }
    applyDisplacementToNodes(1.0);
}

void ViewProviderFemMesh::resetDisplacementByNodeId()
{
    applyDisplacementToNodes(0.0);
    DisplacementVector.clear();
}
/// reaply the node displacement with a certain factor and do a redraw
void ViewProviderFemMesh::applyDisplacementToNodes(double factor)
{
    if (DisplacementVector.empty()) {
        return;
    }

    float x = 0, y = 0, z = 0;
    // set the point coordinates
    long sz = pcCoords->point.getNum();
    SbVec3f* verts = pcCoords->point.startEditing();
    for (long i = 0; i < sz; i++) {
        verts[i].getValue(x, y, z);
        // undo old factor#
        Base::Vector3d oldDisp = DisplacementVector[i] * DisplacementFactor;
        x -= oldDisp.x;
        y -= oldDisp.y;
        z -= oldDisp.z;
        // apply new factor
        Base::Vector3d newDisp = DisplacementVector[i] * factor;
        x += newDisp.x;
        y += newDisp.y;
        z += newDisp.z;
        // set the new value
        verts[i].setValue(x, y, z);
    }
    pcCoords->point.finishEditing();

    DisplacementFactor = factor;
}

void ViewProviderFemMesh::setColorByNodeId(const std::vector<long>& NodeIds,
                                           const std::vector<Base::Color>& NodeColors)
{
    long endId = *(std::max_element(NodeIds.begin(), NodeIds.end()));

    std::vector<Base::Color> colorVec(endId + 1, Base::Color(0, 1, 0));
    long i = 0;
    for (std::vector<long>::const_iterator it = NodeIds.begin(); it != NodeIds.end(); ++it, i++) {
        colorVec[*it] = NodeColors[i];
    }

    setColorByNodeIdHelper(colorVec);
}

void ViewProviderFemMesh::setColorByNodeIdHelper(const std::vector<Base::Color>& colorVec)
{
    pcMatBinding->value = SoMaterialBinding::PER_VERTEX_INDEXED;

    // resizing and writing the color vector:
    pcShapeMaterial->diffuseColor.setNum(vNodeElementIdx.size());
    SbColor* colors = pcShapeMaterial->diffuseColor.startEditing();

    long i = 0;
    for (std::vector<unsigned long>::const_iterator it = vNodeElementIdx.begin();
         it != vNodeElementIdx.end();
         ++it, i++) {
        colors[i] = SbColor(colorVec[*it].r, colorVec[*it].g, colorVec[*it].b);
    }

    pcShapeMaterial->diffuseColor.finishEditing();
}

void ViewProviderFemMesh::resetColorByNodeId()
{
    const Base::Color& c = ShapeAppearance.getDiffuseColor();
    NodeColorArray.setValue(c);
}

void ViewProviderFemMesh::setColorByNodeId(
    const std::map<std::vector<long>, Base::Color>& elemColorMap)
{
    setColorByIdHelper(elemColorMap, vNodeElementIdx, 0, NodeColorArray);
}

void ViewProviderFemMesh::setColorByElementId(
    const std::map<std::vector<long>, Base::Color>& elemColorMap)
{
    setColorByIdHelper(elemColorMap, vFaceElementIdx, 3, ElementColorArray);
}

void ViewProviderFemMesh::setColorByIdHelper(
    const std::map<std::vector<long>, Base::Color>& elemColorMap,
    const std::vector<unsigned long>& vElementIdx,
    int rShift,
    App::PropertyColorList& prop)
{
    std::vector<Base::Color> vecColor(vElementIdx.size());
    std::map<long, const Base::Color*> colorMap;
    for (const auto& m : elemColorMap) {
        for (long i : m.first) {
            colorMap[i] = &m.second;
        }
    }

    Base::Color baseDif = ShapeAppearance.getDiffuseColor();
    int i = 0;
    for (std::vector<unsigned long>::const_iterator it = vElementIdx.begin();
         it != vElementIdx.end();
         ++it, i++) {
        unsigned long ElemIdx = ((*it) >> rShift);
        const std::map<long, const Base::Color*>::const_iterator pos = colorMap.find(ElemIdx);
        vecColor[i] = pos == colorMap.end() ? baseDif : *pos->second;
    }

    prop.setValue(vecColor);
}

void ViewProviderFemMesh::setMaterialOverall() const
{
    const App::Material& mat = ShapeAppearance[0];
    Base::Color baseDif = mat.diffuseColor;
    Base::Color baseAmb = mat.ambientColor;
    Base::Color baseSpe = mat.specularColor;
    Base::Color baseEmi = mat.emissiveColor;
    float baseShi = mat.shininess;
    float baseTra = mat.transparency;

    pcMatBinding->value = SoMaterialBinding::OVERALL;
    pcShapeMaterial->diffuseColor.setNum(0);
    pcShapeMaterial->ambientColor.setNum(0);
    pcShapeMaterial->specularColor.setNum(0);
    pcShapeMaterial->emissiveColor.setNum(0);
    pcShapeMaterial->shininess.setNum(0);
    pcShapeMaterial->transparency.setNum(0);
    pcShapeMaterial->diffuseColor.setValue(baseDif.r, baseDif.g, baseDif.b);
    pcShapeMaterial->ambientColor.setValue(baseAmb.r, baseAmb.g, baseAmb.b);
    pcShapeMaterial->specularColor.setValue(baseSpe.r, baseSpe.g, baseSpe.b);
    pcShapeMaterial->emissiveColor.setValue(baseEmi.r, baseEmi.g, baseEmi.b);
    pcShapeMaterial->shininess.setValue(baseShi);
    pcShapeMaterial->transparency.setValue(baseTra);

    pcFaces->touch();

    return;
}

void ViewProviderFemMesh::setMaterialByColorArray(
    const App::PropertyColorList* prop,
    const std::vector<unsigned long>& vElementIdx) const
{
    const App::Material& baseMat = ShapeAppearance[0];
    Base::Color baseDif = baseMat.diffuseColor;
    Base::Color baseAmb = baseMat.ambientColor;
    Base::Color baseSpe = baseMat.specularColor;
    Base::Color baseEmi = baseMat.emissiveColor;
    float baseShi = baseMat.shininess;
    float baseTra = baseMat.transparency;

    // resizing and writing the color vector:
    std::vector<Base::Color> vecColor = prop->getValue();
    size_t elemSize = vElementIdx.size();
    if (vecColor.size() == 1) {
        pcMatBinding->value = SoMaterialBinding::OVERALL;
        pcShapeMaterial->diffuseColor.setNum(0);
        pcShapeMaterial->ambientColor.setNum(0);
        pcShapeMaterial->specularColor.setNum(0);
        pcShapeMaterial->emissiveColor.setNum(0);
        pcShapeMaterial->shininess.setNum(0);
        pcShapeMaterial->transparency.setNum(0);
        pcShapeMaterial->diffuseColor.setValue(vecColor[0].r, vecColor[0].g, vecColor[0].b);
        pcShapeMaterial->ambientColor.setValue(baseAmb.r, baseAmb.g, baseAmb.b);
        pcShapeMaterial->specularColor.setValue(baseSpe.r, baseSpe.g, baseSpe.b);
        pcShapeMaterial->emissiveColor.setValue(baseEmi.r, baseEmi.g, baseEmi.b);
        pcShapeMaterial->shininess.setValue(baseShi);
        pcShapeMaterial->transparency.setValue(baseTra);

        return;
    }

    if (prop == &ElementColorArray) {
        pcMatBinding->value = SoMaterialBinding::PER_FACE;
    }
    else if (prop == &NodeColorArray) {
        pcMatBinding->value = SoMaterialBinding::PER_VERTEX_INDEXED;
    }

    pcShapeMaterial->diffuseColor.setNum(elemSize);
    SbColor* diffuse = pcShapeMaterial->diffuseColor.startEditing();
    pcShapeMaterial->ambientColor.setNum(elemSize);
    SbColor* ambient = pcShapeMaterial->ambientColor.startEditing();
    pcShapeMaterial->specularColor.setNum(elemSize);
    SbColor* specular = pcShapeMaterial->specularColor.startEditing();
    pcShapeMaterial->emissiveColor.setNum(elemSize);
    SbColor* emissive = pcShapeMaterial->emissiveColor.startEditing();
    pcShapeMaterial->shininess.setNum(elemSize);
    float* shininess = pcShapeMaterial->shininess.startEditing();
    pcShapeMaterial->transparency.setNum(elemSize);
    float* transparency = pcShapeMaterial->transparency.startEditing();

    vecColor.resize(elemSize, baseDif);

    int i = 0;
    for (const Base::Color& c : vecColor) {
        diffuse[i] = SbColor(c.r, c.g, c.b);
        ambient[i] = SbColor(baseAmb.r, baseAmb.g, baseAmb.b);
        specular[i] = SbColor(baseSpe.r, baseSpe.g, baseSpe.b);
        emissive[i] = SbColor(baseEmi.r, baseEmi.g, baseEmi.b);
        shininess[i] = baseShi;
        transparency[i] = baseTra;
        ++i;
    }

    pcShapeMaterial->diffuseColor.finishEditing();
    pcShapeMaterial->ambientColor.finishEditing();
    pcShapeMaterial->specularColor.finishEditing();
    pcShapeMaterial->emissiveColor.finishEditing();
    pcShapeMaterial->shininess.finishEditing();
    pcShapeMaterial->transparency.finishEditing();

    pcFaces->touch();
}

void ViewProviderFemMesh::resetColorByElementId()
{
    const Base::Color& c = ShapeAppearance.getDiffuseColor();
    ElementColorArray.setValue(c);
}

// ----------------------------------------------------------------------------

void ViewProviderFEMMeshBuilder::buildNodes(const App::Property* prop,
                                            std::vector<SoNode*>& nodes) const
{
    SoCoordinate3* pcPointsCoord = nullptr;
    SoIndexedFaceSet* pcFaces = nullptr;
    SoIndexedLineSet* pcLines = nullptr;

    if (nodes.empty()) {
        pcPointsCoord = new SoCoordinate3();
        nodes.push_back(pcPointsCoord);
        pcFaces = new SoIndexedFaceSet();
        pcLines = new SoIndexedLineSet();
        nodes.push_back(pcFaces);
    }
    else if (nodes.size() == 2) {
        if (nodes[0]->getTypeId() == SoCoordinate3::getClassTypeId()) {
            pcPointsCoord = static_cast<SoCoordinate3*>(nodes[0]);
        }
        if (nodes[1]->getTypeId() == SoIndexedFaceSet::getClassTypeId()) {
            pcFaces = static_cast<SoIndexedFaceSet*>(nodes[1]);
        }
    }

    if (pcPointsCoord && pcFaces && pcLines) {
        std::vector<unsigned long> vFaceElementIdx;
        std::vector<unsigned long> vNodeElementIdx;
        bool onlyEdges;
        createMesh(prop,
                   pcPointsCoord,
                   pcFaces,
                   pcLines,
                   vFaceElementIdx,
                   vNodeElementIdx,
                   onlyEdges,
                   false,
                   0);
    }
}

inline void insEdgeVec(std::map<int, std::set<int>>& map, int n1, int n2)
{
    // FIXME: The if-else distinction doesn't make sense
    // if (n1<n2)
    //     map[n2].insert(n1);
    // else
    map[n2].insert(n1);
}

inline unsigned long ElemFold(unsigned long Element, unsigned long FaceNbr)
{
    unsigned long t1 = Element << 3;
    unsigned long t2 = t1 | FaceNbr;
    return t2;
}

void ViewProviderFEMMeshBuilder::createMesh(const App::Property* prop,
                                            SoCoordinate3* coords,
                                            SoIndexedFaceSet* faces,
                                            SoIndexedLineSet* lines,
                                            std::vector<unsigned long>& vFaceElementIdx,
                                            std::vector<unsigned long>& vNodeElementIdx,
                                            bool& onlyEdges,
                                            bool ShowInner,
                                            int MaxFacesShowInner) const
{

    const Fem::PropertyFemMesh* mesh = static_cast<const Fem::PropertyFemMesh*>(prop);

    const SMESHDS_Mesh* data = mesh->getValue().getSMesh()->GetMeshDS();

    int numFaces = data->NbFaces();
    int numNodes = data->NbNodes();
    int numEdges = data->NbEdges();

    if (numFaces + numNodes + numEdges == 0) {
        coords->point.setNum(0);
        faces->coordIndex.setNum(0);
        lines->coordIndex.setNum(0);
        return;
    }
    Base::TimeElapsed Start;
    Base::Console().log(
        "Start: ViewProviderFEMMeshBuilder::createMesh() =================================\n");

    const SMDS_MeshInfo& info = data->GetMeshInfo();
    int numTria = info.NbTriangles();
    int numQuad = info.NbQuadrangles();

    int numVolu = info.NbVolumes();
    int numTetr = info.NbTetras();
    int numHexa = info.NbHexas();
    int numPyrd = info.NbPyramids();
    int numPris = info.NbPrisms();


    bool ShowFaces = (numFaces > 0 && numVolu == 0);

    int numTries;
    if (ShowFaces) {
        numTries =
            numTria + numQuad /*+numPoly*/ + numTetr * 4 + numHexa * 6 + numPyrd * 5 + numPris * 5;
    }
    else {
        numTries = numTetr * 4 + numHexa * 6 + numPyrd * 5 + numPris * 5;
    }
    // It is not 100% sure that a prism in smesh is a pentahedron in any case, but it will be in
    // most cases! See https://forum.freecad.org/viewtopic.php?f=18&t=13583#p109707

    // corner case only edges (Beams) in the mesh. This need some special cases in building up
    // visual
    onlyEdges = false;
    if (numFaces <= 0 && numVolu <= 0 && numEdges > 0) {
        onlyEdges = true;
    }

    std::vector<FemFace> facesHelper(numTries);

    Base::Console().log("    %f: Start build up %i face helper\n",
                        Base::TimeElapsed::diffTimeF(Start, Base::TimeElapsed()),
                        facesHelper.size());
    Base::BoundBox3d BndBox;

    int i = 0;

    if (ShowFaces) {
        SMDS_FaceIteratorPtr aFaceIter = data->facesIterator();
        for (; aFaceIter->more();) {
            const SMDS_MeshFace* aFace = aFaceIter->next();

            int num = aFace->NbNodes();
            switch (num) {
                case 3:
                    // tria3 face = N1, N2, N3
                    BndBox.Add(facesHelper[i++].set(3,
                                                    aFace,
                                                    aFace->GetID(),
                                                    0,
                                                    aFace->GetNode(0),
                                                    aFace->GetNode(1),
                                                    aFace->GetNode(2)));
                    break;
                case 4:
                    // quad4 face = N1, N2, N3, N4
                    BndBox.Add(facesHelper[i++].set(4,
                                                    aFace,
                                                    aFace->GetID(),
                                                    0,
                                                    aFace->GetNode(0),
                                                    aFace->GetNode(1),
                                                    aFace->GetNode(2),
                                                    aFace->GetNode(3)));
                    break;
                case 6:
                    // tria6 face = N1, N4, N2, N5, N3, N6
                    BndBox.Add(facesHelper[i++].set(6,
                                                    aFace,
                                                    aFace->GetID(),
                                                    0,
                                                    aFace->GetNode(0),
                                                    aFace->GetNode(3),
                                                    aFace->GetNode(1),
                                                    aFace->GetNode(4),
                                                    aFace->GetNode(2),
                                                    aFace->GetNode(5)));
                    break;
                case 8:
                    // quad8 face = N1, N5, N2, N6, N3, N7, N4, N8
                    BndBox.Add(facesHelper[i++].set(8,
                                                    aFace,
                                                    aFace->GetID(),
                                                    0,
                                                    aFace->GetNode(0),
                                                    aFace->GetNode(4),
                                                    aFace->GetNode(1),
                                                    aFace->GetNode(5),
                                                    aFace->GetNode(2),
                                                    aFace->GetNode(6),
                                                    aFace->GetNode(3),
                                                    aFace->GetNode(7)));
                    break;
                default:
                    // unknown face type
                    throw std::runtime_error(
                        "Node count not supported by ViewProviderFemMesh, [3|4|6|8] are allowed");
            }
        }
    }
    else {

        // iterate all volumes
        SMDS_VolumeIteratorPtr aVolIter = data->volumesIterator();
        for (; aVolIter->more();) {
            const SMDS_MeshVolume* aVol = aVolIter->next();

            int num = aVol->NbNodes();

            switch (num) {
                // tetra4 volume
                case 4:
                    // face 1 = N1, N2, N3
                    // face 2 = N1, N4, N2
                    // face 3 = N2, N4, N3
                    // face 4 = N3, N4, N1
                    BndBox.Add(facesHelper[i++].set(3,
                                                    aVol,
                                                    aVol->GetID(),
                                                    1,
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(2)));
                    BndBox.Add(facesHelper[i++].set(3,
                                                    aVol,
                                                    aVol->GetID(),
                                                    2,
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(1)));
                    BndBox.Add(facesHelper[i++].set(3,
                                                    aVol,
                                                    aVol->GetID(),
                                                    3,
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(2)));
                    BndBox.Add(facesHelper[i++].set(3,
                                                    aVol,
                                                    aVol->GetID(),
                                                    4,
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(0)));
                    break;
                // pyra5 volume
                case 5:
                    // face 1 = N1, N2, N3, N4
                    // face 2 = N1, N5, N2
                    // face 3 = N2, N5, N3
                    // face 4 = N3, N5, N4
                    // face 5 = N4, N5, N1
                    BndBox.Add(facesHelper[i++].set(4,
                                                    aVol,
                                                    aVol->GetID(),
                                                    1,
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(3)));
                    BndBox.Add(facesHelper[i++].set(3,
                                                    aVol,
                                                    aVol->GetID(),
                                                    2,
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(1)));
                    BndBox.Add(facesHelper[i++].set(3,
                                                    aVol,
                                                    aVol->GetID(),
                                                    3,
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(2)));
                    BndBox.Add(facesHelper[i++].set(3,
                                                    aVol,
                                                    aVol->GetID(),
                                                    4,
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(3)));
                    BndBox.Add(facesHelper[i++].set(3,
                                                    aVol,
                                                    aVol->GetID(),
                                                    5,
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(0)));
                    break;
                // penta6 volume
                case 6:
                    // face 1 = N1, N2, N3
                    // face 2 = N4, N6, N5
                    // face 3 = N1, N4, N5, N2
                    // face 4 = N2, N5, N6, N3
                    // face 5 = N3, N6, N4, N1
                    BndBox.Add(facesHelper[i++].set(3,
                                                    aVol,
                                                    aVol->GetID(),
                                                    1,
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(2)));
                    BndBox.Add(facesHelper[i++].set(3,
                                                    aVol,
                                                    aVol->GetID(),
                                                    2,
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(5),
                                                    aVol->GetNode(4)));
                    BndBox.Add(facesHelper[i++].set(4,
                                                    aVol,
                                                    aVol->GetID(),
                                                    3,
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(1)));
                    BndBox.Add(facesHelper[i++].set(4,
                                                    aVol,
                                                    aVol->GetID(),
                                                    4,
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(5),
                                                    aVol->GetNode(2)));
                    BndBox.Add(facesHelper[i++].set(4,
                                                    aVol,
                                                    aVol->GetID(),
                                                    5,
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(5),
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(0)));
                    break;
                // hexa8 volume
                case 8:
                    // face 1 = N1, N2, N3, N4
                    // face 2 = N5, N8, N7, N6
                    // face 3 = N1, N5, N6, N2
                    // face 4 = N2, N6, N7, N3
                    // face 5 = N3, N7, N8, N4
                    // face 6 = N4, N8, N5, N1
                    BndBox.Add(facesHelper[i++].set(4,
                                                    aVol,
                                                    aVol->GetID(),
                                                    1,
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(3)));
                    BndBox.Add(facesHelper[i++].set(4,
                                                    aVol,
                                                    aVol->GetID(),
                                                    2,
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(7),
                                                    aVol->GetNode(6),
                                                    aVol->GetNode(5)));
                    BndBox.Add(facesHelper[i++].set(4,
                                                    aVol,
                                                    aVol->GetID(),
                                                    3,
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(5),
                                                    aVol->GetNode(1)));
                    BndBox.Add(facesHelper[i++].set(4,
                                                    aVol,
                                                    aVol->GetID(),
                                                    4,
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(5),
                                                    aVol->GetNode(6),
                                                    aVol->GetNode(2)));
                    BndBox.Add(facesHelper[i++].set(4,
                                                    aVol,
                                                    aVol->GetID(),
                                                    5,
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(6),
                                                    aVol->GetNode(7),
                                                    aVol->GetNode(3)));
                    BndBox.Add(facesHelper[i++].set(4,
                                                    aVol,
                                                    aVol->GetID(),
                                                    6,
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(7),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(0)));
                    break;
                // tetra10 volume
                case 10:
                    // face 1 = N1, N5,  N2, N6,  N3, N7
                    // face 2 = N1, N8,  N4, N9,  N2, N5
                    // face 3 = N2, N9,  N4, N10, N3, N6
                    // face 4 = N3, N10, N4, N8,  N1, N7
                    BndBox.Add(facesHelper[i++].set(6,
                                                    aVol,
                                                    aVol->GetID(),
                                                    1,
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(5),
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(6)));
                    BndBox.Add(facesHelper[i++].set(6,
                                                    aVol,
                                                    aVol->GetID(),
                                                    2,
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(7),
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(8),
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(4)));
                    BndBox.Add(facesHelper[i++].set(6,
                                                    aVol,
                                                    aVol->GetID(),
                                                    3,
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(8),
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(9),
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(5)));
                    BndBox.Add(facesHelper[i++].set(6,
                                                    aVol,
                                                    aVol->GetID(),
                                                    4,
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(9),
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(7),
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(6)));
                    break;
                // pyra13 volume
                case 13:
                    // face 1 = N1, N6, N2, N7,  N3,  N8, N4, N9
                    // face 2 = N1, N10, N5, N11, N2, N6
                    // face 3 = N2, N11, N5, N12, N3, N7
                    // face 4 = N3, N12, N5, N13, N4, N8
                    // face 5 = N4, N13, N5, N10, N1, N9
                    BndBox.Add(facesHelper[i++].set(8,
                                                    aVol,
                                                    aVol->GetID(),
                                                    1,
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(5),
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(6),
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(7),
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(8)));
                    BndBox.Add(facesHelper[i++].set(6,
                                                    aVol,
                                                    aVol->GetID(),
                                                    2,
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(9),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(10),
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(5)));
                    BndBox.Add(facesHelper[i++].set(6,
                                                    aVol,
                                                    aVol->GetID(),
                                                    3,
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(10),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(11),
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(6)));
                    BndBox.Add(facesHelper[i++].set(6,
                                                    aVol,
                                                    aVol->GetID(),
                                                    4,
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(11),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(12),
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(7)));
                    BndBox.Add(facesHelper[i++].set(6,
                                                    aVol,
                                                    aVol->GetID(),
                                                    5,
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(12),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(9),
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(8)));
                    break;
                // penta15 volume
                case 15:
                    // face 1 = N1, N7,  N2, N8,  N3, N9
                    // face 2 = N4, N12, N6, N11, N5, N10
                    // face 3 = N1, N13, N4, N10, N5, N14, N2, N7
                    // face 4 = N2, N14, N5, N11, N6, N15, N3, N8
                    // face 5 = N3, N15, N6, N12, N4, N13, N1, N9
                    BndBox.Add(facesHelper[i++].set(6,
                                                    aVol,
                                                    aVol->GetID(),
                                                    1,
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(6),
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(7),
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(8)));
                    BndBox.Add(facesHelper[i++].set(6,
                                                    aVol,
                                                    aVol->GetID(),
                                                    2,
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(11),
                                                    aVol->GetNode(5),
                                                    aVol->GetNode(10),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(9)));
                    BndBox.Add(facesHelper[i++].set(8,
                                                    aVol,
                                                    aVol->GetID(),
                                                    3,
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(12),
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(9),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(13),
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(6)));
                    BndBox.Add(facesHelper[i++].set(8,
                                                    aVol,
                                                    aVol->GetID(),
                                                    4,
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(13),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(10),
                                                    aVol->GetNode(5),
                                                    aVol->GetNode(14),
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(7)));
                    BndBox.Add(facesHelper[i++].set(8,
                                                    aVol,
                                                    aVol->GetID(),
                                                    5,
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(14),
                                                    aVol->GetNode(5),
                                                    aVol->GetNode(11),
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(12),
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(8)));
                    break;
                // hexa20 volume
                case 20:
                    // face 1 = N1, N9,  N2, N10, N3, N11, N4, N12
                    // face 2 = N5, N16, N8, N15, N7, N14, N6, N13
                    // face 3 = N1, N17, N5, N13, N6, N18, N2, N9
                    // face 4 = N2, N18, N6, N14, N7, N19, N3, N10
                    // face 5 = N3, N19, N7, N15, N8, N20, N4, N11
                    // face 6 = N4, N20, N8, N16, N5, N17, N1, N12
                    BndBox.Add(facesHelper[i++].set(8,
                                                    aVol,
                                                    aVol->GetID(),
                                                    1,
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(8),
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(9),
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(10),
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(11)));
                    BndBox.Add(facesHelper[i++].set(8,
                                                    aVol,
                                                    aVol->GetID(),
                                                    2,
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(15),
                                                    aVol->GetNode(7),
                                                    aVol->GetNode(14),
                                                    aVol->GetNode(6),
                                                    aVol->GetNode(13),
                                                    aVol->GetNode(5),
                                                    aVol->GetNode(12)));
                    BndBox.Add(facesHelper[i++].set(8,
                                                    aVol,
                                                    aVol->GetID(),
                                                    3,
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(16),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(12),
                                                    aVol->GetNode(5),
                                                    aVol->GetNode(17),
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(8)));
                    BndBox.Add(facesHelper[i++].set(8,
                                                    aVol,
                                                    aVol->GetID(),
                                                    4,
                                                    aVol->GetNode(1),
                                                    aVol->GetNode(17),
                                                    aVol->GetNode(5),
                                                    aVol->GetNode(13),
                                                    aVol->GetNode(6),
                                                    aVol->GetNode(18),
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(9)));
                    BndBox.Add(facesHelper[i++].set(8,
                                                    aVol,
                                                    aVol->GetID(),
                                                    5,
                                                    aVol->GetNode(2),
                                                    aVol->GetNode(18),
                                                    aVol->GetNode(6),
                                                    aVol->GetNode(14),
                                                    aVol->GetNode(7),
                                                    aVol->GetNode(19),
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(10)));
                    BndBox.Add(facesHelper[i++].set(8,
                                                    aVol,
                                                    aVol->GetID(),
                                                    6,
                                                    aVol->GetNode(3),
                                                    aVol->GetNode(19),
                                                    aVol->GetNode(7),
                                                    aVol->GetNode(15),
                                                    aVol->GetNode(4),
                                                    aVol->GetNode(16),
                                                    aVol->GetNode(0),
                                                    aVol->GetNode(11)));
                    break;
                // unknown volume type
                default:
                    throw std::runtime_error("Node count not supported by ViewProviderFemMesh, "
                                             "[4|5|6|8|10|13|15|20] are allowed");
            }
        }
    }
    int FaceSize = facesHelper.size();


    if (FaceSize < MaxFacesShowInner) {
        Base::Console().log("    %f: Start eliminate internal faces SIMPLE\n",
                            Base::TimeElapsed::diffTimeF(Start, Base::TimeElapsed()));

        // search for double (inside) faces and hide them
        if (!ShowInner) {
            for (int l = 0; l < FaceSize; l++) {
                if (!facesHelper[l].hide) {
                    for (int i = l + 1; i < FaceSize; i++) {
                        if (facesHelper[l].isSameFace(facesHelper[i])) {
                            break;
                        }
                    }
                }
            }
        }
    }
    else {
        Base::Console().log("    %f: Start eliminate internal faces GRID\n",
                            Base::TimeElapsed::diffTimeF(Start, Base::TimeElapsed()));
        BndBox.Enlarge(BndBox.CalcDiagonalLength() / 10000.0);
        // calculate grid properties
        double edge = pow(FaceSize, 1.0 / 3.0);
        double edgeL = BndBox.LengthX() + BndBox.LengthY() + BndBox.LengthZ();
        double gridFactor = 5.0;
        double size = (edgeL / (3 * edge)) * gridFactor;

        unsigned int NbrX = (unsigned int)(BndBox.LengthX() / size) + 1;
        unsigned int NbrY = (unsigned int)(BndBox.LengthY() / size) + 1;
        unsigned int NbrZ = (unsigned int)(BndBox.LengthZ() / size) + 1;
        Base::Console().log("      Size:F:%f,  X:%i  ,Y:%i  ,Z:%i\n", gridFactor, NbrX, NbrY, NbrZ);

        double Xmin = BndBox.MinX;
        double Ymin = BndBox.MinY;
        double Zmin = BndBox.MinZ;
        double Xln = BndBox.LengthX() / NbrX;
        double Yln = BndBox.LengthY() / NbrY;
        double Zln = BndBox.LengthZ() / NbrZ;

        std::vector<FemFaceGridItem> Grid(static_cast<size_t>(NbrX) * static_cast<size_t>(NbrY)
                                          * static_cast<size_t>(NbrZ));


        unsigned int iX = 0;
        unsigned int iY = 0;
        unsigned int iZ = 0;

        for (int l = 0; l < FaceSize; l++) {
            Base::Vector3d point(facesHelper[l].getFirstNodePoint());
            double x = (point.x - Xmin) / Xln;
            double y = (point.y - Ymin) / Yln;
            double z = (point.z - Zmin) / Zln;

            iX = x;
            iY = y;
            iZ = z;

            if (iX >= NbrX || iY >= NbrY || iZ >= NbrZ) {
                Base::Console().log("      Outof range!\n");
            }

            Grid[iX + iY * NbrX + iZ * NbrX * NbrY].push_back(&facesHelper[l]);
        }

        unsigned int max = 0, avg = 0;
        for (const auto& it : Grid) {
            for (size_t l = 0; l < it.size(); l++) {
                if (!it[l]->hide) {
                    for (size_t i = l + 1; i < it.size(); i++) {
                        if (it[l]->isSameFace(*(it[i]))) {
                            break;
                        }
                    }
                }
            }
            if (it.size() > max) {
                max = it.size();
            }
            avg += it.size();
        }
        avg = avg / Grid.size();

        Base::Console().log("      VoxelSize: Max:%i ,Average:%i\n", max, avg);

    }  // if( FaceSize < 1000)


    Base::Console().log("    %f: Start build up node map\n",
                        Base::TimeElapsed::diffTimeF(Start, Base::TimeElapsed()));

    // sort out double nodes and build up index map
    std::map<const SMDS_MeshNode*, int> mapNodeIndex;

    // handling the corner case beams only, means no faces/triangles only nodes and edges
    if (onlyEdges) {

        SMDS_EdgeIteratorPtr aEdgeIte = data->edgesIterator();
        for (; aEdgeIte->more();) {
            const SMDS_MeshEdge* aEdge = aEdgeIte->next();
            int num = aEdge->NbNodes();
            for (int i = 0; i < num; i++) {
                mapNodeIndex[aEdge->GetNode(i)] = 0;
            }
        }
    }
    else {

        for (int l = 0; l < FaceSize; l++) {
            if (!facesHelper[l].hide) {
                for (auto Node : facesHelper[l].Nodes) {
                    if (Node) {
                        mapNodeIndex[Node] = 0;
                    }
                    else {
                        break;
                    }
                }
            }
        }
    }
    Base::Console().log("    %f: Start set point vector\n",
                        Base::TimeElapsed::diffTimeF(Start, Base::TimeElapsed()));

    // set the point coordinates
    coords->point.setNum(mapNodeIndex.size());
    vNodeElementIdx.resize(mapNodeIndex.size());
    std::map<const SMDS_MeshNode*, int>::iterator it = mapNodeIndex.begin();
    SbVec3f* verts = coords->point.startEditing();
    for (int i = 0; it != mapNodeIndex.end(); ++it, i++) {
        verts[i].setValue((float)it->first->X(), (float)it->first->Y(), (float)it->first->Z());
        it->second = i;
        // set selection idx
        vNodeElementIdx[i] = it->first->GetID();
    }
    coords->point.finishEditing();


    // count triangle size
    Base::Console().log("    %f: Start count triangle size\n",
                        Base::TimeElapsed::diffTimeF(Start, Base::TimeElapsed()));
    int triangleCount = 0;
    for (int l = 0; l < FaceSize; l++) {
        if (!facesHelper[l].hide) {
            switch (facesHelper[l].Size) {
                case 3:
                    triangleCount++;
                    break;  // 3-node triangle face   --> 1 triangle
                case 4:
                    triangleCount += 2;
                    break;  // 4-node quadrangle face --> 2 triangles
                case 6:
                    triangleCount += 4;
                    break;  // 6-node triangle face   --> 4 triangles
                case 8:
                    triangleCount += 6;
                    break;  // 8-node quadrangle face --> 6 triangles
                default:
                    throw std::runtime_error(
                        "Face with unknown node count found, only display mode nodes is supported "
                        "for this element (tiangleCount)");
            }
        }
    }
    Base::Console().log("    NumTriangles:%i\n", triangleCount);
    // edge map collect and sort edges of the faces to be shown.
    std::map<int, std::set<int>> EdgeMap;

    // handling the corner case beams only, means no faces/triangles only nodes and edges
    if (onlyEdges) {

        SMDS_EdgeIteratorPtr aEdgeIte = data->edgesIterator();
        for (; aEdgeIte->more();) {
            const SMDS_MeshEdge* aEdge = aEdgeIte->next();
            int num = aEdge->NbNodes();
            switch (num) {
                case 2: {  // Seg2: N1, N2
                    int nIdx0 = mapNodeIndex[aEdge->GetNode(0)];
                    int nIdx1 = mapNodeIndex[aEdge->GetNode(1)];
                    insEdgeVec(EdgeMap, nIdx0, nIdx1);
                    break;
                }

                case 3: {  // Seg3: N1, N2, N3 (N3 is middle Node)
                    int nIdx0 = mapNodeIndex[aEdge->GetNode(0)];
                    int nIdx1 = mapNodeIndex[aEdge->GetNode(1)];
                    int nIdx2 = mapNodeIndex[aEdge->GetNode(2)];
                    insEdgeVec(EdgeMap, nIdx0, nIdx2);
                    insEdgeVec(EdgeMap, nIdx2, nIdx1);
                    break;
                }
            }
        }
    }

    Base::Console().log("    %f: Start build up triangle vector\n",
                        Base::TimeElapsed::diffTimeF(Start, Base::TimeElapsed()));
    // set the triangle face indices
    faces->coordIndex.setNum(4 * triangleCount);
    vFaceElementIdx.resize(triangleCount);
    int index = 0, indexIdx = 0;
    int32_t* indices = faces->coordIndex.startEditing();
    // iterate all non-hidden element faces, always assure CLOCKWISE triangle ordering to allow
    // backface culling
    for (int l = 0; l < FaceSize; l++) {
        if (!facesHelper[l].hide) {
            switch (facesHelper[l].Element->NbNodes()) {
                // 3 nodes
                case 3:
                    // tria3 face
                    switch (facesHelper[l].FaceNo) {
                        case 0: {  // tria3 face, 3-node triangle
                            // prefeche all node indexes of this face
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            // create triangle number 1
                            // ---------------------------------------------- fill in the node
                            // indexes in CLOCKWISE order
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            // add the three edge segments for that triangle
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx0);
                            // remember the element and face number for that triangle
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            break;
                        }
                        default:
                            assert(0);
                    }
                    break;
                // 4 nodes
                case 4:
                    // quad4 face
                    // tetra4 volume, four 3-node triangles
                    switch (facesHelper[l].FaceNo) {
                        case 0: {  // quad4 face, 4-node quadrangle
                            // prefeche all node indexes of this face
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            // create triangle number 1
                            // ---------------------------------------------- fill in the node
                            // indexes in CLOCKWISE order
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            // add the two edge segments for that triangle
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            // remember the element and face number for that triangle
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            // create triangle number 2
                            // ----------------------------------------------
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx0;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            insEdgeVec(EdgeMap, nIdx3, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            break;
                        }
                        case 1: {  // tetra4 volume: face 1, 3-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            break;
                        }
                        case 2: {  // tetra4 volume: face 2, 3-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 1);
                            break;
                        }
                        case 3: {  // tetra4 volume: face 3, 3-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            break;
                        }
                        case 4: {  // tetra4 volume: face 4, 3-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            break;
                        }
                        default:
                            assert(0);
                    }
                    break;
                // 5 nodes
                case 5:
                    // pyra5 volume, one 4-node quadrangle and four 3-node triangles
                    switch (facesHelper[l].FaceNo) {
                        case 1: {  // pyra5 volume: face 1, 4-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx0;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            insEdgeVec(EdgeMap, nIdx3, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            break;
                        }
                        case 2: {  // pyra5 volume: face 2, 3-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 1);
                            break;
                        }
                        case 3: {  // pyra5 volume: face 3, 3-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            break;
                        }
                        case 4: {  // pyra5 volume: face 4, 3-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            break;
                        }
                        case 5: {  // pyra5 volume: face 5, 3-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 4);
                            break;
                        }
                        default:
                            assert(0);
                    }
                    break;
                // 6 nodes
                case 6:
                    // tria6 face
                    // penta6 volume, two 3-node triangle and three 4-node quadrangles
                    switch (facesHelper[l].FaceNo) {
                        case 0: {  // tria6 face, 6-node triangle
                            // prefeche all node indexes of this face
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            // create triangle number 1
                            // ---------------------------------------------- fill in the node
                            // indexes in CLOCKWISE order
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            // add the two edge segments for that triangle
                            insEdgeVec(EdgeMap, nIdx5, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            // remember the element and face number for that triangle
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            // create triangle number 2
                            // ----------------------------------------------
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            // create triangle number 3
                            // ----------------------------------------------
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            // create triangle number 4
                            // ----------------------------------------------
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            // this triangle has no edge (inner triangle).
                            break;
                        }
                        case 1: {  // penta6 volume: face 1, 3-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            break;
                        }
                        case 2: {  // penta6 volume: face 2, 3-node triangle
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx4;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx5);
                            insEdgeVec(EdgeMap, nIdx5, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 1);
                            break;
                        }
                        case 3: {  // penta6 volume: face 3, 4-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx0;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            insEdgeVec(EdgeMap, nIdx3, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            break;
                        }
                        case 4: {  // penta6 volume: face 4, 4-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx0;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            insEdgeVec(EdgeMap, nIdx3, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            break;
                        }
                        case 5: {  // penta6 volume: face 5, 4-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 4);
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx0;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            insEdgeVec(EdgeMap, nIdx3, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 4);
                            break;
                        }
                        default:
                            assert(0);
                    }
                    break;
                // 8 nodes
                case 8:
                    // quad8 face
                    // hexa8 volume, six 4-node quadrangles
                    switch (facesHelper[l].FaceNo) {
                        case 0: {  // quad8 face, 8-node quadrangle
                            // prefeche all node indexes of this face
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            int nIdx6 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx7 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            // create triangle number 1
                            // ---------------------------------------------- fill in the node
                            // indexes in CLOCKWISE order
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            // add the two edge segments for that triangle
                            insEdgeVec(EdgeMap, nIdx7, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            // remember the element and face number for that triangle
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            // create triangle number 2
                            // ----------------------------------------------
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            // create triangle number 3
                            // ----------------------------------------------
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            // create triangle number 4
                            // ----------------------------------------------
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx6);
                            insEdgeVec(EdgeMap, nIdx6, nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            // create triangle number 5
                            // ----------------------------------------------
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            // this triangle has no edge (inner triangle)
                            // create triangle number 6
                            // ----------------------------------------------
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            // this triangle has no edge (inner triangle)
                            break;
                        }
                        case 1: {  // hexa8 volume: face 1, 4-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx0;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            insEdgeVec(EdgeMap, nIdx3, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            break;
                        }
                        case 2: {  // hexa8 volume: face 2, 4-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 1);
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx0;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            insEdgeVec(EdgeMap, nIdx3, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 1);
                            break;
                        }
                        case 3: {  // hexa8 volume: face 3, 4-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx0;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            insEdgeVec(EdgeMap, nIdx3, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            break;
                        }
                        case 4: {  // hexa8 volume: face 4, 4-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx0;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            insEdgeVec(EdgeMap, nIdx3, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            break;
                        }
                        case 5: {  // hexa8 volume: face 5, 4-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 4);
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx0;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            insEdgeVec(EdgeMap, nIdx3, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 4);
                            break;
                        }
                        case 6: {  // hexa8 volume: face 6, 4-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 5);
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx0;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            insEdgeVec(EdgeMap, nIdx3, nIdx0);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 5);
                            break;
                        }
                        default:
                            assert(0);
                    }
                    break;
                // 10 nodes
                case 10:
                    // tetra10 volume, four 6-node triangles
                    switch (facesHelper[l].FaceNo) {
                        case 1: {  // tetra10 volume: face 1, 6-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        case 2: {  // tetra10 volume: face 2, 6-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(8)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 1);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 1);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 1);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        case 3: {  // tetra10 volume: face 3, 6-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(8)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(9)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        case 4: {  // tetra10 volume: face 4, 6-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(9)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        default:
                            assert(0);
                    }
                    break;
                // 13 nodes
                case 13:
                    // pyra13 volume, four 6-node triangle and one 8-node quadrangles
                    switch (facesHelper[l].FaceNo) {
                        case 1: {  // pyra13 volume: face 1, 8-node quadrangles
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            int nIdx6 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx7 = mapNodeIndex[facesHelper[l].Element->GetNode(8)];
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx7, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx6);
                            insEdgeVec(EdgeMap, nIdx6, nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        case 2: {  // pyra13 volume: face 2, 6-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(9)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(10)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        case 3: {  // pyra13 volume: face 3, 6-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(10)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(11)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        case 4: {  // pyra13 volume: face 4, 6-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(11)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(12)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        case 5: {  // pyra13 volume: face 5, 6-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(12)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(9)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(8)];
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        default:
                            assert(0);
                    }
                    break;
                // 15 nodes
                case 15:
                    // penta15 volume, two 6-node triangles and three 8-node quadrangles
                    switch (facesHelper[l].FaceNo) {
                        case 1: {  // penta15 volume: face 1, 6-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(8)];
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        case 2: {  // penta15 volume: face 2, 6-node triangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(11)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(10)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(9)];
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 1);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 1);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 1);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        case 3: {  // penta15 volume: face 3, 8-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(12)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(9)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(13)];
                            int nIdx6 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx7 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx7, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx6);
                            insEdgeVec(EdgeMap, nIdx6, nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        case 4: {  // penta15 volume: face 4, 8-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(13)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(10)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(14)];
                            int nIdx6 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx7 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx7, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx6);
                            insEdgeVec(EdgeMap, nIdx6, nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        case 5: {  // penta15 volume: face 5, 8-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(14)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(11)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(12)];
                            int nIdx6 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx7 = mapNodeIndex[facesHelper[l].Element->GetNode(8)];
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx7, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 4);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 4);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 4);
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx6);
                            insEdgeVec(EdgeMap, nIdx6, nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 4);
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        default:
                            assert(0);
                    }
                    break;
                // 20 nodes
                case 20:
                    // hexa20 volume, six 8-node quadrangles
                    switch (facesHelper[l].FaceNo) {
                        case 1: {  // hexa20 volume: face 1
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(8)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(9)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(10)];
                            int nIdx6 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx7 = mapNodeIndex[facesHelper[l].Element->GetNode(11)];
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx7, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx6);
                            insEdgeVec(EdgeMap, nIdx6, nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        case 2: {  // hexa20 volume: face 2, 8-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(15)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(14)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(13)];
                            int nIdx6 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx7 = mapNodeIndex[facesHelper[l].Element->GetNode(12)];
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx7, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 1);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 1);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 1);
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx6);
                            insEdgeVec(EdgeMap, nIdx6, nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 1);
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        case 3: {  // hexa20 volume: face 3, 8-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(16)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(12)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(17)];
                            int nIdx6 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx7 = mapNodeIndex[facesHelper[l].Element->GetNode(8)];
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx7, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx6);
                            insEdgeVec(EdgeMap, nIdx6, nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 2);
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        case 4: {  // hexa20 volume: face 4, 8-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(17)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(13)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(18)];
                            int nIdx6 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx7 = mapNodeIndex[facesHelper[l].Element->GetNode(9)];
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx7, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx6);
                            insEdgeVec(EdgeMap, nIdx6, nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 3);
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        case 5: {  // hexa20 volume: face 5, 8-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(18)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(14)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(19)];
                            int nIdx6 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx7 = mapNodeIndex[facesHelper[l].Element->GetNode(10)];
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx7, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 4);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 4);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 4);
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx6);
                            insEdgeVec(EdgeMap, nIdx6, nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 4);
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        case 6: {  // hexa20 volume: face 6, 8-node quadrangle
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(19)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(15)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(16)];
                            int nIdx6 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx7 = mapNodeIndex[facesHelper[l].Element->GetNode(11)];
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx7, nIdx0);
                            insEdgeVec(EdgeMap, nIdx0, nIdx1);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 5);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx1, nIdx2);
                            insEdgeVec(EdgeMap, nIdx2, nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 5);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx3, nIdx4);
                            insEdgeVec(EdgeMap, nIdx4, nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 5);
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap, nIdx5, nIdx6);
                            insEdgeVec(EdgeMap, nIdx6, nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 5);
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            break;
                        }
                        default:
                            assert(0);
                    }
                    break;

                // not implemented elements
                default:
                    throw std::runtime_error(
                        "Element with unknown node count found (may be not implemented), only "
                        "display mode nodes is supported for this element (NodeCount)");
            }
        }
    }

    faces->coordIndex.finishEditing();

    Base::Console().log("    %f: Start build up edge vector\n",
                        Base::TimeElapsed::diffTimeF(Start, Base::TimeElapsed()));
    // std::map<int,std::set<int> > EdgeMap;
    // count edges
    int EdgeSize = 0;
    for (std::map<int, std::set<int>>::const_iterator it = EdgeMap.begin(); it != EdgeMap.end();
         ++it) {
        EdgeSize += it->second.size();
    }

    // set the triangle face indices
    lines->coordIndex.setNum(3 * EdgeSize);
    index = 0;
    indices = lines->coordIndex.startEditing();

    for (std::map<int, std::set<int>>::const_iterator it = EdgeMap.begin(); it != EdgeMap.end();
         ++it) {
        for (std::set<int>::const_iterator it2 = it->second.begin(); it2 != it->second.end();
             ++it2) {
            indices[index++] = it->first;
            indices[index++] = *it2;
            indices[index++] = -1;
        }
    }

    lines->coordIndex.finishEditing();
    Base::Console().log("    NumEdges:%i\n", EdgeSize);

    Base::Console().log(
        "    %f: Finish =========================================================\n",
        Base::TimeElapsed::diffTimeF(Start, Base::TimeElapsed()));
}


// Python feature -----------------------------------------------------------------------

namespace Gui
{
/// @cond DOXERR
PROPERTY_SOURCE_TEMPLATE(FemGui::ViewProviderFemMeshPython, FemGui::ViewProviderFemMesh)
/// @endcond

// explicit template instantiation
template class FemGuiExport ViewProviderFeaturePythonT<ViewProviderFemMesh>;
}  // namespace Gui
