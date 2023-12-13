#ifndef MY_RENDERPASS_H
#define MY_RENDERPASS_H

#include <pxr/pxr.h>

#include <pxr/imaging/hd/renderPass.h>
#include <pxr/imaging/hd/renderThread.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/rect2i.h>

#include "renderBuffer.h"
#include "renderDelegate.h"

#include "glad.h"

class MyRenderPass final : public pxr::HdRenderPass
{
public:
    MyRenderPass(
        pxr::HdRenderIndex* index, 
        pxr::HdRprimCollection const& collection,
        pxr::HdRenderThread* renderThread,
        MyRenderDelegate* renderDelegate);

    ~MyRenderPass();

    bool IsConverged() const override;

protected:
    void _Execute(
        pxr::HdRenderPassStateSharedPtr const& renderPassState,
        pxr::TfTokenVector const& renderTags) override;

    void _MarkCollectionDirty() override {}

private:
    MyRenderDelegate* _owner;

    pxr::HdRenderPassAovBindingVector _aovBindings;
    pxr::GfRect2i _dataWindow;
    pxr::GfMatrix4d _viewMatrix;
    pxr::GfMatrix4d _projMatrix;
    MyRenderBuffer _colorBuffer;
    pxr::HdRenderThread* _renderThread;

    bool shaderCreated;
    GLuint _shaderProgram;
    GLuint _gBuffer;
    GLuint _frameBuffer;
};

#endif