#ifndef FI_OPCODE_H
#define FI_OPCODE_H

#include <map>
#include <string>

using byte = unsigned char;

namespace fi {

	enum class ArgType {
		None,
		Byte,
		Short
	};

	enum class Flow {
		Continue, Jump, Read, End
	};

	enum class ArgDomain {
		None,
		Item,
		Quest,
		Rank,
		TextBox,
		TextString
	};

	struct Opcode {
		std::string name;
		fi::ArgType arg_type;
		fi::Flow flow;
		fi::ArgDomain domain;

		std::size_t size(void) const {
			std::size_t result{ 0 };
			if (flow == fi::Flow::Jump || flow == fi::Flow::Read)
				result += 2;
			if (arg_type == fi::ArgType::Short)
				result += 2;
			else if (arg_type == fi::ArgType::Byte)
				++result;
			return result;
		}
	};

	extern const std::map<byte, fi::Opcode> opcodes;

}

#endif
