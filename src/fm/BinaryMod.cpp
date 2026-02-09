#include "BinaryMod.h"
#include <format>

std::vector<byte> fm::BinaryMod::to_bytes(void) const {
	std::vector<byte> result;

	for (const auto& kv : sections) {
		for (char c : kv.first)
			result.push_back(static_cast<byte>(c));
		append_number(kv.second.size(), result);
		result.insert(end(result), begin(kv.second), end(kv.second));
	}

	return result;
}

std::size_t fm::BinaryMod::extract_number(const std::vector<byte>& p_bytes,
	std::size_t p_offset) const {
	return static_cast<std::size_t>(p_bytes.at(p_offset)) +
		256 * static_cast<std::size_t>(p_bytes.at(p_offset + 1));
}

void fm::BinaryMod::set_section(const std::string& p_cat, const std::vector<byte>& p_bytes) {
	sections[p_cat] = p_bytes;
}

void fm::BinaryMod::set_section(const std::string& p_cat, const std::vector<std::size_t>& p_nums) {
	sections[p_cat].clear();
	for (std::size_t num : p_nums)
		append_number(num, sections[p_cat]);
}

fm::BinaryMod::BinaryMod(const std::vector<byte>& p_bin) {
	std::size_t cursor{ 0 };

	while (cursor < p_bin.size()) {
		std::string sec_name;
		for (std::size_t i{ 0 }; i < 3; ++i)
			sec_name.push_back(static_cast<char>(p_bin.at(cursor++)));
		std::size_t byte_size{};
	}
}

fm::BinaryMod::BinaryMod(const std::vector<std::size_t>& p_ptrs, const std::vector<std::size_t>& p_refs,
	const std::vector<byte>& p_bytecode, const std::vector<byte>& p_cats) {
	set_section(c::BMOD_SEC_PTR, p_ptrs);
	set_section(c::BMOD_SEC_REF, p_refs);
	set_section(c::BMOD_SEC_MUS, p_bytecode);
	set_section(c::BMOD_SEC_CAT, p_cats);
}

void fm::BinaryMod::append_number(std::size_t p_value,
	std::vector<byte>& p_bytes) const {
	p_bytes.push_back(static_cast<byte>(p_value % 256));
	p_bytes.push_back(static_cast<byte>(p_value / 256));
}

std::string fm::BinaryMod::to_string(void) const {
	std::string result;
	for (const auto& kv : sections)
		result += std::format("Section {}: {} bytes\n", kv.first, kv.second.size());
	return result;
}

std::vector<byte> fm::BinaryMod::get_ptr_table_bytes(std::size_t p_song_start) const {
	std::vector<byte> bank_addrs;
	for (std::size_t i{ 0 }; i < sections.at(c::BMOD_SEC_PTR).size(); i += 2) {
		std::size_t bank_addr{ extract_number(sections.at(c::BMOD_SEC_PTR), i) +
		p_song_start };
		append_number(bank_addr, bank_addrs);
	}
	return bank_addrs;
}
