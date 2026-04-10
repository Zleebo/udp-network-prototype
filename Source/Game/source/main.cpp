#include "stdafx.h"
#include "GameWorld.h"

namespace
{
void Run();

LRESULT CALLBACK WinProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    ClientHandler::GetInstance().SetWindowHandle(windowHandle);

    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }

    return 0;
}
void Run()
{
    Tga::LoadSettings(TGE_PROJECT_SETTINGS_FILE);

    Tga::EngineConfiguration engineConfiguration;
    engineConfiguration.myApplicationName = L"UDP Network Prototype";
    engineConfiguration.myWinProcCallback = [](HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
    { return WinProc(windowHandle, message, wParam, lParam); };

#ifdef _DEBUG
    engineConfiguration.myActivateDebugSystems = Tga::DebugFeature::Fps | Tga::DebugFeature::Mem |
                                                 Tga::DebugFeature::Filewatcher | Tga::DebugFeature::Cpu |
                                                 Tga::DebugFeature::Drawcalls | Tga::DebugFeature::OptimizeWarnings;
#else
    engineConfiguration.myActivateDebugSystems = Tga::DebugFeature::Filewatcher;
#endif

    if (!Tga::Engine::Start(engineConfiguration))
    {
        ERROR_PRINT("Fatal error! Engine could not start!");
        system("pause");
        return;
    }

    GameWorld gameWorld;
    ClientHandler& client = ClientHandler::GetInstance();

    if (!client.Initialize())
    {
        Tga::Engine::GetInstance()->Shutdown();
        return;
    }

    gameWorld.Init();
    Tga::Engine& engine = *Tga::Engine::GetInstance();

    while (engine.BeginFrame())
    {
        gameWorld.Update(engine.GetDeltaTime());
        gameWorld.Render();
        client.Update();
        client.RenderDebugOverlay();
        engine.EndFrame();
    }

    client.Shutdown();
    Tga::Engine::GetInstance()->Shutdown();
}
} // namespace

int main(const int /*argc*/, const char* /*argv*/[])
{
    Run();
    return 0;
}
