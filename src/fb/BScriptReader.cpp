#include "BScriptReader.h"
#include "fb_constants.h"
#include "./../common/klib/Kfile.h"
#include "./../common/klib/Kstring.h"
#include <format>
#include <stdexcept>

fb::BScriptReader::BScriptReader(const fe::Config& p_config) :
	opcodes{ fb::parse_opcodes(p_config.bmap(c::ID_BSCRIPT_OPCODES)) },
	behavior_ops{ fb::parse_opcodes(p_config.bmap(c::ID_BSCRIPT_BEHAVIORS)) },
	bscript_ptr{ p_config.pointer(c::ID_BSCRIPT_PTR) },
	bscript_count{ p_config.constant(c::ID_SPRITE_COUNT) }
{
}

void fb::BScriptReader::read_asm_file(const std::string& p_filename,
	const fe::Config& p_config) {

	std::map<fb::SectionType, std::vector<std::string>> sections;

	auto l_lines{ klib::file::read_file_as_strings(p_filename) };
	fb::SectionType currentSection{ fb::SectionType::Defines };

	for (auto& line : l_lines) {
		line = klib::str::trim(klib::str::strip_comment(line));

		if (line == c::SECTION_DEFINES) {
			currentSection = fb::SectionType::Defines;
		}
		else if (line == c::SECTION_BSCRIPT) {
			currentSection = fb::SectionType::BScript;
		}
		else if (!line.empty())
			sections[currentSection].push_back(line);
	}

	// populate defines
	if (sections.contains(fb::SectionType::Defines))
		for (const std::string& s : sections[fb::SectionType::Defines]) {
			const auto def{ klib::str::parse_define(s) };
			defines.insert(std::make_pair(def.first, klib::str::parse_numeric(def.second)));
		}

	if (!sections.contains(fb::SectionType::BScript))
		throw std::runtime_error(std::format("File {} is missing section {}", p_filename, c::SECTION_BSCRIPT));

	// parse the asm code

	/*
		 this is where we statically link all jump targets and ptr table refs
		 we also bridge the gap between two safe regions if we overflow

		These terms need to be clearly separate:
		Offset = Actual byte address locations
		Instruction index = The instruction number a label or ptr references

		Here is the general linking strategy we will employ:

		Data we need:
		  A vector to hold our instructions
		  A map from entrypoint (ptr table index) to instruction index
		  A map from label to instruction index
		  A map from label to set of (instruction index, argument index) jumping to that label

		1) We will start with a byte offset of 0
		2) Parse asm line by line
		   - if entrypoint: store the instruction index it points to (current instr.size())
		   - if label: store the instruction index it points to (current instr.size())
		   - else: emit byte data

			 - make instruction
			   - if contains jump argument: store instruction index and arg index and in the label -> instrs map
			   - tentatively store the current byte offset directly in the instruction

		3) Once we have the tentative offsets for all instructions, check if we overflow
		   If we do, find the first End- or Unconditional Jump-instruction fully within
		   safe region 1. Take the first instruction after that one and calculate
		   the byte length from its offset to start of safe region 2.
		   Add this delta to the offsets for all instructions from this one onward.
		4) Once we have the final offsets for all instructions, loop over all labels
		   and ptr table entries, and assign them the byte address of the instruction
		   which indexes they already point to
		5) Once we know all label byte locations, do another pass over all instructions
		   and patch their jump target based on the final offset of the label they jump to
		6) Very final pass over all instructions and pointer table entries: Add the delta
		   between our local data-relative zero addr offset and bank zero addr offset

 */

	instructions.clear();
	ptr_table.clear();

	// make opcode mnemonic reverse lookup
	std::map<std::string, byte> op_mnemonics, bh_mnemonics;
	for (const auto& opc : opcodes)
		op_mnemonics.insert(std::make_pair(
			klib::str::to_lower(opc.second.mnemonic), opc.first
		));
	for (const auto& opc : behavior_ops)
		bh_mnemonics.insert(std::make_pair(
			klib::str::to_lower(opc.second.mnemonic), opc.first
		));

	// map string label to instruction index
	std::map<std::string, std::size_t> label_to_instr_idx;
	// map from ptr table index to instruction index
	std::map<std::size_t, std::size_t> ptr_to_instr_index;
	// map from label to all (instruction index, arg index) having it as target
	std::map<std::string, std::set<std::pair<size_t, std::size_t>>> jump_labels;
	// tentative byte offsets
	std::size_t offset{ 0 };

	for (std::size_t line_no{ 0 }; line_no < sections.at(fb::SectionType::BScript).size(); ++line_no) {
		std::string line{ sections.at(fb::SectionType::BScript).at(line_no) };
		auto rawargs{ klib::str::split_string(line, '=') };
		for (auto& str : rawargs)
			str = klib::str::trim(str);

		std::vector<std::string> args;
		for (const auto& str : rawargs) {
			auto splitargs{ klib::str::split_whitespace(str) };
			args.insert(end(args), begin(splitargs), end(splitargs));
		}

		if (is_label(line, args)) {
			label_to_instr_idx.insert(std::make_pair(get_label(args), instructions.size()));
		}
		else if (is_entrypoint(line, args))
			ptr_to_instr_index.insert(std::make_pair(get_entrypoint(args), instructions.size()));
		else {
			// we have an instruction - generate bytes
			std::string mnemonic{ klib::str::to_lower(args.at(0)) };
			bool real_opcode{ false };
			if (op_mnemonics.contains(mnemonic)) {
				real_opcode = true;
			}
			else if (!bh_mnemonics.contains(mnemonic))
				throw std::runtime_error(std::format("Unknown opcode '{}' on line '{}'", args[0], line));

			byte opcode_byte{ real_opcode ? op_mnemonics.at(mnemonic) : bh_mnemonics.at(mnemonic) };
			const auto& optmpl{ real_opcode ? opcodes.at(opcode_byte) : behavior_ops.at(opcode_byte) };
			const auto argmap{ get_argmap(line, args) };

			fb::BScriptInstruction instr(real_opcode ? opcode_byte : 0x00);
			if (!real_opcode)
				instr.behavior_byte = opcode_byte;

			const auto& argtmp{ optmpl.args };

			for (std::size_t i{ 0 }; i < argtmp.size(); ++i) {
				if (argtmp[i].domain == fb::ArgDomain::Addr ||
					argtmp[i].domain == fb::ArgDomain::TrueAddr ||
					argtmp[i].domain == fb::ArgDomain::FalseAddr) {
					jump_labels[get_label_name(line, argmap, argtmp[i].domain)].insert(
						std::make_pair(instructions.size(), i)
					);
					instr.operands.push_back(fb::ArgInstance(fb::ArgDataType::Word, 0));
				}
				else {
					int value{ resolve_value(line, argmap, argtmp[i].domain) };
					fb::ArgDataType valtype{ fb::ArgDataType::Byte };
					if (argtmp[i].domain == fb::ArgDomain::RAM)
						valtype = fb::ArgDataType::Word;
					std::size_t finalvalue{ 0 };

					if (valtype == fb::ArgDataType::Byte)
						finalvalue = static_cast<std::size_t>(static_cast<byte>(value));
					else
						finalvalue = static_cast<std::size_t>(value);

					instr.operands.push_back(fb::ArgInstance(valtype, finalvalue));
				}
			}

			instr.byte_offset = offset;
			instructions.push_back(instr);
			offset += instr.size();
		}
	}

	// patch all unresolved jump targets
	// and make them point to the correct instruction index
	for (const auto& jt : jump_labels) {
		if (!label_to_instr_idx.contains(jt.first))
			throw std::runtime_error(std::format("Undefined label {}", jt.first));

		std::size_t jumptoinstridx{ label_to_instr_idx.at(jt.first) };
		for (const auto& jumpargs : jt.second) {
			std::size_t instr_no{ jumpargs.first };
			std::size_t arg_no{ jumpargs.second };

			instructions.at(instr_no).operands.at(arg_no).data_value = jumptoinstridx;
		}
	}

	// TODO: Inser overflow-bridge if needed


	// all entry points and jump targets point to the correct instruct idx
	// and all instructions have the correct byte offset
	// we now update with the correct values
	const std::size_t PTR_DELTA{ bscript_ptr.first + 2 * bscript_count - bscript_ptr.second };

	for (auto& instr : instructions)
		instr.byte_offset.value() += PTR_DELTA;

	for (const auto& kv : jump_labels) {
		for (const auto& jumpargs : kv.second) {
			std::size_t instr_no{ jumpargs.first };
			std::size_t arg_no{ jumpargs.second };
			auto& instrarg{ instructions.at(instr_no).operands.at(arg_no) };

			instrarg.data_value = instructions.at(instrarg.data_value.value()).byte_offset.value();
		}
	}

	for (std::size_t i{ 0 }; i < bscript_count; ++i) {
		if (!ptr_to_instr_index.contains(i))
			throw std::runtime_error(std::format("Entrypoint {} not defined", i));
		ptr_table.push_back(instructions.at(ptr_to_instr_index.at(i)).byte_offset.value());
	}
}

std::string fb::BScriptReader::get_label_name(const std::string& p_asm, const std::map<fb::ArgDomain, std::string>& p_argmap,
	fb::ArgDomain domain) const {
	if (!p_argmap.contains(domain) || p_argmap.at(domain).size() < 2)
		throw std::runtime_error(std::format("Missing label target on line '{}'", p_asm));
	else
		return p_argmap.at(domain).substr(1);
}

int fb::BScriptReader::resolve_value(const std::string& p_asm, const std::map<fb::ArgDomain, std::string>& p_argmap,
	fb::ArgDomain domain) const {
	if (!p_argmap.contains(domain))
		return get_default_value(p_asm, domain);
	else {
		const auto& argval{ p_argmap.at(domain) };
		if (defines.contains(argval))
			return defines.at(argval);
		else
			return klib::str::parse_numeric(argval);
	}
}

int fb::BScriptReader::get_default_value(const std::string& p_asm, fb::ArgDomain domain) const {
	if (domain == fb::ArgDomain::Zero)
		return 0;
	else
		throw std::runtime_error(std::format("Missing value and no default value available on line '{}'", p_asm));
}

bool fb::BScriptReader::is_label(const std::string& p_asm, const std::vector<std::string>& p_line) const {
	if (p_line.at(0).at(0) == '@') {
		if (p_line.size() == 1 && p_line.at(0).back() == ':') {
			return true;
		}
		else {
			throw std::runtime_error(std::format("Invalid label definition on line '{}'", p_asm));
		}
	}
	else
		return false;
}

std::string fb::BScriptReader::get_label(const std::vector<std::string>& p_line) const {
	std::string label{ p_line.at(0).substr(1) };
	label.pop_back();
	return label;
}

bool fb::BScriptReader::is_entrypoint(const std::string& p_asm, const std::vector<std::string>& p_line) const {
	if (klib::str::str_equals_icase(p_line.at(0), c::DIRECTIVE_ENTRYPOINT)) {
		if (p_line.size() == 2) {
			return true;
		}
		else {
			throw std::runtime_error(std::format("Invalid label definition on line '{}'", p_asm));
		}
	}
	else
		return false;
}

std::size_t fb::BScriptReader::get_entrypoint(const std::vector<std::string>& p_line) const {
	return klib::str::parse_numeric(p_line.at(1));
}

std::map<fb::ArgDomain, std::string> fb::BScriptReader::get_argmap(const std::string& p_asm,
	const std::vector<std::string>& p_line) const {
	std::map<fb::ArgDomain, std::string> result;

	if (p_line.size() % 2 == 0)
		throw std::runtime_error(std::format("Can not parse line {}", p_asm));

	for (std::size_t i{ 1 }; i < p_line.size(); i += 2) {
		if (!c::STR_ARGDOMAIN.contains(klib::str::to_lower(p_line[i])))
			throw std::runtime_error(std::format("Unknown argument type {} on line {}", p_line[i], p_asm));
		fb::ArgDomain domain{ c::STR_ARGDOMAIN.at(klib::str::to_lower(p_line[i])) };

		result.insert(std::make_pair(domain, p_line[i + 1]));
	}

	return result;
}

std::vector<byte> fb::BScriptReader::to_bytes(void) const {
	std::vector<byte> result;

	for (const std::size_t ep : ptr_table) {
		result.push_back(static_cast<byte>(ep % 256));
		result.push_back(static_cast<byte>(ep / 256));
	}

	for (const auto& instr : instructions) {
		const auto bytes{ instr.get_bytes() };
		result.insert(end(result), begin(bytes), end(bytes));
	}

	return result;
}
