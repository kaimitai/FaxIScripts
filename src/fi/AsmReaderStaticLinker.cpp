#include "AsmReader.h"
#include "fi_constants.h"
#include <format>
#include <map>
#include <set>
#include <stdexcept>

using byte = unsigned char;

/*
 this is where we statically link all jump targets and ptr table refs
 we also bridge the gap between two safe regions if we overflow

There terms need to be clearly separate:
Offset = Actual byte address locations
Instruction index = The instruction number a label or ptr references

Here is the general linking strategy we will employ:

Data we need:
  A vector to hold our instructions
  A map from byte offset to instruction index
  A map from entrypoint (ptr table index) to instruction index
  A map from label to instruction index
  A map from label to set of instruction indexes jumping to that label

1) We will start with a byte offset of 0
2) We lay down all shops and record their byte offsets in a map (shop index -> byte offset)
3) We set our start byte offset to be the end of shop data

4) Parse asm line by line
   - if entrypoint: store the instruction index it points to (current instr.size())
   - if label: store the instruction index it points to (current instr.size())
   - if textbox/opcode: these emit byte data

	 - make instruction
	   - if jump instruction: store its index in the label -> instrs map
	   - if shop: lay down the read address already as it is known before we start

5) Once we have the tentative offsets for all instructions, check if we overflow
   If we do, find the first End- or Unconditional Jump-instruction within
   safe region 1. Take the first instruction after that one and calculate
   the byte length from its offset to start of safe region 2.
   Add this delta to the offsets for all instructions from this one onward.
6) Once we have the final offsets for all instructions, loop over all labels
   and ptr table entries, and assign them the byte address of the instruction
   which indexes they already point to
7) Once we know all label byte locations, do another pass over all instructions
   And patch their jump target based on the final offset of the label they jump to
8) Very final pass over all instructions and pointer table entries: Add the delta
   between our local offset and bank offset

 */
void fi::AsmReader::parse_section_iscript(void) {
	// start by calculating the shop offsets and code offset
	// we are not emitting any bytes in the first pass
	// we will do this with 0-relative offsets in the first pass
	// and recalculate later

	// make an opcode mnemonic reverse lookup
	std::map<std::string, byte> mnemonics;
	for (const auto& opc : fi::opcodes) {
		mnemonics.insert(std::make_pair(
			to_lower(opc.second.name), opc.first
		));
	}

	// map from shop index to byte offsets
	// we can patch these as we go since we put them first
	// no need to touch any offsets here before the very final pass
	std::map<std::size_t, std::size_t> l_shop_ptrs;

	std::size_t offset{ 0 };
	for (const auto& kv : m_shops) {
		l_shop_ptrs.insert(std::make_pair(kv.first, offset));
		offset += kv.second.byte_size();
	}

	m_ptr_table.clear();

	// instruction vector
	m_instructions.clear();
	// map string label to instruction index
	std::map<std::string, std::size_t> label_to_instr_idx;
	// map from ptr table index to instruction index
	std::map<std::size_t, std::size_t> ptr_to_instr_index;
	// map from label to all instruction indexes having it as target
	std::map<std::string, std::set<std::size_t>> jump_labels;
	// map of instruction byte offset to its index
	// std::map<std::size_t, std::size_t> byte_offset_to_instruction_idx;

	// and so it begins...
	for (std::string line : m_sections.at(fi::SectionType::IScript)) {

		// if label - extract and store
		if (contains_label(line)) {
			std::string label{ extract_label(line) };
			auto iter = label_to_instr_idx.find(label);
			if (iter == end(label_to_instr_idx))
				label_to_instr_idx.insert(std::make_pair(label, m_instructions.size()));
			else
				throw std::runtime_error("Multiple definitions for " + label);
		}
		// if entrypoint - extract entrypoint no and update ptr table map
		else if (contains_entrypoint(line)) {
			auto entry_no{ extract_entrypoint(line) };
			if (ptr_to_instr_index.find(entry_no) == end(ptr_to_instr_index))
				ptr_to_instr_index[entry_no] = m_instructions.size();
			else
				throw std::runtime_error(std::format("Multiple definitions for entrypoint {}", entry_no));
		}
		// if textbox - grab the byte, make a pseudo-instruction and advance
		else if (contains_textbox(line)) {
			byte textbox_no{ extract_textbox(line) };

			// byte_offset_to_instruction_idx[offset] = m_instructions.size();
			m_instructions.push_back(fi::Instruction(
				fi::Instruction_type::Directive,
				textbox_no,
				1,
				std::nullopt,
				std::nullopt,
				offset++
			));
		}
		// else it must be an opcode
		else {
			// can't be empty as long as we did our pre-processing correctly
			auto tokens{ split_whitespace(line) };

			const std::string& mnemo{ to_lower(tokens[0]) };
			if (!mnemonics.contains(mnemo)) {
				throw std::runtime_error("Unknown opcode: " + mnemo);
			}

			byte opcode_byte = mnemonics[mnemo];
			std::optional<uint16_t> arg;
			std::optional<uint16_t> target_address;

			const fi::Opcode& op = fi::opcodes.at(opcode_byte);

			// let's validate the params first
			// TODO: Make member of Opcode-class
			std::size_t expected_token_count{ 1 }; // the opcode itself
			if (op.arg_type != fi::ArgType::None)
				expected_token_count += 1;
			if (op.flow == fi::Flow::Jump ||
				op.flow == fi::Flow::Read)
				expected_token_count += 1;

			if (expected_token_count != tokens.size())
				throw std::runtime_error(
					std::format("Opcode '{}' expects {} arguments, got {} (@line: \"{}\")",
						mnemo, expected_token_count - 1, tokens.size() - 1, line)
				);

			// based on the template we calculate everything
			// and generate the instruction. we already know shop
			// offsets but not necessarily labels
			std::size_t current_token{ 1 };

			if (op.arg_type != fi::ArgType::None)
				arg = static_cast<uint16_t>(resolve_token(tokens.at(current_token++)));

			// lay down the shop byte offset immediately
			if (op.flow == fi::Flow::Read)
				target_address = static_cast<uint16_t>(
					l_shop_ptrs.at(resolve_token(tokens.at(current_token++)))
					);
			else if (op.flow == fi::Flow::Jump) {
				// labels: defer until we have all instruction offsets
				auto label{ tokens.at(current_token++) };
				jump_labels[label].insert(m_instructions.size());
			}

			// finally emit the instruction
			// byte_offset_to_instruction_idx[offset] = m_instructions.size();
			m_instructions.push_back(fi::Instruction(
				fi::Instruction_type::OpCode,
				opcode_byte,
				op.size(),
				arg,
				target_address,
				offset
			));
			offset += op.size();
		}
	}

	// first pass done - we now have tentative byte offsets for all instructions
	// we have labels and entrypoint idx to instruction index

	// verify that we can populate each ptr table entry when we need to
	for (std::size_t i{ 0 }; i < c::ISCRIPT_COUNT; ++i)
		if (ptr_to_instr_index.find(i) == end(ptr_to_instr_index))
			throw std::runtime_error(std::format("Entrypoint {} not defined. ", i));

	// we don't know if our instruction byte offsets are correct yet, if we
	// overflow we need to split, so let us check
	// store instruction index and byte offset if it exists
	std::optional<std::size_t> first_unsafe_inst_idx;

	for (std::size_t i{ 0 }; i < m_instructions.size(); ++i)
		if (m_instructions[i].byte_offset.value() + m_instructions[i].size >
			c::ISCRIPT_DATA_SIZE_REGION_1) {
			first_unsafe_inst_idx = i;
			break;
		}

	std::optional<std::size_t> idx_after_last_safe_stream_end;

	if (first_unsafe_inst_idx.has_value()) {
		for (std::size_t i{ first_unsafe_inst_idx.value() - 1 };
			i > 0; --i)
			if (m_instructions[i].byte_offset.value() + m_instructions[i].size
				<= c::ISCRIPT_DATA_SIZE_REGION_1
				&& m_instructions[i].type != fi::Instruction_type::Directive
				&& fi::opcodes.at(m_instructions[i].opcode_byte).ends_stream) {
				idx_after_last_safe_stream_end = i + 1;
				break;
			}
	}

	// handle bridge to region 2
	// we now have the index and byte offset of the first unsafe instruction
	// we need to relocate it to region 2 by adding a certain delta to it,
	// and therefore the same delta to all 
	if (idx_after_last_safe_stream_end.has_value()) {
		// calculate the relocation delta
		std::size_t relocation_delta{ c::ISCRIPT_DATA_OFFSET_REGION_2
		- m_instructions.at(idx_after_last_safe_stream_end.value()).byte_offset.value() };

		for (std::size_t i{ idx_after_last_safe_stream_end.value() };
			i < m_instructions.size(); ++i)
			m_instructions[i].byte_offset.value() += relocation_delta;

	}

	// here all our final instruction offsets are known
	// intermediate pass - patch all previously unresolved labels
	for (const auto& kv : jump_labels) {
		auto iter{ label_to_instr_idx.find(kv.first) };
		if (iter == end(label_to_instr_idx))
			throw std::runtime_error("Unresolved label: " + kv.first);
		else {
			for (std::size_t instr_no : kv.second)
				m_instructions[instr_no].jump_target =
				m_instructions.at(iter->second).byte_offset.value();
		}
	}

	// our offsets are all relative to the start of the script data immediately
	// following the ptr table; make all our ptrs bank relative
	const uint16_t PTR_DELTA{ static_cast<uint16_t>(c::ISCRIPT_ADDR_LO + 2 * c::ISCRIPT_COUNT
	- c::ISCRIPT_PTR_ZERO_ADDR) };
	for (std::size_t ins{ 0 }; ins < m_instructions.size(); ++ins) {
		if (m_instructions[ins].jump_target.has_value()) {
			m_instructions[ins].jump_target = m_instructions[ins].jump_target.value() + PTR_DELTA;
		}

		m_instructions[ins].byte_offset.value() += PTR_DELTA;
	}

	// finally calculate the ptr table
	for (const auto& kv : ptr_to_instr_index)
		m_ptr_table[kv.first] = m_instructions[kv.second].byte_offset.value();
}

std::pair<std::vector<byte>, std::vector<byte>>
fi::AsmReader::get_script_bytes(void) const {
	std::vector<byte> region_1, region_2;

	const uint16_t PTR_DELTA{ static_cast<uint16_t>(c::ISCRIPT_ADDR_LO + 2 * c::ISCRIPT_COUNT
- c::ISCRIPT_PTR_ZERO_ADDR) };

	// lo bytes
	for (std::size_t i{ 0 }; i < c::ISCRIPT_COUNT; ++i)
		region_1.push_back(static_cast<byte>(m_ptr_table.at(i) % 256));
	// hi bytes
	for (std::size_t i{ 0 }; i < c::ISCRIPT_COUNT; ++i)
		region_1.push_back(static_cast<byte>(m_ptr_table.at(i) / 256));

	for (const auto& kv : m_shops) {
		auto shopbytes{ kv.second.to_bytes() };
		region_1.insert(end(region_1), begin(shopbytes), end(shopbytes));
	}

	for (const auto& instr : m_instructions) {
		auto instrbytes{ instr.get_bytes() };

		if (instr.byte_offset < PTR_DELTA + c::ISCRIPT_DATA_SIZE_REGION_1)
			region_1.insert(end(region_1), begin(instrbytes), end(instrbytes));
		else
			region_2.insert(end(region_2), begin(instrbytes), end(instrbytes));
	}

	return std::make_pair(region_1, region_2);
}
