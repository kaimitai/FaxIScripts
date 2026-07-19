#ifndef FH_CONSTANTS_H
#define FH_CONSTANTS_H

#include <cstddef>

using byte = unsigned char;
using word = uint16_t;

namespace fh {

	namespace ROM {
		// bank 12 addresses
		constexpr word IScripts_JumpTable_Ref_U{ 0x8273 }; // us, jp
		constexpr word IScripts_JumpTable_Ref_L{ 0x8277 }; // us, jp
		constexpr word GameLoop_RunScreenEventHandlers{ 0xef4b };

		// bank 15 addresses
		constexpr word Game_Init_JSR_Game_InitMMCAndBank{ 0xc954 };
		constexpr word Game_Init_JSR_Game_InitScreenAndMusic{ Game_Init_JSR_Game_InitMMCAndBank + 3 };
		constexpr word Game_InitMMCAndBank{ 0xcbbf };
		constexpr word Area_SetBlocks{ 0xd7c5 };
		constexpr word GameLoop_RunScreenEventHandlers_CMP_06{ 0xef55 };
		constexpr word EventHandlerPathToMascon{ 0xef69 };
		constexpr word EventHandlerBossScreen{ 0xef9e };
		constexpr word EventHandlerFinalBoss{ 0xefd4 };
		constexpr word GameLoop_RunScreenEventHandlers_LDA_EventTable{ 0xef5a };
	}

	namespace RAM {
		constexpr byte ZP_Temp07{ 0x07 };
		constexpr byte ZP_Temp08{ 0x08 };
		constexpr byte ZP_CurrentWorld{ 0x24 };
		constexpr byte ZP_CurrentScreen{ 0x63 };
		constexpr byte ZP_e2{ 0xe2 };
		constexpr byte ZP_e3{ 0xe3 };
		constexpr byte ZP_e4{ 0xe4 };
		constexpr byte ZP_e5{ 0xe5 };
		constexpr byte ZP_e6{ 0xe6 };
		constexpr byte ZP_e7{ 0xe7 };
		constexpr byte ZP_Temp_Int24_L{ 0xec };
		constexpr byte ZP_Temp_Int24_M{ 0xed };
		constexpr byte ZP_Temp_Int24_U{ 0xee };

		constexpr word CurrentROMBank{ 0x0100 };
		constexpr word Flags{ 0x0101 };
		constexpr word QuestFlags{ 0x042d };
		constexpr word CurrentScreen_SpecialEventID{ 0x042e };
		constexpr word CurrentStage{ 0x0435 };
		constexpr word PlayerIsDead{ 0x0438 };
		constexpr word ScreenBuffer{ 0x0600 };
	}

	namespace c {
		constexpr byte FlagsByteCount{ 0x1f };

		constexpr char ID_ROM_ISCRIPTS_LOADBYTE[]{ "rom_iscripts_loadbyte" };
		constexpr char ID_ROM_ISCRIPTS_SKIPADDRANDINVOKE[]{ "rom_iscripts_skipaddrandinvoke" };
		constexpr char ID_ROM_ISCRIPTS_JUMPTONEXTADDR[]{ "rom_iscripts_jumptonextaddr" };
		constexpr char ID_ROM_ISCRIPTS_INVOKENEXTACTION[]{ "rom_iscripts_invokenextaction" };
		constexpr char ID_ROM_MMC1_UPDATEROMBANK[]{ "rom_mmc1_updaterombank" };
		constexpr char ID_ROM_PLAYER_UPDATEEXPERIENCE[]{ "rom_player_updateexperience" };

		constexpr char ID_HACK_CLEAR_PERSISTENT_FLAGS[]{ "hack_clear_persistent_flags" };
		constexpr char ID_TM_CHANGE_BANK[]{ "hack_tm_change_bank" };
		constexpr char ID_TM_CHANGE_CPU_ADDR[]{ "hack_tm_change_cpu_addr" };
		constexpr char ID_TM_CHANGE_HANDLER_CPU_ADDR[]{ "hack_tm_handler_cpu_addr" };

		constexpr char ID_FLAGS_WRAM_TO_SRAM[]{ "flags_wram_to_sram" };

		constexpr char ID_ISCRIPT_RG2_START[]{ "iscript_data_rg2_start" };

		constexpr byte VANILLA_SCRIPT_COUNT{ 24 };
	}
}

#endif
