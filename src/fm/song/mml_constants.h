#ifndef FM_MML_CONSTANTS_H
#define FM_MML_CONSTANTS_H

#include <string>
#include <vector>

namespace fm {

	enum class ChannelType { sq1, sq2, tri, noise };

	namespace c {
		constexpr char DIRECTIVE_SONG[]{ "#song" };
		// delimiter for raw tick lengths
		constexpr char RAW_DELIM{ '#' };

		inline const std::vector<std::string> CHANNEL_NAMES
		{ "#sq1", "#sq2", "#tri", "#noise" };
		inline const std::vector<fm::ChannelType> CHANNEL_TYPES
		{ fm::ChannelType::sq1, fm::ChannelType::sq2,
		fm::ChannelType::tri, fm::ChannelType::noise };

		constexpr char DIRECTIVE_SQ1[]{ "#sq1" };
		constexpr char DIRECTIVE_SQ2[]{ "#sq2" };
		constexpr char DIRECTIVE_TRIANGLE[]{ "#tri" };
		constexpr char DIRECTIVE_NOISE[]{ "#noise" };

		constexpr char OPCODE_START[]{ "start" };
		constexpr char OPCODE_END[]{ "end" };
		constexpr char OPCODE_RESTART[]{ "restart" };
		constexpr char OPCODE_NOP[]{ "nop" };
		constexpr char OPCODE_JSR[]{ "jsr" };
		constexpr char OPCODE_RETURN[]{ "return" };
		constexpr char OPCODE_LOOPIF[]{ "endloopif" };
		constexpr char OPCODE_ENVELOPE[]{ "envelope" };
		constexpr char OPCODE_DETUNE[]{ "detune" };
		constexpr char OPCODE_PULSE[]{ "pulse" };
		constexpr char OPCODE_EFFECT[]{ "effect" };
	}

}

#endif
