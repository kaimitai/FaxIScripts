#ifndef FI_ISCRIPTLOADER_H
#define FI_ISCRIPTLOADER_H

#include <vector>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include "Opcode.h"
#include "FaxString.h"
#include "Shop.h"
#include "./../fe/Config.h"

using byte = unsigned char;

namespace fi {

	class IScriptLoader {
	public:
		IScriptLoader(const std::vector<uint8_t>& rom);

		// TODO change visibility
	public:
		const std::vector<byte> rom;
		std::vector<std::size_t> ptr_table;
		std::map<std::size_t, fi::Instruction> m_instructions;
		std::vector<fi::Shop> m_shops;
		std::vector<fi::FaxString> m_strings;

		std::set<std::size_t> m_jump_targets;
		// map from ROM address -> shop index
		// we parse shops when we see them and assign an index if it is unique
		std::map<std::size_t, std::size_t> m_shop_addresses;

		void parse_rom(const fe::Config& p_config);
		void parse_strings(const fe::Config& p_config);
		void parse_blob_from_entrypoint(size_t offset, size_t zeroaddr,
			bool at_entrypoint);

		byte read_byte(std::size_t& offset) const;
		uint16_t read_short(std::size_t& offset) const;

		static std::string to_hex(size_t value);
	};

}

#endif
