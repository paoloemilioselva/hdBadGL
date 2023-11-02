#ifndef MY_INSTANCER_H
#define MY_INSTANCER_H

#include <pxr/imaging/hd/instancer.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/pxr.h>
#include <pxr/base/gf/half.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/quatd.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/vtBufferSource.h>

class MyInstancer : public pxr::HdInstancer
{
public:
    MyInstancer(pxr::HdSceneDelegate* delegate, pxr::SdfPath const& id);
    ~MyInstancer();

    void Sync(pxr::HdSceneDelegate* sceneDelegate, pxr::HdRenderParam* renderParam, pxr::HdDirtyBits* dirtyBits) override;

    pxr::VtMatrix4dArray ComputeInstanceTransforms(pxr::SdfPath const& prototypeId);

private:
    void _SyncPrimvars(pxr::HdDirtyBits* dirtyBits);

    pxr::HdTimeSampleArray< pxr::VtMatrix4dArray, 16 > _sampleXforms;

    pxr::TfHashMap<pxr::TfToken, pxr::HdVtBufferSource*, pxr::TfToken::HashFunctor> _primvarMap;
};

#endif