#include "IScriptLoader.h"
#include "fi_constants.h"
#include <algorithm>
#include <format>

fi::IScriptLoader::IScriptLoader(const std::vector<byte>& p_rom) :
	rom{ p_rom }
{
}

void fi::IScriptLoader::parse_rom(void) {
	// parse strings
	parse_strings();

	// parse IScript blob
	ptr_table.clear();

	for (std::size_t i{ 0 }; i < c::ISCRIPT_COUNT; ++i)
		ptr_table.push_back(static_cast<std::size_t>(rom.at(c::ISCRIPT_ADDR_LO + i))
			+ 256 * static_cast<std::size_t>(rom.at(c::ISCRIPT_ADDR_HI + i))
			+ c::ISCRIPT_PTR_ZERO_ADDR);

	for (const auto offset : ptr_table)
		parse_blob_from_entrypoint(offset, true);
}

void fi::IScriptLoader::parse_strings(void) {
	m_strings.clear();
	std::string encodedstring;

	for (std::size_t i{ c::OFFSET_STRINGS }; i < c::OFFSET_STRINGS + c::SIZE_STRINGS; ++i) {
			
		if (rom.at(i) == 0xff) {
			m_strings.push_back(encodedstring);
			encodedstring.clear();
		}
		else {
			byte b{ rom.at(i) };
			auto iter{ c::FAXSTRING_CHARS.find(b) };
			if (iter == end(c::FAXSTRING_CHARS))
				encodedstring += std::format("<${:02x}>", b);
			else
				encodedstring += iter->second;
		}

	}
}

byte fi::IScriptLoader::read_byte(size_t& offset) const {
	if (offset >= rom.size()) throw std::out_of_range("ROM read out of bounds");
	return rom[offset++];
}

uint16_t fi::IScriptLoader::read_short(size_t& offset) const {
	uint8_t lo = read_byte(offset);
	uint8_t hi = read_byte(offset);
	return static_cast<uint16_t>(hi << 8 | lo);
}

std::string fi::IScriptLoader::to_hex(size_t value) {
	return std::format("0x{:x}", value);
}

void fi::IScriptLoader::parse_blob_from_entrypoint(size_t offset, bool at_entrypoint) {
	if (m_instructions.find(offset) != end(m_instructions))
		return;

	size_t cursor = offset;

	if (at_entrypoint)
		m_instructions.insert(std::make_pair(offset,
			fi::Instruction({ Instruction_type::Directive, read_byte(cursor), 1 })
		));

	while (cursor < rom.size()) {
		if (m_instructions.find(cursor) != end(m_instructions))
			return;

		size_t instr_offset = cursor;
		uint8_t opcode_byte = read_byte(cursor);

		auto it = opcodes.find(opcode_byte);
		if (it == opcodes.end()) {
			throw std::runtime_error("Unknown opcode " + to_hex(opcode_byte) +
				" at offset " + to_hex(instr_offset));
		}

		const Opcode& op = it->second;
		std::optional<uint16_t> arg;

		if (op.arg_type == ArgType::Byte) {
			arg = read_byte(cursor);
		}
		else if (op.arg_type == ArgType::Short) {
			arg = read_short(cursor);
		}

		std::optional<std::size_t> target_addr;

		// track jump targets, extract shops
		if (op.flow == Flow::Jump || op.flow == Flow::Read) {
			target_addr = static_cast<std::size_t>(read_short(cursor))
				+ c::ISCRIPT_PTR_ZERO_ADDR;

			if (op.flow == Flow::Jump)
				m_jump_targets.insert(target_addr.value());
			else {
				const auto& shop_iter{ m_shop_addresses.find(target_addr.value()) };

				if (shop_iter == end(m_shop_addresses)) {
					// new shop, parse it and assign index
					fi::Shop newshop;
					std::size_t shop_offset{ target_addr.value() };
					while (rom.at(shop_offset) != 0xff) {
						newshop.add_entry(rom.at(shop_offset),
							rom.at(shop_offset + 1),
							rom.at(shop_offset + 2));
						shop_offset += 3;
					}

					arg = static_cast<uint16_t>(m_shops.size());
					m_shop_addresses[target_addr.value()] = static_cast<std::size_t>(arg.value());
					m_shops.push_back(newshop);

				}
				else {
					// already seen shop, use its index
					arg = static_cast<uint16_t>(shop_iter->second);
				}
			}
		}

		m_instructions.insert(
			std::make_pair(instr_offset,
				fi::Instruction(fi::Instruction_type::OpCode, opcode_byte, it->second.size(),
					arg, target_addr)));

		if (op.flow == Flow::Jump) {
			// parse all branches recursively, but store and restore cursors
			std::size_t temp_cursor{ cursor };
			cursor = target_addr.value();

			parse_blob_from_entrypoint(cursor, false);

			cursor = temp_cursor;
		}

		if (op.flow == Flow::End || opcode_byte == 0x17)
			break;

	}
}
