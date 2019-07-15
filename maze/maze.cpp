#include "cmd_engine.h"
#include <iostream>
#include <stack>

using namespace std;

namespace olc
{
    class maze : public olc::cmd_engine
    {
    private:
        struct cell_attrib
        {
            enum enum_t
            {
                path_n = 0x01,
                path_e = 0x02,
                path_s = 0x04,
                path_w = 0x08,
                visited = 0x10,
            };
        };

    public:
        maze()
            : _maze_w{ 40 }
            , _maze_h{ 25 }
            , _maze(_maze_w* _maze_h, 0)
            , _cell_w{ 3 }
            , _visited_count{ 0 }
        {
            _app_name = L"Maze";
        }

        // Inherited via cmd_engine
        virtual bool on_user_init() override
        {
            _stack.emplace(0, 0);
            _maze[0] = cell_attrib::visited;
            return true;
        }

        virtual bool on_user_update(float elapsed) override
        {
            //
            // === rendering ===
            //

            // draw maze
            int cell_plus_wall_w = _cell_w + 1;
            for (int x = 0; x < _maze_w; ++x) {
                for (int y = 0; y < _maze_h; ++y) {
                    int cell = _maze[y * _maze_w + x];
                    // draw cells
                    for (int px = 0; px < _cell_w; ++px) {
                        for (int py = 0; py < _cell_w; ++py) {
                            if (cell & cell_attrib::visited) {
                                draw(x * cell_plus_wall_w + px, y * cell_plus_wall_w + py, pixel_type::solid, color_t::fg_white);
                            }
                            else {
                                draw(x * cell_plus_wall_w + px, y * cell_plus_wall_w + py, pixel_type::solid, color_t::fg_blue);
                            }

                        }
                    }
                    // draw paths, if there is a path, fill in white at the wall pixel to "break" the wall
                    for (int p = 0; p < _cell_w; ++p) {
                        if (cell & cell_attrib::path_s) {
                            draw(x * cell_plus_wall_w + p, y * cell_plus_wall_w + _cell_w, pixel_type::solid, color_t::fg_white);
                        }
                        if (cell & cell_attrib::path_e) {
                            draw(x * cell_plus_wall_w + _cell_w, y * cell_plus_wall_w + p, pixel_type::solid, color_t::fg_white);
                        }
                    }
                }
            }
            return true;
        }

    private:
        const int _maze_w;
        const int _maze_h;
        vector<int> _maze;
        stack<pair<int, int>> _stack;
        int _visited_count;

        const int _cell_w;  // excluding wall of 1 pixel
    };
}

int main()
{
    try {
        olc::maze maze{};
        maze.construct_console(160, 100, 8, 8);
        maze.start();
    }
    catch (olc::olc_exception& e) {
        wcerr << e.msg() << endl;
    }
    return 0;
}