#include "cmd_engine.h"
#include <array>
#include <stdexcept>


using namespace std;

namespace olc {
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
			wcscpy_s(font_info.FaceName, sizeof(font_info.FaceName), L"Consolas");
			b = SetCurrentConsoleFontEx(_console, false, &font_info);
			if (!b) {
				throw olc_exception(L"SetCurrentConsoleFontEx");
			}

			// minimize window
			SMALL_RECT minrect = { 0, 0, 1, 1 };
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

			// allocate memory for screen buffer
			_screen_buf.resize(w * h, { 0,0 });
		}
		catch (std::exception&) {
			close();
			throw;
		}
	}

	cmd_engine::~cmd_engine()
	{
		close();
	}

	void cmd_engine::close()
	{
		if (_console != INVALID_HANDLE_VALUE) {
			SetConsoleActiveScreenBuffer(_orig_console);
			CloseHandle(_console);
			_console = INVALID_HANDLE_VALUE;
		}
	}

	wstring cmd_engine::format_error(wstring_view msg)
	{
		array<wchar_t, 256> buf;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf.data(), static_cast<DWORD>(buf.size()), nullptr);
		wstring s(L"ERROR: ");
		s.append(msg).append(L"\n\t").append(buf.data());
		return s;
	}
}