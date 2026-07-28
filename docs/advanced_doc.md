# Advanced Modding

<hr>

[Echoes of Eolis](https://github.com/kaimitai/faxedit) and [FaxIScripts](https://github.com/kaimitai/FaxIScripts) include an optional set of advanced features for creating ROM hacks that go beyond the capabilities of the original Faxanadu engine.

These features allow projects to:

- Extend the original script engine with custom opcodes
- Create persistent world state using up to 248 extended flags
- Build reusable script subroutines using `JSR` and `Return`
- Implement new gameplay mechanics entirely through scripts
- Create conditional tilemap changes that permanently alter the world
- Inject runtime code automatically without manually relocating assembly

The advanced systems are completely optional. Projects that do not enable them produce behavior identical to the original game. No knowledge of 6502 assembly is required to use the built-in runtime library. Most projects simply enable the desired opcodes and use them from ordinary FaxIScripts assembly.

<hr>

## Table of Contents

- [Getting Started](#getting-started)
- [Learning the System](#learning-the-system)
- [Extended Script System](#extended-script-system)
  - [Configuring Runtime Opcodes](#configuring-runtime-opcodes)
  - [Assembly Process](#assembly-process)
  - [Compatibility](#compatibility)
  - [Runtime Helpers](#runtime-helpers)
  - [Custom Script Opcode Library Reference](#custom-script-opcode-library-reference)
  - [Example: Keep all Doors in World 1 (Trunk) Unlocked](#example-keep-all-doors-in-world-1-trunk-unlocked)
- [Tilemap Change System](#tilemap-change-system)
  - [Overview](#overview)
  - [How it Works](#how-it-works)
  - [Assembly Syntax](#assembly-syntax)
  - [Typical Workflow](#typical-workflow)
  - [Example: Persisting Mattock-breakable Blocks](#example-persisting-mattock-breakable-blocks)
  - [Configuration](#configuration)
  - [Limitations](#limitations)
- [Bank 15 Hack Injection Points](#bank-15-hack-injection-points)
- [RAM Used by Custom Hacks](#ram-used-by-custom-hacks)

<hr>

## Getting Started

Echoes of Eolis and FaxIScripts load their default configuration from ```eoe_config.xml```. This file should never be modified. It is part of the application and may be replaced when upgrading to a newer version.

Instead, project-specific configuration belongs in ```eoe_config_override.xml```. Any settings present in the override file replace the corresponding defaults in ```eoe_config.xml```, while all omitted settings continue to use the built-in values.

Many projects never need an override file. However, the advanced scripting system uses it to define the project's custom script language, making an override file the recommended starting point for the features described in this document.

A skeleton override file, ```eoe_config_override-advanced.xml```, is included in the ```util``` directory. Copy it to the directory containing the faxedit and faxiscripts executables, rename it to ```eoe_config_override.xml```, and modify it as needed.

When the advanced scripting features are used, ```eoe_config_override.xml``` does more than configure the tools - it also defines the project's scripting language. The iscript_opcodes map specifies which opcodes exist, their mnemonics, argument types, and any runtime implementations they use. **The XML is no longer just configuration - it is the specification of the project's scripting language.**

Both FaxIScripts and Echoes of Eolis read this information. FaxIScripts uses it when assembling scripts and disassembling ROM files, while Echoes of Eolis uses the same definitions to disassemble and display scripts correctly in the editor.

The custom scripting opcodes are defined in ```iscript_opcodes``` in the configuration xml. Opcodes 0-23 define the vanilla scripting language, and should typically be left as-is. Opcodes 24 and up will be configured based on a project's needs. The opcode numbers themselves are arbitrary. Any unused values from 24 upward may be used, as long as the list is dense. For example, if 24 and 26 are defined, then 25 must also be defined.

Most advanced projects only need to:

1. Enable the runtime opcode implementations they intend to use by editing or adding entries to ```iscript_opcodes``` that specify an ```Impl``` value. The ```Impl``` selects one of the built-in runtime implementations provided by FaxIScripts.
2. Use the new opcodes in your assembly source files
3. Assemble their script file as usual

FaxIScripts automatically determines which runtime helpers are required, assembles them into free space, rebuilds the script dispatch table when necessary, and reports the amount of ROM space consumed.

No manual relocation, address calculation, or runtime installation is required by the script author.

<hr>

## Learning the System

The advanced features are designed to be combined, and there are often several ways to solve the same problem. If you are new to the runtime library, it is recommended to create a small "toy" project before modifying a larger ROM.

For example, try creating a script that:

- Sets an extended flag
- Checks that flag on a later visit
- Changes a few metatiles on a screen
- Adds a custom script opcode such as IfWorld or IfScreen

Building a small experiment like this makes it much easier to understand how the runtime library, script engine, and tilemap change subsystem interact before using them in a larger project.

Once you are comfortable with the workflow, the same techniques can be combined to build persistent doors, environmental puzzles, quests, scripted events, and entirely new gameplay mechanics.

<hr>

## Extended Script System

FaxIScripts extends the original Faxanadu scripting engine with an optional runtime library that allows new script opcodes to be added without modifying the original game engine. The system is fully data-driven and is designed to preserve vanilla behavior unless extensions are explicitly enabled.

Unlike the original game, where the script opcode table is fixed, FaxIScripts generates a new opcode dispatch table during assembly. Vanilla opcode implementations are preserved, while any enabled runtime implementations are assembled into free space and appended to the dispatch table automatically.

The runtime library is assembled on demand. Helper routines and opcode implementations are only emitted if they are required by the opcodes configured for the current project. As a result, different projects may produce different runtime layouts while remaining fully compatible with the same scripting system.

The flow of the advanced features is roughly as follows.

```text
      Assembly (.asm)
             │
             ▼
       FaxIScripts
             │
             ▼
       Modified ROM
             │
             ▼
  Echoes of Eolis
     │
     ├── Edit maps
     ├── Assign screen handlers
     └── Save
             │
             ▼
            Test
```


#### Configuring Runtime Opcodes

Runtime opcode implementations are configured through ```eoe_config_override.xml```.

Only opcodes that specify an Impl value are implemented by the runtime library. All remaining opcodes continue to use the game's original implementations.

### Assembly Process

When a script file is assembled, FaxIScripts performs the following steps:

- Reads the configured opcode definitions.
- Determines which runtime implementations and helper routines are required.
- Assembles the runtime library into the configured free space.
- Rebuilds the script opcode jump table.
- Redirects the game's dispatcher to the newly generated table.

This process is completely automatic. Script authors only need to use the desired opcodes in their assembly source; the required runtime support is generated automatically.

### Compatibility

The extended script system preserves the original Faxanadu scripting engine. Existing scripts continue to function without modification, and projects that do not enable runtime opcode implementations produce behavior identical to the original game.

### Runtime Helpers

Several runtime opcodes share common helper routines. These helpers are emitted only when at least one opcode requires them.

Examples include:

- Flag operand decoding
- Quest flag operand decoding
- Generic value comparison used by conditional opcodes
- Player metatile position normalization

This minimizes ROM usage while allowing new opcode implementations to reuse common functionality.

### Custom script opcode library reference

| Impl | Arguments | Description | Example |
|------|-----------|-------------|---------|
| SetFlag | Byte | Sets an extended flag (0-247) | SetFlag 110 ; sets flag 110 |
| ClearFlag | Byte | Clears an extended flag (0-247) | ClearFlag 110 ; clears flag 110 |
| IfFlag | Byte, Label | Jumps if the extended flag is set | IfFlag 110 @target ; jumps to @target if flag 110 is set |
| SetQuestFlag | Byte | Sets a vanilla quest flag (0-7) | SetQuestFlag 3 ; sets  quest flag 3 |
| ClearQuestFlag | Byte | Clears a vanilla quest flag (0-7) | ClearQuestFlag 3 ; clears quest flag 3 |
| IfQuestFlag | Byte, Label | Jumps if the vanilla quest flag is set | IfQuestFlag 3 @target ; jumps to @target if quest flag 3 is set |
| JSR | Label | Jumps to the label and stores current address (jump to subroutine) | JSR @sub ; jump to @sub and prepares a return |
| Return | None | Returns execution to after the last JSR | Return ; returns to the instruction after the last JSR |
| IfWorld | Byte, Label | Jumps if the current world equals the argument | IfWorld 1 @is_trunk ; jumps to @is_trunk if current world is 1 |
| IfScreen | Byte, Label | Jumps if the current screen equals the argument | IfScreen 3 @is_screen_3 ; jumps to @is_screen_3 if current screen is 3 |
| IfStage | Byte, Label | Jumps if the current stage equals the argument | IfStage 2 @is_stage_2 ; jumps to @is_stage_2 if current stage is 2 |
| IfYX | Byte, Label | Jumps if the player's normalized metatile position equals the argument | IfYX $a4 @pos_4_10 ; jumps to @pos_4_10 if player position is (x=4, y=10) |
| IfDoorYX | Byte, Label | Jumps if the currently selected door has the specified packed YX coordinate | IfDoorYX $a4 @door_4_10 ; jumps to @door_4_10 if current door position is (x=4, y=10) |
| ForceDoor | None | Overrides a failed door requirement, allowing the current door transition to proceed | |
| RunScreenHandler | None | Executes the custom screen event handler (used by the tilemap change subsystem) | |
| GetXP | Short (0-65,535) | Gives player xp; note that "next rank" can only increase by 1 each time XP is given | GetXP 100 ; player gets 100xp |
| Die | None | Kills the player when the script ends | |

**Note**: Runtime implementations are intended for use with custom opcodes. Vanilla opcodes (0x00–0x17) continue to use the game's original implementations unless explicitly remapped. This preserves compatibility with existing scripts while allowing projects to extend the scripting language with new functionality.

IfYX and IfDoorYX both take an argument on the form $yx, a hex value where the high nibble is y-position and low nibble is x-position in metatile space.

The difference between them is that IfYX considers the player's current position, and IfDoorYX considers the position of the door being interacted with. IfDoorYX should only be used within scripts that are called via in-game door logic.

## Example: Keep all doors in world 1 (Trunk) unlocked

We will need the following custom opcodes in ```iscript_opcodes``` in ```eoe_config_override.xml```

```xml
<entry byte="24" str="Impl=IfFlag" />
<entry byte="25" str="Impl=SetFlag" />
<entry byte="26" str="Impl=IfWorld" />
<entry byte="27" str="Impl=IfScreen" />
<entry byte="28" str="Impl=IfDoorYX" />
<entry byte="29" str="Impl=JSR" />
<entry byte="30" str="Impl=Return" />
<entry byte="31" str="Impl=ForceDoor" />
```

We only specify the implementation (Impl) value, since the application knows the function signatures. The order here does not matter, but each opcode needs a unique byte value - and all Impl-entries must follow all non Impl-entries. If you want custom mnemonics that is possible too, for example:

```xml
<entry byte="30" str="Impl=Return,Mnemonic=Ret" />
```

would allow you to write "Ret" instead of "Return" in the asm-files. If a mnemonic is not given, it defaults to the Impl-value itself.

Once these opcodes have been defined, the assembler injects their implementations into free space in bank 12 together with the script bytecode.

We know that failure scripts are called per key requirement, so we will create a shared code block using JSR and Return that can be reached from each of these. When a door requirement fails, we will check a corresponding flag, and if it is set we will force ourselves through the door.

We will make sure to set one flag per door the first time a key is used.

In world 1, doors are locked with Key J, Key Q and Key Jo. These have failure scripts 2, 123 and 126, respectively.

In world 1, there are locked doors on screens 11, 30 and 40. On screen 40 there are two locked doors, so we need to distinguish them.

We will use flags 200, 201, 202 and 203 for these 4 doors.

First we make a shared subroutine to check the flags somewhere in the [iscript]-section.

```asm
@door_check:
  IfWorld 1 @is_trunk
  Return ; this was not trunk - return to regular handling

@is_trunk:
  IfScreen 11 @trunk_11
  IfScreen 30 @trunk_30
  IfScreen 40 @trunk_40
  Return ; could not really happen - return can be omitted
  
  ; we know only one door exists on screen 11, so no more checks are needed
  @trunk_11:
  IfFlag 200 @check_successful
  Return ; the flag has not been set - return to regular handling
  
  @trunk_30:
  IfFlag 201 @check_successful
  Return ; the flag has not been set - return to regular handling
  
  @trunk_40:  ; we have two doors on this screen, and need to distinguish them
  IfDoorYX $10 @trunk_40_a ; door with coords (x=0, y=1)
  IfDoorYX $3d @trunk_40_b ; door with coords (x=13, y=3)
  Return ; could not really happen - return can be omitted
  
  @trunk_40_a:
  IfFlag 202 @check_successful
  Return; flag not set - return to regular handling
  
  @trunk_40_b:
  IfFlag 203 @check_successful
  Return ; flag not set - return to regular handling

@check_successful:
  Msg "Entering door"
  ForceDoor ; the corresponding flag was set, enter the door
  End ; success - so end the script without returning and showing the "key needed"-message

```

Since we know the door locations up front, and we only invoke this code block when handling a door, we know at least one branch will be taken. Some of the Return-statements here were not necessary.

Next we will call this shared block from the Jack, Queen and Joker failure scripts. We JSR to the shared door checker which only returns if the check failed - meaning the flag was not set.

```asm
.entrypoint 2
.textbox GENERIC
    JSR @door_check
    Msg "There is a mark<n>of <q>Jack<q><n>by the key hole."
    End
```

```asm
.entrypoint 123
.textbox GENERIC
    JSR @door_check
    MsgNoskip "There is the<n>mark of <q>Queen<q><n>by the key hole."
    End
```

```asm
.entrypoint 126
.textbox GENERIC
    JSR @door_check
    MsgNoskip "There is the<n>mark of <q>Joker<q><n>by the key hole."
    End
```

With this in place, we will pass through the door without having the needed requirement if flags have been set. Finally we have to actually SET the flags the first time we go through any of these doors. This will be done in the key used successfully script, with index 132.

Next we create a second shared subroutine that records which doors have already been unlocked. Each label must be uniquely defined, so I will prefix these with s_.

Add this code block somewhere in the [iscript]-section too;

```asm
@door_set:
  IfWorld 1 @s_is_trunk
  Return ; this was not trunk - return to regular handling

@s_is_trunk:
  IfScreen 11 @s_trunk_11
  IfScreen 30 @s_trunk_30
  IfScreen 40 @s_trunk_40
  Return ; could not really happen - return can be omitted
  
  ; we know only one door exists on screen 11, so no more checks are needed
  @s_trunk_11:
  SetFlag 200
  Return
  
  @s_trunk_30:
  SetFlag 201
  Return
  
  @s_trunk_40:  ; we have two doors on this screen, and need to distinguish them
  IfDoorYX $10 @s_trunk_40_a ; door with coords (x=0, y=1)
  IfDoorYX $3d @s_trunk_40_b ; door with coords (x=13, y=3)
  Return ; could not really happen - return can be omitted
  
  @s_trunk_40_a:
  SetFlag 202
  Return
  
  @s_trunk_40_b:
  SetFlag 203
  Return
```

And finally call this subroutine from the key used script;

```
.entrypoint 132
.textbox GENERIC
    JSR @door_set
    MsgNoskip "I've used key."
    End
```

The result is that each locked door becomes permanently unlocked the first time the corresponding key is used. Subsequent attempts to enter the same door succeed immediately, while all other locked doors continue to behave normally until their own flags have been set.

This example just handled Trunk, but more world checks can be added of course;

```
IfWorld 0 @is_eolis
IfWorld 1 @is_trunk
IfWorld 2 @is_mist
```

...and so on.

The assembler output in this case would say

```
Installed new script library routines (210 bytes)
```

which was the cost of adding these custom opcodes. Custom opcodes compete for space with the actual script code, so try to only add opcodes you will actually use.

**Note**: Door success and failure scripts execute after the textbox has already been opened by the engine. Consequently, calling ForceDoor from a failure script still displays a textbox. This is a limitation of the vanilla script system rather than the ForceDoor opcode itself.

<hr>

### Tilemap Change System

#### Overview

The tilemap change subsystem allows individual metatiles to be modified dynamically at runtime. Unlike permanent ROM edits, tilemap changes are conditional and are evaluated each time a screen is entered. They can also be forced via a direct call to the screen handler code in assembly, or via the script engine with opcode ```RunScreenHandler```.

The primary use case is creating persistent world changes, such as opened passages, collapsed walls, destroyed obstacles, new doors and ladders, or other environmental changes controlled by extended flags.


### How it Works

- The assembler reads the [tilemap_changes] section
- It generates a compact binary data structure
- The data is injected into the configured ROM bank and cpu address
- A custom screen event handler is installed automatically
- When the player enters a screen, the handler checks whether any tilemap changes apply
- Matching tile changes are applied before gameplay resumes

#### Assembly Syntax

A new section type ```[tilemap_changes]``` can be added to the assembly file. If it is present, the tilemap change subsystem will be installed.

```asm
[tilemap_changes]

  world 0
  screen 5
  flag 100

  3,4,17
  3,5,18
  4,5,19
```

This says that for world 0, screen 5 - if flag 100 is set the following tilemap changes take place when entering the screen:

- (x=3, y=4) becomes metatile 17
- (x=3, y=5) becomes metatile 18
- (x=4, y=5) becomes metatile 19

The assembler will sort the data, but world needs to be defined before screens, and flags need to be defined before tilemap changes.

In Echoes of Eolis a keyboard shortcut ```Shift+Ctrl+C``` will copy the selected tilemap rectangle on this format, making it easier to create the tilemap change data. The flag number still needs to be specified by users however.

<hr>

#### Typical Workflow



```text
Temporarily edit screens in Echoes of Eolis
                │
                ▼
        Copy tilemap changes
                │
                ▼
Paste into section [tilemap_changes] in the asm-file
                │
                ▼
        Write script logic
                │
                ▼
     Assemble with FaxIScripts
                │
                ▼
   Reload ROM in Echoes of Eolis
Set screen handler 3 for screens with dynamic tilemap changes
                │
                ▼
  Patch ROM and test in emulator
```

The special copy operation ```Shift+Ctrl+C``` in EoE will only copy rectangular areas. If you want other shapes for the tilemap changes you will have to modify the output yourself.

<hr>

#### Example: Persisting Mattock-breakable blocks

As a minimum we need to have the following opcodes available;

```xml
  <entry byte="24" str="Impl=SetFlag" />
  <entry byte="25" str="Impl=IfWorld" />
  <entry byte="26" str="Impl=IfScreen" />
```

In practice we probably want to add many more than these, but for this example these will suffice.

In a real scenario we certainly want the script opcode to force the tilemap changing screen handler to run on command.

```xml
  <entry byte="27" str="Mnemonic=Render,Impl=RunScreenHandler" />
```

Here we set the mnemonic to "Render". If omitted, the mnemonic would be "RunScreenHandler" like the name of the implementation.

The normal mattock-breakable blocks are on world 1 (Trunk), screen 12.

We will open this screen in Echoes of Eolis, and remove the mattock breakable blocks and use metatile 66 which is the brick background. We then select these two blocks and use keyboard shortcut ```Shift+Ctrl+C``` which copies the tilemap change to the clipboard - which will look like this;

```
 ; tilemap changes generated by Echoes of Eolis

world 1
screen 12
flag ?    ; TODO: specify trigger flag 

3,10,66
3,11,66
```

The copied data describes tiles and positions we want to dynamically change during runtime. Undo the edits in the GUI so the ROM itself remains unchanged, then add the generated block to the assembler source. Let us use flag 100. At the bottom of the asm-file we will then have a section which looks like this;

```
[tilemap_changes]
 ; tilemap changes generated by Echoes of Eolis

world 1
screen 12
flag 100

3,10,66
3,11,66
```

We will then update the script for mattock used to set flag 100, if the mattock was used on this screen.

The script looks like this by default:

```
.entrypoint 129
.textbox GENERIC
    MsgNoskip "I've used<n>Mattock."
    End
```

We will check if we are on world 1, screen 12, and if so we set flag 100.

```
.entrypoint 129
.textbox GENERIC
    MsgNoskip "I've used<n>Mattock."

    IfWorld 1 @mattock_trunk
    End ; some other mattock use, exit
	
    @mattock_trunk:
    IfScreen 12 @mattock_trunk_12
    End ; some other mattock use, exit
	
    @mattock_trunk_12:
    SetFlag 100
    End
```

If we build the asm-file with this, a new message will show in the output:

```
Installed tilemap change subsystem (177 bytes)
```

The reported size includes both the generated runtime code and the compiled tilemap change data. It therefore grows as additional tilemap changes are added.

By default the subsystem is installed in bank 9 cpu address $a000 for 16-bank ROMs, or bank 30 address $8000 for 32-bank ROMs. This is configurable.

**Important**: For 16 bank ROMs you need to make sure this subsystem does not overwrite tilemap data which can also live in bank 9.

The final step is add the screen event handler to the screen. If you had the ROM open in Echoes of Eolis while building this assembly file, you can use "Apply external rom changes" to get the changes in. Otherwise open the file in EoE. Navigate to Trunk screen 12. Go to Sprites and click "Add Event Handler". Assign event handler 3, which is the custom tilemap change handler installed by FaxIScripts. The handler will now execute every time this screen is entered.

The game itself applies a tilemap change immediately when using the mattock, so we do not need to trigger the handler from the script in this case. In other cases you may want to set a flag and then call the screen handler immediately from a script. That is what the script opcode ```RunScreenHandler``` is for. We could have written the following at the end of the mattock script;

```
SetFlag 100
Render
End
```

This would update the tilemap immediately instead of waiting until the next time the screen is entered.

The ```RunScreenHandler``` opcode (which we renamed to ```Render``` above) executes the screen event handler immediately. This allows the tilemap changes to appear without requiring the player to leave and re-enter the screen. Since the handler redraws tiles directly to the screen, it can overwrite parts of an active textbox, so be sure to test textbox placement when using it during scripted events.

The first time the Mattock is used on this screen, the wall is removed immediately. On subsequent visits, the tilemap change subsystem reapplies the same changes automatically because flag 100 remains set - and screen event handler 3 is activated for this screen.

While this example only replaces two Mattock-breakable blocks, the tilemap change subsystem is completely general. Any metatiles on a screen can be replaced based on an extended flag. This makes it possible to create persistent world changes such as:

- Doors that appear or disappear after an event
- Ladders, bridges, or stairways that become available later
- Opened gates or collapsed walls
- NPC houses or buildings that change over time
- Environmental changes after defeating a boss or completing a quest
- Puzzle elements that permanently alter the world

Since tilemap changes are driven by flags, they persist automatically as long as the corresponding flags are preserved (for example through SRAM support). For ROMs without SRAM support, all the extended flags will be cleared on reset and power off.

<hr>

### Configuration

When the dynamic tilemap changes take place, it is possible to configure the amount of frames to wait between drawing each new metatile. This is set in configuration constant ```hack_tm_change_wait_frames```.

It is also possible to play a sound effect for each metatile. This is set in configuration constant ```hack_tm_change_sound_effect```. (set this to $ff to disable sound effects)

The tilemap change subsystem will be installed in the bank given by config item ```hack_tm_change_bank``` and the cpu address given by ```hack_tm_change_cpu_addr``` in that bank. For the translation hack and derivatives this defaults to ```[$1e:$8000]``` and for all other regions ```[$09:$a000]```. This can be changed in the configuration override file if it conflicts with other data in those locations.

<hr>

### Limitations

Currently the dynamic tilemap changes have these limitations:

- At most 63 screens can have tilemap changes on any world
- At most 127 metatile changes for any screen
- Each screen can only be associated with one flag and one set of metatile changes

<hr>

## Bank 15 Hack Injection Points

ROM hacks can be injected by both Echoes of Eolis and FaxIScripts. For bank 15 we use space where normally unreachable code lives to inject these. The config ID-column shows the name of the configuration constant in ```eoe_config.xml``` which corresponds to the injection points. Users can configure them with config overrides if needed.

| Feature | CPU Address  | Size (bytes) | Comments | Config ID |
|---|---|---|---|---|
| Pal2Mus for Sameworld Transitions | $c033 | 30/37 | Echoes of Eolis - Enabled under Settings > Advanced | hack_sw_trans_pal2mus_addr |
| Stage Door Hack - Set Stage | $df99  | 15/15  | Echoes of Eolis - Enabled under Settings > Advanced | hack_set_pending_stage_addr |
| Stage Door Hack - Extract Requirements | $dfa8  | 15/15  | Echoes of Eolis - Enabled under Settings > Advanced | hack_decode_req_addr |
| Stage Door Hack - Load World | $dfb7  | 8/14  | Echoes of Eolis - Enabled under Settings > Advanced | hack_load_world_addr |
| Stage Door Hack - Palette Handler | $f389 | 23/28 | Echoes of Eolis - Enabled under Settings > Advanced | hack_handle_palette_addr |
| Clear Extended Flags on Init ($0101-$011f) | $d005 | 14/17 | FaxIScripts - if any of the opcodes SetFlag, IfFlag or ClearFlag are enabled | hack_clear_persistent_flags |
| Event Handler Table | $cb0c | 8/11 | FaxIScripts - If section [tilemap_changes] exists when a script file is assembled | hack_tm_handler_table_cpu_addr |
| Event Handler Code | $e894 | 23/30 | FaxIScripts - If section [tilemap_changes] exists when a script file is assembled | hack_tm_handler_cpu_addr |

<hr>

Runtime extensions also reserve some RAM locations.

## RAM used by custom hacks

| Feature | RAM Address | Comments | Config ID |
|---|---|---|---|
| Extended flag system | $0101-$011f | Used  by the opcodes SetFlag, IfFlag, ClearFlag and the tilemap change subsystem | - |
| Tilemap Change subsystem | $e2-$e5 | Used as temporary variables when drawing the tilemap changes | - |
| Custom script opcodes JSR and Return | $0182-$0183 | Used to store the return address | hack_script_jsr_ram_addr_lo, hack_script_jsr_ram_addr_hi |
| Stage Door Hack | $07fe-$07ff | Stores the pending destination stage during cross-stage door transitions | - |

The extended flags are cleared on game initialization (rest and power cycles), but will not be stored in mantras. They will only persist across sessions if stored in SRAM.

<hr>
