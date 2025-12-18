#ifndef FM_CONSTANTS_H
#define FM_CONSTANTS_H

#include <map>
#include <string>
#include <vector>

using byte = unsigned char;

namespace fm {

	namespace c {

		constexpr char ID_MUSIC_PTR[]{ "music_ptr" };
		constexpr char ID_MUSIC_COUNT[]{ "music_count" };
		constexpr char ID_MUSIC_DATA_END[]{ "music_data_end" };
		constexpr char ID_MSCRIPT_OPCODES[]{ "mscript_opcodes" };
		constexpr char ID_CHAN_PITCH_OFFSET[]{ "music_channel_pitch_offset" };

		constexpr char ID_DEFINES_ENVELOPE[]{ "mscript_defines_env" };

		constexpr char ID_MSCRIPT_START_OCTAVE[]{ "music_first_octave" };
		constexpr char ID_MSCRIPT_START_NOTE[]{ "music_first_note" };
		constexpr char ID_MSCRIPT_START_BYTE[]{ "music_first_byte_value" };

		constexpr char ID_MSCRIPT_ENTRYPOINT[]{ ".song" };

		constexpr char XML_OPCODE_PARAM_MNEMONIC[]{ "Mnemonic" };
		constexpr char XML_OPCODE_PARAM_ARG[]{ "Arg" };
		constexpr char XML_OPCODE_PARAM_FLOW[]{ "Flow" };
		constexpr char XML_OPCODE_PARAM_TYPE[]{ "Type" };
		constexpr char XML_OPCODE_PARAM_DOMAIN[]{ "ArgDomain" };

		constexpr char SECTION_DEFINES[]{ "[defines]" };
		constexpr char SECTION_MSCRIPT[]{ "[mscript]" };

		constexpr byte HEX_REST{ 0x00 };
		constexpr byte HEX_NOTE_MIN{ 0x01 };
		constexpr byte HEX_NOTE_END{ 0x80 };
		constexpr byte HEX_NOTELENGTH_MIN{ 0x80 };
		constexpr byte HEX_NOTELENGTH_END{ 0xee };

		inline const std::vector<std::string> NOTE_NAMES{
			"c", "c+", "d", "d+", "e", "f", "f+", "g", "g+", "a", "a+", "b"
		};

		inline const std::vector<std::string> CHANNEL_LABELS{
			"SQ1", "SQ2", "TRI", "NOISE" };

	}
}

#endif
