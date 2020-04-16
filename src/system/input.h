#pragma once
#include "common.h"

class Input final
{
	static Input* instance;

	uint8_t keys_[256]{};
	uint8_t mouse_[3]{};
	int cursorX_{}, cursorY_{};
	math::vec2 oldPos_;
	math::vec2 mouseDeltaPos_;
	
	void clearMouse();
	void messageCallback(HWND hwnd, WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void *pData);
	static void sMessageCallback(HWND hwnd, WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void *pData);

public:
	Input();
	~Input();

	// Internal API
	void Init();
	void Free();
	void Update();

public:
	auto IsKeyPressed(KEYBOARD_KEY_CODES key) -> bool;
	auto IsMoisePressed(MOUSE_BUTTON type) -> bool;
	auto GetMouseDeltaPos() ->math::vec2;
	auto GetMousePos() ->math::vec2;
};
