#ifndef FM_CONSTANTS_H
#define FM_CONSTANTS_H

#include <string>
#include <vector>

namespace fm {

	namespace c {

		constexpr char ID_MUSIC_PTR[]{ "music_ptr" };
		constexpr char ID_MUSIC_COUNT[]{ "music_count" };
		constexpr char ID_MUSIC_DATA_END[]{ "music_data_end" };
		constexpr char ID_MSCRIPT_OPCODES[]{ "mscript_opcodes" };

		constexpr char ID_MSCRIPT_ENTRYPOINT[]{ "#song" };

		constexpr char XML_OPCODE_PARAM_MNEMONIC[]{ "Mnemonic" };
		constexpr char XML_OPCODE_PARAM_ARG[]{ "Arg" };
		constexpr char XML_OPCODE_PARAM_FLOW[]{ "Flow" };
		constexpr char XML_OPCODE_PARAM_TYPE[]{ "Type" };

		inline const std::vector<std::string> CHANNEL_LABELS{
			"SQ1", "SQ2", "TRI", "NOISE" };

	}
}

#endif
