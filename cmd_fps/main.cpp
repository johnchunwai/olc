#include "common.h"
#include <iostream>
#include <cmath>
#include <chrono>
#include <cwchar>
#include <vector>


using namespace std;

namespace olc
{
	constexpr int g_screen_width = 120;
	constexpr int g_screen_height = 40;
	constexpr int16_t g_font_size = 16;
	constexpr int g_map_width = 16;
	constexpr int g_map_height = 16;
	constexpr int g_mini_map_offset_x = 1;
	constexpr int g_mini_map_offset_y = 1;

	constexpr float epsilon = 0.00001f;
	constexpr float pi = 3.14159f;
	constexpr float fov = 90.0f * pi / 180.0f;	// 106 degree fov
	constexpr float view_dist = 16.0f;

	wchar_t dist2wall_to_shade(float dist2wall, float view_dist)
	{
		wchar_t shade;
		if (dist2wall <= view_dist / 4.0f)
			shade = 0x2588;		// very close
		else if (dist2wall < view_dist / 3.0f)
			shade = 0x2593;
		else if (dist2wall < view_dist / 2.0f)
			shade = 0x2592;
		else if (dist2wall < view_dist)
			shade = 0x2591;
		else
			shade = L' ';		// too far
		return shade;
	}

	struct player_t
	{
		float x;
		float y;
		float dir;		// player direction, 0 means East. +ve is anti-clockwise
		float vel;		// veclocity
		float ang_vel;	// angular velocity
	};

	struct wall_corner_t
	{
		float x;
		float y;
		float player_to_corner_vec_x;	// unit vec
		float player_to_corner_vec_y;	// unit vec
		float dist_to_player;
		float dot_with_eye;				// dot between 2 unit vec's
	};

	vector<wall_corner_t> init_wall_corners(const player_t& player, float eyex, float eyey, int wall_x, int wall_y)
	{
		vector<wall_corner_t> corners(4);
		for (int tx = 0; tx < 2; ++tx) {
			for (int ty = 0; ty < 2; ++ty) {
				float x = static_cast<float>(wall_x) + tx;
				float y = static_cast<float>(wall_y) + ty;
				float vx = x - player.x;
				float vy = y - player.y;
				float d = sqrt(vx * vx + vy * vy);
				vx = vx / d;
				vy = vy / d;
				float dot = eyex * vx + eyey * vy;
				corners.push_back({ x, y, vx, vy, d, dot });
			}
		}
		return corners;
	}

	bool is_wall_boundary(const vector<wall_corner_t>& corners, wstring& map)
	{
		float bound = 0.01f;
		for (auto corner : corners) {
			// check if it's visible to the player first
			// move from corner to player by 0.5 dist and see if it hits a wall
			float testx = corner.x - corner.player_to_corner_vec_x * 0.5f;
			float testy = corner.y - corner.player_to_corner_vec_y * 0.5f;
			if (map.at(static_cast<int>(testy) * g_map_width + static_cast<int>(testx)) == L'#') {
				// not visible so, not a boundary to be displayed
				// try next corner
				continue;
			}

			// this corner is visible to the player. See if this is within the bounds of the ray cast from eye
			float angle_with_eye = acosf(corner.dot_with_eye);
			if (angle_with_eye < bound) {
				return true;
			}
		}
		return false;
	}

	int main()
	{
		console_screen_buffer scnbuf(g_screen_width, g_screen_height, g_font_size);
		wstring& screen = scnbuf.screen();

		//
		// The map is 16 x 16.
		//
		// Player's movement is floating point. The actual range of movement for player is from 0.0 - 15.99 (note that it goes above 15.0).
		// We're basically treating integer coord 0 for the map as a block holding the range 0.0 - 0.99, and so on.
		//
		// Another thing to note is that windows's y-axis is flipped compare to math models. Therefore, vector calculations and trig functions
		// will have the y component's sign flipped (eg. -sin(dir) instead of sin(dir).
		//
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

		//
		// logic
		//
		// for ray casting
		constexpr float stepsize = 0.1f;
		//
		// We split the screen into top and bottom half. The ceiling height and floor height are the same and increases when distance to wall increases.
		//
		constexpr int max_wall_height = static_cast<int>(g_screen_height * 0.9f);						// closest
		constexpr int min_wall_height = static_cast<int>(g_screen_height * 0.1f);	// farthest
		constexpr int min_ceiling = (g_screen_height - max_wall_height) / 2;		// closest
		constexpr int max_ceiling = (g_screen_height - min_wall_height) / 2;		// farthest
		constexpr float dist_2_ceiling_ratio = (max_ceiling - min_ceiling) / view_dist;

		player_t player{ 14.7f, 5.09f, pi / 2.0f,  5.0f, 5.0f * 0.75f };
		//player_t player{ 15.99f, 1.00f, pi,  1.0f, 0.75f };


		auto curr_time = chrono::system_clock::now();
		auto prev_time = chrono::system_clock::now();

		while (true) {

			// handle timing
			curr_time = chrono::system_clock::now();
			chrono::duration<float> elapsed_dur = curr_time - prev_time;
			prev_time = curr_time;
			float elapsed = elapsed_dur.count();

			// handle input
			// mod with 2pi to avoid it ever overflow
			if (0x8000 & GetAsyncKeyState('A') || 0x8000 & GetAsyncKeyState(VK_LEFT))
				player.dir = fmodf(player.dir + player.ang_vel * elapsed, 2 * pi);
			else if (0x8000 & GetAsyncKeyState('D') || 0x8000 & GetAsyncKeyState(VK_RIGHT))
				player.dir = fmodf(player.dir - player.ang_vel * elapsed, 2 * pi);
			
			float mov_dir = 0.0f;	// 1 for moving forward, -1 for moving backward
			if (0x8000 & GetAsyncKeyState('W') || 0x8000 & GetAsyncKeyState(VK_UP))
				mov_dir = 1.0f;
			else if (0x8000 & GetAsyncKeyState('S') || 0x8000 & GetAsyncKeyState(VK_DOWN))
				mov_dir = -1.0f;
			
			//
			// note that sin(dir) becomes -sin(dir) because the y-axis of windows coord is 0 on top and height - 1 at bottom
			//

			if (fabsf(mov_dir) > epsilon) {
				float movx = cosf(player.dir) * player.vel * elapsed * mov_dir;
				float movy = -sinf(player.dir) * player.vel * elapsed * mov_dir;
				player.x += movx;
				player.y += movy;
				// check for collision
				if (player.x < 0.0f || player.x >= g_map_width || player.y < 0.0f || player.y >= g_map_height ||
					map.at(static_cast<int>(player.y) * g_map_width + static_cast<int>(player.x)) == L'#') {
					player.x -= movx;
					player.y -= movy;
				}
			}

			// render player fov
			// determine player distance to wall/boundary
			for (int x = 0; x < scnbuf.width(); ++x) {
				// for each column, calculate projected ray angle in world space
				auto rayangle = (player.dir + fov / 2.0f) - (x * fov / scnbuf.width());

				// find distance to closest collision
				float dist2wall = 0.0f;
				bool hitwall = false;
				bool boundary = false;

				// unit vec of ray angle
				float eyex = cosf(rayangle);
				float eyey = -sinf(rayangle);

				// increment step by step until hitting wall or max view dist
				while (!hitwall && dist2wall < view_dist) {
					dist2wall += stepsize;
					float testx = player.x + eyex * dist2wall;
					float testy = player.y + eyey * dist2wall;

					// check out of bounds
					if (testx < 0.0f || testx >= g_map_width || testy < 0.0f || testy >= g_map_height) {
						dist2wall = view_dist;
					}
					else {
						// check hit wall
						int wallx = static_cast<int>(testx);
						int wally = static_cast<int>(testy);
						if (map.at(wally * g_map_width + wallx) == L'#') {
							hitwall = true;

							// highlight wall boundaries
							vector<wall_corner_t> wall_corners = init_wall_corners(player, eyex, eyey, wallx, wally);
							boundary = is_wall_boundary(wall_corners, map);
						}
					}
				}

				// calculate dist to ceiling and floor
				// we split the screen into top and bottom half
				float half_scn_height = static_cast<float>(scnbuf.height()) / 2.0f;
				int ceiling = min_ceiling + static_cast<int>(dist2wall * dist_2_ceiling_ratio);
				int floor = scnbuf.height() - ceiling;

				for (int y = 0; y < scnbuf.height(); ++y) {
					// each row
					int scnidx = y * scnbuf.width() + x;
					if (y < ceiling) {
						screen.at(scnidx) = L' ';
					}
					else if (y >= ceiling && y < floor) {
						wchar_t shade = boundary ? L' ' : dist2wall_to_shade(dist2wall, view_dist);
						screen.at(scnidx) = shade;
					}
					else {
						// shade floor based on distance
						float brightness = (static_cast<float>(y) - scnbuf.height() / 2.0f) / (scnbuf.height() / 2.0f);
						wchar_t shade;
						if (brightness < 0.25f) shade = L'#';
						else if (brightness < 0.5f) shade = L'+';
						else if (brightness < 0.75f) shade = L'-';
						else if (brightness < 0.9f) shade = L'.';
						else shade = L' ';
						screen.at(scnidx) = shade;
					}
				}
			}
			// draw mini-map
			for (int mx = 0; mx < g_map_width; ++mx) {
				for (int my = 0; my < g_map_height; ++my) {
					screen.at((my + g_mini_map_offset_y) * scnbuf.width() + mx + g_mini_map_offset_x) = map.at(my * g_map_width + mx);
				}
			}
			// draw player and player orientation on mini-map
			screen.at((static_cast<int>(player.y) + g_mini_map_offset_y) * scnbuf.width() + static_cast<int>(player.x) + g_mini_map_offset_x) = L'P';
			float facex = cosf(player.dir);
			int player_face_x = (fabsf(facex) < 0.5f) ? 0 : (facex > 0.0f) ? 1 : -1;
			float facey = -sinf(player.dir);
			int player_face_y = (fabsf(facey) < 0.5f) ? 0 : (facey > 0.0f) ? 1 : -1;
			screen.at((static_cast<int>(player.y) + player_face_y + g_mini_map_offset_y) * scnbuf.width() + static_cast<int>(player.x) + player_face_x + g_mini_map_offset_x) = L'\x2666';
			// draw player location and orientation in text
			wchar_t text[37];
			swprintf_s(text, sizeof(text) / sizeof(*text), L"x=%05.2f, y=%05.2f, dir=%03d, fps=%05.2f", player.x, player.y, static_cast<int>(player.dir * 180.0f / pi + 360.0f) % 360, 1.0f / elapsed);
			scnbuf.screen().replace(g_mini_map_offset_x, wcslen(text), text);

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
	try {
		olc::main();
	}
	catch (runtime_error& e) {
		cerr << e.what();
	}
}
