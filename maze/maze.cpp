#include "cmd_engine.h"
#include <iostream>
#include <stack>
#include <thread>
#include <time.h>

using namespace std;

namespace olc
{
    constexpr int g_maze_w = 40;
    constexpr int g_maze_h = 25;
    constexpr int g_cell_w = 3;
    constexpr int pixel_offset_x = 1;
    constexpr int pixel_offset_y = 1;
    constexpr int g_window_w = g_maze_w * (g_cell_w + 1) + pixel_offset_x;	// cell true width + wall = g_cell_w + 1
    constexpr int g_window_h = g_maze_h * (g_cell_w + 1) + pixel_offset_y;	// cell true height + wall = g_cell_w + 1

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
                solved = 0x20,
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
            : _maze_w{ g_maze_w }
            , _maze_h{ g_maze_h }
            , _cell_w{ g_cell_w }
            , _maze(_maze_w * _maze_h, 0)
            , _solve_maze(_maze_w* _maze_h, 0)
        {
            _app_name = L"Maze";
        }

        // Inherited via cmd_engine
        virtual bool on_user_init() override
        {
            srand(time(nullptr));
            _stack.emplace(0, 0);
            _maze[0] = cell_attrib::visited;
            _solve_stack.emplace(0, 0);
            _solve_maze[0] = cell_attrib::visited | cell_attrib::solved;
            return true;
        }

        virtual bool on_user_update(float elapsed) override
        {
            // slow down for animation
            //this_thread::sleep_for(2ms);

            //
            // update maze
            //
            // CAUTION: don't capture by [*this], it will make a copy of this object and will invoke the destructor, killing the console buffer
            auto maze_index = [this](int x, int y) {
                return y * _maze_w + x;
            };
            if (!_stack.empty()) {
                vector<direction::enum_t> neighbours;
                auto [curr_x, curr_y] = _stack.top();
                if (curr_y > 0 && (_maze[maze_index(curr_x, curr_y - 1)] & cell_attrib::visited) == 0) {
                    neighbours.push_back(direction::N);
                }
                if (curr_x < _maze_w - 1 && (_maze[maze_index(curr_x + 1, curr_y)] & cell_attrib::visited) == 0) {
                    neighbours.push_back(direction::E);
                }
                if (curr_y < _maze_h - 1 && (_maze[maze_index(curr_x, curr_y + 1)] & cell_attrib::visited) == 0) {
                    neighbours.push_back(direction::S);
                }
                if (curr_x > 0 && (_maze[maze_index(curr_x - 1, curr_y)] & cell_attrib::visited) == 0) {
                    neighbours.push_back(direction::W);
                }

                if (!neighbours.empty()) {
                    // move to next neighbour
                    direction::enum_t next_dir = neighbours[rand() % neighbours.size()];
                    // create path between the curr cell and neighbour
                    switch (next_dir) {
                    case direction::N:
                        _maze[maze_index(curr_x, curr_y)] |= cell_attrib::path_n;
                        _maze[maze_index(curr_x, curr_y - 1)] |= cell_attrib::path_s | cell_attrib::visited;
                        _stack.emplace(curr_x, curr_y - 1);
                        break;
                    case direction::E:
                        _maze[maze_index(curr_x, curr_y)] |= cell_attrib::path_e;
                        _maze[maze_index(curr_x + 1, curr_y)] |= cell_attrib::path_w | cell_attrib::visited;
                        _stack.emplace(curr_x + 1, curr_y);
                        break;
                    case direction::S:
                        _maze[maze_index(curr_x, curr_y)] |= cell_attrib::path_s;
                        _maze[maze_index(curr_x, curr_y + 1)] |= cell_attrib::path_n | cell_attrib::visited;
                        _stack.emplace(curr_x, curr_y + 1);
                        break;
                    case direction::W:
                        _maze[maze_index(curr_x, curr_y)] |= cell_attrib::path_w;
                        _maze[maze_index(curr_x - 1, curr_y)] |= cell_attrib::path_e | cell_attrib::visited;
                        _stack.emplace(curr_x - 1, curr_y);
                        break;
                    }
                }
                else {
                    // no more neighbour, pop
                    _stack.pop();
                }
            }
            else if (!_solve_stack.empty()) {
                // solving
                auto has_path = [this, &maze_index](int x, int y, direction::enum_t dir) {
                    cell_attrib::enum_t dir_val;
                    switch (dir) {
                    case direction::N:
                        dir_val = cell_attrib::path_n;
                        break;
                    case direction::E:
                        dir_val = cell_attrib::path_e;
                        break;
                    case direction::S:
                        dir_val = cell_attrib::path_s;
                        break;
                    case direction::W:
                        dir_val = cell_attrib::path_w;
                        break;
                    }
                    return (_maze[maze_index(x, y)] & dir_val) != 0;
                };
                vector<direction::enum_t> neighbours;
                auto [curr_x, curr_y] = _solve_stack.top();
                if (curr_y > 0 && (_solve_maze[maze_index(curr_x, curr_y - 1)] & cell_attrib::visited) == 0 &&
                    has_path(curr_x, curr_y, direction::N)) {
                    neighbours.push_back(direction::N);
                }
                if (curr_x < _maze_w - 1 && (_solve_maze[maze_index(curr_x + 1, curr_y)] & cell_attrib::visited) == 0 &&
                    has_path(curr_x, curr_y, direction::E)) {
                    neighbours.push_back(direction::E);
                }
                if (curr_y < _maze_h - 1 && (_solve_maze[maze_index(curr_x, curr_y + 1)] & cell_attrib::visited) == 0 &&
                    has_path(curr_x, curr_y, direction::S)) {
                    neighbours.push_back(direction::S);
                }
                if (curr_x > 0 && (_solve_maze[maze_index(curr_x - 1, curr_y)] & cell_attrib::visited) == 0 &&
                    has_path(curr_x, curr_y, direction::W)) {
                    neighbours.push_back(direction::W);
                }

                if (!neighbours.empty()) {
                    // move to next neighbour
                    direction::enum_t next_dir = neighbours[rand() % neighbours.size()];
                    // create path between the curr cell and neighbour
                    switch (next_dir) {
                    case direction::N:
                        _solve_maze[maze_index(curr_x, curr_y)] |= cell_attrib::path_n;
                        _solve_maze[maze_index(curr_x, curr_y - 1)] |= cell_attrib::path_s | cell_attrib::visited | cell_attrib::solved;
                        _solve_stack.emplace(curr_x, curr_y - 1);
                        break;
                    case direction::E:
                        _solve_maze[maze_index(curr_x, curr_y)] |= cell_attrib::path_e;
                        _solve_maze[maze_index(curr_x + 1, curr_y)] |= cell_attrib::path_w | cell_attrib::visited | cell_attrib::solved;
                        _solve_stack.emplace(curr_x + 1, curr_y);
                        break;
                    case direction::S:
                        _solve_maze[maze_index(curr_x, curr_y)] |= cell_attrib::path_s;
                        _solve_maze[maze_index(curr_x, curr_y + 1)] |= cell_attrib::path_n | cell_attrib::visited | cell_attrib::solved;
                        _solve_stack.emplace(curr_x, curr_y + 1);
                        break;
                    case direction::W:
                        _solve_maze[maze_index(curr_x, curr_y)] |= cell_attrib::path_w;
                        _solve_maze[maze_index(curr_x - 1, curr_y)] |= cell_attrib::path_e | cell_attrib::visited | cell_attrib::solved;
                        _solve_stack.emplace(curr_x - 1, curr_y);
                        break;
                    }

                    if (auto [dest_x, dest_y] = _solve_stack.top(); dest_x == _maze_w - 1 && dest_y == _maze_h - 1) {
                        // reached the end, we solved the maze, empty the stack
                        _solve_stack = {};
                    }
                }
                else {
                    // no more neighbour, pop
                    // mark it as not solved
                    _solve_stack.pop();
                    _solve_maze[maze_index(curr_x, curr_y)] &= ~cell_attrib::solved;
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
                    // current cell = red, visited = white, non visited = blue, solved = cyan
                    int cell = _maze[maze_index(x, y)];
                    int solve_cell = _solve_maze[maze_index(x, y)];
                    short cell_color{ color_t::red };
                    if (x != curr_x || y != curr_y) {
                        if (solve_cell & cell_attrib::solved) {
                            cell_color = color_t::cyan;
                        }
                        else {
                            cell_color = (cell & cell_attrib::visited) ? color_t::white : color_t::blue;
                        }
                    }
                    for (int px = 0; px < _cell_w; ++px) {
                        for (int py = 0; py < _cell_w; ++py) {
                            draw(x * cell_plus_wall_w + px + pixel_offset_x, y * cell_plus_wall_w + py + pixel_offset_y, pixel_type::solid, cell_color);
                        }
                    }
                    // draw paths, if there is a path, fill in white/cyan at the wall pixel to "break" the wall
                    for (int p = 0; p < _cell_w; ++p) {
                        if (cell & cell_attrib::path_s) {
                            color_t::enum_t wall_color = (solve_cell & cell_attrib::path_s && _solve_maze[maze_index(x, y + 1)] & cell_attrib::solved) ? color_t::cyan : color_t::white;
                            draw(x * cell_plus_wall_w + p + pixel_offset_x, y * cell_plus_wall_w + _cell_w + pixel_offset_y, pixel_type::solid, wall_color);
                        }
                        if (cell & cell_attrib::path_e) {
                            color_t::enum_t wall_color = (solve_cell & cell_attrib::path_e && _solve_maze[maze_index(x + 1, y)] & cell_attrib::solved) ? color_t::cyan : color_t::white;
                            draw(x * cell_plus_wall_w + _cell_w + pixel_offset_x, y * cell_plus_wall_w + p + pixel_offset_y, pixel_type::solid, wall_color);
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
        vector<int> _solve_maze;
        stack<pair<int, int>> _solve_stack;
    };
}

int main()
{
    try {
        olc::maze maze{};
        maze.construct_console(olc::g_window_w, olc::g_window_h, 8, 8);
        maze.start();
    }
    catch (olc::olc_exception& e) {
        wcerr << e.msg().data() << endl;
    }
    return 0;
}