#ifndef FM_MUSIC_OPCODE_H
#define FM_MUSIC_OPCODE_H

#include <map>
#include <optional>
#include <string>
#include <vector>

using byte = unsigned char;

namespace fm {

	enum class AudioFlow { Jump, Continue, End };
	enum class AudioArgType { None, Byte, Address };
	enum class OpcodeType { Note, Opcode };

	struct MusicOpcode {

		std::string m_mnemonic;
		fm::AudioArgType m_argtype;
		fm::OpcodeType m_opcodetype;
		fm::AudioFlow m_flow;

		MusicOpcode(const std::string& p_mnemonic,
			fm::OpcodeType p_opcodetype,
			fm::AudioArgType p_argtype,
			fm::AudioFlow p_flow);
		MusicOpcode(const std::string& p_params);

		std::size_t size(void) const;
	};

	struct MusicInstruction {
		byte opcode_byte;
		std::optional<byte> operand;
		std::optional<std::size_t> jump_target;
		std::optional<std::size_t> byte_offset;

		std::vector<byte> get_bytes(const std::vector<fm::MusicOpcode>& p_opcodes) const;
	};

	std::vector<fm::MusicOpcode> parse_xml_map(const std::map<byte, std::string>& p_map);

	fm::AudioFlow string_to_enum_flow(const std::string& p_str);
	fm::AudioArgType string_to_enum_argtype(const std::string& p_str);
	fm::OpcodeType string_to_enum_opcodetype(const std::string& p_str);

}

#endif
