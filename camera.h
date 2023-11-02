#ifndef MY_CAMERA_H
#define MY_CAMERA_H

#include <pxr/pxr.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/matrix3f.h>
#include <pxr/base/gf/matrix3d.h>

#include <string>

#include "renderDelegate.h"

class MyCamera : public pxr::HdCamera
{
public:
    MyCamera(pxr::SdfPath const& sprimId, MyRenderDelegate* renderDelegate);
    ~MyCamera() override;

    pxr::HdDirtyBits GetInitialDirtyBitsMask() const override;
    void Sync(pxr::HdSceneDelegate* delegate, pxr::HdRenderParam* renderParam, pxr::HdDirtyBits* dirtyBits) override;

    const pxr::HdTimeSampleArray<pxr::GfMatrix4d, 16>& GetTimeSampleXforms() const {
        return _sampleXforms;
    }

private:
    MyRenderDelegate* _owner;
    pxr::HdTimeSampleArray<pxr::GfMatrix4d, 16> _sampleXforms;
};

#endif