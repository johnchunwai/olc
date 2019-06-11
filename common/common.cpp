#include "common.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;

namespace olc
{
	console_screen_buffer::console_screen_buffer(int w, int h, int16_t font_size)
		: _width(w),
		_height(h),
		_screen(w* h, L' ')
	{
		_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
		_console = CreateConsoleScreenBuffer(GENERIC_WRITE | GENERIC_READ, 0, nullptr, CONSOLE_TEXTMODE_BUFFER, nullptr);
		try {
			CONSOLE_FONT_INFOEX font_info{ sizeof(CONSOLE_FONT_INFOEX) };
			bool b = GetCurrentConsoleFontEx(_console, false, &font_info);
			font_info.dwFontSize = { font_size, font_size };
			b = SetCurrentConsoleFontEx(_console, false, &font_info);
			if (!b) {
				throw std::runtime_error("Failed to set font. GetLastError() returns "s + to_string(GetLastError()));
			}

			//
			// Screen buffer must be >= windows size.
			// Therefore, if we scale down, we must shrink window first, then screen buffer.
			// if we scale up, we must enlarge screen buffer first, then window.
			//
			SMALL_RECT winrect{ 0, 0, short(_width - 1), short(_height - 1) };
			CONSOLE_SCREEN_BUFFER_INFO info;
			GetConsoleScreenBufferInfo(_console, &info);
			auto winx = info.srWindow.Right - info.srWindow.Left;
			auto winy = info.srWindow.Bottom - info.srWindow.Top;
			bool shrinking = winx > w || winy > h;
			if (shrinking) {
				b = SetConsoleWindowInfo(_console, true, &winrect);
				if (!b) {
					auto maxsize = GetLargestConsoleWindowSize(_console);
					throw std::runtime_error("Failed to set console window size. GetLastError() returns "s + to_string(GetLastError()) +
						"\nThe largest window size allowed is (" + to_string(maxsize.X) + ", " + to_string(maxsize.Y) + ")");
				}
			}
			b = SetConsoleScreenBufferSize(_console, { (short)_width, (short)_height });
			if (!b) {
				//CONSOLE_SCREEN_BUFFER_INFO info;
				//GetConsoleScreenBufferInfo(_console, &info);
				//auto winx = info.srWindow.Right - info.srWindow.Left;
				//auto winy = info.srWindow.Bottom - info.srWindow.Top;
				//auto minx = max(GetSystemMetrics(SM_CXMIN), winx);
				//auto miny = max(GetSystemMetrics(SM_CYMIN), winy);
				//auto maxx = info.dwMaximumWindowSize.X;
				//auto maxy = info.dwMaximumWindowSize.Y;
				throw std::runtime_error("Failed to set screen buffer size. GetLastError() returns "s + to_string(GetLastError()));
				//+
				//	"\nmin x = " + to_string(minx) + ", min y = " + to_string(miny) + ", max x = " + to_string(maxx) + ", max y = " + to_string(maxy));
			}
			if (!shrinking) {
				b = SetConsoleWindowInfo(_console, true, &winrect);
				if (!b) {
					auto maxsize = GetLargestConsoleWindowSize(_console);
					throw std::runtime_error("Failed to set console window size. GetLastError() returns "s + to_string(GetLastError()) +
						"\nThe largest window size allowed is (" + to_string(maxsize.X) + ", " + to_string(maxsize.Y) + ")");
				}
			}
			b = SetConsoleActiveScreenBuffer(_console);
			if (!b) {
				throw std::runtime_error("Failed to set active buffer. GetLastError() returns "s + to_string(GetLastError()));
			}
		}
		catch (std::runtime_error&) {
			close();
			throw;
		}
	}

	console_screen_buffer::~console_screen_buffer() {
		close();
	}

	void console_screen_buffer::close() {
		if (_console) {
			SetConsoleActiveScreenBuffer(_stdout);
			CloseHandle(_console);
			_console = 0;
		}
	}

	void console_screen_buffer::display() const {
		DWORD bytes_written;
		WriteConsoleOutputCharacterW(_console, _screen.data(), dimension(), { 0, 0 }, &bytes_written);
	}

	void pause()
	{
		cout << "Press enter to quit";
		cin.ignore();
	}
}