#include "mesh.h"
#include "renderDelegate.h"
#include "renderPass.h"
#include "instancer.h"
#include <pxr/imaging/hd/extComputationUtils.h>
#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/vertexAdjacency.h>
#include <pxr/imaging/hd/smoothNormals.h>
#include <pxr/imaging/hd/instancer.h>
#include <pxr/imaging/pxOsd/tokens.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/usd/usdUtils/pipeline.h>

#include <algorithm> // sort

#include <gl/GL.h>


MyMesh::MyMesh(const pxr::SdfPath& id, MyRenderDelegate* delegate)
    : pxr::HdMesh(id)
    , _adjacencyValid(false)
    , _normalsValid(false)
    , _refined(false)
    , _smoothNormals(false)
    , _doubleSided(false)
    , _cullStyle(pxr::HdCullStyleDontCare)
    , _owner(delegate)
{
}

void
MyMesh::Finalize(pxr::HdRenderParam* renderParam)
{
}

pxr::HdDirtyBits
MyMesh::GetInitialDirtyBitsMask() const
{
    return pxr::HdChangeTracker::AllSceneDirtyBits;
}

pxr::HdDirtyBits
MyMesh::_PropagateDirtyBits(pxr::HdDirtyBits bits) const
{
    return bits;
}

void
MyMesh::_InitRepr(pxr::TfToken const& reprToken,
    pxr::HdDirtyBits* dirtyBits)
{
    TF_UNUSED(dirtyBits);
    _ReprVector::iterator it = std::find_if(_reprs.begin(), _reprs.end(),
        _ReprComparator(reprToken));
    if (it == _reprs.end()) {
        _reprs.emplace_back(reprToken, pxr::HdReprSharedPtr());
    }
}

void
MyMesh::Sync(pxr::HdSceneDelegate* sceneDelegate,
    pxr::HdRenderParam* renderParam,
    pxr::HdDirtyBits* dirtyBits,
    pxr::TfToken const& reprToken)
{
    _MeshReprConfig::DescArray descs = _GetReprDesc(reprToken);
    const pxr::HdMeshReprDesc& desc = descs[0];

    _PopulateMesh(sceneDelegate, dirtyBits, desc);
}

void
MyMesh::_PopulateMesh(pxr::HdSceneDelegate* sceneDelegate,
    pxr::HdDirtyBits* dirtyBits,
    pxr::HdMeshReprDesc const& desc)
{
    const pxr::SdfPath& id = GetId();

    // Synchronize instancer.
    _UpdateInstancer(sceneDelegate, dirtyBits);
    pxr::HdInstancer::_SyncInstancerAndParents(sceneDelegate->GetRenderIndex(), GetInstancerId());


    if (pxr::HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pxr::HdTokens->points)) {
        pxr::VtValue value = sceneDelegate->Get(id, pxr::HdTokens->points);
        _points = value.Get<pxr::VtVec3fArray>();
        _normalsValid = false;
    }

    if (pxr::HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pxr::HdTokens->displayColor)) {
        pxr::VtValue value = sceneDelegate->Get(id, pxr::HdTokens->displayColor);
        _displayColors = value.Get<pxr::VtVec3fArray>();
    }

    if (pxr::HdChangeTracker::IsTopologyDirty(*dirtyBits, id)) {
        pxr::PxOsdSubdivTags subdivTags = _topology.GetSubdivTags();
        int refineLevel = _topology.GetRefineLevel();
        _topology = pxr::HdMeshTopology(GetMeshTopology(sceneDelegate), refineLevel);
        _topology.SetSubdivTags(subdivTags);
        _adjacencyValid = false;
    }
    if (pxr::HdChangeTracker::IsSubdivTagsDirty(*dirtyBits, id) &&
        _topology.GetRefineLevel() > 0) {
        _topology.SetSubdivTags(sceneDelegate->GetSubdivTags(id));
    }
    if (pxr::HdChangeTracker::IsDisplayStyleDirty(*dirtyBits, id)) {
        pxr::HdDisplayStyle const displayStyle = sceneDelegate->GetDisplayStyle(id);
        _topology = pxr::HdMeshTopology(_topology,
            displayStyle.refineLevel);
    }

    if (pxr::HdChangeTracker::IsVisibilityDirty(*dirtyBits, id)) {
        _UpdateVisibility(sceneDelegate, dirtyBits);
    }

    if (pxr::HdChangeTracker::IsCullStyleDirty(*dirtyBits, id)) {
        _cullStyle = GetCullStyle(sceneDelegate);
    }
    if (pxr::HdChangeTracker::IsDoubleSidedDirty(*dirtyBits, id)) {
        _doubleSided = IsDoubleSided(sceneDelegate);
    }
    if (pxr::HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pxr::HdTokens->normals) ||
        pxr::HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pxr::HdTokens->widths) ||
        pxr::HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pxr::HdTokens->primvar)) {
        //_UpdatePrimvarSources(sceneDelegate, *dirtyBits);
    }

    bool _materialChanged = false;
    auto matId = sceneDelegate->GetMaterialId(id);
    if (matId != GetMaterialId())
    {
        SetMaterialId(matId);
        _materialChanged = true;
    }


    bool doRefine = (desc.geomStyle == pxr::HdMeshGeomStyleSurf);
    doRefine = doRefine && (_topology.GetScheme() != pxr::PxOsdOpenSubdivTokens->none);
    doRefine = doRefine && (_topology.GetRefineLevel() > 0);
    _smoothNormals = !desc.flatShadingEnabled;
    _smoothNormals = _smoothNormals &&
        (_topology.GetScheme() != pxr::PxOsdOpenSubdivTokens->none) &&
        (_topology.GetScheme() != pxr::PxOsdOpenSubdivTokens->bilinear);
    bool authoredNormals = false;
    if (_primvarSourceMap.count(pxr::HdTokens->normals) > 0) {
        authoredNormals = true;
    }
    _smoothNormals = _smoothNormals && !authoredNormals;

    bool newMesh = false;
    if (pxr::HdChangeTracker::IsTopologyDirty(*dirtyBits, id) || doRefine != _refined)
    {
        newMesh = true;

        // Destroy the old mesh, if it exists.

        _refined = doRefine;

        // Force the smooth normals code to rebuild the "normals" primvar the
        // next time smooth normals is enabled.
        _normalsValid = false;
    }

 
    // Add mesh to our delegate
    if (newMesh || pxr::HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pxr::HdTokens->points))
    {
        _topology = sceneDelegate->GetMeshTopology(id);
        pxr::HdMeshUtil meshUtil(&_topology, id);
        pxr::VtIntArray trianglePrimitiveParams;
        meshUtil.ComputeTriangleIndices(&_triangulatedIndices, &trianglePrimitiveParams);

        // Get normals (smooth them for now)
        //
        auto& normals = sceneDelegate->Get(id, pxr::HdTokens->normals).Get<pxr::VtVec3fArray>();
        pxr::Hd_VertexAdjacency _adjacency;
        _adjacency.BuildAdjacencyTable(&_topology);
        auto _computedNormals = pxr::Hd_SmoothNormals::ComputeSmoothNormals(&_adjacency, _points.size(), _points.cdata());

        std::lock_guard<std::mutex> guard(_owner->rendererMutex());
        _owner->addMesh(id, this);
    }

    if (pxr::HdChangeTracker::IsTransformDirty(*dirtyBits, id))
    {
        // sample transform...
        _transform = pxr::GfMatrix4f(sceneDelegate->GetTransform(id));

        // ...and instancers
        if (!GetInstancerId().IsEmpty())
        {
            // retrieve instance transforms from the instancer.
            pxr::HdInstancer* instancer = sceneDelegate->GetRenderIndex().GetInstancer(GetInstancerId());
            _instancerTransforms = static_cast<MyInstancer*>(instancer)->ComputeInstanceTransforms(GetId());
        }
    }


    // Clean all dirty bits.
    *dirtyBits &= ~pxr::HdChangeTracker::AllSceneDirtyBits;
}

void MyMesh::drawGL()
{
    glPushMatrix();
    
    //if(_instancerTransforms.size() == 0)
    //glMultMatrixf( _transform.data() );

    // constant color for the whole mesh
    if (_displayColors.size() == 1)
    {
        auto& c = _displayColors[0];
        glColor3f(c.data()[0], c.data()[1], c.data()[2]);
    }

    size_t instances = pxr::GfMax(size_t(1), _instancerTransforms.size());

    for (size_t pi = 0; pi < instances; ++pi)
    {
        glPushMatrix();
        if (_instancerTransforms.size() > 0)
        {
            glMultMatrixd(_instancerTransforms[pi].data());
        }
        glMultMatrixf(_transform.data());
        glBegin(GL_TRIANGLES);
        for (int i = 0; i < _triangulatedIndices.size(); ++i)
        {
            for (int ti = 0; ti < 3; ++ti)
            {
                auto& p = _points[_triangulatedIndices[i][ti]];
                if (_displayColors.size() == _points.size())
                {
                    // per-vertex color
                    auto& c = _displayColors[_triangulatedIndices[i][ti]];
                    glColor3f(c.data()[0], c.data()[1], c.data()[2]);
                }
                glVertex3f(p.data()[0], p.data()[1], p.data()[2]);
            }
        }
        glEnd();
        glPopMatrix();
    }

    glPopMatrix();
}
