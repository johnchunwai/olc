#pragma once
//#pragma comment(lib, "winmm.lib")

#ifndef UNICODE
#error Please enable UNICODE for your compiler! VS: Project Properties -> General -> \
Character Set -> Use Unicode. Thanks! - Javidx9
#endif

#include <windows.h>
#include <string>
#include <memory>
#include <array>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <mutex>

using namespace std::string_literals;

namespace olc
{
	//
	// utility functions
	//
	void pause();

	//
	// Calculated by the basic FOREGROUD_XXX, BACKGROUND_XXX flags defined in windows.h that includes only red, green, blue, intensity (gray) colors.
	// The followings are calculated by mixing those basic colors.
	//
	namespace color_t
	{
		enum enum_t
		{
			fg_black = 0x0000,
			fg_dark_blue = 0x0001,
			fg_dark_green = 0x0002,
			fg_dark_cyan = 0x0003,
			fg_dark_red = 0x0004,
			fg_dark_magenta = 0x0005,
			fg_dark_yellow = 0x0006,
			fg_grey = 0x0007, // thanks ms :-/
			fg_dark_grey = 0x0008,
			fg_blue = 0x0009,
			fg_green = 0x000a,
			fg_cyan = 0x000b,
			fg_red = 0x000c,
			fg_magenta = 0x000d,
			fg_yellow = 0x000e,
			fg_white = 0x000f,

			bg_black = 0x0000,
			bg_dark_blue = 0x0010,
			bg_dark_green = 0x0020,
			bg_dark_cyan = 0x0030,
			bg_dark_red = 0x0040,
			bg_dark_magenta = 0x0050,
			bg_dark_yellow = 0x0060,
			bg_grey = 0x0070,
			bg_dark_grey = 0x0080,
			bg_blue = 0x0090,
			bg_green = 0x00a0,
			bg_cyan = 0x00b0,
			bg_red = 0x00c0,
			bg_magenta = 0x00d0,
			bg_yellow = 0x00e0,
			bg_white = 0x00f0,
		};
	}

	//
	// Unicode characters test display blocks in different "shade".
	// Check https://www.fileformat.info/info/unicode/char/<character-code>/browsertest.htm
	// by replace <character-code> with, say, 2592, to see what it looks like.
	//
	namespace pixel_type
	{
		enum enum_t
		{
			solid = 0x2588,
			threequarters = 0x2593,
			half = 0x2592,
			quarter = 0x2591,
		};
	}

	struct keystate {
		bool pressed;
		bool released;
		bool held;
	};

	//
	// Custom exception class to support unicode msg. Implementation mostly copied from std::runtime_error
	//
	class olc_exception : public std::exception { // base of all runtime-error exceptions
	public:
		using _Mybase = exception;

		explicit olc_exception(std::wstring_view msg) : _Mybase(), _msg(msg) { // construct from message string
		}

		std::wstring_view msg() const
		{
			return _msg;
		}

#if _HAS_EXCEPTIONS

#else // _HAS_EXCEPTIONS
	protected:
		virtual void _Doraise() const { // perform class-specific exception handling
			_RAISE(*this);
		}
#endif // _HAS_EXCEPTIONS
	private:
		std::wstring _msg;
	};

	class cmd_engine
	{
	public:
		static constexpr int g_num_keys = 256;
		static constexpr int g_num_mouse_buttons = 5;

	public:
		virtual ~cmd_engine();

		void construct_console(int w, int h, int fontw, int fonth);
		void start();
		virtual void close();

		int width() const { return _width; }
		int height() const { return _height; }

		keystate get_key(int key_id) const { return _keys[key_id]; }
		keystate get_mouse(int button_id) const { return _mouse[button_id]; }
		int get_mouse_x() const { return _mousex; }
		int get_mouse_y() const { return _mousey; }

		bool is_focused() const { return _in_focus; }

		//
		// draw methods
		//
		void draw(int x, int y, wchar_t c = pixel_type::solid, short color = color_t::fg_white);
		void draw_no_bound_check(int x, int y, wchar_t c = pixel_type::solid, short color = color_t::fg_white);
		// x2, y2, is inclusive
		void fill(int x1, int y1, int x2, int y2, wchar_t c = pixel_type::solid, short color = color_t::fg_white);
		void draw_string(int x, int y, const std::wstring& s, short color = color_t::fg_white);
		void draw_string_alpha(int x, int y, const std::wstring& s, short color = color_t::fg_white);
		// x, y are inclusive
		void draw_line(int x1, int y1, int x2, int y2, wchar_t c = pixel_type::solid, short color = color_t::fg_white);

		// must override
		virtual bool on_user_init() = 0;
		virtual bool on_user_update(float elapsed) = 0;

		// optional
		virtual bool on_user_destroy() { return true; }

	private:
		// Main game thread
		void gamethread();

	private:
		bool out_of_bound(int x, int y) const { return (x < 0 || x >= _width || y < 0 || y >= _height); }
		int screen_index(int x, int y) const { return y * _width + x; }
		void clip(int &x, int &y) {
			x = std::clamp(x, 0, _width - 1);
			y = std::clamp(y, 0, _height - 1);
		}

		std::wstring format_error(std::wstring_view msg) const;

		// static as it's very hacky to pass in instance method to SetConsoleCtrlHandler
		static bool console_close_handler(DWORD event);

	protected:
		std::wstring _app_name{ L"cmd engine"s };

	private:
		HANDLE _console{ INVALID_HANDLE_VALUE };
		HANDLE _orig_console{ INVALID_HANDLE_VALUE };
		HANDLE _stdin{ INVALID_HANDLE_VALUE };
		int _width{ 0 };
		int _height{ 0 };
		SMALL_RECT _rect;
		std::vector<CHAR_INFO> _screen_buf;

		std::array<keystate, g_num_keys> _keys;
		std::array<short, g_num_keys> _key_old_state;
		std::array<short, g_num_keys> _key_new_state;
		std::array<keystate, g_num_mouse_buttons> _mouse;
		std::array<bool, g_num_mouse_buttons> _mouse_old_state;
		std::array<bool, g_num_mouse_buttons> _mouse_new_state;
		int _mousex = 0;
		int _mousey = 0;
		bool _in_focus{ true };

		// static as will be used by static console_close_handler
		static std::atomic<bool> _active;
		static std::condition_variable _gamethread_ended_cv;
		static std::mutex _gamethread_mutex;
	};
}