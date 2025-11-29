#include "AsmWriter.h"
#include "./../common/klib/Kfile.h"
#include "Opcode.h"
#include "fi_constants.h"

#include <format>
#include <map>

void fi::AsmWriter::generate_asm_file(const fe::Config& p_config,
	const std::string& p_filename,
	const std::map<std::size_t, fi::Instruction>& p_instructions,
	const std::vector<std::size_t>& p_entrypoints,
	const std::set<std::size_t>& p_jump_targets,
	const std::vector<fi::FaxString>& p_strings,
	const std::vector<fi::Shop>& p_shops,
	bool p_shop_comments) const {

	const auto& get_next_label = [](std::size_t p_offset,
		int p_lastentry, int& p_lastlabel,
		std::map<std::size_t, std::string>& p_labels) -> std::string {
			auto labiter{ p_labels.find(p_offset) };
			if (labiter != end(p_labels))
				return labiter->second;

			std::string tmp_label{ std::format("@iscript_{:03}_{:02}",
				p_lastentry, p_lastlabel++) };
			p_labels[p_offset] = tmp_label;
			return tmp_label;
		};

	// make copies from config instead of passing the obj down
	m_reserved_str_idx = p_config.vset_as_set(c::ID_STRING_RESERVED);
	m_def_textbox = p_config.bmap(c::ID_DEFINES_TEXTBOX);
	m_def_item = p_config.bmap(c::ID_DEFINES_ITEM);
	m_def_rank = p_config.bmap(c::ID_DEFINES_RANK);
	m_def_quest = p_config.bmap(c::ID_DEFINES_QUEST);

	// we will report on savings at the end for information
	std::set<std::string> l_used_strings;

	// make a map from offset to vector of entrypoints
	std::map<std::size_t, std::vector<std::size_t>> l_eps;
	for (std::size_t i{ 0 }; i < p_entrypoints.size(); ++i)
		l_eps[p_entrypoints[i]].push_back(i);

	std::string af{
		" ; IScript assembly file extracted by FaxIScripts v0.3\n ; https://github.com/kaimitai/FaxIScripts\n\n"
	};

	append_defines_section(af);
	append_strings_section(af, p_strings);
	append_shops_section(af, p_shops);

	// here we generate the actual assembly
	af += "\n[iscript]";

	int lastentry{ 0 }, lastlabel{ 0 };
	std::map<std::size_t, std::string> l_labels;

	// loop over all instructions and append to output
	for (const auto& kv : p_instructions) {
		const fi::Instruction& instr{ kv.second };
		std::size_t offset{ kv.first };

		auto ep{ l_eps.find(offset) };
		if (ep != end(l_eps)) {
			af += "\n";
			for (std::size_t i{ 0 }; i < ep->second.size(); ++i) {
				af += std::format(".entrypoint {}\n", ep->second[i]);
				lastentry = static_cast<int>(ep->second[i]);
				lastlabel = 0;
			}
		}

		if (p_jump_targets.find(offset) != end(p_jump_targets)) {
			af += std::format("{}:\n",
				get_next_label(offset, lastentry, lastlabel, l_labels));
		}

		if (ep != end(l_eps)) {
			af += std::format(".textbox {}\n", get_define(fi::ArgDomain::TextBox, instr.opcode_byte));
		}
		else {
			const auto& op{ fi::opcodes.find(instr.opcode_byte)->second };
			af += std::format("    {}", op.name);

			if (op.domain == fi::ArgDomain::TextString) {
				std::size_t str_ind{ static_cast<std::size_t>(instr.operand.value()) };

				if (str_ind == 0 ||
					str_ind > p_strings.size())
					af += std::format(" {} ; invalid string index", str_ind);
				else {
					std::string l_out_str{ p_strings.at(str_ind - 1).get_string() };
					af += std::format(" \"{}\"", l_out_str);
					l_used_strings.insert(l_out_str);
				}

			}
			else if (op.arg_type != fi::ArgType::None) {
				if (op.arg_type == fi::ArgType::Byte)
					af += std::format(" {}",
						get_define(op.domain, static_cast<byte>(instr.operand.value()))
					);
				else
					af += std::format(" {}",
						instr.operand.value()
					);
			}

			if (op.flow == fi::Flow::Jump)
				af += std::format(" {}",
					get_next_label(instr.jump_target.value(),
						lastentry, lastlabel, l_labels)
				);
			else if (op.flow == fi::Flow::Read) {
				if (instr.operand.value() >= p_shops.size()) {
					af += std::format(" ${:02x} ; ERROR: Invalid shop index", instr.operand.value());
				}
				else {
					if (p_shop_comments) {
						// we have a shop, add it to comments
						af += std::format(" {} ; {}", instr.operand.value(),
							serialize_shop_as_string(p_shops.at(instr.operand.value()))
						);
					}
					else {
						af += std::format(" {}", instr.operand.value());
					}
				}
			}

			af += "\n";
		}
	}

	// report on unused strings
	std::set<std::string> l_discarded_strs;
	for (std::size_t i{ 0 }; i < p_strings.size(); ++i) {
		if (m_reserved_str_idx.count(static_cast<int>(i + 1)) == 0 &&
			!l_used_strings.contains(p_strings[i].get_string()))
			l_discarded_strs.insert(p_strings[i].get_string());
	}

	if (!l_discarded_strs.empty()) {
		af += "\n ; Discarded strings (strings with no references)\n";
		for (const auto& str : l_discarded_strs) {
			af += std::format(" ; \"{}\"\n", str);
		}
	}

	klib::file::write_string_to_file(af, p_filename);
}

void fi::AsmWriter::append_defines_section(std::string& p_asm) const {
	std::string result{ "[defines]\n ; Item constants\n" };

	for (const auto& kv : m_def_item)
		result += std::format("define {} ${:02x}\n", kv.second, kv.first);

	result += "\n ; Rank constants\n";

	for (const auto& kv : m_def_rank)
		result += std::format("define {} ${:02x}\n", kv.second, kv.first);

	result += "\n ; Textbox constants\n";

	for (const auto& kv : m_def_textbox)
		result += std::format("define {} ${:02x}\n", kv.second, kv.first);

	result += "\n ; Quest constants\n";

	for (const auto& kv : m_def_quest)
		result += std::format("define {} ${:02x}\n", kv.second, kv.first);

	p_asm += result;
}

std::string fi::AsmWriter::get_define(fi::ArgDomain domain, byte arg) const {
	if (domain == fi::ArgDomain::Item)
		return get_define(m_def_item, arg);
	else if (domain == fi::ArgDomain::Quest)
		return get_define(m_def_quest, arg);
	else if (domain == fi::ArgDomain::Rank)
		return get_define(m_def_rank, arg);
	else if (domain == fi::ArgDomain::TextBox)
		return get_define(m_def_textbox, arg);
	else
		return std::format("{}", arg);
}

std::string fi::AsmWriter::get_define(const std::map<byte, std::string>& p_map, byte arg) const {
	const auto iter{ p_map.find(arg) };
	if (iter != end(p_map))
		return iter->second;
	else
		return std::format("${:02x}", arg);
}

void fi::AsmWriter::append_strings_section(std::string& p_asm,
	const std::vector<fi::FaxString>& p_strings) const {
	p_asm += std::format("\n ; string indexes referenced directly by game code\n ; change the contents - but not the indexes\n{}\n",
		c::SECTION_STRINGS);

	for (std::size_t i{ 0 }; i < p_strings.size(); ++i)
		if (m_reserved_str_idx.find(static_cast<int>(i + 1))
			!= end(m_reserved_str_idx))
			p_asm += std::format("{}: \"{}\"\n",
				i + 1,
				p_strings[i].get_string());
}

void fi::AsmWriter::append_shops_section(std::string& p_asm,
	const std::vector<fi::Shop>& p_shops) const {
	p_asm += "\n[shops]\n";

	for (std::size_t i{ 0 }; i < p_shops.size(); ++i) {
		std::string l_shop_string{ std::format("{}: {}", i,
			serialize_shop_as_string(p_shops[i])) };

		p_asm += l_shop_string + "\n";
	}
}

std::string fi::AsmWriter::serialize_shop_as_string(const fi::Shop& p_shop) const {
	std::string l_shop_string;

	for (std::size_t j{ 0 }; j < p_shop.m_entries.size(); ++j) {
		l_shop_string += std::format("({} {})",
			get_define(fi::ArgDomain::Item, p_shop.m_entries[j].m_item),
			p_shop.m_entries[j].m_price);
		if (j != p_shop.m_entries.size() - 1)
			l_shop_string.push_back(' ');
	}

	return l_shop_string;
}
