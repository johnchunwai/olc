#include "common.h"
#include <list>
#include <chrono>
#include <thread>
#include <iterator>
#include <cstdlib>

using namespace std;

namespace olc {

	constexpr int g_screen_width = 120;
	constexpr int g_screen_height = 30;
	constexpr int g_font_size = 16;

	constexpr int g_field_top = 3;
	constexpr int g_gameover_x = 40;
	constexpr int g_gameover_y = 15;

	struct snake_segment {
		int x;
		int y;
	};

	int main()
	{
		console_screen_buffer scnbuf(g_screen_width, g_screen_height, g_font_size);
		wstring& screen = scnbuf.screen();

		auto now = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
		srand((uint32_t)now);

		bool quit = false;
		while (!quit) {

			// reset to known state
			bool dead = false;
			bool out_of_bound = false;
			int score = 0;
			list<snake_segment> snake = { {60,15},{61,15},{62,15},{63,15},{64,15},{65,15},{66,15},{67,15},{68,15},{69,15} };
			int snake_dir_x = -1;
			int snake_dir_y = 0;
			// hardcode so the initial state won't have the food overlap the snake at the beginning
			int foodx = 30;
			int foody = 15;
			int grow_per_food = 5;

			while (!dead) {
				// ==== input

				//this_thread::sleep_for(200ms);
				auto t1 = chrono::system_clock::now();
				bool key_pressed = false;
				// vertical moves slower to compensate for aspect ratio
				while ((chrono::system_clock::now() - t1) < (snake_dir_x == 0 ? 200ms : 120ms)) {
					if (!key_pressed) {
						if (0x8000 & GetAsyncKeyState(VK_UP) && snake_dir_y == 0) {
							snake_dir_x = 0;
							snake_dir_y = -1;
							key_pressed = true;
						}
						else if (0x8000 & GetAsyncKeyState(VK_DOWN) && snake_dir_y == 0) {
							snake_dir_x = 0;
							snake_dir_y = 1;
							key_pressed = true;
						}
						else if (0x8000 & GetAsyncKeyState(VK_RIGHT) && snake_dir_x == 0) {
							snake_dir_x = 1;
							snake_dir_y = 0;
							key_pressed = true;
						}
						else if (0x8000 & GetAsyncKeyState(VK_LEFT) && snake_dir_x == 0) {
							snake_dir_x = -1;
							snake_dir_y = 0;
							key_pressed = true;
						}
					}
				}

				// === logic
				snake.push_front({ snake.front().x + snake_dir_x, snake.front().y + snake_dir_y });
				const snake_segment& head = snake.front();

				// collision with food
				if (head.x == foodx && head.y == foody) {
					++score;
					foodx = rand() % scnbuf.width();
					foody = rand() % (scnbuf.height() - g_field_top) + g_field_top;
					for (int i = 0; i < 5; ++i) {
						snake.push_back({ snake.back().x, snake.back().y });
					}
				}

				// chop off the tail
				// delay this until here as when snake eats food, we want to add a few tail to and and let it grow as it moves
				snake.pop_back();

				// collision with snake
				for (auto it = next(snake.begin()); it != snake.end(); it++) {
					if (head.x == it->x && head.y == it->y) {
						dead = true;
					}
				}

				// collision with boundary
				if (head.x < 0 || head.x >= scnbuf.width() || head.y < g_field_top || head.y >= scnbuf.height()) {
					dead = true;
					out_of_bound = true;
				}

				// ==== Render

				// clear screen
				fill(screen.begin(), screen.end(), L' ');

				// draw snake
				for (auto it = next(snake.begin()); it != snake.end(); it++) {
					screen.at(it->y * scnbuf.width() + it->x) = dead ? L'+' : L'O';
				}
				// draw snake head
				if (!out_of_bound) {
					screen.at(snake.front().y * scnbuf.width() + snake.front().x) = dead ? L'X' : L'@';
				}

				// draw food
				screen.at(foody * scnbuf.width() + foodx) = L'%';

				// draw stats and border
				wchar_t text[50];
				constexpr size_t textsize = sizeof(text) / sizeof(*text);
				swprintf_s(text, textsize, L"Command line SNAKE.            SCORE: %d", score);
				screen.replace(scnbuf.width() + 5, wcslen(text), text);
				screen.replace((g_field_top - 1) * scnbuf.width(), scnbuf.width(), scnbuf.width(), L'=');

				if (dead) {
					swprintf_s(text, textsize, L"    PRESS 'SPACE' TO PLAY AGAIN    ");
					screen.replace(g_gameover_y * scnbuf.width() + g_gameover_x, wcslen(text), text);
				}

				// display
				scnbuf.display();
			}

			bool retry = false;
			do {
				retry = (0x8000 & GetAsyncKeyState(VK_SPACE));
				quit = (0x8000 & GetAsyncKeyState(VK_ESCAPE));
			} while (!retry && !quit);
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