#ifndef MY_RENDERDELEGATE_H
#define MY_RENDERDELEGATE_H

#include <pxr/pxr.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/resourceRegistry.h>
#include <pxr/imaging/hd/renderThread.h>
#include <pxr/base/tf/staticTokens.h>

#include <map>

#include "mesh.h"

using UpdateRenderSettingFunction = std::function<bool(pxr::VtValue const& value)>;

class MyRenderDelegate final : public pxr::HdRenderDelegate
{
public:
    MyRenderDelegate();
    MyRenderDelegate(pxr::HdRenderSettingsMap const& settingsMap);
    virtual ~MyRenderDelegate() override;
    
    MyRenderDelegate(const MyRenderDelegate&) = delete;
    MyRenderDelegate& operator=(const MyRenderDelegate&) = delete;

    virtual const pxr::TfTokenVector& GetSupportedRprimTypes() const override;
    virtual const pxr::TfTokenVector& GetSupportedSprimTypes() const override;
    virtual const pxr::TfTokenVector& GetSupportedBprimTypes() const override;

    virtual pxr::HdRenderParam* GetRenderParam() const override;
    //virtual HdRenderSettingDescriptorList GetRenderSettingDescriptors() const override;

    virtual pxr::HdAovDescriptor GetDefaultAovDescriptor(pxr::TfToken const& name) const override;

    virtual pxr::HdRprim* CreateRprim(pxr::TfToken const& typeId, pxr::SdfPath const& rprimId) override;
    virtual pxr::HdSprim* CreateSprim(pxr::TfToken const& typeId, pxr::SdfPath const& sprimId) override;
    virtual pxr::HdBprim* CreateBprim(pxr::TfToken const& typeId, pxr::SdfPath const& bprimId) override;

    virtual void DestroyRprim(pxr::HdRprim* rPrim) override;
    virtual void DestroySprim(pxr::HdSprim* sprim) override;
    virtual void DestroyBprim(pxr::HdBprim* bprim) override;

    virtual pxr::HdSprim* CreateFallbackSprim(pxr::TfToken const& typeId) override;
    virtual pxr::HdBprim* CreateFallbackBprim(pxr::TfToken const& typeId) override;

    virtual pxr::HdInstancer* CreateInstancer(pxr::HdSceneDelegate* delegate, pxr::SdfPath const& id) override;
    virtual void DestroyInstancer(pxr::HdInstancer* instancer) override;


    virtual pxr::HdRenderPassSharedPtr CreateRenderPass(pxr::HdRenderIndex* index, pxr::HdRprimCollection const& collection) override;

    virtual pxr::HdResourceRegistrySharedPtr GetResourceRegistry() const override;
    virtual void CommitResources(pxr::HdChangeTracker* tracker) override;
    virtual void SetRenderSetting(pxr::TfToken const& key, pxr::VtValue const& value) override;
    virtual pxr::VtValue GetRenderSetting(pxr::TfToken const& key) const override;

    std::mutex& rendererMutex() { return _rendererMutex; }
    std::mutex& primIndexMutex() { return _primIndexMutex; }

    bool UpdateScene();

    void addMesh(const pxr::SdfPath& i_path, MyMesh* i_mesh)
    {
        _myMeshes[i_path] = i_mesh;
    }

    pxr::VtDictionary GetRenderStats() const;

private:
    void _Initialize();

    static const pxr::TfTokenVector SUPPORTED_RPRIM_TYPES;
    static const pxr::TfTokenVector SUPPORTED_SPRIM_TYPES;
    static const pxr::TfTokenVector SUPPORTED_BPRIM_TYPES;

    static std::mutex _mutexResourceRegistry;
    static std::atomic_int _counterResourceRegistry;
    static pxr::HdResourceRegistrySharedPtr _resourceRegistry;

    std::mutex _rendererMutex;
    std::mutex _primIndexMutex;

    std::map<pxr::TfToken, UpdateRenderSettingFunction> _settingFunctions;

    static std::map<pxr::SdfPath, MyMesh*> _myMeshes;

    pxr::HdRenderThread _renderThread;
};

#endif