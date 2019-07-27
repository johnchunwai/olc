#include "cmd_engine.h"

using namespace std;

constexpr auto g_screen_width = 160;
constexpr auto g_screen_height = 100;
constexpr auto g_car_y = 80;
constexpr auto g_car_w = 14;


namespace olc {
    struct track_section
    {
        float curvature;
        float dist;

        track_section(float curv, float dist) : curvature{ curv }, dist{ dist } {}
    };

    class racing : public cmd_engine {
    private:
        float _car_pos = 0.0f;
        float _car_dist = 0.0f;
        float _car_speed = 0.0f;

        float _curvature = 0.0f;

        vector<track_section> _track;

    public:
        // Inherited via cmd_engine
        virtual bool on_user_init() override
        {
            _track.emplace_back(0.0f, 10.0f);    // short section for start/finish
            _track.emplace_back(0.0f, 200.0f);
            _track.emplace_back(1.0f, 200.0f);
            _track.emplace_back(0.0f, 400.0f);
            _track.emplace_back(-1.0f, 200.0f);
            _track.emplace_back(0.0f, 200.0f);
            _track.emplace_back(-1.0f, 200.0f);
            _track.emplace_back(1.0f, 200.0f);
            _track.emplace_back(0.0f, 200.0f);
            _track.emplace_back(0.2f, 500.0f);
            _track.emplace_back(0.0f, 200.0f);

	        return true;
        }

        virtual bool on_user_update(float elapsed) override
        {
            if (get_key(VK_UP).held) {
                _car_speed += 2.0f * elapsed;
            }
            else {
                _car_speed -= 1.0f * elapsed;
            }
            _car_speed = std::clamp(_car_speed, 0.0f, 1.0f);
            _car_dist += 70.0f * _car_speed * elapsed;

            // get point on track
            int track_section = 0;
            float track_dist = _track[0].dist;

            while (track_section < _track.size() && track_dist <= _car_dist) {
                ++track_section;
                track_dist += _track[track_section].dist;
            }

            float target_curvature = _track[track_section].curvature;
            // gradually change curvature to target curvature over 1 sec
            _curvature += (target_curvature - _curvature) * elapsed * _car_speed;

            // draw road
            for (auto y = 0; y < height() / 2; ++y) {
                // bottom of the screen is for the road
                for (auto x = 0; x < width(); ++x) {
                    auto perspective = (float) y / (height() / 2.0f);

                    auto mid_pt = 0.5f + _curvature * powf(1.0f - perspective, 3.0f);
                    // 0.1f is min width at the top and 0.9f is the max width at the bottom
                    auto road_w = 0.1f + perspective * 0.8f;
                    auto clip_w = road_w * 0.15f;

                    road_w *= 0.5f;

                    // 20.0f - determines the number of strips for the grass, the larger, the more strips
                    // powf exponent - determines how drastically the height of the grass strips are based on view dist,
                    //                 the higher the exp, the more dramatic
                    // -0.1f * _car_dist - controls the phase of scrolling for the strips
                    // sine function - makes the strip runs in periodic manner
                    auto grass_color = sinf(20.0f * powf(perspective - 1, 3.0f) - 0.1f * _car_dist) >= 0.0f ? color_t::dark_green : color_t::fg_green;
                    auto clip_color = sinf(80.0f * powf(perspective - 1, 2.0f) - _car_dist) >= 0.0f ? color_t::red : color_t::white;

                    auto grass_left_end = (mid_pt - road_w - clip_w) * width();
                    auto clip_left_end = (mid_pt - road_w) * width();
                    auto clip_right_start = (mid_pt + road_w) * width();
                    auto grass_right_start = (mid_pt + road_w + clip_w) * width();

                    int row = height() / 2 + y;
                    if (x < grass_left_end || x >= grass_right_start) {
                        draw(x, row, pixel_type::solid, grass_color);
                    }
                    else if (x < clip_left_end || x >= clip_right_start) {
                        draw(x, row, pixel_type::solid, clip_color);
                    }
                    else {
                        draw(x, row, pixel_type::solid, color_t::fg_grey);
                    }
                }
            }

            // draw car
            int car_x = width() / 2 + static_cast<int>(_car_pos * width() / 2.0f) - g_car_w / 2;
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