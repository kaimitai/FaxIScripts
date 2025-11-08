#include <iostream>
#include "./fi/cli/Cli.h"


int main(int argc, char** argv) try {
	fi::Cli cli(argc, argv);
}
catch (const std::runtime_error& ex) {
	std::cerr << "Runtime error: " << ex.what() << "\n";
}
catch (const std::exception& ex) {
	std::cerr << "Runtime exception: " << ex.what() << "\n";
}
catch (...) {
	std::cerr << "Unknown runtime error:\n";
}
