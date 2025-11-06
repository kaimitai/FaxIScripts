#include "FaxString.h"
#include "fi_constants.h"

fi::FaxString::FaxString(const std::vector<byte>& p_rom, std::size_t& p_offset) {
	for (; p_offset < c::OFFSET_STRINGS + c::SIZE_STRINGS; ++p_offset) {

		if (p_rom.at(p_offset) == 0xff)
			break;
		else {
			unsigned char b{ p_rom.at(p_offset) };
			if (b == 0xfc)
				m_string += "{p}";
			else if (b == 0xfd)
				m_string.push_back(' ');
			else if (b == 0xfe)
				m_string += "{n}";
			else if (b == 0xfb)
				m_string += "{title}";
			else if (b == '\"')
				m_string += "{q}";
			else
				m_string.push_back(b);
		}

	}
}

fi::FaxString::FaxString(const std::string& p_string) :
	m_string{ p_string }
{
}

const std::string& fi::FaxString::get_string(void) const {
	return m_string;
}

std::vector<byte> fi::FaxString::to_bytes(void) const {
	std::vector<byte> result;

	for (char c : m_string)
		result.push_back(static_cast<byte>(c));

	result.push_back(0xff);

	return result;
}
