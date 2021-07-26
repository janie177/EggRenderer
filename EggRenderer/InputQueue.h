#pragma once
#include <queue>
#include <mutex>
#include <cmath>

namespace egg
{
	/*
	 * Actions a key can perform.
	 * Usually this is up and down.
	 * Some say there are keys that can move *sideways*.
	 * These are only crazy fisherman's tales of course, and nobody has actually seen a key move in that direction.
	 * Nobody that could tell the tale...
	 */
	enum class KeyboardAction
	{
		KEY_PRESSED,
		KEY_RELEASED,

		NONE
	};

	enum class ButtonState : std::uint8_t
	{
		HELD_DOWN,
		PRESSED_RELEASED,
		NOT_PRESSED,
		FIRST_PRESSED,
	};

	/*
	 * Types of button on the mouse.
	 */
	enum class MouseButton : std::uint8_t
	{
		LMB,
		MMB,
		RMB,
		NONE
	};

	/*
	 * Actions you can perform with a mouse.
	 * I'm not including those fancy macro mouse options.
	 */
	enum class MouseAction
	{
		//Clicks
		CLICK,
		RELEASE,

		//Scroll the mouse up or down.
		SCROLL,

		//Move the mouse in the X or Y direction.
		MOVE_X,
		MOVE_Y,

		NONE
	};

	/*
	 * A keyboard event contains a key code and information about the type of press.
	 */
	struct KeyboardEvent
	{
		//Empty event.
		KeyboardEvent()
		{
			keyCode = 0;
			action = KeyboardAction::NONE;
		}

		KeyboardEvent(KeyboardAction action, std::uint16_t keyCode)
		{
			this->action = action;
			this->keyCode = keyCode;
		}

		std::int16_t keyCode;
		KeyboardAction action;
	};

	/*
	 * MouseEvent contains information about the type of event.
	 * This could be movement or button up/down.
	 *
	 * Value contains a value associated with a movement or scroll optionally.
	 */
	struct MouseEvent
	{
		//Empty event.
		MouseEvent()
		{
			action = MouseAction::NONE;
			button = MouseButton::NONE;
			value = 0;
		}

		MouseEvent(MouseAction action, std::float_t value, MouseButton button)
		{
			this->action = action;
			this->value = value;
			this->button = button;
		}

		MouseAction action;
		MouseButton button;
		std::float_t value;
	};

	class InputData
	{
	public:
		InputData();

		/*
		 * Create a new InputData object that clears the original.
		 */
		InputData TakeData();

		/*
		 * Get the next mouse event if available.
		 * True is returned if an event was copied into the passed reference.
		 * False is returned if no more events were queued up.
		 */
		bool GetNextEvent(KeyboardEvent& a_KeyboardEvent);

		/*
		 * Get the next queued mouse event.
		 * True is returned if a mouse event was copied into the passes reference.
		 * False is returned if no more mouse events were queried up.
		 */
		bool GetNextEvent(MouseEvent& a_MouseEvent);

		/*
		 * Add a mouse event to the queue.
		 */
		void AddMouseEvent(const MouseEvent& a_Event);

		/*
		 * Add a keyboard event to the queue.
		 */
		void AddKeyboardEvent(const KeyboardEvent& a_Event);

		/*
		 *	Set the state of the given key.
		 */
		void SetKeyState(std::uint16_t keyCode, ButtonState a_State);

		/*
		 * Set the state of a mouse button.
		 */
		void SetMouseButtonState(MouseButton a_Button, ButtonState a_State);

		/*
		 * Get the state of the given key.
		 */
		ButtonState GetKeyState(std::uint16_t a_KeyCode) const;

		/*
		 * Get the state of a mouse button.
		 */
		ButtonState GetMouseButtonState(MouseButton a_Button) const;

	private:
		std::queue<KeyboardEvent> m_KeyboardEvents;
		std::queue<MouseEvent> m_MouseEvents;

		/*
		 * Keys may be held down, which means there won't always be an event.
		 * This enum keeps track of whether a key was pressed briefly or held down.
		 */
		ButtonState m_KeyStates[512];
		ButtonState m_MouseStates[3];
	};

	/*
	 * Thread-safe queue that can be read from.
	 */
	class InputQueue
	{
	public:
		InputQueue() = default;

		InputQueue(const InputQueue&) = delete;
		InputQueue(InputQueue&&) = delete;
		InputQueue& operator =(const InputQueue&) = delete;
		InputQueue& operator =(InputQueue&&) = delete;

		/*
		 * Copy the data container that has all queued events.
		 * This clears the queue in the original item.
		 */
		InputData GetQueuedEvents();

		/*
		 * Add a mouse event to the queue.
		 */
		void AddMouseEvent(const MouseEvent& a_Event);

		/*
		 * Add a keyboard event to the queue.
		 */
		void AddKeyboardEvent(const KeyboardEvent& a_Event);

		/*
		 *	Set the state of the given key.
		 */
		void SetKeyState(std::uint16_t a_KeyCode, ButtonState a_State);

		/*
		 * Get the state of the given key.
		 */
		ButtonState GetKeyState(std::uint16_t a_KeyCode) const;

		/*
		 *	Set the state for the given mouse button.
		 */
		void SetMouseState(MouseButton a_Button, ButtonState a_State);

		/*
		 * Get the state for the given mouse button.
		 */
		ButtonState GetMouseState(MouseButton a_Button) const;

	private:
		//Mutex to ensure only one thread can access the data.
		std::mutex mutex;
		InputData data;
	};
}