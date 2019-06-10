#include <cstdint>
#include <chrono>
#include <thread>
#include <memory>
#include <array>
#include <string_view>
#include <vector>
#include <random>
#include <iostream>
#include "windows.h"
#include "common.h"

using namespace std;

namespace olc
{
	constexpr int g_screen_width = 80;
	constexpr int g_screen_height = 30;
	constexpr int16_t g_font_size = 30;
	constexpr int g_field_width = 12;
	constexpr int g_field_height = 18;
	constexpr int g_field_offset_y = 6;
	constexpr int g_field_offset_x = 2;
	constexpr int g_next_piece_offset_y = 2;
	constexpr int g_next_piece_offset_x = g_field_offset_x + 1;
	constexpr int g_tetromino_count = 7;

	constexpr int g_piece_per_lvl = 7;
	constexpr int g_speed_per_lvl = 2;
	constexpr int g_max_lvl = 10;

	constexpr wstring_view g_tiles = L" ABCDEFG=#";
	constexpr string_view g_keys("\x25\x27\x28ZX");


	array<wstring, g_tetromino_count> g_tetromino;
	array<uint8_t, g_field_width* g_field_height> field;


	class randgen
	{
	public:
		randgen(int low, int high)
			: _engine(random_device()()),
			_uniform_dist(low, high)
		{
		}

		int next() {
			return _uniform_dist(_engine);
		}

	private:
		default_random_engine _engine;
		uniform_int_distribution<int> _uniform_dist;
	};

	int rotate(int px, int py, int r)
	{
		int pi = 0;
		switch (r % 4) {
		case 0:
			pi = py * 4 + px;
			break;
		case -3:
		case 1:
			pi = 12 - (px * 4) + py;
			break;
		case -2:
		case 2:
			pi = 15 - px - (py * 4);
			break;
		case -1:
		case 3:
			pi = 3 + (px * 4) - py;
			break;
		}
		return pi;
	}

	bool does_piece_fit(int piece, int posx, int posy, int r)
	{
		for (int px{ 0 }; px < 4; ++px) {
			for (int py{ 0 }; py < 4; ++py) {
				int pi{ rotate(px, py, r) };
				int x{ posx + px };
				int y{ posy + py };
				int fi{ (py + posy) * g_field_width + (px + posx) };
				// ignore out of bounds, those are blank space. Since all pieces move one block each time,
				// any non blank space would have collided with field edge before.
				if (x >= 0 && x < g_field_width && y >= 0 && y < g_field_height) {
					if (g_tetromino.at(piece).at(pi) != L'.' && field.at(fi) != 0) {
						return false;
					}
				}
			}
		}
		return true;
	}


	int main()
	{
		randgen tetr_gen(0, g_tetromino_count - 1);

		// init screen buffer
		console_screen_buffer screenbuf(g_screen_width, g_screen_height, g_font_size);
		wstring& screen = screenbuf.screen();

		// initialize the 7 block types
		// each tetromino is 4x4
		g_tetromino[0].append(L"..x.");
		g_tetromino[0].append(L"..x.");
		g_tetromino[0].append(L"..x.");
		g_tetromino[0].append(L"..x.");

		g_tetromino[1].append(L"..x.");
		g_tetromino[1].append(L".xx.");
		g_tetromino[1].append(L"..x.");
		g_tetromino[1].append(L"....");

		g_tetromino[2].append(L"..x.");
		g_tetromino[2].append(L".xx.");
		g_tetromino[2].append(L".x..");
		g_tetromino[2].append(L"....");

		g_tetromino[3].append(L".x..");
		g_tetromino[3].append(L".xx.");
		g_tetromino[3].append(L"..x.");
		g_tetromino[3].append(L"....");

		g_tetromino[4].append(L"....");
		g_tetromino[4].append(L".xx.");
		g_tetromino[4].append(L".xx.");
		g_tetromino[4].append(L"....");

		g_tetromino[5].append(L".x..");
		g_tetromino[5].append(L".x..");
		g_tetromino[5].append(L".xx.");
		g_tetromino[5].append(L"....");

		g_tetromino[6].append(L"..x.");
		g_tetromino[6].append(L"..x.");
		g_tetromino[6].append(L".xx.");
		g_tetromino[6].append(L"....");

		// init play field
		for (int x{ 0 }; x < g_field_width; ++x) {
			for (int y{ 0 }; y < g_field_height; ++y) {
				field.at(y * g_field_width + x) = (x == 0 || x == g_field_width - 1 || y == g_field_height - 1) ? 9 : 0;
			}
		}

		// game logic
		array<bool, g_keys.size()> keydown;
		int next_piece = tetr_gen.next();
		int cur_piece = tetr_gen.next();
		int curx = g_field_width / 2;
		int cury = 0;
		int cur_rotate = 0;
		bool rotate_hold_z = false;
		bool rotate_hold_x = false;
		int speed = 20;
		int speed_count = 0;
		bool force_down = false;
		vector<int> fill_lines;
		int piece_count = 0;
		int lvl = 1;
		int score = 0;
		bool gameover = false;

		while (!gameover) {
			// timing
			this_thread::sleep_for(50ms);
			force_down = (speed_count == speed);
			++speed_count;

			// input
			// VK_LEFT=0x25, VK_RIGHT=0x27, VK_DOWN=0x28, z key=0x5A
			for (int i = 0; i < g_keys.size(); ++i) {
				// 0x8000 - the most significant bit of short indicates key down.
				keydown.at(i) = 0x8000 & GetAsyncKeyState(g_keys.at(i));
			}
			curx += (keydown.at(0) && does_piece_fit(cur_piece, curx - 1, cury, cur_rotate)) ? -1 : 0;
			curx += (keydown.at(1) && does_piece_fit(cur_piece, curx + 1, cury, cur_rotate)) ? 1 : 0;
			cury += (keydown.at(2) && does_piece_fit(cur_piece, curx, cury + 1, cur_rotate)) ? 1 : 0;

			// don't keep rotate if key is held down
			if (keydown.at(3)) {
				cur_rotate += (!rotate_hold_z && does_piece_fit(cur_piece, curx, cury, cur_rotate + 1)) ? 1 : 0;
				rotate_hold_z = true;
			}
			else {
				rotate_hold_z = false;

				if (keydown.at(4)) {
					cur_rotate += (!rotate_hold_x && does_piece_fit(cur_piece, curx, cury, cur_rotate - 1)) ? -1 : 0;
					rotate_hold_x = true;
				}
				else {
					rotate_hold_x = false;
				}
			}

			// force down
			if (force_down) {
				speed_count = 0;

				if (does_piece_fit(cur_piece, curx, cury + 1, cur_rotate)) {
					++cury;
				}
				else {
					// update difficulty
					++piece_count;
					if (piece_count % g_piece_per_lvl == 0 && speed >= g_speed_per_lvl) {
						speed -= g_speed_per_lvl;
						lvl++;
						if (lvl == g_max_lvl) {
							gameover = true;
						}
					}

					// freeze the piece
					for (int px{ 0 }; px < 4; ++px) {
						for (int py{ 0 }; py < 4; ++py) {
							if (g_tetromino.at(cur_piece).at(rotate(px, py, cur_rotate)) != L'.') {
								// +1 at the end to skip blank at 0
								field.at((cury + py) * g_field_width + (curx + px)) = cur_piece + 1;
							}
						}
					}

					// check lines
					for (int py{ 0 }; py < 4; ++py) {
						// -1 because field boundary line doesn't count
						if ((cury + py) < g_field_height - 1) {
							bool isline = true;
							for (int fx{ 1 }; fx < g_field_width - 1; ++fx) {
								if (field.at((cury + py) * g_field_width + fx) == 0) {
									isline = false;
									break;
								}
							}

							if (isline) {
								// set line to =
								for (int fx{ 1 }; fx < g_field_width - 1; ++fx) {
									field.at((cury + py) * g_field_width + fx) = 8;
								}
								fill_lines.push_back(cury + py);
							}
						}
					}

					// add score
					score += 25;
					if (!fill_lines.empty()) {
						score += 100 * (1 << fill_lines.size());
					}

					// new piece
					cur_piece = next_piece;
					next_piece = tetr_gen.next();
					curx = g_field_width / 2;
					cury = 0;
					cur_rotate = 0;

					// check gameover
					if (!does_piece_fit(cur_piece, curx, cury, cur_rotate)) {
						gameover = true;
					}
				}
			}

			// draw field
			for (int x{ 0 }; x < g_field_width; ++x) {
				for (int y{ 0 }; y < g_field_height; ++y) {
					screen.at((y + g_field_offset_y) * screenbuf.width() + (x + g_field_offset_x)) = g_tiles.at(field.at(y * g_field_width + x));
				}
			}

			// draw next piece
			for (int px{ 0 }; px < 4; ++px) {
				for (int py{ 0 }; py < 4; ++py) {
					const wchar_t tile = (g_tetromino.at(next_piece).at(px + py * 4) != L'.') ? g_tiles.at(next_piece + 1) : g_tiles.at(0);
					screen.at((g_next_piece_offset_y + py) * screenbuf.width() + (g_next_piece_offset_x + px)) = tile;
				}
			}

			// draw current piece
			for (int px{ 0 }; px < 4; ++px) {
				for (int py{ 0 }; py < 4; ++py) {
					if (g_tetromino.at(cur_piece).at(rotate(px, py, cur_rotate)) != L'.') {
						screen.at((cury + g_field_offset_y + py) * screenbuf.width() + (curx + g_field_offset_x + px)) = g_tiles.at(cur_piece + 1);
					}
				}
			}

			// draw score
			swprintf_s(&screen.at(screenbuf.width() * 2 + g_field_width + g_field_offset_x + 2), 16, L"Level: %8d", lvl);
			screen.at(screenbuf.width() * 2 + g_field_width + g_field_offset_x + 2 + 15) = L' ';
			swprintf_s(&screen.at(screenbuf.width() * 4 + g_field_width + g_field_offset_x + 2), 16, L"Score: %8d", score);
			screen.at(screenbuf.width() * 4 + g_field_width + g_field_offset_x + 2 + 15) = L' ';

			// animate line complete
			if (!fill_lines.empty()) {
				// show frame for a while
				screenbuf.display();
				this_thread::sleep_for(400ms);

				// remove the line by shifting everything in the field down
				for (auto& fill_y : fill_lines) {
					for (int fx{ 1 }; fx < g_field_width - 1; ++fx) {
						for (int fy{ fill_y }; fy > 0; --fy) {
							field.at(fy * g_field_width + fx) = field.at((fy - 1) * g_field_width + fx);
						}
						field.at(fx) = 0;
					}
				}
				fill_lines.clear();
			}

			// display frame
			screenbuf.display();
		}

		// shutdown
		screenbuf.close();
		if (lvl == g_max_lvl) {
			cout << "Congrats! You beat the game!\n";
		}
		else {
			cout << "Game Over!!\n";
		}
		cout << "Level: " << lvl << " Score:" << score << endl;
		cin.ignore();
		return 0;
	}
}


int main()
{
	olc::main();
}
