#include "cmd_engine.h"

using namespace std;

namespace olc {
    class racing : public cmd_engine {
    public:
        // Inherited via cmd_engine
        virtual bool on_user_init() override
        {
	        return true;
        }
        virtual bool on_user_update(float elapsed) override
        {
            for (auto y = 0; y < height() / 2; ++y) {
                // bottom of the screen is for the road
                for (auto x = 0; x < width(); ++x) {
                    auto mid_pt = 0.5f;
                    auto road_w = 0.6f;
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
	        return true;
        }
    };
}

int main() {
    olc::racing game{};
    game.construct_console(160, 100, 8, 8);
    game.start();
}