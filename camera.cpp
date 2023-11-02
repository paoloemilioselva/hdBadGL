#include "camera.h"

MyCamera::MyCamera(pxr::SdfPath const& rprimId, MyRenderDelegate* renderDelegate ) :
    pxr::HdCamera(rprimId),
    _owner(renderDelegate)
{
}

MyCamera::~MyCamera()
{
}

pxr::HdDirtyBits MyCamera::GetInitialDirtyBitsMask() const
{
    return pxr::HdCamera::DirtyParams;
}

void MyCamera::Sync(pxr::HdSceneDelegate* delegate, pxr::HdRenderParam* renderParam, pxr::HdDirtyBits* dirtyBits )
{
    // this is triggered for all cameras in the scene AND for the "viewport" camera
    // so, unless we look for a specific camera and only update that, we
    // should do the official update for the "viewport" camera directly in RenderPass
    // instead of here.
    // We are going to sample the transforms for the camera here
    // and use it later in RenderPass

    const auto& id = GetId();

    if (*dirtyBits & DirtyTransform)
    {
        delegate->SampleTransform(id, &_sampleXforms);
    }

    pxr::HdCamera::Sync(delegate, renderParam, dirtyBits);

    // We don't need to clear the dirty bits since HdCamera::Sync always clears
    // all the dirty bits.
}