#ifndef FI_ISCRIPTLOADER_H
#define FI_ISCRIPTLOADER_H

#include <vector>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include "Opcode.h"
#include "Shop.h"

using byte = unsigned char;

namespace fi {

	enum Instruction_type { OpCode, Directive };

	struct Instruction {
		Instruction_type type;
		byte opcode_byte;
		std::size_t size;
		std::optional<uint16_t> operand;
		std::optional<std::size_t> jump_target;
	};

	class IScriptLoader {
	public:
		IScriptLoader(const std::vector<uint8_t>& rom);

		// TODO change visibility
	public:
		const std::vector<byte> rom;
		std::vector<std::size_t> ptr_table;
		std::map<std::size_t, fi::Instruction> m_instructions;
		std::vector<std::string> m_strings;
		std::vector<fi::Shop> m_shops;

		std::set<std::size_t> m_jump_targets;
		// map from ROM address -> shop index
		// we parse shops when we see them and assign an index if it is unique
		std::map<std::size_t, std::size_t> m_shop_addresses;

		void parse_rom(void);
		void parse_strings(void);
		void parse_blob_from_entrypoint(size_t offset, bool at_entrypoint);

		byte read_byte(std::size_t& offset) const;
		uint16_t read_short(std::size_t& offset) const;

		static std::string to_hex(size_t value);
	};

}

#endif
