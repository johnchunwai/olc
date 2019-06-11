#include "common.h"
#include <list>
#include <chrono>
#include <thread>
#include <iterator>

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


		while (true) {

			// reset to known state
			bool dead = false;
			bool out_of_bound = false;
			int score = 0;
			list<snake_segment> snake = { {60,15},{61,15},{62,15},{63,15},{64,15},{65,15},{66,15},{67,15},{68,15},{69,15} };
			int snake_dir_x = -1;
			int snake_dir_y = 0;

			while (!dead) {
				// ==== input

				//this_thread::sleep_for(200ms);
				auto t1 = chrono::system_clock::now();
				bool key_pressed = false;
				// vertical moves slower to compensate for aspect ratio
				while ((chrono::system_clock::now() - t1) < (snake_dir_x == 0 ? 275ms : 150ms)) {
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
				for (auto s : snake) {
					auto head = snake.front();
					head.x += snake_dir_x;
					head.y += snake_dir_y;
					snake.push_front({ snake.front().x + snake_dir_x, snake.front().y + snake_dir_y });
					snake.pop_back();
				}

				const snake_segment& head = snake.front();
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

				// draw stats and border
				wchar_t text[50];
				swprintf_s(text, sizeof(text), L"Command line SNAKE.            SCORE: %d", score);
				screen.replace(scnbuf.width() + 5, wcslen(text), text);
				screen.replace((g_field_top - 1) * scnbuf.width(), scnbuf.width(), scnbuf.width(), L'=');

				if (dead) {
					swprintf_s(text, sizeof(text), L"    PRESS 'SPACE' TO PLAY AGAIN    ");
					screen.replace(g_gameover_y * scnbuf.width() + g_gameover_x, wcslen(text), text);
				}

				// display
				scnbuf.display();
			}
			while ((0x8000 & GetAsyncKeyState(VK_SPACE)) == 0);
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