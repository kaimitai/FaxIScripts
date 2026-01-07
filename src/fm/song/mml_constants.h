#ifndef FM_MML_CONSTANTS_H
#define FM_MML_CONSTANTS_H

namespace fm {

	namespace c {
		constexpr char DIRECTIVE_SONG[]{ "#song" };
		constexpr char DIRECTIVE_TEMPO[]{ "#tempo" };

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
		constexpr char OPCODE_LOOPIF[]{ "loopif" };
		constexpr char OPCODE_ENVELOPE[]{ "envelope" };
		constexpr char OPCODE_DETUNE[]{ "detune" };
		constexpr char OPCODE_PULSE[]{ "pulse" };
		constexpr char OPCODE_EFFECT[]{ "effect" };
	}

}

#endif
