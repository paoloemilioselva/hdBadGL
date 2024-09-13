#include "rendererPlugin.h"

#include <pxr/imaging/hd/rendererPluginRegistry.h>
#include "renderDelegate.h"

#include <iostream>

PXR_NAMESPACE_USING_DIRECTIVE

TF_REGISTRY_FUNCTION(TfType)
{
    pxr::HdRendererPluginRegistry::Define<BadGLPlugin>();
}

BadGLPlugin::BadGLPlugin() 
{
    std::cout << __FUNCTION__ << std::endl;
}

BadGLPlugin::~BadGLPlugin() 
{
    std::cout << __FUNCTION__ << std::endl;
}

pxr::HdRenderDelegate* BadGLPlugin::CreateRenderDelegate()
{
    std::cout << __FUNCTION__ << std::endl;
    return new MyRenderDelegate();
}

pxr::HdRenderDelegate* BadGLPlugin::CreateRenderDelegate(
    pxr::HdRenderSettingsMap const& settingsMap)
{
    std::cout << __FUNCTION__ << std::endl;
    return new MyRenderDelegate(settingsMap);
}

void BadGLPlugin::DeleteRenderDelegate(pxr::HdRenderDelegate* renderDelegate)
{
    std::cout << __FUNCTION__ << std::endl;
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

