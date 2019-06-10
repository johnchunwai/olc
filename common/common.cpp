#include "common.h"
#include <iostream>

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
		CONSOLE_FONT_INFOEX font_info{ sizeof(CONSOLE_FONT_INFOEX) };
		bool b = GetCurrentConsoleFontEx(_console, true, &font_info);
		font_info.dwFontSize = { 0, font_size };
		b = SetCurrentConsoleFontEx(_console, true, &font_info);
		b = SetConsoleScreenBufferSize(_console, { (short)_width, (short)_height });
		SMALL_RECT rect{ 0, 0, short(_width - 1), short(_height - 1) };
		auto tmp = GetLargestConsoleWindowSize(_console);
		b = SetConsoleWindowInfo(_console, true, &rect);
		b = SetConsoleActiveScreenBuffer(_console);
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