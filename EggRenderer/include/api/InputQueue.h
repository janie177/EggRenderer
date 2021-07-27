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


	/*
	 * The same as GLFW keys, 
	 */
#define EGG_KEY_UNKNOWN            -1
#define EGG_KEY_SPACE              32
#define EGG_KEY_APOSTROPHE         39  /* ' */
#define EGG_KEY_COMMA              44  /* , */
#define EGG_KEY_MINUS              45  /* - */
#define EGG_KEY_PERIOD             46  /* . */
#define EGG_KEY_SLASH              47  /* / */
#define EGG_KEY_0                  48
#define EGG_KEY_1                  49
#define EGG_KEY_2                  50
#define EGG_KEY_3                  51
#define EGG_KEY_4                  52
#define EGG_KEY_5                  53
#define EGG_KEY_6                  54
#define EGG_KEY_7                  55
#define EGG_KEY_8                  56
#define EGG_KEY_9                  57
#define EGG_KEY_SEMICOLON          59  /* ; */
#define EGG_KEY_EQUAL              61  /* = */
#define EGG_KEY_A                  65
#define EGG_KEY_B                  66
#define EGG_KEY_C                  67
#define EGG_KEY_D                  68
#define EGG_KEY_E                  69
#define EGG_KEY_F                  70
#define EGG_KEY_G                  71
#define EGG_KEY_H                  72
#define EGG_KEY_I                  73
#define EGG_KEY_J                  74
#define EGG_KEY_K                  75
#define EGG_KEY_L                  76
#define EGG_KEY_M                  77
#define EGG_KEY_N                  78
#define EGG_KEY_O                  79
#define EGG_KEY_P                  80
#define EGG_KEY_Q                  81
#define EGG_KEY_R                  82
#define EGG_KEY_S                  83
#define EGG_KEY_T                  84
#define EGG_KEY_U                  85
#define EGG_KEY_V                  86
#define EGG_KEY_W                  87
#define EGG_KEY_X                  88
#define EGG_KEY_Y                  89
#define EGG_KEY_Z                  90
#define EGG_KEY_LEFT_BRACKET       91  /* [ */
#define EGG_KEY_BACKSLASH          92  /* \ */
#define EGG_KEY_RIGHT_BRACKET      93  /* ] */
#define EGG_KEY_GRAVE_ACCENT       96  /* ` */
#define EGG_KEY_WORLD_1            161 /* non-US #1 */
#define EGG_KEY_WORLD_2            162 /* non-US #2 */
#define EGG_KEY_ESCAPE             256
#define EGG_KEY_ENTER              257
#define EGG_KEY_TAB                258
#define EGG_KEY_BACKSPACE          259
#define EGG_KEY_INSERT             260
#define EGG_KEY_DELETE             261
#define EGG_KEY_RIGHT              262
#define EGG_KEY_LEFT               263
#define EGG_KEY_DOWN               264
#define EGG_KEY_UP                 265
#define EGG_KEY_PAGE_UP            266
#define EGG_KEY_PAGE_DOWN          267
#define EGG_KEY_HOME               268
#define EGG_KEY_END                269
#define EGG_KEY_CAPS_LOCK          280
#define EGG_KEY_SCROLL_LOCK        281
#define EGG_KEY_NUM_LOCK           282
#define EGG_KEY_PRINT_SCREEN       283
#define EGG_KEY_PAUSE              284
#define EGG_KEY_F1                 290
#define EGG_KEY_F2                 291
#define EGG_KEY_F3                 292
#define EGG_KEY_F4                 293
#define EGG_KEY_F5                 294
#define EGG_KEY_F6                 295
#define EGG_KEY_F7                 296
#define EGG_KEY_F8                 297
#define EGG_KEY_F9                 298
#define EGG_KEY_F10                299
#define EGG_KEY_F11                300
#define EGG_KEY_F12                301
#define EGG_KEY_F13                302
#define EGG_KEY_F14                303
#define EGG_KEY_F15                304
#define EGG_KEY_F16                305
#define EGG_KEY_F17                306
#define EGG_KEY_F18                307
#define EGG_KEY_F19                308
#define EGG_KEY_F20                309
#define EGG_KEY_F21                310
#define EGG_KEY_F22                311
#define EGG_KEY_F23                312
#define EGG_KEY_F24                313
#define EGG_KEY_F25                314
#define EGG_KEY_KP_0               320
#define EGG_KEY_KP_1               321
#define EGG_KEY_KP_2               322
#define EGG_KEY_KP_3               323
#define EGG_KEY_KP_4               324
#define EGG_KEY_KP_5               325
#define EGG_KEY_KP_6               326
#define EGG_KEY_KP_7               327
#define EGG_KEY_KP_8               328
#define EGG_KEY_KP_9               329
#define EGG_KEY_KP_DECIMAL         330
#define EGG_KEY_KP_DIVIDE          331
#define EGG_KEY_KP_MULTIPLY        332
#define EGG_KEY_KP_SUBTRACT        333
#define EGG_KEY_KP_ADD             334
#define EGG_KEY_KP_ENTER           335
#define EGG_KEY_KP_EQUAL           336
#define EGG_KEY_LEFT_SHIFT         340
#define EGG_KEY_LEFT_CONTROL       341
#define EGG_KEY_LEFT_ALT           342
#define EGG_KEY_LEFT_SUPER         343
#define EGG_KEY_RIGHT_SHIFT        344
#define EGG_KEY_RIGHT_CONTROL      345
#define EGG_KEY_RIGHT_ALT          346
#define EGG_KEY_RIGHT_SUPER        347
#define EGG_KEY_MENU               348
#define EGG_KEY_LAST               EGG_KEY_MENU

}