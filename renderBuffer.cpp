#include "renderBuffer.h"
#include <pxr/base/gf/half.h>
#include <pxr/base/gf/vec3i.h>

MyRenderBuffer::MyRenderBuffer(pxr::SdfPath const& id)
    : pxr::HdRenderBuffer(id)
    , _width(0)
    , _height(0)
    , _format(pxr::HdFormatInvalid)
    , _multiSampled(false)
    , _buffer()
    , _sampleBuffer()
    , _sampleCount()
    , _mappers(0)
    , _converged(false)
{
}

MyRenderBuffer::~MyRenderBuffer() = default;

/*virtual*/
void
MyRenderBuffer::Sync(pxr::HdSceneDelegate* sceneDelegate,
    pxr::HdRenderParam* renderParam,
    pxr::HdDirtyBits* dirtyBits)
{
    if (*dirtyBits & DirtyDescription) {
        // Embree has the background thread write directly into render buffers,
        // so we need to stop the render thread before reallocating them.
        //static_cast<MyRenderParam*>(renderParam)->AcquireSceneForEdit();
    }


    HdRenderBuffer::Sync(sceneDelegate, renderParam, dirtyBits);
}

/*virtual*/
void
MyRenderBuffer::Finalize(pxr::HdRenderParam* renderParam)
{
    // Embree has the background thread write directly into render buffers,
    // so we need to stop the render thread before removing them.
    //static_cast<MyRenderParam*>(renderParam)->AcquireSceneForEdit();

    HdRenderBuffer::Finalize(renderParam);
}

/*virtual*/
void
MyRenderBuffer::_Deallocate()
{
    _width = 0;
    _height = 0;
    _format = pxr::HdFormatInvalid;
    _multiSampled = false;
    _buffer.resize(0);
    _sampleBuffer.resize(0);
    _sampleCount.resize(0);

    _mappers.store(0);
    _converged.store(false);
}

/*static*/
size_t
MyRenderBuffer::_GetBufferSize(pxr::GfVec2i const& dims, pxr::HdFormat format)
{
    return dims[0] * dims[1] * pxr::HdDataSizeOfFormat(format);
}

/*static*/
pxr::HdFormat
MyRenderBuffer::_GetSampleFormat(pxr::HdFormat format)
{
    pxr::HdFormat component = pxr::HdGetComponentFormat(format);
    size_t arity = HdGetComponentCount(format);

    if (component == pxr::HdFormatUNorm8 || component == pxr::HdFormatSNorm8 ||
        component == pxr::HdFormatFloat16 || component == pxr::HdFormatFloat32) {
        if (arity == 1) { return pxr::HdFormatFloat32; }
        else if (arity == 2) { return pxr::HdFormatFloat32Vec2; }
        else if (arity == 3) { return pxr::HdFormatFloat32Vec3; }
        else if (arity == 4) { return pxr::HdFormatFloat32Vec4; }
    }
    else if (component == pxr::HdFormatInt32) {
        if (arity == 1) { return pxr::HdFormatInt32; }
        else if (arity == 2) { return pxr::HdFormatInt32Vec2; }
        else if (arity == 3) { return pxr::HdFormatInt32Vec3; }
        else if (arity == 4) { return pxr::HdFormatInt32Vec4; }
    }
    return pxr::HdFormatInvalid;
}

/*virtual*/
bool
MyRenderBuffer::Allocate(pxr::GfVec3i const& dimensions,
    pxr::HdFormat format,
    bool multiSampled)
{
    _Deallocate();

    if (dimensions[2] != 1) {
        //TF_WARN("Render buffer allocated with dims <%d, %d, %d> and"
        //    " format %s; depth must be 1!",
        //    dimensions[0], dimensions[1], dimensions[2],
        //    TfEnum::GetName(format).c_str());
        return false;
    }

    _width = dimensions[0];
    _height = dimensions[1];
    _format = format;
    _buffer.resize(_GetBufferSize(pxr::GfVec2i(_width, _height), format));

    _multiSampled = multiSampled;
    if (_multiSampled) {
        _sampleBuffer.resize(_GetBufferSize(pxr::GfVec2i(_width, _height),
            _GetSampleFormat(format)));
        _sampleCount.resize(_width * _height);
    }

    return true;
}

template<typename T>
static void _WriteSample(pxr::HdFormat format, uint8_t* dst,
    size_t valueComponents, T const* value)
{
    pxr::HdFormat componentFormat = pxr::HdGetComponentFormat(format);
    size_t componentCount = HdGetComponentCount(format);

    for (size_t c = 0; c < componentCount; ++c) {
        if (componentFormat == pxr::HdFormatInt32) {
            ((int32_t*)dst)[c] +=
                (c < valueComponents) ? (int32_t)(value[c]) : 0;
        }
        else {
            ((float*)dst)[c] +=
                (c < valueComponents) ? (float)(value[c]) : 0.0f;
        }
    }
}

template<typename T>
static void _WriteOutput(pxr::HdFormat format, uint8_t* dst,
    size_t valueComponents, T const* value)
{
    pxr::HdFormat componentFormat = pxr::HdGetComponentFormat(format);
    size_t componentCount = HdGetComponentCount(format);

    for (size_t c = 0; c < componentCount; ++c) {
        if (componentFormat == pxr::HdFormatInt32) {
            ((int32_t*)dst)[c] =
                (c < valueComponents) ? (int32_t)(value[c]) : 0;
        }
        else if (componentFormat == pxr::HdFormatFloat16) {
            ((uint16_t*)dst)[c] =
                (c < valueComponents) ? pxr::GfHalf(value[c]).bits() : 0;
        }
        else if (componentFormat == pxr::HdFormatFloat32) {
            ((float*)dst)[c] =
                (c < valueComponents) ? (float)(value[c]) : 0.0f;
        }
        else if (componentFormat == pxr::HdFormatUNorm8) {
            ((uint8_t*)dst)[c] =
                (c < valueComponents) ? (uint8_t)(value[c] * 255.0f) : 0.0f;
        }
        else if (componentFormat == pxr::HdFormatSNorm8) {
            ((int8_t*)dst)[c] =
                (c < valueComponents) ? (int8_t)(value[c] * 127.0f) : 0.0f;
        }
    }
}

void
MyRenderBuffer::Write(
    pxr::GfVec3i const& pixel, size_t numComponents, float const* value)
{
    size_t idx = pixel[1] * _width + pixel[0];
    if (_multiSampled) {
        size_t formatSize = pxr::HdDataSizeOfFormat(_GetSampleFormat(_format));
        uint8_t* dst = &_sampleBuffer[idx * formatSize];
        _WriteSample(_format, dst, numComponents, value);
        _sampleCount[idx]++;
    }
    else {
        size_t formatSize = pxr::HdDataSizeOfFormat(_format);
        uint8_t* dst = &_buffer[idx * formatSize];
        _WriteOutput(_format, dst, numComponents, value);
    }
}

void
MyRenderBuffer::Write(
    pxr::GfVec3i const& pixel, size_t numComponents, int const* value)
{
    size_t idx = pixel[1] * _width + pixel[0];
    if (_multiSampled) {
        size_t formatSize = pxr::HdDataSizeOfFormat(_GetSampleFormat(_format));
        uint8_t* dst = &_sampleBuffer[idx * formatSize];
        _WriteSample(_format, dst, numComponents, value);
        _sampleCount[idx]++;
    }
    else {
        size_t formatSize = pxr::HdDataSizeOfFormat(_format);
        uint8_t* dst = &_buffer[idx * formatSize];
        _WriteOutput(_format, dst, numComponents, value);
    }
}

void
MyRenderBuffer::Clear(size_t numComponents, float const* value)
{
    size_t formatSize = pxr::HdDataSizeOfFormat(_format);
    for (size_t i = 0; i < _width * _height; ++i) {
        uint8_t* dst = &_buffer[i * formatSize];
        _WriteOutput(_format, dst, numComponents, value);
    }

    if (_multiSampled) {
        std::fill(_sampleCount.begin(), _sampleCount.end(), 0);
        std::fill(_sampleBuffer.begin(), _sampleBuffer.end(), 0);
    }
}

void
MyRenderBuffer::Clear(size_t numComponents, int const* value)
{
    size_t formatSize = pxr::HdDataSizeOfFormat(_format);
    for (size_t i = 0; i < _width * _height; ++i) {
        uint8_t* dst = &_buffer[i * formatSize];
        _WriteOutput(_format, dst, numComponents, value);
    }

    if (_multiSampled) {
        std::fill(_sampleCount.begin(), _sampleCount.end(), 0);
        std::fill(_sampleBuffer.begin(), _sampleBuffer.end(), 0);
    }
}

/*virtual*/
void
MyRenderBuffer::Resolve()
{
    // Resolve the image buffer: find the average value per pixel by
    // dividing the summed value by the number of samples.

    if (!_multiSampled) {
        return;
    }

    pxr::HdFormat componentFormat = pxr::HdGetComponentFormat(_format);
    size_t componentCount = pxr::HdGetComponentCount(_format);
    size_t formatSize = pxr::HdDataSizeOfFormat(_format);
    size_t sampleSize = pxr::HdDataSizeOfFormat(_GetSampleFormat(_format));

    for (unsigned int i = 0; i < _width * _height; ++i) {

        int sampleCount = _sampleCount[i];
        // Skip pixels with no samples.
        if (sampleCount == 0) {
            continue;
        }

        uint8_t* dst = &_buffer[i * formatSize];
        uint8_t* src = &_sampleBuffer[i * sampleSize];
        for (size_t c = 0; c < componentCount; ++c) {
            if (componentFormat == pxr::HdFormatInt32) {
                ((int32_t*)dst)[c] = ((int32_t*)src)[c] / sampleCount;
            }
            else if (componentFormat == pxr::HdFormatFloat16) {
                ((uint16_t*)dst)[c] = pxr::GfHalf(
                    ((float*)src)[c] / sampleCount).bits();
            }
            else if (componentFormat == pxr::HdFormatFloat32) {
                ((float*)dst)[c] = ((float*)src)[c] / sampleCount;
            }
            else if (componentFormat == pxr::HdFormatUNorm8) {
                ((uint8_t*)dst)[c] = (uint8_t)
                    (((float*)src)[c] * 255.0f / sampleCount);
            }
            else if (componentFormat == pxr::HdFormatSNorm8) {
                ((int8_t*)dst)[c] = (int8_t)
                    (((float*)src)[c] * 127.0f / sampleCount);
            }
        }
    }
}

