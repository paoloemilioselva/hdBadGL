#include "renderPass.h"
#include "renderBuffer.h"
#include "renderDelegate.h"
#include "camera.h"

#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/glf/glContext.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/quaternion.h>
#include <pxr/base/gf/matrix3d.h>

#include <iostream>
#include <bitset>

#include <gl/gl.h>

MyRenderPass::MyRenderPass(
    pxr::HdRenderIndex* index, 
    pxr::HdRprimCollection const& collection,
    pxr::HdRenderThread* renderThread,
    MyRenderDelegate* renderDelegate) 
    : pxr::HdRenderPass(index, collection)
    , _viewMatrix(1.0f) // == identity
    , _projMatrix(1.0f) // == identity
    , _aovBindings()
    , _colorBuffer(pxr::SdfPath::EmptyPath())
    , _renderThread(renderThread)
    , _owner(renderDelegate)
{
}

MyRenderPass::~MyRenderPass()
{
}

bool MyRenderPass::IsConverged() const
{
    if (_aovBindings.size() == 0) 
        return true;

    for (size_t i = 0; i < _aovBindings.size(); ++i) 
        if (_aovBindings[i].renderBuffer && !_aovBindings[i].renderBuffer->IsConverged()) 
            return false;
    return true;
}

static
pxr::GfRect2i
_GetDataWindow(pxr::HdRenderPassStateSharedPtr const& renderPassState)
{
    const pxr::CameraUtilFraming& framing = renderPassState->GetFraming();
    if (framing.IsValid()) {
        return framing.dataWindow;
    }
    else {
        const pxr::GfVec4f vp = renderPassState->GetViewport();
        return pxr::GfRect2i(pxr::GfVec2i(0), int(vp[2]), int(vp[3]));
    }
}

void MyRenderPass::_Execute(
    pxr::HdRenderPassStateSharedPtr const& renderPassState,
    pxr::TfTokenVector const& renderTags)
{
    bool needStartRender = false;

    // has the camera moved ?
    //
    const pxr::GfMatrix4d view = renderPassState->GetWorldToViewMatrix();
    const pxr::GfMatrix4d proj = renderPassState->GetProjectionMatrix();

    if (_viewMatrix != view || _projMatrix != proj) 
    {
        _renderThread->StopRender();
        needStartRender = true;
        _viewMatrix = view;
        _projMatrix = proj;
    }

    // has the frame been resized ?
    //
    const pxr::GfRect2i dataWindow = _GetDataWindow(renderPassState);
    if (_dataWindow != dataWindow) 
    {
        _renderThread->StopRender();
        needStartRender = true;
        _dataWindow = dataWindow;
        const pxr::GfVec3i dimensions(_dataWindow.GetWidth(), _dataWindow.GetHeight(), 1);
        _colorBuffer.Allocate(dimensions, pxr::HdFormatFloat16Vec4, false);

        // resize your custom buffer, if any
        //_owner->ResizeBuffer(_dataWindow.GetWidth(), _dataWindow.GetHeight());
    }

    // empty AOVs ?
    //
    pxr::HdRenderPassAovBindingVector aovBindings = renderPassState->GetAovBindings();
    if (aovBindings.empty())
    {
        _renderThread->StopRender();
        needStartRender = true;
        pxr::HdRenderPassAovBinding colorAov;
        colorAov.aovName = pxr::HdAovTokens->color;
        colorAov.renderBuffer = &_colorBuffer;
        colorAov.clearValue = pxr::VtValue(pxr::GfVec4f(0.5f, 0.0f, 0.0f, 1.0f));
        aovBindings.push_back(colorAov);
    }

    //if (aovBindings.size() > 0 && aovBindings[0].aovName == HdAovTokens->color)
    //{
    //    std::cout << "found color" << std::endl;

    //    MyRenderBuffer* rb = static_cast<MyRenderBuffer*>(aovBindings[0].renderBuffer);
    //    //rb->Map();
    //    
    //    GfVec4f sample(1.0,0.0,0.0,1.0);
    //    float* sampleData = sample.data();
    //    for(int x =0;x<100;++x)
    //        for (int y = 0; y < 100; ++y)
    //            rb->Write(GfVec3i(x,y, 1), 4, sampleData);

    //    //rb->Unmap();
    //}

    if(_aovBindings != aovBindings)
    {
        _renderThread->StopRender();
        needStartRender = true;
        _aovBindings = aovBindings;
    }

    if( needStartRender )
    {
        _renderThread->StartRender();
    }

    // get camera from renderPassState or from RenderSettings
    // renderPassState camera wins (from prman)
    pxr::SdfPath hdCameraPath;
    if (const MyCamera* const cam =
        static_cast<const MyCamera*>(
            renderPassState->GetCamera()))
    {
        hdCameraPath = cam->GetId();
    }
    else
    {
        // get from _owner->GetRenderSettings...

    }
    //auto* hdCamera = static_cast<const MyCamera*>(
    //    GetRenderIndex()->GetSprim(pxr::HdPrimTypeTokens->camera, hdCameraPath));
    
    // update camera view
    // Do we need to get the sampleXform param here instead ?
    //auto& passMatrix = hdCamera->GetTransform();

    // update your camera-matrix here...
    glDisable(GL_BLEND);
    glDisable(GL_LIGHTING);

    glPushMatrix();
    //glClearColor(0.18f, 0.18f, 0.18, 1.0f);
    //glClear(GL_DEPTH_BUFFER_BIT);

    //glViewport(i_posx, i_posy, i_width, i_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMultMatrixd(proj.data());
    glMultMatrixd(view.data());
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // ...update/draw your scene
    bool needsRestart = _owner->UpdateScene();

    glPopMatrix();
}
