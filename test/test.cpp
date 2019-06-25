#include "cmd_engine.h"
#include <iostream>
#include <thread>

using namespace std;
using namespace olc;


class test_engine : public cmd_engine
{
public:
	explicit test_engine(std::wstring app_name) : cmd_engine() {
		_app_name = app_name;
	}

protected:
	// Inherited via cmd_engine
	virtual bool on_user_init() override
	{
		return true;
	}
	virtual bool on_user_update(float elapsed) override
	{
		draw(4, 2);
		fill(6, 5, 40, 30, pixel_type::half, color_t::fg_cyan | color_t::bg_dark_magenta);
		fill(30, 43, 60, 200, pixel_type::half, color_t::fg_cyan | color_t::bg_dark_magenta);
		draw_string(3, 40, L"What is going on?"s);
		draw_string_alpha(4, 41, L"What is going on alpha?"s);
		draw_string(50, 42, L"What is going on?0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"s);
		draw_line(0, 0, width(), height());
		draw_line(100, 30, 0, 60);
		draw_line(60, 5, 40, 95);
		draw_line(40, 5, 60, 95);

		return true;
	}
	virtual bool on_user_destroy() override
	{
		this_thread::sleep_for(1s);
		return true;
	}
};

int main()
{
	try {
		test_engine game(L"test engine"s);
		game.construct_console(100, 100, 12, 12);
		game.start();
	}
	catch (olc_exception& e) {
		wcerr << e.msg() << endl;
	}
	cout << "game ended" << endl;
	pause();
}