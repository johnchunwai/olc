#include "cmd_engine.h"
#include <iostream>
#include <thread>

using namespace std;
using namespace olc;


class test_engine : public cmd_engine
{
public:
	explicit test_engine(std::wstring app_name) : cmd_engine() {
		_app_name = app_name;
	}

protected:
	// Inherited via cmd_engine
	virtual bool on_user_init() override
	{
		return true;
	}
	virtual bool on_user_update(float elapsed) override
	{
		return true;
	}
	virtual bool on_user_destroy() override
	{
		static bool b = false;
		if (!b) {
			b = true;
			return false;
		}
		return true;
	}
};

int main()
{
	try {
		test_engine game(L"test engine"s);
		game.construct_console(100, 100, 12, 12);
		game.start();
	}
	catch (olc_exception& e) {
		wcerr << e.msg() << endl;
	}
	cout << "game ended" << endl;
	pause();
}