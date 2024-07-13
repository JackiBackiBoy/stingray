#include <iostream>

#include "core/application.hpp"

using namespace sr;

class Sandbox final : public Application {
public:
    Sandbox() : Application("Sandbox") {}
    ~Sandbox() {}

    void onInitialize() override {

    }

    void onUpdate() override {

    }

private:
};

int main() {
    Sandbox* sandbox = new Sandbox();

    sandbox->run();

    delete sandbox;
    return 0;
}