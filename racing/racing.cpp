#include "cmd_engine.h"
#include <list>
#include <string>
#include <array>

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

        float _track_curv_accum = 0.0f;
        float _car_curv_accum = 0.0f;

        float _lap_time = 0.0f;
        list<float> _lap_time_hist{};

        vector<track_section> _track{};
        float _track_dist_total = 0.0f;

    public:
        racing() : _lap_time_hist(5, 0.0f) {
            _app_name = L"Classic Racing";
        }

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
            
            for (const auto& section : _track) {
                _track_dist_total += section.dist;
            }

	        return true;
        }

        virtual bool on_user_update(float elapsed) override
        {
            constexpr int dir_neutral = 0;
            constexpr int dir_right = 1;
            constexpr int dir_left = 2;

            int car_dir = dir_neutral;

            if (get_key(VK_UP).held) {
                _car_speed += 2.0f * elapsed;
            }
            else if (get_key(VK_DOWN).held) {
                _car_speed -= 2.0f * elapsed;
            }
            else {
                if (_car_speed > 0.0f) {
                    _car_speed = fmaxf(0.0f, _car_speed - 1.0f * elapsed);
                }
                else {
                    _car_speed = fminf(0.0f, _car_speed + 1.0f * elapsed);
                }
            }

            if (get_key(VK_RIGHT).held) {
                _car_curv_accum += 0.7f * elapsed;
                car_dir = dir_right;
            }
            else if (get_key(VK_LEFT).held) {
                _car_curv_accum -= 0.7f * elapsed;
                car_dir = dir_left;
            }

            if (fabs(_car_curv_accum - _track_curv_accum) >= 0.8f) {
                if (_car_speed > 0.0f) {
                    _car_speed = fmaxf(0.0f, _car_speed - 5.0f * elapsed);
                }
                else {
                    _car_speed = fminf(0.0f, _car_speed + 5.0f * elapsed);
                }
            }

            _car_speed = std::clamp(_car_speed, -1.0f, 1.0f);
            _car_dist += 70.0f * _car_speed * elapsed;
            if (_car_dist >= _track_dist_total) {
                _car_dist -= _track_dist_total;
                _lap_time_hist.push_front(_lap_time);
                _lap_time_hist.pop_back();
                _lap_time = 0.0f;
            }
            _lap_time += elapsed;

            // get point on track
            int track_section = 0;
            float track_dist = _track[0].dist;
            auto car_dist_on_track = fmod(_car_dist, _track_dist_total);

            while (track_section < _track.size() && track_dist <= car_dist_on_track) {
                ++track_section;
                track_dist += _track[track_section].dist;
            }

            float target_curvature = _track[track_section].curvature;
            // gradually change curvature to target curvature over 1 sec
            _curvature += (target_curvature - _curvature) * elapsed * abs(_car_speed);
            _track_curv_accum += _curvature * elapsed * abs(_car_speed);

            // draw sky
            for (auto y = 0; y < height() / 2; ++y) {
                for (auto x = 0; x < width(); ++x) {
                    draw(x, y, y < height() / 4 ? pixel_type::half : pixel_type::solid, color_t::fg_dark_blue);
                }
            }

            // draw scenary - our hills are a rectified sine wave where phase is adjusted by accumulated track curvature
            for (auto x = 0; x < width(); ++x) {
                int hill_height = static_cast<int>(fabs(sinf(x * 0.01f - _track_curv_accum) * 16.0f));
                for (auto y = height() / 2 - hill_height; y < height() / 2; ++y) {
                    draw(x, y, pixel_type::solid, color_t::fg_dark_yellow);
                }
            }

            // draw road
            for (auto y = 0; y < height() / 2; ++y) {
                // bottom of the screen is for the road

                auto perspective = (float)y / (height() / 2.0f);

                // 20.0f - determines the number of strips for the grass, the larger, the more strips
                // powf exponent - determines how drastically the height of the grass strips are based on view dist,
                //                 the higher the exp, the more dramatic
                // -0.1f * _car_dist - controls the phase of scrolling for the strips
                // sine function - makes the strip runs in periodic manner
                auto grass_color = sinf(20.0f * powf(perspective - 1, 3.0f) - 0.1f * _car_dist) >= 0.0f ? color_t::fg_dark_green : color_t::fg_green;
                auto clip_color = sinf(80.0f * powf(perspective - 1, 2.0f) + _car_dist) >= 0.0f ? color_t::fg_red : color_t::fg_white;
                auto road_color = color_t::fg_grey;
                if (track_section == 0) {
                    // at the start/finish line
                    road_color = sinf(60.0f * powf(perspective - 1, 2.0f) + _car_dist) >= 0.0f ? color_t::fg_white : color_t::fg_dark_grey;
                }

                for (auto x = 0; x < width(); ++x) {

                    auto mid_pt = 0.5f + _curvature * powf(1.0f - perspective, 3.0f);
                    // 0.1f is min width at the top and 0.9f is the max width at the bottom
                    auto road_w = 0.1f + perspective * 0.8f;
                    auto clip_w = road_w * 0.15f;

                    road_w *= 0.5f;

                    int grass_left_end = static_cast<int>((mid_pt - road_w - clip_w) * width());
                    int clip_left_end = static_cast<int>((mid_pt - road_w) * width());
                    int clip_right_start = static_cast<int>((mid_pt + road_w) * width());
                    int grass_right_start = static_cast<int>((mid_pt + road_w + clip_w) * width());


                    int row = height() / 2 + y;
                    if (x < grass_left_end || x >= grass_right_start) {
                        draw(x, row, pixel_type::solid, grass_color);
                    }
                    else if (x < clip_left_end || x >= clip_right_start) {
                        draw(x, row, pixel_type::solid, clip_color);
                    }
                    else {
                        draw(x, row, pixel_type::solid, road_color);
                    }
                }
            }

            // draw car
            _car_pos = _car_curv_accum - _track_curv_accum;
            int car_x = width() / 2 + static_cast<int>(_car_pos * width() / 2.0f) - g_car_w / 2;
            int car_y = g_car_y;
            switch (car_dir) {
            case dir_neutral:
                draw_string_alpha(car_x, car_y++, L"   ||####||   ");
                draw_string_alpha(car_x, car_y++, L"      ##      ");
                draw_string_alpha(car_x, car_y++, L"     ####     ");
                draw_string_alpha(car_x, car_y++, L"     ####     ");
                draw_string_alpha(car_x, car_y++, L"|||  ####  |||");
                draw_string_alpha(car_x, car_y++, L"|||########|||");
                draw_string_alpha(car_x, car_y++, L"|||  ####  |||");
                break;
            case dir_right:
                draw_string_alpha(car_x, car_y++, L"      //####//");
                draw_string_alpha(car_x, car_y++, L"         ##   ");
                draw_string_alpha(car_x, car_y++, L"       ####   ");
                draw_string_alpha(car_x, car_y++, L"      ####    ");
                draw_string_alpha(car_x, car_y++, L"///  ####//// ");
                draw_string_alpha(car_x, car_y++, L"//#######///O ");
                draw_string_alpha(car_x, car_y++, L"/// #### //// ");
                break;
            case dir_left:
                draw_string_alpha(car_x, car_y++, LR"(\\####\\      )");
                draw_string_alpha(car_x, car_y++, LR"(   ##         )");
                draw_string_alpha(car_x, car_y++, LR"(   ####       )");
                draw_string_alpha(car_x, car_y++, LR"(    ####      )");
                draw_string_alpha(car_x, car_y++, LR"( \\\\####  \\\)");
                draw_string_alpha(car_x, car_y++, LR"( O\\\#######\\)");
                draw_string_alpha(car_x, car_y++, LR"( \\\\ #### \\\)");
                break;
            }

            // draw stats
            int stats_y = 0;
            draw_string(0, stats_y++, L"Distance: " + to_wstring(_car_dist));
            draw_string(0, stats_y++, L"Target Curvature: " + to_wstring(target_curvature));
            draw_string(0, stats_y++, L"Track Curvature Accum: " + to_wstring(_track_curv_accum));
            draw_string(0, stats_y++, L"Car Curvature Accum: " + to_wstring(_car_curv_accum));
            draw_string(0, stats_y++, L"Car Speed: " + to_wstring(_car_speed));

            auto time2string = [](float t) {
                int min = static_cast<int>(t / 60.0f);
                t -= min * 60.0f;
                int sec = static_cast<int>(t);
                int ms = static_cast<int>((t - (float)sec) * 1000.0f);
                return to_wstring(min) + L":" + to_wstring(sec) + L"." + to_wstring(ms);
            };
            draw_string(10, 8, time2string(_lap_time));
            stats_y = 10;
            for (auto lt : _lap_time_hist) {
                draw_string(10, stats_y++, time2string(lt));
            }

	        return true;
        }
    };
}

int main() {
    olc::racing game{};
    game.construct_console(160, 100, 8, 8);
    game.start();
}