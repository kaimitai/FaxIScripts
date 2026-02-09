#ifndef FM_BINARYMOD_H
#define FM_BINARYMOD_H

#include <map>
#include <string>
#include <utility>
#include <vector>

using byte = unsigned char;

namespace fm {

	struct BinaryMod {
		std::map<std::string, std::vector<byte>> sections;
		void append_number(std::size_t p_value, std::vector<byte>& p_bytes) const;
		std::size_t extract_number(const std::vector<byte>& p_bytes, std::size_t p_offset) const;
		void set_section(const std::string& p_cat, const std::vector<byte>& p_bytes);
		void set_section(const std::string& p_cat, const std::vector<std::size_t>& p_nums);

		BinaryMod(void) = default;
		BinaryMod(const std::vector<byte>& p_bin);
		BinaryMod(const std::vector<std::size_t>& p_ptrs, const std::vector<std::size_t>& p_refs,
			const std::vector<byte>& p_bytecode, const std::vector<byte>& p_cats);
		std::vector<byte> to_bytes(void) const;
		std::string to_string(void) const;

		std::vector<byte> get_ptr_table_bytes(std::size_t p_song_start) const;
	};

	namespace c {
		constexpr char BMOD_SEC_PTR[]{ "PTR" };
		constexpr char BMOD_SEC_MUS[]{ "MUS" };
		constexpr char BMOD_SEC_REF[]{ "REF" };
		constexpr char BMOD_SEC_CAT[]{ "CAT" };
	}

}

#endif
