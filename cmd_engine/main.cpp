#include "cmd_engine.h"
#include <iostream>
#include <thread>

using namespace std;
using namespace olc;

void pause()
{
	cout << "Press enter to quit";
	cin.ignore();
}

int main()
{
	try {
		cmd_engine game;
		game.construct_console(100, 100, 12, 12);

		this_thread::sleep_for(5s);
	}
	catch (olc_exception& e) {
		wcerr << e.msg() << endl;
	}
	cout << "game ended" << endl;
	pause();
}