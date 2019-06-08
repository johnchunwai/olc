#include "common.h"
#include <iostream>
#include <cmath>

using namespace std;

namespace olc
{
	constexpr int g_screen_width = 120;
	constexpr int g_screen_height = 40;
	constexpr int g_map_width = 16;
	constexpr int g_map_height = 16;

	wchar_t dist2wall_to_shade(float dist2wall, float depth)
	{
		wchar_t shade;
		if (dist2wall <= depth / 4.0f)
			shade = 0x2588;		// very close
		else if (dist2wall < depth / 3.0f)
			shade = 0x2593;
		else if (dist2wall < depth / 2.0f)
			shade = 0x2592;
		else if (dist2wall < depth)
			shade = 0x2591;
		else
			shade = L' ';		// too far
		return shade;
	}

	int main()
	{
		console_screen_buffer scnbuf(120, 40);
		wstring& screen = scnbuf.screen();

		wstring map{
			L"#########......."
			L"#..............."
			L"#.......########"
			L"#..............#"
			L"#......##......#"
			L"#......##......#"
			L"#..............#"
			L"###............#"
			L"##.............#"
			L"#......####..###"
			L"#......#.......#"
			L"#......#.......#"
			L"#..............#"
			L"#......#########"
			L"#..............#"
			L"################" };

		// logic
		constexpr float pi = 3.14159f;
		// for ray casting
		constexpr float stepsize = 0.1f;

		float player_x = 14.7f;
		float player_y = 5.09f;
		float player_a = pi / 2.0f;	// player angle, 0 means E. +ve is anti-clockwise. Default facing north.
		float fov = pi / 4.0f;	// field of view
		float depth = 16.0f;	// max render distance

		while (true) {

			// render player fov
			// determine player distance to wall/boundary
			for (int x = 0; x < scnbuf.width(); ++x) {
				// for each column, calculate projected ray angle in world space
				auto rayangle = (player_a + fov / 2.0f) - (x * scnbuf.width() / fov);

				// find distance to closest collision
				float dist2wall = 0.0f;
				bool hitwall = false;

				// unit vec of ray angle
				float eyex = cosf(rayangle);
				float eyey = sinf(rayangle);

				// increment step by step until hitting wall or max view dist
				while (!hitwall && dist2wall < depth) {
					dist2wall += stepsize;
					int testx = static_cast<int>(player_x + eyex * dist2wall);
					int testy = static_cast<int>(player_y + eyey * dist2wall);

					// check out of bounds
					if (testx < 0 || testx >= g_map_width || testy < 0 || testy >= g_map_height) {
						hitwall = true;
						dist2wall = depth;
					}
					else {
						// check hit wall
						if (map.at(testy * g_map_width + testx) == L'#') {
							hitwall = true;

							// TODO: highlight wall boundaries
						}
					}
				}

				// calculate dist to ceiling and floor
				// we split the screen into top and bottom half
				float half_scn_height = static_cast<float>(scnbuf.height()) / 2.0f;
				int ceiling = static_cast<int>(half_scn_height - half_scn_height / dist2wall);
				int floor = scnbuf.height() - ceiling;

				wchar_t shade = dist2wall_to_shade(dist2wall, depth);

				for (int y = 0; y < scnbuf.height(); ++y) {
					// each row
					int scnidx = y * scnbuf.width() + x;
					if (y <= ceiling) {
						screen.at(scnidx) = L' ';
					}
					else if (y > ceiling && y <= floor) {
						screen.at(scnidx) = shade;
					}
					else {
						screen.at(scnidx) = L'*';
					}
				}
			}
			// draw mini-map
			for (int mx = 0; mx < g_map_width; ++mx) {
				for (int my = 0; my < g_map_height; ++my) {
					screen.at((my + 1) * scnbuf.width() + mx) = map.at(my * g_map_width + mx);
				}
			}
			screen.at(static_cast<int>(player_y) * scnbuf.width() + static_cast<int>(player_x)) = L'P';

			// display
			scnbuf.display();
		}

		scnbuf.close();
		pause();

		return 0;
	}
}


int main()
{
	olc::main();
}
