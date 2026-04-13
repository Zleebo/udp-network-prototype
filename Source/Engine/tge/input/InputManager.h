#pragma once
#include "Windows.h"
#include <bitset>
#include <tge/math/Vector.h>

namespace Tga
{
/// <summary>
/// Minimal input manager implementation.
/// It currently focuses on keyboard state tracking, while
/// following the same update pattern used for other input types.
/// </summary>
class InputManager
{
	// Holds the "live" state that is being updated by
	// the message pump thread. This can be used in place
	// of myCurrentState but depending on how the game is
	// threaded it may be prudent to keep them separate.
	std::bitset<256> myTentativeState{};

	// The current snapshot when we last ran Update.
	std::bitset<256> myCurrentState{};

	// The previous snapshot.
	std::bitset<256> myPreviousState{};

	HWND myOwnerHWND;
	

	Vector2i myTentativeMousePosition;
	Vector2i myCurrentMousePosition;
	Vector2i myPreviousMousePosition;

	Vector2i myTentativeMouseDelta;
	Vector2i myMouseDelta;

	float myTentativeMouseWheelDelta;
	float myMouseWheelDelta;

	
public:
	
	InputManager(HWND aWindowHandle);

	bool IsKeyHeld(const int aKeyCode) const;
	bool IsKeyPressed(const int aKeyCode) const;
	bool IsKeyReleased(const int aKeyCode) const;

	Vector2f GetMouseDelta() const;
	Vector2f GetMousePosition() const;

	void ShowMouse() const;
	void HideMouse() const;

	void CaptureMouse() const;
	void ReleaseMouse() const;

	bool UpdateEvents(UINT message, WPARAM wParam, LPARAM lParam);
	void Update();

	void BindAction(std::wstring Action, void (*actionInputFunction)());
	void BindAxis(std::wstring Axis, void (*axisInputFunction)(Vector2f axisValue));
};

} // namespace Tga
