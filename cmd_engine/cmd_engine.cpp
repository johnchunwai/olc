#include "cmd_engine.h"
#include <array>
#include <stdexcept>
#include <thread>
#include <iostream>
#include <boost/format.hpp>


using namespace std;

namespace olc {
	//
	// Utility functions
	//
	void pause()
	{
		cout << "Press enter to quit";
		cin.ignore();
	}

	//
	// cmd_engine class
	//
	std::atomic<bool> cmd_engine::_active{ false };
	std::condition_variable cmd_engine::_gamethread_ended_cv;
	std::mutex cmd_engine::_gamethread_mutex;

	cmd_engine::~cmd_engine()
	{
		OutputDebugString(L"~cmd_engine()\n");
		close();
	}

	void cmd_engine::close()
	{
		OutputDebugString(L"close()\n");
		// TODO: sound clean up

		if (_console != INVALID_HANDLE_VALUE) {
			SetConsoleActiveScreenBuffer(_orig_console);
			CloseHandle(_console);
			_console = INVALID_HANDLE_VALUE;
		}
	}

	bool cmd_engine::console_close_handler(DWORD ctrl_type)
	{
		// handles notifications from windows similar to windows app
		// we're only interested in the event when user closes the console window
		if (ctrl_type == CTRL_CLOSE_EVENT) {
			OutputDebugString(L"console_close_handler() begin\n");
			// init shutdown sequence
			_active = false;

			// wait for game thread to be exited (to a max of 15 sec)
			unique_lock<mutex> lk(_gamethread_mutex);
			_gamethread_ended_cv.wait(lk);
			OutputDebugString(L"console_close_handler() end\n");
		}
		// return true marks the event as processed so events like Ctrl-C won't kill our game.
		return true;
	}

	void cmd_engine::construct_console(int w, int h, int fontw, int fonth)
	{
		_width = w;
		_height = h;
		_rect = { 0, 0, (short)w - 1, (short)h - 1 };

		_stdin = GetStdHandle(STD_INPUT_HANDLE);
		_orig_console = GetStdHandle(STD_OUTPUT_HANDLE);
		_console = CreateConsoleScreenBuffer(GENERIC_WRITE | GENERIC_READ, 0, nullptr, CONSOLE_TEXTMODE_BUFFER, nullptr);
		try {
			if (_console == INVALID_HANDLE_VALUE) {
				throw olc_exception(format_error(L"CreateConsoleScreenBuffer"));
			}

			//
			// Screen buffer must be >= windows size.
			// Shrink window to minimal size so that we can freely set the screen buffer size.
			// After we resize screen buffer, we can resize window.
			//

			// make the new console active first. Or changing font size have no effect.
			bool b = SetConsoleActiveScreenBuffer(_console);
			if (!b) {
				throw olc_exception(L"SetConsoleActiveScreenBuffer");
			}

			// Update fonts first so that it correctly determine windows size.
			CONSOLE_FONT_INFOEX font_info{ sizeof(CONSOLE_FONT_INFOEX) };
			font_info.nFont = 0;
			font_info.dwFontSize = { (short)fontw, (short)fonth };
			font_info.FontFamily = FF_DONTCARE;
			font_info.FontWeight = FW_NORMAL;
			wcscpy_s(font_info.FaceName, sizeof(font_info.FaceName) / sizeof(font_info.FaceName[0]), L"Consolas");
			b = SetCurrentConsoleFontEx(_console, false, &font_info);
			if (!b) {
				throw olc_exception(L"SetCurrentConsoleFontEx");
			}

			// minimize window
			SMALL_RECT minrect = { (short)0, (short)0, (short)1, (short)1 };
			b = SetConsoleWindowInfo(_console, true, &minrect);
			if (!b) {
				throw olc_exception(L"SetConsoleWindowInfo when minimize window");
			}

			// set screen buffer size
			b = SetConsoleScreenBufferSize(_console, { (short)w, (short)h });
			if (!b) {
				throw olc_exception(L"SetConsoleScreenBufferSize");
			}

			// check max allowed window size. throw if exceeded
			CONSOLE_SCREEN_BUFFER_INFO info;
			b = GetConsoleScreenBufferInfo(_console, &info);
			if (!b) {
				throw olc_exception(L"GetConsoleScreenBufferInfo");
			}
			if (w > info.dwMaximumWindowSize.X) {
				throw olc_exception(L"Screen width / font width too big. Allowed max width="s + to_wstring(info.dwMaximumWindowSize.X));
			}
			if (h > info.dwMaximumWindowSize.Y) {
				throw olc_exception(L"Screen height / font height too big. Allowed max height="s + to_wstring(info.dwMaximumWindowSize.Y));
			}

			// set console window size
			b = SetConsoleWindowInfo(_console, true, &_rect);
			if (!b) {
				throw olc_exception(L"SetConsoleWindowInfo");
			}

			// set console mode to allow mouse input
			b = SetConsoleMode(_stdin, ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT);
			if (!b) {
				throw olc_exception(L"SetConsoleMode");
			}

			// allocate memory for screen buffer
			_screen_buf.resize(w * h, { 0,0 });

			// set console event handler
			SetConsoleCtrlHandler((PHANDLER_ROUTINE) console_close_handler, true);
		}
		catch (std::exception&) {
			close();
			throw;
		}
	}

	void cmd_engine::start()
	{
		_active = true;
		auto t = thread(&cmd_engine::gamethread, this);
		t.join();
	}

	void cmd_engine::gamethread()
	{
		constexpr short keydown = 0x8000;

		// init user resources
		if (!on_user_init()) {
			_active = false;
		}


		// TODO: sound

		// init time
		auto prev_time = chrono::system_clock::now();
		auto curr_time = chrono::system_clock::now();

		while (_active) {
			//
			// handle timing
			//
			curr_time = chrono::system_clock::now();
			float elapsed = chrono::duration<float>(curr_time - prev_time).count();
			prev_time = curr_time;

			//
			// handle input
			//

			// keyboard
			for (int i = 0; i < g_num_keys; ++i) {
				_key_new_state[i] = GetAsyncKeyState(i);

				// pressed is only true if the key changes from not down to down
				// released is only true if the key changes from down to not down
				_keys[i].pressed = false;
				_keys[i].released = false;

				if (_key_new_state[i] != _key_old_state[i]) {
					// 0x8000 is key down
					if (_key_new_state[i] & keydown) {
						_keys[i].pressed = !_keys[i].held;
						_keys[i].held = true;
						OutputDebugString(boost::str(boost::wformat(L"key %1% pressed\n") % i).c_str());
					}
					else {
						_keys[i].released = true;
						_keys[i].held = false;
						OutputDebugString(boost::str(boost::wformat(L"key %1% released\n") % i).c_str());
					}
				}
				_key_old_state[i] = _key_new_state[i];
			}

			// mouse
			array<INPUT_RECORD, 32> inevents;
			DWORD nevents;
			GetNumberOfConsoleInputEvents(_stdin, &nevents);
			if (nevents > 0) {
				ReadConsoleInput(_stdin, inevents.data(), inevents.size(), &nevents);
			}
			for (DWORD i = 0; i < nevents; ++i) {
				switch (inevents[i].EventType) {
				case FOCUS_EVENT:
				{
					_in_focus = inevents[i].Event.FocusEvent.bSetFocus;
					OutputDebugString(boost::str(boost::wformat(L"In focus = %1%\n") % _in_focus).c_str());
					break;
				}
				case MOUSE_EVENT:
				{
					const auto& mouseevent = inevents[i].Event.MouseEvent;
					switch (mouseevent.dwEventFlags) {
					case 0:		// button is clicked
					{
						for (int m = 0; m < g_num_mouse_buttons; ++m) {
							_mouse_new_state[m] = (mouseevent.dwButtonState & (1 << m)) > 0;
						}
						break;
					}
					case MOUSE_MOVED:
					{
						_mousex = mouseevent.dwMousePosition.X;
						_mousey = mouseevent.dwMousePosition.Y;
						OutputDebugString(boost::str(boost::wformat(L"mouse (%1%, %2%)\n") % _mousex % _mousey).c_str());
						break;
					}
					default:
						break;
					}
					break;
				}
				default:
					break;
				}
			}
			for (int m = 0; m < g_num_mouse_buttons; ++m) {
				_mouse[m].pressed = false;
				_mouse[m].released = false;

				if (_mouse_new_state[m] != _mouse_old_state[m]) {
					if (_mouse_new_state[m]) {
						_mouse[m].pressed = !_mouse[m].held;
						_mouse[m].held = true;
						OutputDebugString(boost::str(boost::wformat(L"mouse %1% pressed\n") % m).c_str());
					}
					else {
						_mouse[m].released = true;
						_mouse[m].held = false;
						OutputDebugString(boost::str(boost::wformat(L"mouse %1% released\n") % m).c_str());
					}
				}

				_mouse_old_state[m] = _mouse_new_state[m];
			}

			//
			// handle update
			//
			if (!on_user_update(elapsed)) {
				_active = false;
			}

			//
			// present screen buffer
			//
			auto title = boost::str(boost::wformat(L"OLC - Console Game Engine - %1% - FPS: %2$+3.2f") % _app_name % (1.0f / elapsed));
			SetConsoleTitle(title.c_str());
			WriteConsoleOutput(_console, _screen_buf.data(), { (short)_width, (short)_height }, { 0, 0 }, &_rect);
		}

		// clean up
		on_user_destroy();
		close();
		// notify the gamethread ends
		_gamethread_ended_cv.notify_all();
	}

	//
	// Draw methods
	//
	void cmd_engine::draw(int x, int y, wchar_t c, short color)
	{
		if (out_of_bound(x, y)) {
			return;
		}
		draw_no_bound_check(x, y, c, color);
	}

	void cmd_engine::draw_no_bound_check(int x, int y, wchar_t c, short color)
	{
		_screen_buf[screen_index(x, y)].Char.UnicodeChar = c;
		_screen_buf[screen_index(x, y)].Attributes = color;
	}

	// x2, y2, is inclusive
	void cmd_engine::fill(int x1, int y1, int x2, int y2, wchar_t c, short color)
	{
		clip(x1, y1);
		clip(x2, y2);
		for (auto x = x1; x <= x2; ++x) {
			for (auto y = y1; y < y2; ++y) {
				draw_no_bound_check(x, y, c, color);
			}
		}
	}

	void cmd_engine::draw_string(int x, int y, const std::wstring& s, short color)
	{
		if (out_of_bound(x, y)) {
			return;
		}
		if (out_of_bound(x + s.length(), y)) {
			return;
		}
		for (auto i = 0; i < s.length(); ++i) {
			draw_no_bound_check(x + i, y, s[i]);
		}
	}

	void cmd_engine::draw_string_alpha(int x, int y, const std::wstring& s, short color)
	{
		if (out_of_bound(x, y)) {
			return;
		}
		if (out_of_bound(x + s.length(), y)) {
			return;
		}
		for (auto i = 0; i < s.length(); ++i) {
			auto c = s[i];
			if (!iswblank(c)) {
				draw_no_bound_check(x + i, y, c);
			}
		}
	}

	// x, y are inclusive
	// https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm for integer arithmetic
	void cmd_engine::draw_line(int x1, int y1, int x2, int y2, wchar_t c, short color)
	{
		int x, y, dx, dy, dx_abs, dy_abs, Dx, Dy, xe, ye;
		dx = x2 - x1;
		dy = y2 - y1;
		dx_abs = abs(dx);
		dy_abs = abs(dy);
		Dx = 2 * dy_abs - dx_abs;
		Dy = 2 * dx_abs - dy_abs;
		if (dy_abs <= dx_abs) {
			// horizontal-ish line
			if (dx >= 0) {
				x = x1; y = y1; xe = x2;
			}
			else {
				x = x2; y = y2; xe = x1;
			}
			draw(x, y, c, color);
			for (x = x + 1; x <= xe; ++x) {
				if (Dx >= 0) {
					// y has gone from middle of a pixel to the edge of the next one
					y += (dx < 0 && dy < 0 || dx > 0 && dy > 0) ? 1 : -1;
					Dx -= 2 * dx_abs;
				}
				Dx += 2 * dy_abs;
				draw(x, y, c, color);
			}
		}
		else
		{
			// verical-ish line
			if (dy >= 0) {
				x = x1; y = y1; ye = y2;
			}
			else {
				x = x2; y = y2; ye = y1;
			}
			draw(x, y, c, color);
			for (y = y + 1; y <= ye; ++y) {
				if (Dy >= 0) {
					// x has gone from middle of a pixel to the edge of the next one
					x += (dx < 0 && dy < 0 || dx > 0 && dy > 0) ? 1 : -1;
					Dy -= 2 * dy_abs;
				}
				Dy += 2 * dx_abs;
				draw(x, y, c, color);
			}
		}
	}

	wstring cmd_engine::format_error(wstring_view msg) const
	{
		array<wchar_t, 256> buf;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf.data(), static_cast<DWORD>(buf.size()), nullptr);
		wstring s(L"ERROR: ");
		s.append(msg).append(L"\n\t").append(buf.data());
		return s;
	}
}