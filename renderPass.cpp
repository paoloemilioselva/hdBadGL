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

GLuint CreateVBO(const GLfloat* data, const GLuint size)
{
    GLuint id;
    glGenBuffers(1, &id);
    glBindBuffer(GL_ARRAY_BUFFER, id);
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    return id;
}

void BindVBO(const GLuint idx, const GLuint N, const GLuint id)
{
    glEnableVertexAttribArray(idx);
    glBindBuffer(GL_ARRAY_BUFFER, id);
    glVertexAttribPointer(idx, N, GL_FLOAT, GL_FALSE, 0, (void*)0);
}

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
    , shaderCreated(false)
    , _shaderProgram()
    , _gBuffer()
    , _frameBuffer()
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
    auto* hdCamera = static_cast<const MyCamera*>(
        GetRenderIndex()->GetSprim(pxr::HdPrimTypeTokens->camera, hdCameraPath));
    // Do we need to get the sampleXform param here instead ?
    auto& passMatrix = hdCamera->GetTransform();

    if (!shaderCreated)
    {
        shaderCreated = true;

        //glGenFramebuffers(1, &_frameBuffer);

        //glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);

        glGenTextures(1, &_gBuffer);
        //glBindTexture(GL_TEXTURE_2D, _gBuffer);
        //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 768, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        //GLuint depthrenderbuffer;
        //glGenRenderbuffers(1, &depthrenderbuffer);
        //glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
        //glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 1024, 768);
        //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffer);

        //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _gBuffer, 0);
        //GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
        //glDrawBuffers(1, DrawBuffers);

        //glBindFramebuffer(GL_FRAMEBUFFER, 0);



        const char* vertexShaderSource = "#version 430\n"
            "layout(location = 0) in vec4 position;\n"
            "layout(location = 1) in vec2 iTexcoord;\n"
            "layout(location = 2) in vec4 incolor;\n"
            "out vec2 texcoord;\n"
            "uniform mat4 projection;\n"
            "uniform mat4 view;\n"
            "uniform mat4 model;\n"
            "void main()\n"
            "{\n"
            "   gl_Position = projection * view * position;\n"
            "   texcoord = iTexcoord;\n"
            "}\0";

        const char* fragmentShaderSource = "#version 430\n"
            "layout(origin_upper_left) in vec4 gl_FragCoord;\n"
            "in vec3 normal;\n"
            "in vec2 texcoord;\n"
            "layout(location = 0) out vec4 FragColor;\n"
            "void main()\n"
            "{\n"
            "   float alpha = 1.0;\n"
            "   float posX = gl_FragCoord.x;\n"
            "   //float posZ = gl_DepthRange.diff * (gl_FragCoord.z/gl_FragCoord.w) / 2.0;\n"
            "   float posZ = gl_DepthRange.diff * (gl_FragCoord.z/gl_FragCoord.w) / 2.0;\n"
            "   if(posZ < 1.0) FragColor = vec4(1,0,0,alpha);\n"
            "   else if(posZ < 2.0) FragColor = vec4(0,1,0,alpha);\n"
            "   else if(posZ < 3.0) FragColor = vec4(0,0,1,alpha);\n"
            "   else if(posZ < 4.0) FragColor = vec4(1,0,1,alpha);\n"
            "   else if(posZ < 5.0) FragColor = vec4(1,1,0,alpha);\n"
            "   else if(posZ < 6.0) FragColor = vec4(0,1,1,alpha);\n"
            "   //else FragColor = vec4(gl_FragCoord.x/1000.0, gl_FragCoord.y/1000.0, posZ, alpha);\n"
            "   //FragColor = vec4(gl_FragCoord.x/1000.0, gl_FragCoord.y/1000.0, posZ, alpha);\n"
            "   //FragColor = vec4(gl_ClipDistance, gl_PrimitiveID/100, posZ, alpha);\n"
            "   FragColor = vec4(uvOut.x, uvOut.y, 1.0, alpha);\n"
            "}\n\0";

        // vertex shader
        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);
        // check for shader compile errors
        int success;
        char infoLog[512];
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        }
        // fragment shader
        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);
        // check for shader compile errors
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        }
        // link shaders
        _shaderProgram = glCreateProgram();
        //glAttachShader(_shaderProgram, vertexShader);
        glAttachShader(_shaderProgram, fragmentShader);
        glLinkProgram(_shaderProgram);
        // check for linking errors
        glGetProgramiv(_shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(_shaderProgram, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        }
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    glPushMatrix();
    glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    glPushAttrib(GL_ALL_ATTRIB_BITS);

    //glDisable(GL_BLEND);
    glDisable(GL_LIGHTING);
    glDepthFunc(GL_LESS);
    glClearColor(0.0,0.0,0.0,1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMultMatrixd(proj.data());
    glMultMatrixd(view.data());
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    //glUseProgram(_shaderProgram);
    
    //glUniformMatrix4dv(glGetUniformLocation(_shaderProgram, "projection"), 1, GL_FALSE, proj.data());
    //glUniformMatrix4dv(glGetUniformLocation(_shaderProgram, "view"), 1, GL_FALSE, view.data());

    // ...update/draw your scene
    bool needsRestart = _owner->UpdateScene();

    {
        std::lock_guard<std::mutex> guardxx(_owner->rendererMutex());
        
        if (!_owner->ValidPixels(_dataWindow.GetWidth(), _dataWindow.GetHeight()))
            _owner->ResetPixels(_dataWindow.GetWidth(), _dataWindow.GetHeight());

        float* pixels = _owner->GetPixels();
        glReadPixels(0, 0, _dataWindow.GetWidth(), _dataWindow.GetHeight(), GL_RGBA, GL_FLOAT, pixels);
    }

    //glUseProgram(0);

    glPopAttrib();
    glPopMatrix();
}
