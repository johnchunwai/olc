#include "cmd_engine.h"
#include <iostream>
#include <stack>
#include <thread>
#include <time.h>

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

        struct direction
        {
            enum enum_t
            {
                N, E, S, W,
            };
        };

    public:
        maze()
            : _maze_w{ 30 }
            , _maze_h{ 20 }
            , _maze(_maze_w * _maze_h, 0)
            , _cell_w{ 3 }
            //, _visited_count{ 0 }
        {
            _app_name = L"Maze";
        }

        // Inherited via cmd_engine
        virtual bool on_user_init() override
        {
            srand(time(nullptr));
            _stack.emplace(0, 0);
            _maze[0] = cell_attrib::visited;
            //++_visited_count;
            return true;
        }

        virtual bool on_user_update(float elapsed) override
        {
            // slow down for animation
            this_thread::sleep_for(5ms);

            //
            // update maze
            //
            // CAUTION: don't capture by [*this], it will make a copy of this object and will invoke the destructor, killing the console buffer
            auto index = [this](int x, int y) {
                return y * _maze_w + x;
            };
            if (!_stack.empty()) {
                vector<direction::enum_t> neighbours;
                auto [curr_x, curr_y] = _stack.top();
                if (curr_y > 0 && (_maze[index(curr_x, curr_y - 1)] & cell_attrib::visited) == 0) {
                    neighbours.push_back(direction::N);
                }
                if (curr_x < _maze_w - 1 && (_maze[index(curr_x + 1, curr_y)] & cell_attrib::visited) == 0) {
                    neighbours.push_back(direction::E);
                }
                if (curr_y < _maze_h - 1 && (_maze[index(curr_x, curr_y + 1)] & cell_attrib::visited) == 0) {
                    neighbours.push_back(direction::S);
                }
                if (curr_x > 0 && (_maze[index(curr_x - 1, curr_y)] & cell_attrib::visited) == 0) {
                    neighbours.push_back(direction::W);
                }

                if (!neighbours.empty()) {
                    // move to next neighbour
                    direction::enum_t next_dir = neighbours[rand() % neighbours.size()];
                    // create path between the curr cell and neighbour
                    switch (next_dir) {
                    case direction::N:
                        _maze[index(curr_x, curr_y)] |= cell_attrib::path_n;
                        _maze[index(curr_x, curr_y - 1)] |= cell_attrib::path_s | cell_attrib::visited;
                        _stack.emplace(curr_x, curr_y - 1);
                        break;
                    case direction::E:
                        _maze[index(curr_x, curr_y)] |= cell_attrib::path_e;
                        _maze[index(curr_x + 1, curr_y)] |= cell_attrib::path_w | cell_attrib::visited;
                        _stack.emplace(curr_x + 1, curr_y);
                        break;
                    case direction::S:
                        _maze[index(curr_x, curr_y)] |= cell_attrib::path_s;
                        _maze[index(curr_x, curr_y + 1)] |= cell_attrib::path_n | cell_attrib::visited;
                        _stack.emplace(curr_x, curr_y + 1);
                        break;
                    case direction::W:
                        _maze[index(curr_x, curr_y)] |= cell_attrib::path_w;
                        _maze[index(curr_x - 1, curr_y)] |= cell_attrib::path_e | cell_attrib::visited;
                        _stack.emplace(curr_x - 1, curr_y);
                        break;
                    }
                }
                else {
                    // no more neighbour, pop
                    _stack.pop();
                }
            }

            //
            // === rendering ===
            //

            // draw maze
            
            int cell_plus_wall_w = _cell_w + 1;
            auto [curr_x, curr_y] = (!_stack.empty()) ? _stack.top() : pair<int, int>(-1, -1);
            // draw cells
            for (int x = 0; x < _maze_w; ++x) {
                for (int y = 0; y < _maze_h; ++y) {
                    // current cell = red, visited = white, non visited = blue
                    int cell = _maze[index(x, y)];
                    short cell_color{ color_t::red };
                    if (x != curr_x || y != curr_y) {
                        cell_color = (cell & cell_attrib::visited) ? color_t::white : color_t::blue;
                    }
                    for (int px = 0; px < _cell_w; ++px) {
                        for (int py = 0; py < _cell_w; ++py) {
                            draw(x * cell_plus_wall_w + px, y * cell_plus_wall_w + py, pixel_type::solid, cell_color);
                        }
                    }
                    // draw paths, if there is a path, fill in white at the wall pixel to "break" the wall
                    for (int p = 0; p < _cell_w; ++p) {
                        if (cell & cell_attrib::path_s) {
                            draw(x * cell_plus_wall_w + p, y * cell_plus_wall_w + _cell_w, pixel_type::solid, color_t::white);
                        }
                        if (cell & cell_attrib::path_e) {
                            draw(x * cell_plus_wall_w + _cell_w, y * cell_plus_wall_w + p, pixel_type::solid, color_t::white);
                        }
                    }
                }
            }
            return true;
        }

    private:
        const int _maze_w;
        const int _maze_h;
        const int _cell_w;  // excluding wall of 1 pixel
        vector<int> _maze;
        stack<pair<int, int>> _stack;
        //int _visited_count;
    };
}

int main()
{
    try {
        olc::maze maze{};
        maze.construct_console(120, 80, 8, 8);
        maze.start();
    }
    catch (olc::olc_exception& e) {
        wcerr << e.msg() << endl;
    }
    return 0;
}