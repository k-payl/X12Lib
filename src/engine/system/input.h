#pragma once
#include "common.h"

namespace engine
{
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
		auto X12_API IsKeyPressed(KEYBOARD_KEY_CODES key) -> bool;
		auto X12_API IsMoisePressed(MOUSE_BUTTON type) -> bool;
		auto X12_API GetMouseDeltaPos() ->math::vec2;
		auto X12_API GetMousePos() ->math::vec2;
	};
}
