#include "core/application.hpp"

#include <Windows.h>

using namespace sr;

class Sandbox final : public Application {
public:
    Sandbox() : Application("Stingray", 3440, 1440) {}
    ~Sandbox() {}

    void onInitialize() override {

    }

    void onUpdate() override {

    }

private:
};

#ifdef SR_WINDOWS
int APIENTRY wWinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPWSTR lpCmdLine,
	int nCmdShow) {

	Sandbox* sandbox = new Sandbox();
	sandbox->run();

	delete sandbox;
	sandbox = nullptr;
	return 0;
}
#else
int main() {
	Sandbox* sandbox = new Sandbox();
	sandbox->run();

	delete sandbox;
	sandbox = nullptr;
	return 0;
}
#endif