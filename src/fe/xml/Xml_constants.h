#ifndef FE_XML_CONSTANTS_H
#define FE_XML_CONSTANTS_H

namespace fe {
	namespace xml {
		namespace c {

			constexpr char TAG_CONFIG_ROOT[]{ "eoe_config" };

			constexpr char TAG_REGIONS[]{ "regions" };
			constexpr char TAG_REGION[]{ "region" };
			constexpr char TAG_SIGNATURE[]{ "signature" };
			constexpr char TAG_CONSTANTS[]{ "consts" };
			constexpr char TAG_CONSTANT[]{ "const" };

			constexpr char TAG_POINTERS[]{ "pointers" };
			constexpr char TAG_POINTER[]{ "pointer" };
			constexpr char TAG_SETS[]{ "sets" };
			constexpr char TAG_SET[]{ "set" };
			constexpr char TAG_BYTE_TO_STR_MAPS[]{ "byte_to_string_maps" };
			constexpr char TAG_BYTE_TO_STR_MAP[]{ "byte_to_string_map" };
			constexpr char TAG_ENTRY[]{ "entry" };

			constexpr char ATTR_NAME[]{ "name" };
			constexpr char ATTR_FILE_SIZE[]{ "file_size" };
			constexpr char ATTR_NO[]{ "no" };
			constexpr char ATTR_OFFSET[]{ "offset" };
			constexpr char ATTR_VALUES[]{ "values" };
			constexpr char ATTR_VALUE[]{ "value" };
			constexpr char ATTR_ZERO_ADDR[]{ "zero_addr" };
			constexpr char ATTR_BYTE[]{ "byte" };
			constexpr char ATTR_STRING[]{ "str" };
		}
	}
}

#endif
