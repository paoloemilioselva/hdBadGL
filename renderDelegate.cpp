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

#include "glad.h"

std::mutex MyRenderDelegate::_mutexResourceRegistry;
std::atomic_int MyRenderDelegate::_counterResourceRegistry;
pxr::HdResourceRegistrySharedPtr MyRenderDelegate::_resourceRegistry;
std::map<pxr::SdfPath, MyMesh*> MyRenderDelegate::_myMeshes;

std::set<pxr::SdfPath> MyRenderDelegate::_dataSharingIds;
std::set<pxr::SdfPath> MyRenderDelegate::_instancerIds;

float* MyRenderDelegate::_pixelsData;
int MyRenderDelegate::_pixelsWidth;
int MyRenderDelegate::_pixelsHeight;

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
    : HdRenderDelegate(settingsMap), _currentStatsTime(0)
{
    _Initialize();
}

void MyRenderDelegate::_Initialize()
{
    gladLoadGL();

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
    return new MyInstancer(delegate, id, this);
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

    stats[pxr::TfToken("percentDone")] = pxr::VtValue(100);

    _currentStatsTime++;

    std::vector< std::string > lines;

    {
        std::stringstream tokenStr;
        tokenStr << "meshes: " << _myMeshes.size();
        for (int i = tokenStr.str().size(); i < 100; ++i) tokenStr << " ";
        lines.emplace_back(tokenStr.str());
    }
    {
        for (auto& d : _myMeshes)
        {
            std::stringstream tokenStr;
            tokenStr << " - " << d.first.GetText();
            for (int i = tokenStr.str().size(); i < 100; ++i) tokenStr << " ";
            lines.emplace_back(tokenStr.str());
        }
    }

    {
        size_t instances = 0;
        for (auto& m : _myMeshes)
            instances += m.second->numInstances();
        std::stringstream tokenStr;
        tokenStr << "instances: " << instances;
        for (int i = tokenStr.str().size(); i < 100; ++i) tokenStr << " ";
        lines.emplace_back(tokenStr.str());
    }

    {
        std::stringstream tokenStr;
        tokenStr << "dataSharingIds: " << _dataSharingIds.size();
        for (int i = tokenStr.str().size(); i < 100; ++i) tokenStr << " ";
        lines.emplace_back(tokenStr.str());
    }

    {
        for (auto& d : _dataSharingIds)
        {
            std::stringstream tokenStr;
            tokenStr << " - " << d.GetText();
            for (int i = tokenStr.str().size(); i < 100; ++i) tokenStr << " ";
            lines.emplace_back(tokenStr.str());
        }
    }

    {
        std::stringstream tokenStr;
        tokenStr << "instancerIds: " << _instancerIds.size();
        for (int i = tokenStr.str().size(); i < 100; ++i) tokenStr << " ";
        lines.emplace_back(tokenStr.str());
    }

    {
        for (auto& d : _instancerIds)
        {
            std::stringstream tokenStr;
            tokenStr << " - " << d.GetText();
            for (int i = tokenStr.str().size(); i < 100; ++i) tokenStr << " ";
            lines.emplace_back(tokenStr.str());
        }
    }

    int width = 150, height = 60;

    bool drawCubes = false;
    if(drawCubes)
    {
        float A = _currentStatsTime / 100.f;
        float B = _currentStatsTime / 100.f;
        float C = _currentStatsTime / 100.f;
        float zBuffer[150 * 60];
        char buffer[150 * 60];
        int backgroundASCIICode = ' ';
        int distanceFromCam = _currentStatsTime / 10;
        float cubeWidth = 20;
        float horizontalOffset = 0.0f;// cubeWidth;
        float K1 = 40;
        float incrementSpeed = 0.6;
        float x, y, z;
        float ooz;
        int xp, yp;
        int idx;

        if (_myMeshes.size() > 0)
        {
            A = 3.14155 * (45.0 / 180.0);
            B = 3.14155 * ( 45.0 / 180.0 );
            C = 0.0;
            distanceFromCam = 100;
            for (std::map<pxr::SdfPath, MyMesh*>::iterator it = _myMeshes.begin(); it != _myMeshes.end(); ++it)
            {
                if (&it)
                {
                    //pxr::GfQuatf rot = it->second->getTransform().ExtractRotationQuat();
                    //float angle = rot.GetReal();
                    //pxr::GfVec3f axis = rot.GetImaginary();
                    //A = axis[0] / sin(acos(angle));
                    //B = axis[1] / sin(acos(angle));
                    //C = axis[2] / sin(acos(angle));
                    //horizontalOffset = it->second->getTransform().GetRow3(3)[0];
                    break;
                }
            }
        }

        memset(buffer, backgroundASCIICode, width * height);
        memset(zBuffer, 0, width * height * 4);


#define CALC_X(i,j,k) j * sin(A) * sin(B) * cos(C) - k * cos(A) * sin(B) * cos(C) + j * cos(A)* sin(C) + k * sin(A) * sin(C) + i * cos(B) * cos(C)
#define CALC_Y(i,j,k) j * cos(A) * cos(C) + k * sin(A) * cos(C) - j * sin(A)* sin(B)* sin(C) + k * cos(A) * sin(B) * sin(C) - i * cos(B) * sin(C)
#define CALC_Z(i,j,k) k * cos(A) * cos(B) - j * sin(A) * cos(B) + i * sin(B)

#define CALC_SURFACE(cubeX,cubeY,cubeZ,ch) {\
    x = CALC_X(cubeX, cubeY, cubeZ);\
    y = CALC_Y(cubeX, cubeY, cubeZ);\
    z = CALC_Z(cubeX, cubeY, cubeZ) + distanceFromCam;\
    ooz = 1 / z;\
    xp = (int)(width / 2 + horizontalOffset + K1 * ooz * x * 2);\
    yp = (int)(height / 2 + K1 * ooz * y);\
    idx = xp + yp * width;\
    if (idx >= 0 && idx < width * height) {\
        if (ooz > zBuffer[idx]) {\
            zBuffer[idx] = ooz;\
            buffer[idx] = ch;\
        }\
    }\
}

        for (float cubeX = -cubeWidth; cubeX < cubeWidth; cubeX += incrementSpeed) {
            for (float cubeY = -cubeWidth; cubeY < cubeWidth;
                cubeY += incrementSpeed) {
                CALC_SURFACE(cubeX, cubeY, -cubeWidth, '@');
                CALC_SURFACE(cubeWidth, cubeY, cubeX, '$');
                CALC_SURFACE(-cubeWidth, cubeY, -cubeX, '~');
                CALC_SURFACE(-cubeX, cubeY, cubeWidth, '#');
                CALC_SURFACE(cubeX, -cubeWidth, -cubeY, ';');
                CALC_SURFACE(cubeX, cubeWidth, cubeY, '+');
            }
        }

        for (int y = 0; y < height; ++y)
        {
            std::stringstream tokenStr;
            for (int x = 0; x < width; ++x)
            {
                tokenStr << buffer[y*width+x];
            }
            lines.emplace_back(tokenStr.str());
        }
    }

    bool drawBuffer = false;
    if (drawBuffer)
    {
        for (int i = lines.size(); i < height; ++i)
        {
            lines.emplace_back(std::string());
        }
        
        std::string grad = " .:x#";
        for (int y = 0; y < height; ++y)
        {
            std::stringstream tokenStr;
            for (int x = 0; x < width; ++x)
            {
                int mapx = x * _pixelsWidth / width;
                int mapy = (height-y-1) * _pixelsHeight / height;
                float red = _pixelsData[(mapy * _pixelsWidth + mapx) * 4];
                int mapred = red * (grad.size() - 1);
                tokenStr << grad[mapred];
            }
            lines[y] = tokenStr.str();
        }
    }

    int gap = (height) - lines.size();
    for (int i = 0; i < height; ++i)
    {
        if(lines.size()>0 && gap>0 && i>=gap)
            stats[pxr::TfToken(std::to_string(i))] = pxr::VtValue(lines[i-gap]);
        else
            stats[pxr::TfToken(std::to_string(i))] = pxr::VtValue(std::string());
    }

    return stats;
}