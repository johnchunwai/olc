#pragma once

#include <string>
#include "windows.h"

class console_screen_buffer
{
public:
	console_screen_buffer(int w, int h);

	virtual ~console_screen_buffer();

	void close();

	void display() const;

	int width() const { return _width; }
	int height() const { return _height; }
	int dimension() const { return _width * _height; }

	std::wstring& screen() { return _screen; }

private:
	HANDLE _console;
	HANDLE _stdout;
	const int _width;
	const int _height;
	std::wstring _screen;
};

