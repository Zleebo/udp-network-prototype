#pragma once
#include <tge/math/matrix4x4.h>
#include <tge/render/RenderObject.h>
#include <d3dcommon.h>
#include <d3d11.h>
#include <tge/EngineDefines.h>
#include <wrl/client.h>
#include <functional>

using Microsoft::WRL::ComPtr;

namespace Tga
{
    class GraphicsEngine;
    class Engine;
    class Shader
    {
    public:
        Shader(Engine& aEngine);
        virtual ~Shader();
        virtual bool Init(){ return false; }

		typedef std::function<void(const std::string& aBlob)> callback_layout;
		bool CreateShaders(const wchar_t* aVertex, const wchar_t* aPixel, callback_layout aLayout = nullptr);
        bool PrepareRender() const;

    protected:
		virtual bool CreateInputLayout(const std::string& aVS) { aVS; return false; }
        void DoOneFrameUpdates() const;
        ComPtr<ID3D11VertexShader> myVertexShader;    // the vertex shader
        ComPtr<ID3D11PixelShader> myPixelShader;     // the pixel shader
        ComPtr<ID3D11InputLayout> myLayout;            // the pointer to the input layout
       
        GraphicsEngine* myGraphicsEngine;
        Engine* myEngine;

		bool myIsReadyToRender;
		int myRandomSeed;

        std::wstring myVertexShaderFile;
        std::wstring myPixelShaderFile;
    };
}