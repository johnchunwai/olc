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
		draw_triangle(50, 20, 40, 60, 58, 23);
		draw_circle(90, 100, 55, pixel_type::solid, color_t::bg_dark_red);
		fill_circle(90, 100, 52, pixel_type::solid, color_t::bg_dark_yellow);
		sprite s1(9, 15);
		for (int i = 0; i < s1.width(); ++i) {
			s1.set_color(i, 7, color_t::fg_dark_green);
			s1.set_glyph(i, 7, pixel_type::threequarters);
		}
		for (int j = 0; j < s1.height(); ++j) {
			s1.set_color(4, j, color_t::fg_dark_yellow);
			s1.set_glyph(4, j, pixel_type::solid);
		}
		draw_sprite(10, 70, s1);
		draw_partial_sprite(146, 140, s1, 3, 6, 5, 8);
        vector<pair<float, float>> poly{ {5.f, 10.f}, {-5.f, 10.f}, {-5.f, -10.f}, {5.f, -10.f} };
        draw_wire_polygon(poly, 10, 10, 0.43, 0.5, pixel_type::solid, color_t::bg_dark_red);

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
		game.construct_console(150, 150, 6, 6);
        game.start();
	}
	catch (olc_exception& e) {
		wcerr << e.msg() << endl;
	}
	cout << "game ended" << endl;
	pause();
}