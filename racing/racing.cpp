#include "cmd_engine.h"

using namespace std;

constexpr auto g_screen_width = 160;
constexpr auto g_screen_height = 100;
constexpr auto g_car_y = 80;
constexpr auto g_car_w = 14;


namespace olc {
    class racing : public cmd_engine {
    private:
        float car_pos = 0.0f;

    public:
        // Inherited via cmd_engine
        virtual bool on_user_init() override
        {
	        return true;
        }
        virtual bool on_user_update(float elapsed) override
        {
            // draw road
            for (auto y = 0; y < height() / 2; ++y) {
                // bottom of the screen is for the road
                for (auto x = 0; x < width(); ++x) {
                    auto perspective = (float) y / (height() / 2.0f);

                    auto mid_pt = 0.5f;
                    // 0.1f is min width at the top and 0.9f is the max width at the bottom
                    auto road_w = 0.1f + perspective * 0.8f;
                    auto clip_w = road_w * 0.15f;

                    road_w *= 0.5f;

                    auto grass_left_end = (mid_pt - road_w - clip_w) * width();
                    auto clip_left_end = (mid_pt - road_w) * width();
                    auto clip_right_start = (mid_pt + road_w) * width();
                    auto grass_right_start = (mid_pt + road_w + clip_w) * width();

                    int row = height() / 2 + y;
                    if (x < grass_left_end || x >= grass_right_start) {
                        draw(x, row, pixel_type::solid, color_t::green);
                    }
                    else if (x < clip_left_end || x >= clip_right_start) {
                        draw(x, row, pixel_type::solid, color_t::red);
                    }
                    else {
                        draw(x, row, pixel_type::solid, color_t::grey);
                    }
                }
            }

            // draw car
            int car_x = width() / 2 + static_cast<int>(car_pos * width() / 2.0f) - g_car_w / 2;
            int car_y = g_car_y;
            draw_string_alpha(car_x, car_y++, L"   ||####||   ");
            draw_string_alpha(car_x, car_y++, L"      ##      ");
            draw_string_alpha(car_x, car_y++, L"     ####     ");
            draw_string_alpha(car_x, car_y++, L"     ####     ");
            draw_string_alpha(car_x, car_y++, L"|||  ####  |||");
            draw_string_alpha(car_x, car_y++, L"|||########|||");
            draw_string_alpha(car_x, car_y++, L"|||  ####  |||");

	        return true;
        }
    };
}

int main() {
    olc::racing game{};
    game.construct_console(160, 100, 8, 8);
    game.start();
}