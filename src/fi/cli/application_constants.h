#ifndef FI_APPLICATION_CONSTANTS
#define FI_APPLICATION_CONSTANTS

#include <string>
#include <utility>
#include <vector>

namespace fi {

	namespace appc {

		constexpr char APP_NAME[]{ "FaxIScripts" };
		constexpr char APP_VERSION[]{ "0.2" };

		inline const std::pair<std::string, std::string> CMD_EXTRACT{ "extract" , "x" };
		inline const std::pair<std::string, std::string> CMD_BUILD{ "build" , "b" };

		inline const std::vector<std::pair<std::string, std::string>> CLI_FLAGS{
			{"--no-shop-comments", "-p"},
			{"--original-size", "-o"},
			{"--force", "-f"}
		};

		inline const std::pair<std::string, std::string> CLI_SOURCE_ROM
		{ "--source-rom", "-s" };

	}
}

#endif
