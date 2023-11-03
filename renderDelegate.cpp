#include <pxr/imaging/hd/driver.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/glf/glContext.h>

#include "renderDelegate.h"
#include "renderPass.h"
#include "renderBuffer.h"
#include "mesh.h"
#include "camera.h"
#include "instancer.h"

#include <iostream>

#include <gl/GL.h>

std::mutex MyRenderDelegate::_mutexResourceRegistry;
std::atomic_int MyRenderDelegate::_counterResourceRegistry;
pxr::HdResourceRegistrySharedPtr MyRenderDelegate::_resourceRegistry;
std::map<pxr::SdfPath, MyMesh*> MyRenderDelegate::_myMeshes;

const pxr::TfTokenVector MyRenderDelegate::SUPPORTED_RPRIM_TYPES = {
    pxr::HdPrimTypeTokens->mesh,
};

const pxr::TfTokenVector MyRenderDelegate::SUPPORTED_SPRIM_TYPES = {
    pxr::HdPrimTypeTokens->camera,
};

const pxr::TfTokenVector MyRenderDelegate::SUPPORTED_BPRIM_TYPES = {
    pxr::HdPrimTypeTokens->renderBuffer
};

pxr::TfTokenVector const& MyRenderDelegate::GetSupportedRprimTypes() const
{
    return SUPPORTED_RPRIM_TYPES;
}

pxr::TfTokenVector const& MyRenderDelegate::GetSupportedSprimTypes() const
{
    return SUPPORTED_SPRIM_TYPES;
}

pxr::TfTokenVector const& MyRenderDelegate::GetSupportedBprimTypes() const
{
    return SUPPORTED_BPRIM_TYPES;
}

MyRenderDelegate::MyRenderDelegate()
    : HdRenderDelegate()
{
    _Initialize();
}

MyRenderDelegate::MyRenderDelegate(
    pxr::HdRenderSettingsMap const& settingsMap)
    : HdRenderDelegate(settingsMap)
{
    _Initialize();
}

void MyRenderDelegate::_Initialize()
{
    // initialize your context here if you need to
    //gladLoadGL();

    std::lock_guard<std::mutex> guard(_mutexResourceRegistry);
    if (_counterResourceRegistry.fetch_add(1) == 0) {
        _resourceRegistry = std::make_shared<pxr::HdResourceRegistry>();
    }

}

MyRenderDelegate::~MyRenderDelegate()
{
    std::lock_guard<std::mutex> guard(_mutexResourceRegistry);
    if (_counterResourceRegistry.fetch_sub(1) == 1) {
        _resourceRegistry.reset();
    }
}

pxr::HdResourceRegistrySharedPtr MyRenderDelegate::GetResourceRegistry() const
{
    return _resourceRegistry;
}

void MyRenderDelegate::CommitResources(pxr::HdChangeTracker* /* tracker */)
{
    _resourceRegistry->Commit();
}

void MyRenderDelegate::SetRenderSetting(pxr::TfToken const& key, pxr::VtValue const& value)
{
}

pxr::VtValue MyRenderDelegate::GetRenderSetting(pxr::TfToken const& key) const
{
    return HdRenderDelegate::GetRenderSetting(key);
}

pxr::HdRenderPassSharedPtr MyRenderDelegate::CreateRenderPass(
    pxr::HdRenderIndex* index,
    pxr::HdRprimCollection const& collection)
{
    return pxr::HdRenderPassSharedPtr(new MyRenderPass(index, collection, &_renderThread, this));
}

pxr::HdRprim* MyRenderDelegate::CreateRprim(pxr::TfToken const& typeId, pxr::SdfPath const& rprimId)
{
    if (typeId == pxr::HdPrimTypeTokens->mesh)
    {
        return new MyMesh(rprimId, this);
    }
    return nullptr;
}

void MyRenderDelegate::DestroyRprim(pxr::HdRprim* rPrim)
{
    delete rPrim;
}

pxr::HdSprim* MyRenderDelegate::CreateSprim(pxr::TfToken const& typeId, pxr::SdfPath const& sprimId)
{
    if (typeId == pxr::HdPrimTypeTokens->camera)
    {
        return new MyCamera(sprimId, this);
    }
    return nullptr;
}

pxr::HdSprim* MyRenderDelegate::CreateFallbackSprim(pxr::TfToken const& typeId)
{
    if (typeId == pxr::HdPrimTypeTokens->camera)
    {
        return new MyCamera(pxr::SdfPath::EmptyPath(), this);
    }
    return nullptr;
}

void MyRenderDelegate::DestroySprim(pxr::HdSprim* sPrim)
{
    delete sPrim;
}

pxr::HdBprim* MyRenderDelegate::CreateBprim(pxr::TfToken const& typeId, pxr::SdfPath const& bprimId)
{
    if (typeId == pxr::HdPrimTypeTokens->renderBuffer)
    {
        return new MyRenderBuffer(bprimId);
    }
    return nullptr;
}

pxr::HdBprim* MyRenderDelegate::CreateFallbackBprim(pxr::TfToken const& typeId)
{    
    return nullptr;
}

void MyRenderDelegate::DestroyBprim(pxr::HdBprim* bPrim)
{
}

pxr::HdInstancer* MyRenderDelegate::CreateInstancer(pxr::HdSceneDelegate* delegate, pxr::SdfPath const& id)
{
    return new MyInstancer(delegate, id);
}

void MyRenderDelegate::DestroyInstancer(pxr::HdInstancer* instancer)
{
    delete instancer;
}

pxr::HdRenderParam* MyRenderDelegate::GetRenderParam() const
{
    return nullptr;
}

pxr::HdAovDescriptor MyRenderDelegate::GetDefaultAovDescriptor(pxr::TfToken const& name) const
{
    if (name == pxr::HdAovTokens->color)
    {
        return pxr::HdAovDescriptor(pxr::HdFormatFloat16Vec4, false, pxr::VtValue(pxr::GfVec4f(0.0f)));
    }

    return pxr::HdAovDescriptor(pxr::HdFormatInvalid, false, pxr::VtValue());
}

bool MyRenderDelegate::UpdateScene()
{
    bool updated = false;

    // your scene rendered/updated/etc

    for (std::map<pxr::SdfPath, MyMesh*>::iterator it = _myMeshes.begin(); it != _myMeshes.end(); ++it)
    {
        if (&it)
        {
            it->second->drawGL();
        }
    }


    return updated;
}

pxr::VtDictionary MyRenderDelegate::GetRenderStats() const
{
    pxr::VtDictionary stats;

    static const pxr::TfToken percentDone("percentDone");
    stats[percentDone] = pxr::VtValue(100);

    static const pxr::TfToken meshes(" - meshes ");
    stats[meshes] = pxr::VtValue( _myMeshes.size() );
    
    return stats;
}