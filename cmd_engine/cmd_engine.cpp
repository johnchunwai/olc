#include "cmd_engine.h"
#include <array>
#include <stdexcept>
#include <thread>
#include <iostream>
#include <boost/format.hpp>
#include <cstdio>


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
	// sprite class
	//
	sprite::sprite(int w, int h)
	{
		create(w, h);
	}

	sprite::sprite(const std::wstring& file)
	{
		if (!load(file)) {
			create(8, 8);
		}
	}

	void sprite::create(int w, int h)
	{
		_width = w;
		_height = h;
		_glyphs.resize(w * h, L' ');
		_colors.resize(w * h, color_t::fg_black);
	}

	bool sprite::save(const std::wstring& file) const
	{
		FILE* f{ nullptr };
		_wfopen_s(&f, file.c_str(), L"wb");
		if (!f) {
			return false;
		}

		fwrite(&_width, sizeof(_width), 1, f);
		fwrite(&_height, sizeof(_height), 1, f);
		fwrite(_glyphs.data(), sizeof(wchar_t), _width * _height, f);
		fwrite(_colors.data(), sizeof(color_t::enum_t), _width * _height, f);

		fclose(f);
		return true;
	}

    bool sprite::load(const std::wstring& file)
    {
        _width = 0;
        _height = 0;
        _glyphs.clear();
        _colors.clear();

        FILE* f{ nullptr };
        _wfopen_s(&f, file.c_str(), L"rb");
        if (!f) {
            return false;
        }

        fread(&_width, sizeof(_width), 1, f);
        fread(&_height, sizeof(_height), 1, f);
        create(_width, _height);
        fread(_glyphs.data(), sizeof(wchar_t), _width * _height, f);
        fread(_colors.data(), sizeof(color_t::enum_t), _width * _height, f);

        fclose(f);
        return true;
    }

    bool sprite::load_from_resource(uint32_t id)
    {
        // RT_RCDATA is resource type raw cdata.
        HRSRC resinfo = FindResource(nullptr, MAKEINTRESOURCE(id), RT_RCDATA);
        if (!resinfo) {
            return false;
        }
        HGLOBAL res = LoadResource(nullptr, resinfo);
        if (!res) {
            return false;
        }
        char *res_data = (char*)LockResource(res);
        DWORD res_size = SizeofResource(nullptr, resinfo);

        _width = 0;
        _height = 0;
        _glyphs.clear();
        _colors.clear();

        int *res_dimension = reinterpret_cast<int*>(res_data);
        std::copy_n(res_dimension, 1, &_width);
        std::copy_n(res_dimension + 1, 1, &_height);
        create(_width, _height);
        wchar_t *res_glyphs = reinterpret_cast<wchar_t*>(res_data + sizeof(int) * 2);
        std::copy_n(res_glyphs, _width * _height, _glyphs.begin());
        short *res_color = reinterpret_cast<short*>(res_glyphs + _width * _height);
        std::copy_n(res_color, _width * _height, _colors.begin());

        return true;
    }

	void sprite::set_glyph(int x, int y, wchar_t c)
	{
		if (!out_of_bound(x, y)) {
			_glyphs[y * _width + x] = c;
		}
	}

	void sprite::set_color(int x, int y, short c)
	{
		if (!out_of_bound(x, y)) {
			_colors[y * _width + x] = c;
		}
	}

	wchar_t sprite::get_glyph(int x, int y) const
	{
		if (!out_of_bound(x, y)) {
			return _glyphs[y * _width + x];
		}
		else {
			return L' ';
		}
	}

	short sprite::get_color(int x, int y) const
	{
		if (!out_of_bound(x, y)) {
			return _colors[y * _width + x];
		}
		else {
			return color_t::fg_black;
		}
	}

	wchar_t sprite::sample_glyph(float x, float y) const
	{
		int sx = static_cast<int>(x * _width);
		int sy = static_cast<int>(y * _height);
		return get_glyph(sx, sy);
	}

	short sprite::sample_color(float x, float y) const
	{
		int sx = static_cast<int>(x * _width);
		int sy = static_cast<int>(y * _height);
		return get_color(sx, sy);
	}

	bool sprite::out_of_bound(int x, int y) const
	{
		return x < 0 || x >= _width || y < 0 || y >= _height;
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
						//OutputDebugString(boost::str(boost::wformat(L"key %1% pressed\n") % i).c_str());
					}
					else {
						_keys[i].released = true;
						_keys[i].held = false;
						//OutputDebugString(boost::str(boost::wformat(L"key %1% released\n") % i).c_str());
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
						//OutputDebugString(boost::str(boost::wformat(L"mouse (%1%, %2%)\n") % _mousex % _mousey).c_str());
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
						//OutputDebugString(boost::str(boost::wformat(L"mouse %1% pressed\n") % m).c_str());
					}
					else {
						_mouse[m].released = true;
						_mouse[m].held = false;
						//OutputDebugString(boost::str(boost::wformat(L"mouse %1% released\n") % m).c_str());
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
	// Bresenham's line algorithm
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

	void cmd_engine::draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, wchar_t c, short color)
	{
		draw_line(x1, y1, x2, y2, c, color);
		draw_line(x1, y1, x3, y3, c, color);
		draw_line(x2, y2, x3, y3, c, color);
	}

	// Bresenham’s circle drawing algorithm
	// https://www.geeksforgeeks.org/bresenhams-circle-drawing-algorithm/
	void cmd_engine::draw_circle(int xc, int yc, int r, wchar_t c, short color)
	{
		if (r <= 0) {
			return;
		}
		int x = 0, y = r;
		int d = 3 - 2 * r;
		// loop through 1/8 of a circle
		while (y >= x) {
			draw(xc + x, yc + y, c, color);
			draw(xc + x, yc - y, c, color);
			draw(xc - x, yc + y, c, color);
			draw(xc - x, yc - y, c, color);
			draw(xc + y, yc + x, c, color);
			draw(xc + y, yc - x, c, color);
			draw(xc - y, yc + x, c, color);
			draw(xc - y, yc - x, c, color);
			if (d < 0) {
				d += 4 * x++ + 6;
			}
			else {
				d += 4 * (x++ - y--) + 10;
			}
		}
	}

	void cmd_engine::fill_circle(int xc, int yc, int r, wchar_t c, short color)
	{
		if (r <= 0) {
			return;
		}
		int x = 0, y = r;
		int d = 3 - 2 * r;

		auto draw_scanline = [this, c, color](int sx, int sy, int ex) {
			for (int x = sx; x <= ex; ++x) {
				draw(x, sy, c, color);
			}
		};

		// loop through 1/8 of a circle
		while (y >= x) {
			// modified to draw scan lines instead
			draw_scanline(xc - x, yc + y, xc + x);
			draw_scanline(xc - x, yc - y, xc + x);
			draw_scanline(xc - y, yc + x, xc + y);
			draw_scanline(xc - y, yc - x, xc + y);
			if (d < 0) {
				d += 4 * x++ + 6;
			}
			else {
				d += 4 * (x++ - y--) + 10;
			}
		}
	}

	void cmd_engine::draw_sprite(int x, int y, const sprite& sprite)
	{
		for (int i = 0; i < sprite.width(); ++i) {
			if (x + i >= _width) {
				break;
			}
			for (int j = 0; j < sprite.height(); ++j) {
				if (y + j >= _height) {
					break;
				}
				wchar_t glyph = sprite.get_glyph(i, j);
				if (glyph != L' ') {
					draw(x + i, y + j, glyph, sprite.get_color(i, j));
				}
			}
		}
	}

	// Draws part of the sprite
	void cmd_engine::draw_partial_sprite(int x, int y, const sprite& sprite, int sx, int sy, int w, int h)
	{
		assert("assert: sprite.width() > sx + w" && sprite.width() > sx + w);
		assert("assert: (sprite.height() > sy + h" && sprite.height() > sy + h);
		for (int i = 0; i < w; ++i) {
			if (x + i >= _width) {
				break;
			}
			for (int j = 0; j < h; ++j) {
				if (y + j >= _height) {
					break;
				}
				wchar_t glyph = sprite.get_glyph(sx + i, sy + j);
				if (glyph != L' ') {
					draw(x + i, y + j, glyph, sprite.get_color(sx + i, sy + j));
				}
			}
		}
	}

    // r is rotation of the model, rotation is base on (0,0) of the model; s is scale
    void cmd_engine::draw_wire_polygon(const std::vector<std::pair<float, float>>& model, float x, float y, float r, float s,
        wchar_t c, short color)
    {
        // rotate -> scale -> translate -> draw
        auto transformed{ model };
        for (auto &vert : transformed) {
            // rotate
            // note the rotation equation is diff from typical math book as our y-axis is inverted
            auto xr = vert.first * cosf(r) - vert.second * sinf(r);
            auto yr = -vert.first * sinf(r) - vert.second * cosf(r);
            vert.first = xr;
            vert.second = yr;
            // scale
            vert.first *= s;
            vert.second *= s;
            // translate
            vert.first += x;
            vert.second += y;
        }

        // draw closed polygon
        size_t nverts = model.size();
        for (size_t i = 0; i < nverts; ++i) {
            auto j = i + 1;
            draw_line((int)transformed[i % nverts].first, (int)transformed[i % nverts].second,
                (int)transformed[j % nverts].first, (int)transformed[j % nverts].second, c, color);
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