#include "test_runner.hpp"

int main() {
    return lxace::test::TestRegistry::instance().run_all();
}
