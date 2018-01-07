#include "pch.h"
#include "FreeTypeChatMain.h"
#include "Common\DirectXHelper.h"

using namespace FreeTypeChat;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;

// The DirectX 12 Application template is documented at https://go.microsoft.com/fwlink/?LinkID=613670&clcid=0x409

// Loads and initializes application assets when the application is loaded.
FreeTypeChatMain::FreeTypeChatMain() :
	m_CursorIndex(0)
{
	// TODO: Change the timer settings if you want something other than the default variable timestep mode.
	// e.g. for 60 FPS fixed timestep update logic, call:
	/*
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/
}

// Creates and initializes the renderers.
void FreeTypeChatMain::CreateRenderers(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
	// TODO: Replace this with your app's content initialization.
	m_sceneRenderer = std::unique_ptr<Sample3DSceneRenderer>(new Sample3DSceneRenderer(deviceResources));

	OnWindowSizeChanged();
}

// Updates the application state once per frame.
void FreeTypeChatMain::Update()
{
	if (!m_sceneRenderer->IsLoadingComplete())
	{
		return;
	}

	// Update scene objects.
	m_timer.Tick([&,this]()
	{
		// typing
		DirectX::XMFLOAT2 curPos = m_sceneRenderer->GetTextfield().GetCaretPosByIndex(m_CursorIndex);	// TODO: do not ask it every frame
		// consume one input queue item
		if (m_InputQueue.size() > 0)
		{
			bool hasKeyToProcess = false;	// flag to pop the queue and process "key"
			KeyPressed key;
			{
				std::lock_guard<std::mutex> locker_inputqueue(m_InputQueueMutex);	// auto-unlock on block quit
				key = m_InputQueue.front();

				if (key.m_VirtualKey == Windows::System::VirtualKey::None)
				{
					GlyphInTexture glyph;
					if (key.m_CharCode != ' ')	// [SPACE]
					{
						if (m_sceneRenderer->GetGlyph(key.m_CharCode, glyph))	// mutex synched
						{
							hasKeyToProcess = true;		// glyph is in cache, can continue to render it to output string
						}
						else
						{
							// issue render glyph to texture. TODO: it is performed every frame until the glyph is found (?)
							m_sceneRenderer->AddCharToCache(key.m_CharCode);	// mutex synched
							hasKeyToProcess = false;		// keep the key in the InputQueue until it gets glyph
						}
					}
					else
					{
						hasKeyToProcess = true;	// space
					}
				}
				else
				{
					hasKeyToProcess = true;		// cursor movement, always pass
				}

				// check is key is consumed from the queue on this iteration of Update
				if (hasKeyToProcess)
				{
					// character typed
					m_InputQueue.pop();	// TODO: do not pop if not in cache
				}

			}	// release locker_inputqueue


			if (hasKeyToProcess)
			{
				// key = key pressed

				if (key.m_VirtualKey == Windows::System::VirtualKey::None)
				{

					GlyphInTexture glyph;
					if (key.m_CharCode != ' ')	// [SPACE]
					{
						if (!m_sceneRenderer->GetGlyph(key.m_CharCode, glyph))
						{
							// ERROR! should be there as hasKeyToProcess passed
						}
					}
					else
					{
						glyph.m_TexCoord.m_Size.x = 0.02f;	// TODO: put space for [SPACE] here depends on font
						glyph.m_TexCoord.m_Size.y = 0.0f;
					}
					// here glyph = glyph for key.m_CharCode

					m_sceneRenderer->GetCursor().ResetBlink(m_timer.GetTotalSeconds());

					// type to textfield
					float tx = glyph.m_TexCoord.m_Pos.x;
					float ty = glyph.m_TexCoord.m_Pos.y;
					float w = glyph.m_TexCoord.m_Size.x;
					float h = glyph.m_TexCoord.m_Size.y;

					Windows::Foundation::Size windowSize = m_sceneRenderer->GetOutputSize();
					float aspectRatio = windowSize.Width / windowSize.Height;

					CharGlyphInTexture c =
					{
						Rectangle(DirectX::XMFLOAT2(tx, ty), DirectX::XMFLOAT2(w, h)),
						glyph.m_Baseline
					};

					m_sceneRenderer->GetTextfield().AddCharacter(m_CursorIndex, c);

					m_CursorIndex += 1;
					curPos = m_sceneRenderer->GetTextfield().GetCaretPosByIndex(m_CursorIndex);	// refresh cursor. TODO: fix it
				}
				else
				{
					// cursor movement
					switch (key.m_VirtualKey)
					{
					case Windows::System::VirtualKey::Left:
						if (m_CursorIndex > 0)
						{
							// cursor step back
							m_CursorIndex -= 1;
							curPos = m_sceneRenderer->GetTextfield().GetCaretPosByIndex(m_CursorIndex);	// refresh cursor. TODO: fix it
							m_sceneRenderer->GetCursor().ResetBlink(m_timer.GetTotalSeconds());
						}
						break;
					case Windows::System::VirtualKey::Right:
						if (m_CursorIndex < m_sceneRenderer->GetTextfield().GetNumberOfChars())
						{
							// cursor step forward
							m_CursorIndex += 1;
							curPos = m_sceneRenderer->GetTextfield().GetCaretPosByIndex(m_CursorIndex);	// refresh cursor. TODO: fix it
							m_sceneRenderer->GetCursor().ResetBlink(m_timer.GetTotalSeconds());
						}
						break;
					case Windows::System::VirtualKey::Back:
						if (m_CursorIndex > 0)
						{
							m_CursorIndex -= 1;
							m_sceneRenderer->GetTextfield().DeleteCharacter(m_CursorIndex);
							curPos = m_sceneRenderer->GetTextfield().GetCaretPosByIndex(m_CursorIndex);	// refresh cursor. TODO: fix it
							m_sceneRenderer->GetCursor().ResetBlink(m_timer.GetTotalSeconds());
						}
						break;
					case Windows::System::VirtualKey::Delete:
						if (m_CursorIndex < m_sceneRenderer->GetTextfield().GetNumberOfChars())
						{
							m_sceneRenderer->GetTextfield().DeleteCharacter(m_CursorIndex);
							curPos = m_sceneRenderer->GetTextfield().GetCaretPosByIndex(m_CursorIndex);	// refresh cursor. TODO: fix it
							m_sceneRenderer->GetCursor().ResetBlink(m_timer.GetTotalSeconds());
						}
						break;
					case Windows::System::VirtualKey::Home:
						m_CursorIndex = 0;
						curPos = m_sceneRenderer->GetTextfield().GetCaretPosByIndex(m_CursorIndex);	// refresh cursor. TODO: fix it
						m_sceneRenderer->GetCursor().ResetBlink(m_timer.GetTotalSeconds());
						break;
					case Windows::System::VirtualKey::End:
						m_CursorIndex = m_sceneRenderer->GetTextfield().GetNumberOfChars();
						curPos = m_sceneRenderer->GetTextfield().GetCaretPosByIndex(m_CursorIndex);	// refresh cursor. TODO: fix it
						m_sceneRenderer->GetCursor().ResetBlink(m_timer.GetTotalSeconds());
						break;
					}
				}
			}	// if (hasKeyToProcess)

		}	// if (m_InputQueue.size() > 0)

		m_sceneRenderer->GetCursor().UpdateCursor(m_timer.GetTotalSeconds(), DirectX::XMFLOAT2(curPos.x, curPos.y));

		m_sceneRenderer->Update(m_timer);
	});
}

// Renders the current frame according to the current application state.
// Returns true if the frame was rendered and is ready to be displayed.
bool FreeTypeChatMain::Render()
{
	// Don't try to render anything before the first Update.
	if (m_timer.GetFrameCount() == 0)
	{
		return false;
	}

	// Render the scene objects.
	// TODO: Replace this with your app's content rendering functions.
	return m_sceneRenderer->Render();
}

// Updates application state when the window's size changes (e.g. device orientation change)
void FreeTypeChatMain::OnWindowSizeChanged()
{
	// TODO: Replace this with the size-dependent initialization of your app's content.
	m_sceneRenderer->CreateWindowSizeDependentResources();
}

// Notifies the app that it is being suspended.
void FreeTypeChatMain::OnSuspending()
{
	// TODO: Replace this with your app's suspending logic.

	// Process lifetime management may terminate suspended apps at any time, so it is
	// good practice to save any state that will allow the app to restart where it left off.

	m_sceneRenderer->SaveState();

	// If your application uses video memory allocations that are easy to re-create,
	// consider releasing that memory to make it available to other applications.
}

// Notifes the app that it is no longer suspended.
void FreeTypeChatMain::OnResuming()
{
	// TODO: Replace this with your app's resuming logic.
}

// Notifies renderers that device resources need to be released.
void FreeTypeChatMain::OnDeviceRemoved()
{
	// TODO: Save any necessary application or renderer state and release the renderer
	// and its resources which are no longer valid.
	m_sceneRenderer->SaveState();
	m_sceneRenderer = nullptr;
}



void FreeTypeChatMain::AddKeyToQueue(KeyPressed _key)
{
	std::lock_guard<std::mutex> locker_inputqueue(m_InputQueueMutex);	// auto-unlock on block quit
	m_InputQueue.push(_key);
}

