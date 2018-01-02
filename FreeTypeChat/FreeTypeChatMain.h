#pragma once

#include <deque>
#include <queue>

#include "Common\StepTimer.h"
#include "Common\DeviceResources.h"
#include "Content\Sample3DSceneRenderer.h"

// Renders Direct3D content on the screen.
namespace FreeTypeChat
{

	// item in the input queue
	struct KeyPressed
	{
		KeyPressed() :
		  m_VirtualKey(Windows::System::VirtualKey::None)
		, m_CharCode(0)
		{}

		KeyPressed(Windows::System::VirtualKey _VirtualKey, UINT _CharCode) :
		  m_VirtualKey(_VirtualKey)
		, m_CharCode(_CharCode)
		{}

		Windows::System::VirtualKey m_VirtualKey;		// Windows::System::VirtualKey. Left+Right.
		UINT						m_CharCode;			// UTF-32, [BACKSPACE]=8, [SPACE]=32, [ENTER]=13
	};


	class FreeTypeChatMain
	{
	public:
		FreeTypeChatMain();
		void CreateRenderers(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void Update();
		bool Render();

		void OnWindowSizeChanged();
		void OnSuspending();
		void OnResuming();
		void OnDeviceRemoved();


		void AddKeyToQueue(KeyPressed _key);

	private:
		// TODO: Replace with your own content renderers.
		std::unique_ptr<Sample3DSceneRenderer> m_sceneRenderer;

		// Rendering loop timer.
		DX::StepTimer m_timer;


		// input queue
		std::mutex m_InputQueueMutex;
		std::queue<KeyPressed, std::deque<KeyPressed>> m_InputQueue;		// typed characters to be processed

		UINT	m_CursorIndex;	// 0-based index of the cursor position in the input string
	};
}