#ifndef MY_MESH_H
#define MY_MESH_H

#include <pxr/imaging/hd/mesh.h>
#include <pxr/imaging/hd/enums.h>
#include <pxr/imaging/hd/vertexAdjacency.h>
#include <pxr/imaging/hd/vtBufferSource.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/imaging/hd/meshUtil.h>
#include <pxr/pxr.h>

class MyRenderDelegate;

class MyMesh final : public pxr::HdMesh
{
public:
    MyMesh(const pxr::SdfPath& id, MyRenderDelegate* delegate);

    virtual ~MyMesh() = default;

    virtual pxr::HdDirtyBits GetInitialDirtyBitsMask() const override;

    virtual void Sync(pxr::HdSceneDelegate* sceneDelegate,
        pxr::HdRenderParam* renderParam,
        pxr::HdDirtyBits* dirtyBits,
        pxr::TfToken const& reprToken) override;

    virtual void Finalize(pxr::HdRenderParam* renderParam) override;

    void drawGL();

protected:
    virtual void _InitRepr(pxr::TfToken const& reprToken,
        pxr::HdDirtyBits* dirtyBits) override;
    virtual pxr::HdDirtyBits _PropagateDirtyBits(pxr::HdDirtyBits bits) const override;

private:

    void _PopulateMesh(pxr::HdSceneDelegate* sceneDelegate,
        pxr::HdDirtyBits* dirtyBits,
        pxr::HdMeshReprDesc const& desc);

private:
    pxr::VtVec3fArray _points;
    pxr::VtVec3fArray _displayColors;
    pxr::HdMeshTopology _topology;
    pxr::GfMatrix4f _transform;
    pxr::VtMatrix4dArray _instancerTransforms;
    pxr::VtVec3iArray _triangulatedIndices;
    pxr::VtIntArray _trianglePrimitiveParams;
    pxr::VtVec3fArray _computedNormals;
    pxr::Hd_VertexAdjacency _adjacency;
    bool _adjacencyValid;
    bool _normalsValid;
    bool _refined;
    bool _smoothNormals;
    bool _doubleSided;
    pxr::HdCullStyle _cullStyle;
    struct PrimvarSource {
        pxr::VtValue data;
        pxr::HdInterpolation interpolation;
    };
    pxr::TfHashMap<pxr::TfToken, PrimvarSource, pxr::TfToken::HashFunctor> _primvarSourceMap;

    MyMesh(const MyMesh&) = delete;
    MyMesh& operator =(const MyMesh&) = delete;

    MyRenderDelegate* _owner;
};

#endif
