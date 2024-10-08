
#include "instancer.h"
#include "renderDelegate.h"
#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/quath.h>

#include <iostream>

MyInstancer::MyInstancer(pxr::HdSceneDelegate* delegate, pxr::SdfPath const& id, MyRenderDelegate* renderDelegate) :
    pxr::HdInstancer(delegate, id),
    _owner(renderDelegate)
{
}

MyInstancer::~MyInstancer()
{
    TF_FOR_ALL(it, _primvarMap)
    {
        delete it->second;
    }
    _primvarMap.clear();

    std::lock_guard<std::mutex> guard(_owner->rendererMutex());
    _owner->removeInstancer(GetId());
}

void MyInstancer::Sync(
    pxr::HdSceneDelegate* delegate, pxr::HdRenderParam* /*renderParam*/, pxr::HdDirtyBits* dirtyBits)
{
    std::lock_guard<std::mutex> guard(_owner->rendererMutex());
    _owner->addInstancerId(GetId());

    _UpdateInstancer(delegate, dirtyBits);
    _SyncPrimvars(dirtyBits);

}

void MyInstancer::_SyncPrimvars(pxr::HdDirtyBits* dirtyBits)
{
    pxr::HdSceneDelegate* delegate = GetDelegate();
    const pxr::SdfPath& id = GetId();

    if (pxr::HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, id))
    {
        pxr::HdPrimvarDescriptorVector primvars =
            delegate->GetPrimvarDescriptors(id, pxr::HdInterpolationInstance);

        for (pxr::HdPrimvarDescriptor const& pv : primvars)
        {
            /*if (pv.name == pxr::HdInstancerTokens->instanceTransform ||
                pv.name == pxr::HdInstancerTokens->rotate ||
                pv.name == pxr::HdInstancerTokens->scale ||
                pv.name == pxr::HdInstancerTokens->translate) {
                continue;
            }*/

            if (pxr::HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pv.name))
            {
                pxr::VtValue value = delegate->Get(id, pv.name);
                if (!value.IsEmpty())
                {
                    if (_primvarMap.count(pv.name) > 0)
                    {
                        delete _primvarMap[pv.name];
                    }
                    _primvarMap[pv.name] = new pxr::HdVtBufferSource(pv.name, value);
                }
            }
        }
    }
}

bool SampleBuffer(pxr::HdVtBufferSource const& buffer, int index, void* value, pxr::HdTupleType dataType)
{
    if (buffer.GetNumElements() <= (size_t)index || buffer.GetTupleType() != dataType)
    {
        return false;
    }

    size_t elemSize = pxr::HdDataSizeOfTupleType(dataType);
    size_t offset = elemSize * index;

    std::memcpy(value, static_cast<const uint8_t*>(buffer.GetData()) + offset, elemSize);

    return true;
}

pxr::VtMatrix4dArray MyInstancer::ComputeInstanceTransforms(pxr::SdfPath const& prototypeId)
{
    // The transforms for this level of instancer are computed by:
    // foreach(index : indices) {
    //     instancerTransform * translate(index) * rotate(index) *
    //     scale(index) * instanceTransform(index)
    // }
    // If any transform isn't provided, it's assumed to be the identity.

    pxr::GfMatrix4d instancerTransform = GetDelegate()->GetInstancerTransform(GetId());
    pxr::VtIntArray instanceIndices = GetDelegate()->GetInstanceIndices(GetId(), prototypeId);

    pxr::VtMatrix4dArray transforms(instanceIndices.size());
    for (size_t i = 0; i < instanceIndices.size(); ++i)
    {
        transforms[i] = instancerTransform;
    }
    pxr::HdVtBufferSource* pBuffer = nullptr;

    if (_primvarMap.count(pxr::HdInstancerTokens->instanceTranslations) > 0)
        pBuffer = _primvarMap[pxr::HdInstancerTokens->instanceTranslations];
    if (pBuffer)
    {
        for (size_t i = 0; i < instanceIndices.size(); ++i)
        {
            pxr::GfVec3f translate;
            if (SampleBuffer(*pBuffer, instanceIndices[i], &translate, { pxr::HdTypeFloatVec3, 1 }))
            {
                pxr::GfMatrix4d translateMat(1);
                translateMat.SetTranslate(pxr::GfVec3d(translate));
                transforms[i] = translateMat * transforms[i];
            }
        }
    }

    if (_primvarMap.count(pxr::HdInstancerTokens->instanceRotations) > 0)
        pBuffer = _primvarMap[pxr::HdInstancerTokens->instanceRotations];
    if (pBuffer)
    {
        for (size_t i = 0; i < instanceIndices.size(); ++i)
        {
            pxr::GfQuath quat;
            if (SampleBuffer(*pBuffer, instanceIndices[i], &quat, { pxr::HdTypeHalfFloatVec4, 1 }))
            {
                pxr::GfMatrix4d rotateMat(1);
                rotateMat.SetRotate(quat);
                transforms[i] = rotateMat * transforms[i];
            }
            else
            {
                pxr::GfVec4f quatf;
                if (SampleBuffer(*pBuffer, instanceIndices[i], &quatf, { pxr::HdTypeFloatVec4, 1 }))
                {
                    pxr::GfMatrix4d rotateMat(1);
                    rotateMat.SetRotate(pxr::GfQuatd(quatf[0],quatf[1],quatf[2],quatf[3]));
                    transforms[i] = rotateMat * transforms[i];
                }
            }
        }
    }

    if (_primvarMap.count(pxr::HdInstancerTokens->instanceScales) > 0)
        pBuffer = _primvarMap[pxr::HdInstancerTokens->instanceScales];
    if (pBuffer)
    {
        for (size_t i = 0; i < instanceIndices.size(); ++i)
        {
            pxr::GfVec3f scale;
            if (SampleBuffer(*pBuffer, instanceIndices[i], &scale, { pxr::HdTypeFloatVec3, 1 }))
            {
                pxr::GfMatrix4d scaleMat(1);
                scaleMat.SetScale(pxr::GfVec3d(scale));
                transforms[i] = scaleMat * transforms[i];
            }
        }
    }

    if (_primvarMap.count(pxr::HdInstancerTokens->instanceTransforms) > 0)
        pBuffer = _primvarMap[pxr::HdInstancerTokens->instanceTransforms];
    if (pBuffer)
    {
        for (size_t i = 0; i < instanceIndices.size(); ++i)
        {
            pxr::GfMatrix4d instanceTransform;
            if (SampleBuffer(
                *pBuffer, instanceIndices[i], &instanceTransform, { pxr::HdTypeDoubleMat4, 1 }))
            {
                transforms[i] = instanceTransform * transforms[i];
            }
        }
    }

    if (GetParentId().IsEmpty())
    {
        return transforms;
    }

    pxr::HdInstancer* parentInstancer = GetDelegate()->GetRenderIndex().GetInstancer(GetParentId());
    if (!parentInstancer)
    {
        return transforms;
    }

    std::cout << "PARENT instancer for " << GetId() << " found in " << GetParentId() << std::endl;

    // The transforms taking nesting into account are computed by:
    // parentTransforms = parentInstancer->ComputeInstanceTransforms(GetId())
    // foreach (parentXf : parentTransforms, xf : transforms) {
    //     parentXf * xf
    // }
    pxr::VtMatrix4dArray parentTransforms =
        static_cast<MyInstancer*>(parentInstancer)->ComputeInstanceTransforms(GetId());

    pxr::VtMatrix4dArray final(parentTransforms.size() * transforms.size());
    for (size_t i = 0; i < parentTransforms.size(); ++i)
    {
        for (size_t j = 0; j < transforms.size(); ++j)
        {
            final[i * transforms.size() + j] = transforms[j] * parentTransforms[i];
        }
    }
    return final;
}