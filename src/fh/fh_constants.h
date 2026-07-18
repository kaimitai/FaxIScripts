#ifndef FH_CONSTANTS_H
#define FH_CONSTANTS_H

#include <cstddef>

using byte = unsigned char;
using word = uint16_t;

namespace fh {

	namespace ROM {
		// bank 12 addresses
		const word IScripts_JumpTable_Ref_U{ 0x8273 }; // us, jp
		const word IScripts_JumpTable_Ref_L{ 0x8277 }; // us, jp
		const word GameLoop_RunScreenEventHandlers{ 0xef4b };

		// bank 15 addresses
		const word Game_Init_JSR_Game_InitMMCAndBank{ 0xc954 };
		const word Game_Init_JSR_Game_InitScreenAndMusic{ Game_Init_JSR_Game_InitMMCAndBank + 3 };
		const word Game_InitMMCAndBank{ 0xcbbf };
	}


	namespace RAM {
		constexpr byte ZP_Temp07{ 0x07 };

		constexpr word Flags{ 0x0101 };
		constexpr word CurrentScreen_SpecialEventID{ 0x042e };
	}

	namespace c {
		constexpr byte FlagsByteCount{ 0x1f };

		constexpr char ID_ROM_ISCRIPTS_LOADBYTE[]{ "rom_iscripts_loadbyte" };
		constexpr char ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE[]{ "rom_iscripts_skipaddrandinvoke" };
		constexpr char ID_ROM_ISCRIPTS_JUMPTONEXTADDR[]{ "rom_iscripts_jumptonextaddr" };
		constexpr char ID_ROM_ISCRIPTS_INVOKENEXTACTION[]{ "rom_iscripts_invokenextaction" };

		constexpr char ID_HACK_CLEAR_PERSISTENT_FLAGS[]{ "hack_clear_persistent_flags" };

		constexpr char ID_FLAGS_WRAM_TO_SRAM[]{ "flags_wram_to_sram" };

		constexpr char ID_ISCRIPT_RG2_START[]{ "iscript_data_rg2_start" };

		constexpr byte VANILLA_SCRIPT_COUNT{ 24 };
	}
}

#endif
