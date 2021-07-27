#include "api/InputQueue.h"

#include <cassert>

namespace egg
{
	InputData::InputData() : m_KeyStates(), m_MouseStates()
	{
		//Init keys to not pressed.
		for (auto& keyState : m_KeyStates)
		{
			keyState = ButtonState::NOT_PRESSED;
		}

		for (auto& buttonState : m_MouseStates)
		{
			buttonState = ButtonState::NOT_PRESSED;
		}
	}

	InputData InputData::TakeData()
	{
		//Create new data object.
		InputData data;

		//Swap contents of the queues.
		data.m_MouseEvents.swap(m_MouseEvents);
		data.m_KeyboardEvents.swap(m_KeyboardEvents);

		//Copy the key events and reset any that were marked as PRESSED_RELEASED because they are no longer pressed.
		for (auto i = 0; i < 512; i++)
		{
			const ButtonState state = m_KeyStates[i];
			data.m_KeyStates[i] = state;

			//If the key was briefly pressed and then released, set it as unpressed again.
			if (state == ButtonState::PRESSED_RELEASED)
			{
				m_KeyStates[i] = ButtonState::NOT_PRESSED;
			}

			//First pressed keys now become held down.
			if (state == ButtonState::FIRST_PRESSED)
			{
				m_KeyStates[i] = ButtonState::HELD_DOWN;
			}
		}

		//Copy the mouse states over.
		for (auto i = 0; i < 3; i++)
		{
			const ButtonState state = m_MouseStates[i];
			data.m_MouseStates[i] = state;

			if (state == ButtonState::PRESSED_RELEASED)
			{
				m_MouseStates[i] = ButtonState::NOT_PRESSED;
			}
		}

		//Move the data object.
		return data;
	}

	bool InputData::GetNextEvent(KeyboardEvent& keyboardEvent)
	{
		const bool hasData = !m_KeyboardEvents.empty();
		if (hasData)
		{
			keyboardEvent = m_KeyboardEvents.front();
			m_KeyboardEvents.pop();
		}
		return hasData;
	}

	bool InputData::GetNextEvent(MouseEvent& mouseEvent)
	{
		const bool hasData = !m_MouseEvents.empty();
		if (hasData)
		{
			mouseEvent = m_MouseEvents.front();
			m_MouseEvents.pop();
		}

		return hasData;
	}

	void InputData::AddMouseEvent(const MouseEvent& event)
	{
		//Mouse buttons can be held down between queries.
		//This keeps track of that state and only changes it once it has been released.
		if (event.action == MouseAction::CLICK)
		{
			if (m_MouseStates[static_cast<uint16_t>(event.button)] != ButtonState::HELD_DOWN)
			{
				m_MouseStates[static_cast<uint16_t>(event.button)] = ButtonState::FIRST_PRESSED;
			}
		}
		else if (event.action == MouseAction::RELEASE)
		{
			m_MouseStates[static_cast<uint16_t>(event.button)] = ButtonState::PRESSED_RELEASED;
		}

		m_MouseEvents.push(event);
	}

	void InputData::AddKeyboardEvent(const KeyboardEvent& event)
	{
		//Invalid or unknown keys are skipped.
		if (event.keyCode < 0 || event.keyCode > 511)
		{
			return;
		}

		//Keys can be held down for several ticks.
		//This keeps track of state until a button was released.
		//Only then will the key state change.
		if (event.action == KeyboardAction::KEY_PRESSED)
		{
			if (m_KeyStates[event.keyCode] != ButtonState::HELD_DOWN)
			{
				m_KeyStates[event.keyCode] = ButtonState::FIRST_PRESSED;
			}
		}
		else if (event.action == KeyboardAction::KEY_RELEASED)
		{
			m_KeyStates[event.keyCode] = ButtonState::PRESSED_RELEASED;
		}

		m_KeyboardEvents.push(event);
	}

	ButtonState InputData::GetMouseButtonState(MouseButton button) const
	{
		return m_MouseStates[static_cast<std::uint8_t>(button)];
	}

	void InputData::SetKeyState(std::uint16_t keyCode, ButtonState state)
	{
		assert(keyCode < 512);
		m_KeyStates[keyCode] = state;
	}

	void InputData::SetMouseButtonState(MouseButton button, ButtonState state)
	{
		assert(static_cast<unsigned char>(button) < 3);
		m_MouseStates[static_cast<unsigned char>(button)] = state;
	}

	ButtonState InputData::GetKeyState(std::uint16_t keyCode) const
	{
		assert(keyCode < 512);
		return m_KeyStates[keyCode];
	}

	InputData InputQueue::GetQueuedEvents()
	{
		//Make sure no input can be added for now.
		mutex.lock();

		//Take the data 
		InputData copy = data.TakeData();

		//Allow new input.
		mutex.unlock();

		//Move the new data object that now contains the data.
		return copy;
	}

	void InputQueue::AddMouseEvent(const MouseEvent& a_Event)
	{
		mutex.lock();
		data.AddMouseEvent(a_Event);
		mutex.unlock();
	}

	void InputQueue::AddKeyboardEvent(const KeyboardEvent& a_Event)
	{
		mutex.lock();
		data.AddKeyboardEvent(a_Event);
		mutex.unlock();
	}

	void InputQueue::SetKeyState(std::uint16_t a_KeyCode, ButtonState a_State)
	{
		mutex.lock();
		data.SetKeyState(a_KeyCode, a_State);
		mutex.unlock();
	}

	ButtonState InputQueue::GetKeyState(std::uint16_t a_KeyCode) const
	{
		return data.GetKeyState(a_KeyCode);
	}

	void InputQueue::SetMouseState(MouseButton a_Button, ButtonState a_State)
	{
		mutex.lock();
		data.SetMouseButtonState(a_Button, a_State);
		mutex.unlock();
	}

	ButtonState InputQueue::GetMouseState(MouseButton a_Button) const
	{
		return data.GetMouseButtonState(a_Button);
	}
}
