#include "rendererPlugin.h"

#include <pxr/imaging/hd/rendererPluginRegistry.h>
#include "renderDelegate.h"

PXR_NAMESPACE_USING_DIRECTIVE

TF_REGISTRY_FUNCTION(TfType)
{
    pxr::HdRendererPluginRegistry::Define<BadGLPlugin>();
}

BadGLPlugin::BadGLPlugin() {}

BadGLPlugin::~BadGLPlugin() {}

pxr::HdRenderDelegate* BadGLPlugin::CreateRenderDelegate()
{
    return new MyRenderDelegate();
}

pxr::HdRenderDelegate* BadGLPlugin::CreateRenderDelegate(
    pxr::HdRenderSettingsMap const& settingsMap)
{
    return new MyRenderDelegate(settingsMap);
}

void BadGLPlugin::DeleteRenderDelegate(pxr::HdRenderDelegate* renderDelegate)
{
    delete renderDelegate;
}

#if PXR_VERSION > 2205
bool BadGLPlugin::IsSupported(bool glEnabled) const
#else
bool BadGLPlugin::IsSupported() const 
#endif
{
    return true;
}

