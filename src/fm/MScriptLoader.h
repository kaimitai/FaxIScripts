#ifndef FM_MML_LOADER_H
#define FM_MML_LOADER_H

#include <map>
#include <set>
#include <vector>
#include "./../fe/Config.h"
#include "MusicOpcode.h"

using byte = unsigned char;

namespace fm {

	class MScriptLoader {

		// TODO: Change visibility
	public:

		const std::vector<byte> m_rom;
		std::vector<std::size_t> m_ptr_table;
		std::vector<fm::MusicOpcode> m_opcodes;
		std::map<std::size_t, fm::MusicInstruction> m_instrs;
		std::set<std::size_t> m_jump_targets;

		void parse_blob_from_entrypoint(size_t offset, size_t zeroaddr);

		byte read_byte(std::size_t& offset) const;
		uint16_t read_short(std::size_t& offset) const;

		MScriptLoader(const std::vector<byte>& p_rom);
		void parse_rom(const fe::Config& p_config);
	};

}

#endif
