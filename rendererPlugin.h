#ifndef MY_RENDERERPLUGIN_H
#define MY_RENDERERPLUGIN_H

#include <pxr/imaging/hd/rendererPlugin.h>
#include <pxr/pxr.h>

class BadGLPlugin final : public pxr::HdRendererPlugin
{
public:
    BadGLPlugin();
    virtual ~BadGLPlugin();

    pxr::HdRenderDelegate* CreateRenderDelegate() override;
    pxr::HdRenderDelegate* CreateRenderDelegate(pxr::HdRenderSettingsMap const& settingsMap) override;

    void DeleteRenderDelegate(pxr::HdRenderDelegate* renderDelegate) override;

#if PXR_VERSION > 2205
    bool IsSupported(bool) const override;
#else
    bool IsSupported() const override;
#endif

private:
    BadGLPlugin(const BadGLPlugin&) = delete;
    BadGLPlugin& operator =(const BadGLPlugin&) = delete;

};

#endif
