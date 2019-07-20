#include "cmd_engine.h"


using namespace std;

namespace olc {

	class game_of_life : public cmd_engine
	{
	public:
		game_of_life()
			: _grids(2)
			, _active_grid_index{0}
		{
		}

		// Inherited via cmd_engine
		virtual bool on_user_init() override
		{
			_grids[0] = vector<int>(width() * height());
			_grids[1] = vector<int>(width() * height());

			auto &grid = _grids[_active_grid_index];

			//grid[grid_index(3, 3)] = 1;
			//grid[grid_index(4, 3)] = 1;
			//grid[grid_index(5, 3)] = 1;
			srand(time(nullptr));
			for (auto &cell : grid) {
				cell = (rand() % 2 == 0) ? 1 : 0;
			}
			return true;
		}

		virtual bool on_user_update(float elapsed) override
		{
			this_thread::sleep_for(100ms);

			//
			// update cells
			//
			auto &grid = _grids[_active_grid_index];
			int update_grid_index = (_active_grid_index + 1) % 2;
			auto &update_grid = _grids[update_grid_index];
			
			// use active grid state to write update to update_grid
			for (int x = 0; x < width(); ++x) {
				for (int y = 0; y < height(); ++y) {
					auto count = neighbour_count(grid, x, y);
					auto idx = grid_index(x, y);
					if (count == 3) {
						update_grid[idx] = 1;
					}
					else if (count == 2) {
						update_grid[idx] = grid[idx];
					}
					else {
						update_grid[idx] = 0;
					}
				}
			}
			_active_grid_index = update_grid_index;

			//
			// render
			//
			for (int x = 0; x < width(); ++x) {
				for (int y = 0; y < height(); ++y) {
					color_t::enum_t c = grid[grid_index(x, y)] ? color_t::white : color_t::black;
					draw(x, y, pixel_type::solid, c);
				}
			}
			return true;
		}

	private:
		int grid_index(int x, int y) {
			return y * width() + x;
		}

		int neighbour_count(const vector<int>& grid, int x, int y) {
			int count = 0;
			for (auto xoff : { -1, 0, 1 }) {
				for (auto yoff : { -1, 0, 1 }) {
					if (xoff != 0 || yoff != 0) {
						auto px = x + xoff;
						auto py = y + yoff;
						if (px >= 0 && px < width() - 1 && py >= 0 && py < height() - 1) {
							count += (grid[grid_index(px, py)] > 0) ? 1 : 0;
						}
					}
				}
			}
			return count;
		}

	private:
		vector<vector<int>> _grids;

		// the grid to be rendered
		int _active_grid_index;
	};
}

int main() {
	int grid_w = 160;
	int grid_h = 100;
	olc::game_of_life game{};
	game.construct_console(grid_w, grid_h, 8, 8);
	game.start();

	return 0;
}