//
// GrayMUL.H
// Copyright Menace Software (www.menasoft.com).
//
// Define in the MUL files.
//

#ifndef _INC_GRAYMUL_H
#define _INC_GRAYMUL_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

//---------------------------MUL FILE DEFS---------------------------

// All these structures must be BYTE packed.
#if defined _WIN32 && (!__MINGW32__)
// Microsoft dependant pragma
#pragma pack(1)	
#define PACK_NEEDED
#else
// GCC based compiler you can add:
#define PACK_NEEDED __attribute__ ((packed))
#endif

// NOTE: !!! ALL Multi bytes in file ASSUME big endian !!!!

#define UO_MAP_VIEW_SIGHT	12	// True max sight distance of creatures is 12
#define UO_MAP_VIEW_SIZE	18 // Visibility for normal items
#define UO_MAP_VIEW_RADAR	31 // Visibility for castles, keeps and boats

/////////////////////////////////////////////////////////////////
// File blocks

#define UO_BLOCK_SIZE		8       // Base size of a block.
#define UO_BLOCK_ALIGN(i) 	((i) &~ 7 )
#define UO_BLOCK_OFFSET(i)	((i) & 7 )	// i%UO_BLOCK_SIZE
#define UO_BLOCK_ROUND(i)	UO_BLOCK_ALIGN((i)+7)
#define UO_BLOCKS_X			768		// blocks
#define UO_BLOCKS_X_REAL	640		// Minus the dungeons.
#define UO_BLOCKS_Y			512		// 0x200
#define UO_SIZE_X			(UO_BLOCKS_X*UO_BLOCK_SIZE)	// Total Size In meters
#define UO_SIZE_X_REAL		(UO_BLOCKS_X_REAL*UO_BLOCK_SIZE)		// The actual world is only this big
#define UO_SIZE_Y			(UO_BLOCKS_Y*UO_BLOCK_SIZE)
#define UO_SIZE_Z			127
#define UO_SIZE_MIN_Z		-127

#define UO_SEX_CENTER_X 	1323	// Center of the sextant world (LB's throne)
#define UO_SEX_CENTER_Y		1624

#define MAP_SIZE_X			UO_SIZE_X - 1
#define MAP_SIZE_Y			UO_SIZE_Y - 1

#define PLAYER_HEIGHT 15	// We need x units of room to walk under something. (human) ??? this should vary based on creature type.
// This is why NPCs walk over blocking objects 
// the value was too high we'll see if 3 is ok
#define PLAYER_PLATFORM_STEP 5

// Terrain samples 
typedef WORD		TERRAIN_TYPE;
enum
{
	TERRAIN_UNUSED  = 0,
	TERRAIN_VOID2	= 1,		// "VOID"
	TERRAIN_HOLE	= 0x0002,	// "NODRAW" we can pas thru this.
	TERRAIN_INVALID = 0x004c,	// a bad terrain that ius used some place.

	TERRAIN_WATER1	= 0x00a8,
	TERRAIN_WATER2	= 0x00a9,
	TERRAIN_WATER3	= 0x00aa,
	TERRAIN_WATER4	= 0x00ab,
	TERRAIN_ROCK1	= 0x00dc,
	TERRAIN_ROCK2	= 0x00e7,
	TERRAIN_WATER5	= 0x0136,
	TERRAIN_WATER6	= 0x0137,

	TERRAIN_LAVA	= 0x1f4,
	TERRAIN_TILE_BLUE = 0x210,

	TERRAIN_NULL	= 0x0244,	// impassible interdungeon

	TERRAIN_STONE_ORANGE = 0x3FF7,
	TERRAIN_GRASS	= 0x3ffe,	// Last real tile.
	TERRAIN_QTY     = 0x4000,	// Terrain tile qyt
};

struct CUOMapMeter	// 3 bytes (map0.mul)
{
	WORD m_wTerrainIndex;	// TERRAIN_TYPE index to Radarcol and CUOTerrainTypeRec
	signed char m_z;

	bool IsTerrainWater() const;
	bool IsTerrainRock() const;
	bool IsTerrainGrass() const;

} PACK_NEEDED;

struct CUOMapBlock	// 196 byte block = 8x8 meters, (map0.mul)
{
	WORD m_wID1;	// ?
	WORD m_wID2;
	CUOMapMeter m_Meter[ UO_BLOCK_SIZE * UO_BLOCK_SIZE ];
} PACK_NEEDED;

#define VERDATA_MAKE_INDEX( f, i ) ((f)<< 26 | (i))

enum VERFILE_TYPE		// skew list. (verdata.mul)
{
	VERFILE_MAP			= 0x00,	// "map0.mul" = Terrain data
	VERFILE_STAIDX		= 0x01,	// "staidx0.mul" = Index into STATICS0
	VERFILE_STATICS		= 0x02,	// "statics0.mul" = Static objects on the map
	VERFILE_ARTIDX		= 0x03,	// "artidx.mul" = Index to ART
	VERFILE_ART			= 0x04, // "art.mul" = Artwork such as ground, objects, etc.
	VERFILE_ANIMIDX		= 0x05,	// "anim.idx" = 2454ah animations.
	VERFILE_ANIM		= 0x06,	// "anim.mul" = Animations such as monsters, people, and armor.
	VERFILE_SOUNDIDX	= 0x07, // "soundidx.mul" = Index into SOUND
	VERFILE_SOUND		= 0x08, // "sound.mul" = Sampled sounds 
	VERFILE_TEXIDX		= 0x09, // "texidx.mul" = Index into TEXMAPS
	VERFILE_TEXMAPS		= 0x0A,	// "texmaps.mul" = Texture map data (the ground).
	VERFILE_GUMPIDX		= 0x0B, // "gumpidx.mul" = Index to GUMPART
	VERFILE_GUMPART		= 0x0C, // "gumpart.mul" = Gumps. Stationary controller bitmaps such as windows, buttons, paperdoll pieces, etc.
	VERFILE_MULTIIDX	= 0x0D,	// "multi.idx" = 
	VERFILE_MULTI		= 0x0E,	// "multi.mul" = Groups of art (houses, castles, etc)
	VERFILE_SKILLSIDX	= 0x0F, // "skills.idx" =
	VERFILE_SKILLS		= 0x10, // "skills.mul" =

	// Not really sure about these...
	VERFILE_RADARCOL	, // ? "radarcol.mul" (not sure). = Colors for converting the terrain data to radar view
	VERFILE_FONTS		, // ? "fonts.mul" =  Fixed size bitmaps style fonts. 
	VERFILE_PALETTE		, // ? "palette.mul" = Contains an 8 bit palette (use unknown)
	VERFILE_LIGHT		, // ? "light.mul" = light pattern bitmaps.
	VERFILE_LIGHTIDX	, // ? "lightidx.mul" = light pattern bitmaps.
	VERFILE_HUES		, // ? "hues.mul"
	VERFILE_VERDATA		, // ? "verdata.mul" = This version file.
	// ...

	VERFILE_TILEDATA	= 0x1E, // "tiledata.mul" = Data about tiles in ART. name and flags, etc
	VERFILE_ANIMDATA	= 0x1F, // "animdata.mul" = ?
	VERFILE_QTY,				// NOTE: 020 is used for something !
};

struct CUOVersionBlock	// skew list. (verdata.mul)
{
	// First 4 bytes of this file = the qty of records.
private:
	DWORD m_file;		// file type id. VERFILE_TYPE (ex.tiledata = 0x1E)
	UINT m_block;		// tile number. ( items = itemid + 0x200 )
public:
	DWORD m_filepos;	// pos in this file to find the patch block.
	DWORD m_length;

	WORD  m_wVal3;		// stuff that would have been in CUOIndexRec
	WORD  m_wVal4;
public:
	UINT GetIndex() const	// a single sortable index.
	{
		return( VERDATA_MAKE_INDEX( m_file, m_block ));
	}
	VERFILE_TYPE GetFileIndex() const
	{
		return( (VERFILE_TYPE) m_file );
	}
	UINT GetBlockIndex() const
	{
		return( m_block );
	}

} PACK_NEEDED;

struct CUOIndexRec	// 12 byte block = used for table indexes. (staidx0.mul,multi.idx,anim.idx)
{
private:
	DWORD	m_dwOffset;	// 0xFFFFFFFF = nothing here ! else pointer to something (CUOStaticItemRec possibly)
	DWORD 	m_dwLength; // Length of the object in question.
public:
	WORD 	m_wVal3;	// Varied uses. ex. GumpSizey
	WORD 	m_wVal4;	// Varied uses. ex. GumpSizex

public:
	DWORD GetFileOffset() const
	{
		return( m_dwOffset );
	}
	DWORD GetBlockLength() const
	{
		return( m_dwLength &~ 0x80000000 );
	}
	bool IsVerData() const
	{
		return( ( m_dwLength & 0x80000000 ) ? true : false );
	}
	bool HasData() const
	{
		return( m_dwOffset != 0xFFFFFFFF && m_dwLength != 0 );
	}
	void Init()
	{
		m_dwLength = 0;
	}
	void CopyIndex( const CUOVersionBlock * pVerData )
	{
		// Get an index rec from the verdata rec.
		m_dwOffset = pVerData->m_filepos;
		m_dwLength = pVerData->m_length | 0x80000000;
		m_wVal3 = pVerData->m_wVal3;
		m_wVal4 = pVerData->m_wVal4;
	}
	void SetupIndex( DWORD dwOffset, DWORD dwLength )
	{
		m_dwOffset = dwOffset;
		m_dwLength = dwLength;
	}

} PACK_NEEDED;

struct CUOStaticItemRec	// 7 byte block = static items on the map (statics0.mul)
{
	WORD	m_wTileID;		// ITEMID_TYPE = Index to tile CUOItemTypeRec
	BYTE	m_x;		// x <= 7 = offset from block.
	BYTE 	m_y;		// y <= 7
	signed char m_z;	//
	WORD 	m_wColor;	// COLOR_TYPE modifier for the item

	// For internal caching purposes only. overload this.
	// LOBYTE(m_wColor) = Blocking flags for this item. (CAN_I_BLOCK)
	// HIBYTE(m_wColor) = Height of this object.

} PACK_NEEDED;

#define UOTILE_BLOCK_QTY	32	// Come in blocks of 32.

struct CUOTerrainTypeRec	// size = 0x1a = 26 (tiledata.mul)
{	// First half of tiledata.mul file is for terrain tiles.
	UINT m_flags;	// 0xc0=water, 0x40=dirt or rock, 0x60=lava, 0x50=cave, 0=floor
	WORD m_index;	// just counts up.  0 = unused.
	char m_name[20];
} PACK_NEEDED;

struct CUOTerrainTypeBlock
{
	UINT m_dwUnk;
	CUOTerrainTypeRec m_Tiles[UOTILE_BLOCK_QTY];
} PACK_NEEDED;

	// 0x68800 = (( 0x4000 / 32 ) * 4 ) + ( 0x4000 * 26 )
#define UOTILE_TERRAIN_SIZE ((( TERRAIN_QTY / UOTILE_BLOCK_QTY ) * 4 ) + ( TERRAIN_QTY * sizeof( CUOTerrainTypeRec )))

struct CUOItemTypeRec	// size = 37 (tiledata.mul)
{	// Second half of tiledata.mul file is for item tiles (ITEMID_TYPE).
	// if all entries are 0 then this is unused and undisplayable.
#define UFLAG1_FLOOR		0x00000001	// floor (Walkable at base position)
#define UFLAG1_EQUIP		0x00000002	// equipable. m_layer is LAYER_TYPE
#define UFLAG1_NONBLOCKING	0x00000004	// Signs and railings that do not block.
#define UFLAG1_LIQUID		0x00000008	// blood,Wave,Dirt,webs,stain, 
#define UFLAG1_WALL			0x00000010	// wall type = wall/door/fireplace
#define UFLAG1_DAMAGE		0x00000020	// damaging. (Sharp, hot or poisonous)
#define UFLAG1_BLOCK		0x00000040	// blocked for normal human. (big and heavy)
#define UFLAG1_WATER		0x00000080	// water/wet (blood/water)
#define UFLAG2_ZERO1		0x00000100	// 0 NOT USED (i checked)
#define UFLAG2_PLATFORM		0x00000200	// platform/flat (can stand on, bed, floor, )
#define UFLAG2_CLIMBABLE	0x00000400	// climbable (stairs). m_height /= 2(For Stairs+Ladders)
#define UFLAG2_STACKABLE	0x00000800	// pileable/stackable (m_dwUnk7 = stack size)
#define UFLAG2_WINDOW		0x00001000	// window/arch/door can walk thru it
#define UFLAG2_WALL2		0x00002000	// another type of wall. (not sure why there are 2)
#define UFLAG2_A			0x00004000	// article a
#define UFLAG2_AN			0x00008000	// article an
#define UFLAG3_DESCRIPTION  0x00010000	// descriptional tile. (Regions, professions, ...)
#define UFLAG3_TRANSPARENT	0x00020000	// Transparent (Is used for leaves and sails)
#define UFLAG3_CLOTH		0x00040000	// Probably dyeable ?
#define UFLAG3_ZERO8		0x00080000	// 0 NOT USED (i checked)
#define UFLAG3_MAP			0x00100000	// map
#define UFLAG3_CONTAINER	0x00200000	// container.
#define UFLAG3_EQUIP2		0x00400000	// equipable (not sure why there are 2 of these)
#define UFLAG3_LIGHT		0x00800000	// light source
#define UFLAG4_ANIM			0x01000000	// animation with next several object frames. (layer = number of frames)
#define UFLAG4_UNK1			0x02000000	// archway, easel, fountain - I have no idea.
#define UFLAG4_UNK2			0x04000000	// lots of stuff. I have no idea.
#define UFLAG4_BODYITEM		0x08000000	// Whole body item (ex.British", "Blackthorne", "GM Robe" and "Death shroud")
#define UFLAG4_ROOF			0x10000000	//
#define UFLAG4_DOOR			0x20000000	// door
#define UFLAG4_STAIRS		0x40000000	//
#define UFLAG4_WALKABLE		0x80000000	// We can walk here.

	UINT m_flags;
	BYTE m_weight;		// 255 = unmovable.
	BYTE m_layer;		// LAYER_TYPE for UFLAG1_EQUIP, UFLAG3_EQUIP2 or UFLAG1_* = 0 (candle?), number of frames for _ANIM
	UINT m_dwUnk6;		// qty in the case of UFLAG2_STACKABLE, Gump in the case of equipable.
	UINT m_dwAnim;		// equipable items animation index. (50000 = male offset, 60000=female)
	WORD m_wUnk14;
	BYTE m_height;		// z height but may not be blocking. ex.UFLAG2_WINDOW
	char m_name[20];	// sometimes legit not to have a name
} PACK_NEEDED;

struct CUOItemTypeBlock
{
	UINT m_dwUnk;
	CUOItemTypeRec m_Tiles[UOTILE_BLOCK_QTY];
} PACK_NEEDED;

struct CUOMultiItemRec // (Multi.mul)
{
	// Describe multi's like houses and boats. One single tile.
	// From Multi.Idx and Multi.mul files.
	WORD  m_wTileID;	// ITEMID_TYPE = Index to tile CUOItemTypeRec
	short m_dx;		// signed delta.
	short m_dy;
	short m_dz;
	UINT m_visible;	// 0 or 1
} PACK_NEEDED;

struct CUOFontImageHeader // (FONTS.MUL)
{
	BYTE m_bCols;
	BYTE m_bRows;
	BYTE m_bUnk;
} PACK_NEEDED;

struct CUOArtImageHeader // (ART.MUL)
{
	WORD m_wDataSize;	// can be 0
	WORD m_wTrigger;	// can be ffff
	WORD m_wHeight;
	WORD m_wWidth;
} PACK_NEEDED;

struct CUOAnimGroup // ANIM.MUL
{
	// AnimationGroup
	WORD m_Palette[256];
	UINT m_FrameCount;
	// DWORD m_FrameOffset[1];
} PACK_NEEDED;

struct CUOAnimFrame // ANIM.MUL
{
	signed short m_ImageCenterX;
	signed short m_ImageCenterY;
	WORD m_Width;
	WORD m_Height;
} PACK_NEEDED;

struct CUOSoundFrame // SOUND.MUL
{
	// Contains sampled sounds from the game.
	// All sounds seem to be 16 bit stereo and sampled at 11025hz.
	char m_name[16];
	UINT m_format[4];	// unknown format data.
	// WORD m_data[1];	// the sampled sound data.
};

struct CUOHueEntry	// HUE.MUL
{
	WORD m_ColorTable[32];
	WORD m_TableStart;
	WORD m_TableEnd;
	char m_Name[20];
};

struct CUOHueGroup // HUE.MUL
{
	UINT m_Header;
	CUOHueEntry m_Entries[8]; 
};

// see also http://dkbush.cablenet-va.com/alazane/file_formats.html for more info.

// Turn off structure packing.
#if defined _WIN32 && (!__MINGW32__)
#pragma pack()
#endif

////////////////////////////////////////////////////////////////////////
// Shared enum types.

// 20 colors of 10 hues and 5 brightnesses, which gives us 1000 colors.
//  plus black, and default. 
// Skin color is similar, except there are 7 basic colors of 8 hues. 
// For hair there are 7 colors of 7 hues. 
// 
#define UO_COLOR_SIZE 2	// 16 bit colors all over the place.
typedef WORD COLOR_TYPE;

enum COLOR_CODE
{
	COLOR_DEFAULT		= 0x0000,

	COLOR_BLACK			= 0x0001,
	COLOR_BLUE_LOW		= 0x0002,	// lowest dyeable color.
	COLOR_BLUE_NAVY		= 0x0003,		
	COLOR_INDIGO_DARK	= 0x0007,
	COLOR_INDIGO		= 0x0008,
	COLOR_INDIGO_LIGHT	= 0x0009,
	COLOR_VIOLET_DARK	= 0x000c,
	COLOR_VIOLET		= 0x000d,
	COLOR_VIOLET_LIGHT	= 0x000e,
	COLOR_MAGENTA_DARK	= 0x0011,
	COLOR_MAGENTA		= 0x0012,
	COLOR_MAGENTA_LIGHT	= 0x0013,
	COLOR_RED_DARK		= 0x0020,
	COLOR_RED			= 0x0022,
	COLOR_RED_LIGHT		= 0x0023,
	COLOR_ORANGE_DARK	= 0x002a,
	COLOR_ORANGE		= 0x002b,
	COLOR_ORANGE_LIGHT	= 0x002c,
	COLOR_YELLOW_DARK	= 0x0034,
	COLOR_YELLOW		= 0x0035,
	COLOR_YELLOW_LIGHT	= 0x0036,
	COLOR_GREEN_DARK	= 0x003e,
	COLOR_GREEN			= 0x003f,
	COLOR_GREEN_LIGHT	= 0x0040,
	COLOR_CYAN_DARK		= 0x0057,
	COLOR_CYAN			= 0x0058,
	COLOR_CYAN_LIGHT	= 0x0059,

	COLOR_BLUE_DARK		= 0x0061,
	COLOR_BLUE			= 0x0062,
	COLOR_BLUE_LIGHT	= 0x0063,

	COLOR_GRAY_DARK		= 0x0386,	// gray range.
	COLOR_GRAY			= 0x0387,
	COLOR_GRAY_LIGHT	= 0x0388,

	COLOR_TEXT_DEF		= 0x03b2,	// light gray color.

	COLOR_DYE_HIGH		= 0x03e9,	// highest dyeable color = 1001

	COLOR_SKIN_LOW		= 0x03EA,
	COLOR_SKIN_HIGH		= 0x0422,

	COLOR_SPECTRAL1_LO	= 0x0423,	// unassigned color range i think.
	COLOR_SPECTRAL1_HI	= 0x044d,

	// Strange mixed colors.
	COLOR_HAIR_LOW		= 0x044e,	// valorite
	COLOR_BLACKMETAL	= 0x044e,	// Sort of a dark gray.
	COLOR_GOLDMETAL		= 0x046e,
	COLOR_BLUE_SHIMMER	= 0x0480,	// is a shimmering blue...like a water elemental blue...
	COLOR_WHITE			= 0x0481,	// white....yup! a REAL white...
	COLOR_STONE			= 0x0482,	// kinda like rock when you do it to a monster.....mabey for the Stone Harpy? 
	COLOR_HAIR_HIGH		= 0x04ad,

	COLOR_MASK			= 0x07FF,	// mask for items. (not really a valid thing to do i know)

	COLOR_QTY = 3000,	// 0x0bb8 Number of valid colors in hue table.

	COLOR_SPECTRAL		= 0x4631, // Add more colors!
	COLOR_TRANSLUCENT	= 0x4fff, // almost invis

	COLOR_UNDERWEAR		= 0x8000,	// Only can be used on humans.
};

#define SOUND_QTY	0x300
typedef WORD SOUND_TYPE;	// Sound ID

#define MULTI_QTY	0x1400	// total address space for multis.
typedef WORD MULTI_TYPE;	// define a multi (also defined by ITEMID_MULTI)

enum
{
	SOUND_ANVIL			= 0x02a,

	SOUND_TURN_PAGE		= 0x055,	// open spellbook
	SOUND_SPELL_FIZZLE	= 0x05C,
	SOUND_OPEN_METAL	= 0x0ec,		// open bank box.

	SOUND_HOOF_MARBLE_1	= 0x121,		// marble echoing sound. (horse)
	SOUND_HOOF_MARBLE_2	= 0x122,
	SOUND_HOOF_BRIDGE_1 = 0x123,
	SOUND_HOOF_BRIDGE_2 = 0x124,
	SOUND_HOOF_DIRT_1	= 0x125,
	SOUND_HOOF_DIRT_2	= 0x126,
	SOUND_HOOF_QUIET_1 = 0x129, // quiet
	SOUND_HOOF_QUIET_2 = 0x12a, // quiet, horse run on grass.

	SOUND_FEET_STONE_1	= 0x12b,	// on stone (default)
	SOUND_FEET_STONE_2	= 0x12c,
	SOUND_FEET_GRASS_1	= 0x12d,
	SOUND_FEET_GRASS_2	= 0x12e,
	SOUND_FEET_PAVE_1	= 0x12f,		// on slate sound
	SOUND_FEET_PAVE_2	= 0x130,

	SOUND_HIT_10		= 0x013e,

	SOUND_GHOST_1	= 382,
	SOUND_GHOST_2,
	SOUND_GHOST_3,
	SOUND_GHOST_4,
	SOUND_GHOST_5,

	SOUND_SWORD_1		= 0x023b,
	SOUND_SWORD_7		= 0x023c,

	SOUND_CLEAR		= 0xFFFF,	// not sure why OSI server sends this.
};

typedef WORD MIDI_TYPE;	// Music id

enum ITEMID_TYPE	// InsideUO is great for this stuff.
{
	ITEMID_NOTHING		= 0x0000,	// Used for lightning.
	ITEMID_NODRAW = 1,
	ITEMID_ANKH_S,
	ITEMID_ANKH_N,
	ITEMID_ANKH_W,
	ITEMID_ANKH_E,

	ITEMID_STONE_WALL			= 0x0080,

	ITEMID_DOOR_SECRET_1		= 0x00E8,
	ITEMID_DOOR_SECRET_2		= 0x0314,
	ITEMID_DOOR_SECRET_3		= 0x0324,
	ITEMID_DOOR_SECRET_4		= 0x0334,
	ITEMID_DOOR_SECRET_5		= 0x0344,
	ITEMID_DOOR_SECRET_6		= 0x0354,

	ITEMID_DOOR_METAL_S			= 0x0675,	// 1
	ITEMID_DOOR_METAL_S_2		= 0x0677,
	ITEMID_DOOR_METAL_S_3		= 0x067D,
	ITEMID_DOOR_BARRED			= 0x0685,	// 2
	ITEMID_DOOR_RATTAN			= 0x0695,	// 3
	ITEMID_DOOR_WOODEN_1		= 0x06A5,	// 4
	ITEMID_DOOR_WOODEN_1_o		= 0x06A6,	// 4
	ITEMID_DOOR_WOODEN_1_2		= 0x06A7, 
	ITEMID_DOOR_WOODEN_2		= 0x06B5,	// 5
	ITEMID_DOOR_METAL_L			= 0x06C5,	// 6
	ITEMID_DOOR_WOODEN_3		= 0x06D5,	// 7
	ITEMID_DOOR_WOODEN_4		= 0x06E5,	// 8
	ITEMID_DOOR_HI				= 0x06f4,

	ITEMID_DOOR_IRONGATE_1		= 0x0824,
	ITEMID_DOOR_WOODENGATE_1	= 0x0839,
	ITEMID_DOOR_IRONGATE_2		= 0x084C,
	ITEMID_DOOR_WOODENGATE_2	= 0x0866,

	ITEMID_ROCK_1_LO		= 0x8e0,
	ITEMID_ROCK_1_HI		= 0x8ea,

	ITEMID_BEEHIVE			= 0x091a,

	ITEMID_FOOD_BACON	 = 0x0976,
	ITEMID_FOOD_FISH_RAW = 0x97a,
	ITEMID_FOOD_FISH	= 0x097b,

	ITEMID_RBASKET		= 0x0990,	//0x0E78,

	ITEMID_BOOZE_LIQU_B1 = 0x099b,
	ITEMID_BOOZE_LIQU_B2 = 0x099c,
	ITEMID_BOOZE_LIQU_B3 = 0x099d,
	ITEMID_BOOZE_LIQU_B4 = 0x099e,
	ITEMID_BOOZE_ALE_B1 = 0x099f, 
	ITEMID_BOOZE_ALE_B2 = 0x09a0, 
	ITEMID_BOOZE_ALE_B3 = 0x09a1, 
	ITEMID_BOOZE_ALE_B4 = 0x09a2, 

	ITEMID_PITCHER		= 0x09a7,	// empty.
	ITEMID_BOX_METAL    = 0x09a8,
	ITEMID_CRATE7		= 0x09a9,
	ITEMID_BOX_WOOD1	= 0x09aa,
	ITEMID_CHEST_SILVER2= 0x09ab,
	ITEMID_BASKET		= 0x09ac,
	ITEMID_POUCH2		= 0x09b0,
	ITEMID_BASKET2		= 0x09b1,
	ITEMID_BANK_BOX		= 0x09b2, // another pack really but used as bank box.

	ITEMID_FOOD_EGGS_RAW	= 0x09b5,
	ITEMID_FOOD_EGGS		= 0x09b6,
	ITEMID_FOOD_BIRD1		= 0x09b7,
	ITEMID_FOOD_BIRD2		= 0x09b8,
	ITEMID_FOOD_BIRD1_RAW	= 0x09b9,
	ITEMID_FOOD_BIRD2_RAW	= 0x09ba,

	ITEMID_FOOD_SAUSAGE = 0x09c0,
	ITEMID_BOOZE_WINE_B1	= 0x09c4,
	ITEMID_BOOZE_WINE_B2	= 0x09c5,
	ITEMID_BOOZE_WINE_B3	= 0x09c6,
	ITEMID_BOOZE_WINE_B4	= 0x09c7,

	ITEMID_FOOD_HAM		= 0x09C9,

	ITEMID_FISH_1		= 0x09CC,
	ITEMID_FISH_2		= 0x09CD,
	ITEMID_FISH_3		= 0x09CE,
	ITEMID_FISH_4		= 0x09CF,

	ITEMID_FRUIT_APPLE	= 0x09d0,
	ITEMID_FRUIT_PEACH1	= 0x09d2,
	ITEMID_FRUIT_GRAPE	= 0x09d7,

	ITEMID_FOOD_CAKE		= 0x09e9,

	ITEMID_JAR_HONEY		= 0x09EC,
	ITEMID_BOOZE_ALE_M1		= 0x09ee,
	ITEMID_BOOZE_ALE_M2		= 0x09ef,

	ITEMID_FOOD_MEAT_RAW	= 0x09f1,
	ITEMID_FOOD_MEAT		= 0x09f2,

	ITEMID_LANTERN			= 0x0A25,

	ITEMID_BEDROLL_O_EW	= 0x0a55,
	ITEMID_BEDROLL_O_NS,
	ITEMID_BEDROLL_C,
	ITEMID_BEDROLL_C_NS,	
	ITEMID_BEDROLL_C_EW	= 0x0a59,

	ITEMID_BED1_1			= 0x0a5a,
	// some things in here are not bed but sheets and blankets.
	ITEMID_BED1_X			= 0x0a91,

	ITEMID_BOOKSHELF1		= 0x0a97, // book shelf
	ITEMID_BOOKSHELF2		= 0x0a98, // book shelf
	ITEMID_BOOKSHELF3		= 0x0a99, // book shelf
	ITEMID_BOOKSHELF4		= 0x0a9a, // book shelf 
	ITEMID_BOOKSHELF5		= 0x0a9b, // book shelf
	ITEMID_BOOKSHELF6		= 0x0a9c, // book shelf
	ITEMID_BOOKSHELF7		= 0x0a9d, // book shelf 
	ITEMID_BOOKSHELF8		= 0x0a9e, // book shelf

	ITEMID_WATER_TROUGH_1	= 0x0B41,	
	ITEMID_WATER_TROUGH_2	= 0x0B44,

	ITEMID_PLANT_COTTON1	= 0x0c4f,// old
	ITEMID_PLANT_COTTON2	= 0x0c50,
	ITEMID_PLANT_COTTON3	= 0x0c51,
	ITEMID_PLANT_COTTON4	= 0x0c52,
	ITEMID_PLANT_COTTON5	= 0x0c53,
	ITEMID_PLANT_COTTON6	= 0x0c54,// young
	ITEMID_PLANT_WHEAT1	= 0x0c55,
	ITEMID_PLANT_WHEAT2	= 0x0c56,
	ITEMID_PLANT_WHEAT3	= 0x0c57,
	ITEMID_PLANT_WHEAT4	= 0x0c58,
	ITEMID_PLANT_WHEAT5	= 0x0c59,
	ITEMID_PLANT_WHEAT6	= 0x0c5a,
	ITEMID_PLANT_WHEAT7	= 0x0c5b,

	ITEMID_PLANT_VINE1	= 0x0c5e,
	ITEMID_PLANT_VINE2	= 0x0c5f,
	ITEMID_PLANT_VINE3	= 0x0c60,

	ITEMID_PLANT_TURNIP1	= 0x0c61,
	ITEMID_PLANT_TURNIP2	= 0x0c62,
	ITEMID_PLANT_TURNIP3	= 0x0c63,
	ITEMID_SPROUT_NORMAL	= 0x0c68,
	ITEMID_PLANT_ONION	= 0x0c6f,
	ITEMID_PLANT_CARROT	= 0x0c76,
	ITEMID_PLANT_CORN1	= 0x0c7d,
	ITEMID_PLANT_CORN2	= 0x0c7e,

	ITEMID_FRUIT_WATERMELLON1		= 0x0c5c,
	ITEMID_FRUIT_WATERMELLON2		= 0x0c5d,
	ITEMID_FRUIT_GOURD1	= 0x0c64,
	ITEMID_FRUIT_GOURD2	= 0x0c65,
	ITEMID_FRUIT_GOURD3	= 0x0c66,
	ITEMID_FRUIT_GOURD4	= 0x0c67,
	ITEMID_SPROUT_NORMAL2= 0x0c69,
	ITEMID_FRUIT_ONIONS1	= 0x0c6d,
	ITEMID_FRUIT_ONIONS2	= 0x0c6e,
	ITEMID_FRUIT_PUMPKIN1= 0x0c6a,
	ITEMID_FRUIT_PUMPKIN2= 0x0c6b,
	ITEMID_FRUIT_PUMPKIN3= 0x0c6c,
	ITEMID_FRUIT_LETTUCE1= 0x0c70,
	ITEMID_FRUIT_LETTUCE2= 0x0c71,
	ITEMID_FRUIT_SQUASH1	= 0x0c72,
	ITEMID_FRUIT_SQUASH2	= 0x0c73,
	ITEMID_FRUIT_HONEYDEW_MELLON1	= 0x0c74,
	ITEMID_FRUIT_HONEYDEW_MELLON2	= 0x0c75,
	ITEMID_FRUIT_CARROT1	= 0x0c77,
	ITEMID_FRUIT_CARROT2	= 0x0c78,
	ITEMID_FRUIT_CANTALOPE1			= 0x0c79,
	ITEMID_FRUIT_CANTALOPE2			= 0x0c7a,
	ITEMID_FRUIT_CABBAGE1= 0x0c7b,
	ITEMID_FRUIT_CABBAGE2= 0x0c7c,
	ITEMID_FRUIT_CORN1	= 0x0c7f,
	ITEMID_FRUIT_CORN2	= 0x0c80,
	ITEMID_FRUIT_CORN3	= 0x0c81,
	ITEMID_FRUIT_CORN4	= 0x0c82,

	ITEMID_TREE_COCONUT	= 0x0c95,
	ITEMID_TREE_DATE		= 0x0c96,

	ITEMID_TREE_BANANA1	= 0x0ca8,
	ITEMID_TREE_BANANA2	= 0x0caa,
	ITEMID_TREE_BANANA3	= 0x0cab,

	ITEMID_TREE_LO			= 0x0cca,
	ITEMID_TREE_HI			= 0x0ce8,

	ITEMID_PLANT_GRAPE	= 0x0d1a,	// fruit
	ITEMID_PLANT_GRAPE1	= 0x0d1b,
	ITEMID_PLANT_GRAPE2	= 0x0d1c,
	ITEMID_PLANT_GRAPE3	= 0x0d1d,
	ITEMID_PLANT_GRAPE4	= 0x0d1e,
	ITEMID_PLANT_GRAPE5	= 0x0d1f,
	ITEMID_PLANT_GRAPE6	= 0x0d20,
	ITEMID_PLANT_GRAPE7	= 0x0d21,
	ITEMID_PLANT_GRAPE8	= 0x0d22,
	ITEMID_PLANT_GRAPE9	= 0x0d23,
	ITEMID_PLANT_GRAPE10	= 0x0d24,

	ITEMID_FRUIT_TURNIP1	= 0x0d39,
	ITEMID_FRUIT_TURNIP2	= 0x0d3a,

	ITEMID_TREE2_LO = 0xd41,
	ITEMID_TREE2_HI = 0xd44,

	ITEMID_TREE3_LO = 0x0d57,
	ITEMID_TREE3_HI = 0x0d5b,

	ITEMID_TREE4_LO = 0x0d6e,
	ITEMID_TREE4_HI = 0x0d72,

	ITEMID_TREE5_LO = 0x0d84,
	ITEMID_TREE5_HI = 0x0d86,

	ITEMID_TREE_APPLE_BARK1	= 0x0d94,
	ITEMID_TREE_APPLE_EMPTY1= 0x0d95,
	ITEMID_TREE_APPLE_FULL1	= 0x0d96,
	ITEMID_TREE_APPLE_FALL1	= 0x0d97,
	ITEMID_TREE_APPLE_BARK2	= 0x0d98,
	ITEMID_TREE_APPLE_EMPTY2= 0x0d99,
	ITEMID_TREE_APPLE_FULL2	= 0x0d9a,
	ITEMID_TREE_APPLE_FALL2	= 0x0d9b,
	ITEMID_TREE_PEACH_BARK1	= 0x0d9c,
	ITEMID_TREE_PEACH_EMPTY1= 0x0d9d,
	ITEMID_TREE_PEACH_FULL1	= 0x0d9e,
	ITEMID_TREE_PEACH_FALL1	= 0x0d9f,
	ITEMID_TREE_PEACH_BARK2	= 0x0da0,
	ITEMID_TREE_PEACH_EMPTY2= 0x0da1,
	ITEMID_TREE_PEACH_FULL2	= 0x0da2,
	ITEMID_TREE_PEACH_FALL2	= 0x0da3,
	ITEMID_TREE_PEAR_BARK1	= 0x0da4,
	ITEMID_TREE_PEAR_EMPTY1	= 0x0da5,
	ITEMID_TREE_PEAR_FULL1	= 0x0da6,
	ITEMID_TREE_PEAR_FALL1	= 0x0da7,
	ITEMID_TREE_PEAR_BARK2	= 0x0da8,
	ITEMID_TREE_PEAR_EMPTY2	= 0x0da9,
	ITEMID_TREE_PEAR_FULL2	= 0x0daa,
	ITEMID_TREE_PEAR_FALL2	= 0x0dab,

	ITEMID_PLANT_WHEAT8	= 0x0dae,
	ITEMID_PLANT_WHEAT9	= 0x0daf,

	ITEMID_SIGN_BRASS_1		= 0x0bd1,
	ITEMID_SIGN_BRASS_2		= 0x0bd2,

	ITEMID_BED2_1			= 0x0db0,
	ITEMID_BED2_5			= 0x0db5,
	
	ITEMID_FISH_POLE1	= 0x0dbf,
	ITEMID_FISH_POLE2	= 0x0dc0,

	ITEMID_MOONGATE_RED	= 0x0dda,

	ITEMID_FRUIT_COTTON	= 0x0def,
	ITEMID_SCISSORS1	= 0x0dfc,
	ITEMID_SCISSORS2	= 0x0dfd,

	ITEMID_KINDLING1	= 0x0de1,
	ITEMID_KINDLING2	= 0x0de2,
	ITEMID_CAMPFIRE		= 0x0de3,
	ITEMID_EMBERS		= 0x0de9,

	ITEMID_WAND			= 0x0df2,

	ITEMID_COTTON_RAW	= 0x0def,
	ITEMID_WOOL			= 0x0df8,
	ITEMID_COTTON		= 0x0df9,
	ITEMID_FEATHERS		= 0x0dfa,
	ITEMID_FEATHERS2	= 0x0dfb,

	ITEMID_GAME_BACKGAM = 0x0e1c,
	ITEMID_YARN1		= 0x0e1d,
	ITEMID_YARN2		= 0x0e1e,
	ITEMID_YARN3		= 0x0e1f,

	ITEMID_BANDAGES_BLOODY1 = 0x0e20,
	ITEMID_BANDAGES1		= 0x0e21,	// clean
	ITEMID_BANDAGES_BLOODY2 = 0x0e22,

	ITEMID_EMPTY_VIAL	= 0x0e24,
	ITEMID_BOTTLE1_1	= 0x0e25,
	ITEMID_BOTTLE1_DYE	= 0x0e27,
	ITEMID_BOTTLE1_8	= 0x0e2c,

	ITEMID_CRYSTAL_BALL1 = 0x0e2d,
	ITEMID_CRYSTAL_BALL4 = 0x0e30,

	ITEMID_SCROLL2_BLANK= 0x0e34,
	ITEMID_SCROLL2_B1	= 0x0e35,
	ITEMID_SCROLL2_B6	= 0x0e3a,
	ITEMID_SPELLBOOK2	= 0x0E3b,	// ??? looks like a spellbook but doesn open corectly !

	ITEMID_CRATE1		= 0x0e3c,	// n/s	
	ITEMID_CRATE2		= 0x0e3d,	// e/w
	ITEMID_CRATE3		= 0x0e3e,
	ITEMID_CRATE4		= 0x0e3f,

	ITEMID_CHEST_METAL2_1 = 0x0e40,
	ITEMID_CHEST_METAL2_2 = 0x0e41,	// 2 tone chest.
	ITEMID_CHEST3		= 0x0e42,
	ITEMID_CHEST4		= 0x0e43,

	ITEMID_Cannon_Ball	= 0x0e73,
	ITEMID_Cannon_Balls	= 0x0e74,
	ITEMID_BACKPACK		= 0x0E75,	// containers.
	ITEMID_BAG			= 0x0E76,
	ITEMID_BARREL		= 0x0E77,
	ITEMID_BASIN		= 0x0e78,
	ITEMID_POUCH		= 0x0E79,
	ITEMID_SBASKET		= 0x0E7A,	// picknick basket
	ITEMID_CHEST_SILVER	= 0x0E7C,	// all grey. BANK BOX
	ITEMID_BOX_WOOD2	= 0x0E7D,
	ITEMID_CRATE5		= 0x0E7E,
	ITEMID_KEG			= 0x0E7F,
	ITEMID_BRASSBOX		= 0x0E80,

	ITEMID_HERD_CROOK1	= 0x0E81,	// Shepherds Crook
	ITEMID_HERD_CROOK2	= 0x0e82,
	ITEMID_Pickaxe1		= 0x0e85,
	ITEMID_Pickaxe2		= 0x0e86,
	ITEMID_Pitchfork	= 0x0e87,

	ITEMID_Cannon_N_1	= 0x0e8b,
	ITEMID_Cannon_N_3	= 0x0e8d,
	ITEMID_Cannon_W_1	= 0x0e8e,
	ITEMID_Cannon_W_3	= 0x0e90,
	ITEMID_Cannon_S_1	= 0x0e91,
	ITEMID_Cannon_S_3	= 0x0e93,
	ITEMID_Cannon_E_1	= 0x0e94,
	ITEMID_Cannon_E_3	= 0x0e96,

	ITEMID_MORTAR		= 0x0e9b,

	ITEMID_MUSIC_DRUM	= 0x0e9c,
	ITEMID_MUSIC_TAMB1,
	ITEMID_MUSIC_TAMB2,

	ITEMID_BBOARD_MSG	= 0x0eb0,	// a message on the bboard

	ITEMID_MUSIC_HARP_S	= 0x0eb1,
	ITEMID_MUSIC_HARP_L,
	ITEMID_MUSIC_LUTE1,
	ITEMID_MUSIC_LUTE2,

	ITEMID_SKELETON_1	= 0x0eca,
	ITEMID_SKELETON_2,
	ITEMID_SKELETON_3,
	ITEMID_SKELETON_4,
	ITEMID_SKELETON_5,
	ITEMID_SKELETON_6,
	ITEMID_SKELETON_7,
	ITEMID_SKELETON_8,
	ITEMID_SKELETON_9,

	ITEMID_GUILDSTONE1= 0x0EDD,
	ITEMID_GUILDSTONE2= 0x0EDE,

	ITEMID_WEB1_1		= 0x0ee3,
	ITEMID_WEB1_4		= 0x0ee6,	
	
	ITEMID_BANDAGES2	= 0x0ee9,	// clean
	ITEMID_COPPER_C1	= 0x0EEA,
	ITEMID_GOLD_C1		= 0x0EED,	// big pile
	ITEMID_SILVER_C1	= 0x0ef0,	
	ITEMID_SILVER_C3	= 0x0ef2,

	ITEMID_SCROLL1_BLANK= 0x0ef3,
	ITEMID_SCROLL1_B1	= 0x0ef4,
	ITEMID_SCROLL1_B6	= 0x0ef9,
	ITEMID_SPELLBOOK	= 0x0EFA,

	ITEMID_BOTTLE2_1	= 0x0efb,
	ITEMID_BOTTLE2_DYE	= 0x0eff,
	ITEMID_BOTTLE2_10	= 0x0f04,

	ITEMID_POTION_BLACK	= 0x0f06,
	ITEMID_POTION_ORANGE,
	ITEMID_POTION_BLUE,
	ITEMID_POTION_WHITE,
	ITEMID_POTION_GREEN,
	ITEMID_POTION_RED,
	ITEMID_POTION_YELLOW,
	ITEMID_POTION_PURPLE= 0x0f0d,
	ITEMID_EMPTY_BOTTLE = 0x0f0e,

	ITEMID_GEM1			= 0x0f0f,
	ITEMID_GEMS			= 0x0F20,
	ITEMID_GEML			= 0x0F30,

	ITEMID_HAY			= 0x0f36,	// sheif of hay.

	ITEMID_Shovel1		= 0x0f39,
	ITEMID_Shovel2		= 0x0f3a,

	ITEMID_FRUIT_HAY1		= 0x0f36,
	ITEMID_Arrow		= 0x0f3f,	// Need these to use a bow.
	ITEMID_ArrowX       = 0x0f42,
	ITEMID_DAGGER		= 0x0F51,

	ITEMID_MOONGATE_BLUE	= 0x0f6c,
	ITEMID_REAG_1		= 0x0f78,	// batwing

	ITEMID_REAG_BP		= 0x0f7a,	// black pearl.
	ITEMID_REAG_BM		= 0x0f7b,	//'Blood Moss'
	ITEMID_REAG_GA		= 0x0f84,	//'Garlic'
	ITEMID_REAG_GI		= 0x0f85,	//'Ginseng'
	ITEMID_REAG_MR		= 0x0f86,	//'Mandrake Root'
	ITEMID_REAG_NS		= 0x0f88,	//'Nightshade'
	ITEMID_REAG_SA		= 0x0f8c,	//'Sulfurous Ash'
	ITEMID_REAG_SS		= 0x0f8d,	//'Spider's Silk'

	ITEMID_REAG_26		= 0x0f91,	// Worms heart

	ITEMID_CLOTH_BOLT1	= 0x0f95,
	ITEMID_CLOTH_BOLT8	= 0x0f9c,
	ITEMID_SEWINGKIT	= 0x0f9d,
	ITEMID_SCISSORS3	= 0x0f9e,
	ITEMID_SCISSORS4	= 0x0f9f,
	ITEMID_THREAD1		= 0x0fa0,
	ITEMID_THREAD2		= 0x0fa1,

	ITEMID_GAME_BOARD	= 0x0fa6,
	ITEMID_GAME_DICE	= 0x0fa7,

	ITEMID_DYE			= 0x0FA9,
	ITEMID_DYEVAT		= 0x0FAB,
	ITEMID_GAME_BACKGAM_2 = 0x0fad,
	ITEMID_BARREL_2		= 0x0FAE,
	ITEMID_ANVIL1		= 0x0FAF,
	ITEMID_ANVIL2		= 0x0FB0,
	ITEMID_FORGE_1		= 0x0FB1,

	ITEMID_BOOK1		= 0x0fbd,
	ITEMID_BOOK2		= 0x0fbe,

	ITEMID_BOOK3		= 0x0fef,
	ITEMID_BOOK8		= 0x0ff4,

	ITEMID_PITCHER_WATER = 0x0ff8,

	ITEMID_ARCHERYBUTTE_E	= 0x100a,
	ITEMID_ARCHERYBUTTE_S	= 0x100b,

	ITEMID_FRUIT_HAY2		= 0x100c,
	ITEMID_FRUIT_HAY3		= 0x100d,

	ITEMID_KEY_COPPER	= 0x100e,
	// ...
	ITEMID_KEY_RING0	= 0x1011,
	ITEMID_KEY_MAGIC	= 0x1012,
	ITEMID_KEY_RUSTY	= 0x1013,

	ITEMID_SPINWHEEL1	= 0x1015,
	ITEMID_SPINWHEEL2	= 0x1019,
	ITEMID_WOOL2		= 0x101f,

	ITEMID_SHAFTS		= 0x1024,
	ITEMID_SHAFTS2		= 0x1025,

ITEMID_CHISELS_1	= 0x1026,
	ITEMID_CHISELS_2	= 0x1027,
	ITEMID_DOVETAIL_SAW_1	= 0x1028,
	ITEMID_DOVETAIL_SAW_2	= 0x1029,
	ITEMID_HAMMER_1		= 0x102a,
	ITEMID_HAMMER_2		= 0x102b,
	ITEMID_SAW_1		= 0x1034,
	ITEMID_SAW_2		= 0x1035,

	ITEMID_FOOD_BREAD   = 0x103b,
	ITEMID_FOOD_DOUGH_RAW = 0x103d,
	ITEMID_FOOD_COOKIE_RAW = 0x103f,
	ITEMID_FLOUR		= 0x1039,
	ITEMID_FOOD_PIZZA	= 0x1040,
	ITEMID_FOOD_PIE		= 0x1041,
	ITEMID_FOOD_PIE_RAW	= 0x1042,

	ITEMID_CLOCK1		= 0x104B,
	ITEMID_CLOCK2		= 0x104C,

	ITEMID_SEXTANT_1	= 0x1057,
	ITEMID_SEXTANT_2	= 0x1058,
	ITEMID_LOOM1		= 0x105f,
	ITEMID_LOOM2		= 0x1063,

	ITEMID_LEATHER_1	= 0x1067,
	ITEMID_LEATHER_2	= 0x1068,

	ITEMID_DUMMY1		= 0x1070,	// normal training dummy.
	ITEMID_FX_DUMMY1	= 0x1071,
	ITEMID_DUMMY2		= 0x1074,
	ITEMID_FX_DUMMY2	= 0x1075,

	ITEMID_HIDES		= 0x1078,
	ITEMID_HIDES_2		= 0x1079,
	ITEMID_FOOD_PIZZA_RAW = 0x1083,

	ITEMID_WEB2_1		= 0x10b8,
	ITEMID_WEB2_x		= 0x10d7,	

	ITEMID_TRAP_FACE1	= 0x10f5,
	ITEMID_TRAP_FX_FACE1 = 0x10f6,
	ITEMID_TRAP_FACE2	= 0x10fc,
	ITEMID_TRAP_FX_FACE2 = 0x10fd,

	ITEMID_FX_SPARKLES	= 0x1153,	// magic sparkles.

	ITEMID_GRAVE_1		= 0x1165,
	ITEMID_GRAVE_32		= 0x1184,

	ITEMID_TRAP_CRUMBLEFLOOR = 0x11c0,

	ITEMID_BED3_1		= 0x11ce,
	ITEMID_BED3_X		= 0x11d5,

	ITEMID_FURS			= 0x11fa,

	ITEMID_BLOOD1		= 0x122a,
	ITEMID_BLOOD6		= 0x122f,

	ITEMID_ROCK_B_LO	= 0x134f,	// boulder.
	ITEMID_ROCK_B_HI	= 0x1362,
	ITEMID_ROCK_2_LO	= 0x1363,
	ITEMID_ROCK_2_HI	= 0x136d,

	ITEMID_BOW1			= 0x13b1,
	ITEMID_BOW2,

	ITEMID_SMITH_HAMMER = 0x13E4,

	ITEMID_HERD_CROOK3		= 0x13f4,
	ITEMID_HERD_CROOK4		= 0x13f5,

	ITEMID_BEE_WAX		= 0x1423,
	ITEMID_GRAIN		= 0x1449,

	ITEMID_BONE_ARMS    = 0x144e,
	ITEMID_BONE_ARMOR   = 0x144f,
	ITEMID_BONE_GLOVES  = 0x1450,
	ITEMID_BONE_HELM	= 0x1451,
	ITEMID_BONE_LEGS    = 0x1452,

	ITEMID_TELESCOPE1	= 0x1459,	// Big telescope
	ITEMID_TELESCOPEX	= 0x149a,

	ITEMID_MAP			= 0x14EB,
	ITEMID_MAP_2		= 0x14ec,

	ITEMID_DEED1		= 0x14ef,
	ITEMID_DEED2		= 0x14f0,
	ITEMID_SHIP_PLANS1	= 0x14f1,
	ITEMID_SHIP_PLANS2	= 0x14f2,

	ITEMID_SPYGLASS_1	= 0x14f5,
	ITEMID_SPYGLASS_2	= 0x14f6,

	ITEMID_LOCKPICK1	= 0x14fb,
	ITEMID_LOCKPICK4	= 0x14fe,

	ITEMID_SHIRT1		= 0x1517,
	ITEMID_PANTS1		= 0x152E,
	ITEMID_PANTS_FANCY	= 0x1539,

	ITEMID_HELM_BEAR	= 0x1545,
	ITEMID_HELM_DEER	= 0x1547,
	ITEMID_MASK_TREE	= 0x1549,
	ITEMID_MASK_VOODOO	= 0x154b,

	ITEMID_FOOD_LEG1_RAW= 0x1607,
	ITEMID_FOOD_LEG1	= 0x1608,
	ITEMID_FOOD_LEG2_RAW= 0x1609,
	ITEMID_FOOD_LEG2	= 0x160a,

	ITEMID_FOOD_COOKIES = 0x160b,
	ITEMID_BLOOD_SPLAT	= 0x1645,

	ITEMID_WHIP1		= 0x166e,
	ITEMID_WHIP2		= 0x166f,

	ITEMID_SANDALS		 = 0x170d,
	ITEMID_SHOES		= 0x170F,

	ITEMID_HAT_WIZ		= 0x1718,
	ITEMID_HAT_JESTER	= 0x171c,

	ITEMID_FRUIT_BANANA1	= 0x171f,
	ITEMID_FRUIT_BANANA2	= 0x1720,
	ITEMID_FRUIT_COCONUT2= 0x1723,
	ITEMID_FRUIT_COCONUT3= 0x1724,
	ITEMID_FRUIT_COCONUT1= 0x1726,
	ITEMID_FRUIT_DATE1	= 0x1727,
	ITEMID_FRUIT_LEMON	= 0x1728,
	ITEMID_FRUIT_LIME		= 0x172a,
	ITEMID_FRUIT_PEACH2	= 0x172c,
	ITEMID_FRUIT_PEAR		= 0x172d,

	ITEMID_CLOTH1		= 0x175d,
	ITEMID_CLOTH8		= 0x1764,

	ITEMID_KEY_RING1	= 0x1769,
	ITEMID_KEY_RING3	= 0x176a,
	ITEMID_KEY_RING5	= 0x176b,

	ITEMID_ROCK_3_LO	= 0x1771,
	ITEMID_ROCK_3_HI	= 0x177c,

	ITEMID_ALCH_SYM_1	= 0x181d,
	ITEMID_ALCH_SYM_2	= 0x181e,
	ITEMID_ALCH_SYM_3	= 0x181f,
	ITEMID_ALCH_SYM_4	= 0x1820,
	ITEMID_ALCH_SYM_5	= 0x1821,
	ITEMID_ALCH_SYM_6	= 0x1822,
	ITEMID_ALCH_SYM_7	= 0x1823,
	ITEMID_ALCH_SYM_8	= 0x1824,
	ITEMID_ALCH_SYM_9	= 0x1825,
	ITEMID_ALCH_SYM_10	= 0x1826,
	ITEMID_ALCH_SYM_11	= 0x1827,
	ITEMID_ALCH_SYM_12	= 0x1828,

	ITEMID_FRUIT_MANDRAKE_ROOT1	= 0x18dd,
	ITEMID_FRUIT_MANDRAKE_ROOT2	= 0x18de,

	ITEMID_PLANT_MANDRAKE1	= 0x18df,
	ITEMID_PLANT_MANDRAKE2	= 0x18e0,
	ITEMID_PLANT_GARLIC1	= 0x18e1,
	ITEMID_PLANT_GARLIC2	= 0x18e2,
	ITEMID_FRUIT_GARLIC1	= 0x18e3,
	ITEMID_FRUIT_GARLIC2	= 0x18e4,
	ITEMID_PLANT_NIGHTSHADE1= 0x18e5,
	ITEMID_PLANT_NIGHTSHADE2= 0x18e6,
	ITEMID_FRUIT_NIGHTSHADE1		= 0x18e7,
	ITEMID_FRUIT_NIGHTSHADE2		= 0x18e8,
	ITEMID_PLANT_GENSENG1	= 0x18e9,
	ITEMID_PLANT_GENSENG2	= 0x18ea,
	ITEMID_FRUIT_GENSENG1= 0x18eb,
	ITEMID_FRUIT_GENSENG2= 0x18ec,

	ITEMID_Keg2			= 0x1940,

	ITEMID_BELLOWS_1	= 0x197a,
	ITEMID_BELLOWS_2	= 0x197b,
	ITEMID_BELLOWS_3	= 0x197c,
	ITEMID_BELLOWS_4	= 0x197d,
	ITEMID_FORGE_2		= 0x197e,
	ITEMID_FORGE_3		= 0x197f,
	ITEMID_FORGE_4		= 0x1980,
	ITEMID_FORGE_5		= 0x1981,
	ITEMID_FORGE_6		= 0x1982,
	ITEMID_FORGE_7		= 0x1983,
	ITEMID_FORGE_8		= 0x1984,
	ITEMID_FORGE_9		= 0x1985,
	ITEMID_BELLOWS_5	= 0x1986,
	ITEMID_BELLOWS_6	= 0x1987,
	ITEMID_BELLOWS_7	= 0x1988,
	ITEMID_BELLOWS_8	= 0x1989,
	ITEMID_FORGE_10		= 0x198a,
	ITEMID_FORGE_11		= 0x198b,
	ITEMID_FORGE_12		= 0x198c,
	ITEMID_FORGE_13		= 0x198d,
	ITEMID_FORGE_14		= 0x198e,
	ITEMID_FORGE_15		= 0x198f,
	ITEMID_FORGE_16		= 0x1990,
	ITEMID_FORGE_17		= 0x1991,
	ITEMID_BELLOWS_9	= 0x1992,
	ITEMID_BELLOWS_10	= 0x1993,
	ITEMID_BELLOWS_11	= 0x1994,
	ITEMID_BELLOWS_12	= 0x1995,
	ITEMID_FORGE_18		= 0x1996,
	ITEMID_FORGE_19		= 0x1997,
	ITEMID_FORGE_20		= 0x1998,
	ITEMID_FORGE_21		= 0x1999,
	ITEMID_FORGE_22		= 0x199a,
	ITEMID_FORGE_23		= 0x199b,
	ITEMID_FORGE_24		= 0x199c,
	ITEMID_FORGE_25		= 0x199d,
	ITEMID_BELLOWS_13	= 0x199e,
	ITEMID_BELLOWS_14	= 0x199f,
	ITEMID_BELLOWS_15	= 0x19a0,
	ITEMID_BELLOWS_16	= 0x19a1,
	ITEMID_FORGE_26		= 0x19a2,
	ITEMID_FORGE_27		= 0x19a3,
	ITEMID_FORGE_28		= 0x19a4,
	ITEMID_FORGE_29		= 0x19a5,
	ITEMID_FORGE_30		= 0x19a6,
	ITEMID_FORGE_31		= 0x19a7,
	ITEMID_FORGE_32		= 0x19a8,
	ITEMID_FORGE_33		= 0x19a9,

	ITEMID_FIRE			= 0x19ab,

	ITEMID_ORE_1			= 0x19b7, // can't mine this, it's just leftover from smelting
	ITEMID_ORE_3			= 0x19b8,
	ITEMID_ORE_4			= 0x19b9,
	ITEMID_ORE_2			= 0x19ba,

	ITEMID_PLANT_HAY1 = 0x1a92,
	ITEMID_PLANT_HAY2, 
	ITEMID_PLANT_HAY3,
	ITEMID_PLANT_HAY4,
	ITEMID_PLANT_HAY5 = 0x1a96,

	ITEMID_PLANT_FLAX1	= 0x1a99,
	ITEMID_PLANT_FLAX2	= 0x1a9a,
	ITEMID_PLANT_FLAX3	= 0x1a9b,

	ITEMID_FRUIT_FLAX1	= 0x1a9c,
	ITEMID_FRUIT_FLAX2	= 0x1a9d,
	ITEMID_FRUIT_HOPS		= 0x1aa2,

	ITEMID_PLANT_HOPS1	= 0x1a9e,
	ITEMID_PLANT_HOPS2	= 0x1a9f,
	ITEMID_PLANT_HOPS3	= 0x1aa0,
	ITEMID_PLANT_HOPS4	= 0x1aa1,

	ITEMID_MOONGATE_FX_RED  = 0x1ae5,
	ITEMID_MOONGATE_FX_BLUE = 0x1af3,

	ITEMID_LEAVES1		= 0x1b1f,
	ITEMID_LEAVES2		= 0x1b20,
	ITEMID_LEAVES3		= 0x1b21,
	ITEMID_LEAVES4		= 0x1b22,

	ITEMID_HOLE			= 0x1b71,

	ITEMID_FEATHERS3	= 0x1bd1,
	ITEMID_FEATHERS4	= 0x1bd2,
	ITEMID_FEATHERS5	= 0x1bd3,
	ITEMID_SHAFTS3		= 0x1bd4,
	ITEMID_SHAFTS4		= 0x1bd5,
	ITEMID_SHAFTS5		= 0x1bd6,
	ITEMID_LUMBER		= 0x1bd7,	// boards

	ITEMID_LOG_1		= 0x1bdd,
	ITEMID_LOG_2		= 0x1bde,
	ITEMID_LOG_3		= 0x1bdf,
	ITEMID_LOG_4		= 0x1be0,
	ITEMID_LOG_5		= 0x1be1,
	ITEMID_LOG_6		= 0x1be2,

	ITEMID_INGOT_COPPER = 0x1be3,
	ITEMID_INGOT_GOLD	= 0x1be9,
	ITEMID_INGOT_IRON	= 0x1bef,
	ITEMID_INGOT_SILVER	= 0x1bf5,	// 
	ITEMID_INGOTX		= 0x1BFA,
	ITEMID_XBolt		= 0x1bfb,
	ITEMID_XBoltX		= 0x1BFE,

	ITEMID_BOOK_OFTRUTH = 0x1c13,

	ITEMID_FOOD_FISH2	= 0x1e1c,	// ??? whole fish ?
	ITEMID_BOOK_X		= 0x1e20,

	ITEMID_PICKPOCKET_NS2 = 0x1e2c,
	ITEMID_PICKPOCKET_EW2 = 0x1e2d,

	ITEMID_Bulletin1	= 0x1e5e,	// secure trades are here also. bboard
	ITEMID_Bulletin2	= 0x1e5f,
	ITEMID_WorldGem		= 0x1ea7,	// Typically a spawn item
	ITEMID_CRATE6		= 0x1e80,
	ITEMID_CRATE6_2		= 0x1e81,	// flipped crate

	ITEMID_TINKER		= 0x1ebc,
	ITEMID_FRUIT_WHEAT	= 0x1ebd,
	ITEMID_SPROUT_WHEAT1	= 0x1ebe,
	ITEMID_SPROUT_WHEAT2	= 0x1ebf,

	ITEMID_PICKPOCKET_NS = 0x1ec0,
	ITEMID_PICKPOCKET_NS_FX,
	ITEMID_PICKPOCKET_EW = 0x1ec3,
	ITEMID_PICKPOCKET_EW_FX,

	ITEMID_GEM_LIGHT1	= 0x1ecd,
	ITEMID_GEM_LIGHT6	= 0x1ed2,

	ITEMID_DOOR_MAGIC_SI_NS	= 0x1ed9,
	ITEMID_DOOR_MAGIC_GR_NS	= 0x1ee2,
	ITEMID_DOOR_MAGIC_SI_EW	= 0x1EEc,	// not 1eeb
	ITEMID_DOOR_MAGIC_GR_EW	= 0x1ef4,

	ITEMID_SHIRT_FANCY	= 0x1EFD,

	ITEMID_ROBE			= 0x1F03,
	ITEMID_HELM_ORC		= 0x1f0b,
	ITEMID_RECALLRUNE	= 0x1f14,

	ITEMID_BEDROLL_O_W	= 0x1f24,	// open
	ITEMID_BEDROLL_O_E,
	ITEMID_BEDROLL_O_N,
	ITEMID_BEDROLL_O_S	= 0x1f27,

	ITEMID_SCROLL_1		= 0x1f2d,	// Reactive armor.
	ITEMID_SCROLL_2,	
	ITEMID_SCROLL_3,
	ITEMID_SCROLL_4,
	ITEMID_SCROLL_5,
	ITEMID_SCROLL_6,
	ITEMID_SCROLL_7,
	ITEMID_SCROLL_8,
	ITEMID_SCROLL_64	= 0x1f6c,	// summon water

	ITEMID_SCROLL_A		= 0x1f6d,
	ITEMID_SCROLL_B		= 0x1f6e,
	ITEMID_SCROLL_C		= 0x1f6f,
	ITEMID_SCROLL_D		= 0x1f70,
	ITEMID_SCROLL_E		= 0x1f71,
	ITEMID_SCROLL_F		= 0x1f72,

	ITEMID_BOOZE_LIQU_G1 = 0x1f85,
	ITEMID_BOOZE_LIQU_G2 = 0x1f86,
	ITEMID_BOOZE_LIQU_G3 = 0x1f87,
	ITEMID_BOOZE_LIQU_G4 = 0x1f88,
	ITEMID_BOOZE_WINE_G1 = 0x1f8d,
	ITEMID_BOOZE_WINE_G2 = 0x1f8e,
	ITEMID_BOOZE_WINE_G3 = 0x1f8f,
	ITEMID_BOOZE_WINE_G4 = 0x1f90,
	ITEMID_BOOZE_ALE_P1  = 0x1f95,
	ITEMID_BOOZE_ALE_P2  = 0x1f96,
	ITEMID_BOOZE_LIQU_P1 = 0x1f99,
	ITEMID_BOOZE_LIQU_P2 = 0x1f9a,
	ITEMID_BOOZE_WINE_P1 = 0x1f9b,
	ITEMID_BOOZE_WINE_P2 = 0x1f9c,

	ITEMID_MOONGATE_GRAY = 0x1fd4,

	ITEMID_DOOR_BAR_METAL  = 0x1fed,

	ITEMID_CORPSE		= 0x2006,	// This is all corpses.

	ITEMID_MEMORY		= 0x2007,	// NonGen Marker.
	ITEMID_NPC_1		= 0x2008,	// NPC peasant object ?
	ITEMID_NPC_X		= 0x2036,	// NPC 

	ITEMID_HAIR_SHORT		= 0x203B,
	ITEMID_HAIR_LONG		= 0x203C,
	ITEMID_HAIR_PONYTAIL	= 0x203D,
	ITEMID_HAIR_MOHAWK		= 0x2044,
	ITEMID_HAIR_PAGEBOY		= 0x2045,
	ITEMID_HAIR_BUNS		= 0x2046,
	ITEMID_HAIR_AFRO		= 0x2047,
	ITEMID_HAIR_RECEDING	= 0x2048,
	ITEMID_HAIR_2_PIGTAILS	= 0x2049,
	ITEMID_HAIR_TOPKNOT		= 0x204A,	// KRISNA

	ITEMID_BEARD_LONG		= 0x203E,
	ITEMID_BEARD_SHORT		= 0x203F,
	ITEMID_BEARD_GOATEE		= 0x2040,
	ITEMID_BEARD_MOUSTACHE	= 0x2041,
	ITEMID_BEARD_SH_M		= 0x204B,
	ITEMID_BEARD_LG_M		= 0x204C,
	ITEMID_BEARD_GO_M		= 0x204D,	// VANDYKE

	ITEMID_DEATHSHROUD		= 0x204E,
	ITEMID_GM_ROBE			= 0x204f,

	ITEMID_RHAND_POINT_NW	= 0x2053,	// point nw on the map.
	ITEMID_RHAND_POINT_W	= 0x205a,

	ITEMID_HAND_POINT_NW	= 0x206a,	// point nw on the map.
	ITEMID_HAND_POINT_W		= 0x2071,

	ITEMID_SPELL_1			= 0x2080,
	ITEMID_SPELL_6			= 0x2085,	// light or night sight.
	ITEMID_SPELL_64			= 0x20bf,

	ITEMID_SPELL_CIRCLE1	= 0x20c0,
	ITEMID_SPELL_CIRCLE8	= 0x20c7,

	// Item equiv of creatures.
	ITEMID_TRACK_BEGIN		= 0x20c8,
	ITEMID_TRACK_ETTIN		= 0x20c8,
	ITEMID_TRACK_MAN_NAKED	= 0x20cd,
	ITEMID_TRACK_ELEM_EARTH = 0x20d7,
	ITEMID_TRACK_OGRE		= 0x20df,
	ITEMID_TRACK_TROLL		= 0x20e9,
	ITEMID_TRACK_ELEM_AIR	= 0x20ed,
	ITEMID_TRACK_ELEM_FIRE  = 0x20f3,
	ITEMID_TRACK_SEASERP	= 0x20fe,
	ITEMID_TRACK_WISP		= 0x2100,	
	ITEMID_TRACK_MAN		= 0x2106,
	ITEMID_TRACK_WOMAN		= 0x2107,
	ITEMID_TRACK_ELEM_WATER = 0x210b,	
	ITEMID_TRACK_HORSE		= 0x2120,
	ITEMID_TRACK_RABBIT		= 0x2125,
	ITEMID_TRACK_PACK_HORSE	= 0x2126,
	ITEMID_TRACK_PACK_LLAMA	= 0x2127,
	ITEMID_TRACK_END		= 0x213e,	

	ITEMID_VENDOR_BOX		= 0x2af8, // Vendor container 

	ITEMID_ROCK_4_LO		= 0x3421,
	ITEMID_ROCK_4_HI		= 0x3435,
	ITEMID_ROCK_5_LO		= 0x3486,
	ITEMID_ROCK_5_HI		= 0x348f,
	ITEMID_ROCK_6_LO		= 0x34ac,
	ITEMID_ROCK_6_HI		= 0x34b4,

	// effects.
	ITEMID_FX_SPLASH		= 0x352d,

	ITEMID_GAME1_CHECKER= 0x3584,	// white
	ITEMID_GAME1_BISHOP = 0x3585,
	ITEMID_GAME1_ROOK	= 0x3586,
	ITEMID_GAME1_QUEEN	= 0x3587,
	ITEMID_GAME1_KNIGHT	= 0x3588,
	ITEMID_GAME1_PAWN	= 0x3589, 
	ITEMID_GAME1_KING	= 0x358a,

	ITEMID_GAME2_CHECKER= 0x358b,	// brown
	ITEMID_GAME2_BISHOP = 0x358c,
	ITEMID_GAME2_ROOK	= 0x358d,
	ITEMID_GAME2_QUEEN	= 0x358e,
	ITEMID_GAME2_KNIGHT	= 0x358f,
	ITEMID_GAME2_PAWN	= 0x3590, 
	ITEMID_GAME2_KING	= 0x3591,

	ITEMID_GAME_HI		= 0x35a1,	// ?

	ITEMID_FX_EXPLODE_3	= 0x36b0,
	ITEMID_FX_EXPLODE_2	= 0x36bd,
	ITEMID_FX_EXPLODE_1	= 0x36ca,
	ITEMID_FX_FIRE_BALL	= 0x36d4,
	ITEMID_FX_MAGIC_ARROW	= 0x36e4,
	ITEMID_FX_FIRE_BOLT	= 0x36f4, // fire snake
	ITEMID_FX_EXPLODE_BALL = 0x36fe, // Not used.
	ITEMID_FX_FLAMESTRIKE	= 0x3709,
	ITEMID_FX_TELE_VANISH	= 0x372A,	// teleport vanish
	ITEMID_FX_SPELL_FAIL	= 0x3735,
	ITEMID_FX_BLESS_EFFECT	= 0x373A,
	ITEMID_FX_CURSE_EFFECT	= 0x374A,
	ITEMID_FX_SPARK_EFFECT	= 0x375A,	// UNUSED
	ITEMID_FX_HEAL_EFFECT	= 0x376A,
	ITEMID_FX_ADVANCE_EFFECT= 0x3779,	// sparkle.
	ITEMID_FX_BLUEMOONSTART= 0x3789,	// ? swirl "death vortex"
	ITEMID_FX_ENERGY_BOLT	= 0x379f,
	ITEMID_FX_BLADES_EMERGE = 0x37a0,	// unused
	ITEMID_FX_GLOW			= 0x37b9,	// unused
	ITEMID_FX_GLOW_SPIKE	= 0x37c3,	// unused "glow" spike
	ITEMID_FX_DEATH_FUNNEL	= 0x37cc,	// "Death votex" funnel
	ITEMID_FX_BLADES		= 0x37eb,
	ITEMID_FX_STATIC		= 0x3818,	// unused. pink static.
	ITEMID_FX_POISON_F_EW	= 0x3914,
	ITEMID_FX_POISON_F_NS	= 0x3920,
	ITEMID_FX_ENERGY_F_EW	= 0x3947,
	ITEMID_FX_ENERGY_F_NS	= 0x3956,
	ITEMID_FX_PARA_F_EW	= 0x3967,
	ITEMID_FX_PARA_F_NS	= 0x3979,
	ITEMID_FX_FIRE_F_EW	= 0x398c,	// E/W
	ITEMID_FX_FIRE_F_NS	= 0x3996,	// N/S

	ITEMID_SHIP_TILLER_1	= 0x3e4a,
	ITEMID_SHIP_TILLER_2	= 0x3e4b,
	ITEMID_SHIP_TILLER_3	= 0x3e4c,
	ITEMID_SHIP_TILLER_4	= 0x3e4d,
	ITEMID_SHIP_TILLER_5	= 0x3e4e,
	ITEMID_SHIP_TILLER_6,
	ITEMID_SHIP_TILLER_7	= 0x3e50,
	ITEMID_SHIP_TILLER_8,
	ITEMID_SHIP_TILLER_12	= 0x3e55,

	ITEMID_SHIP_PLANK_S2_O	= 0x3e84,
	ITEMID_SHIP_PLANK_S2_C	= 0x3e85,
	ITEMID_SHIP_PLANK_S_O	= 0x3e86,
	ITEMID_SHIP_PLANK_S_C	= 0x3e87,
	ITEMID_SHIP_PLANK_N_O	= 0x3e89,
	ITEMID_SHIP_PLANK_N_C	= 0x3e8a,

	ITEMID_M_HORSE1		= 0x3E9F,	// horse item when ridden	
	ITEMID_M_HORSE2		= 0x3EA0,
	ITEMID_M_HORSE3		= 0x3EA1,
	ITEMID_M_HORSE4		= 0x3EA2,
	ITEMID_M_OSTARD_DES	= 0x3ea3,	// t2A
	ITEMID_M_OSTARD_Frenz = 0x3ea4,	// t2A
	ITEMID_M_OSTARD_For	= 0x3ea5,	// t2A
	ITEMID_M_LLAMA		= 0x3ea6,	// t2A

	ITEMID_SHIP_PLANK_E_C	= 0x3ea9,
	ITEMID_SHIP_PLANK_W_C	= 0x3eb1,
	ITEMID_SHIP_PLANK_E2_C	= 0x3eb2,
	ITEMID_SHIP_PLANK_E_O	= 0x3ed3,
	ITEMID_SHIP_PLANK_E2_O	= 0x3ed4,
	ITEMID_SHIP_PLANK_W_O	= 0x3ed5,

	ITEMID_SHIP_HATCH_E = 0x3e65,	// for an east bound ship.
	ITEMID_SHIP_HATCH_W	= 0x3e93,
	ITEMID_SHIP_HATCH_N	= 0x3eae,
	ITEMID_SHIP_HATCH_S	= 0x3eb9,

	ITEMID_CORPSE_1	= 0x3d64,	// 'dead orc captain'
	ITEMID_CORPSE_2,	// 'corpse of orc'
	ITEMID_CORPSE_3,	// 'corpse of skeleton
	ITEMID_CORPSE_4,	// 'corpse'
	ITEMID_CORPSE_5,	// 'corpse'
	ITEMID_CORPSE_6,	// 'deer corpse'
	ITEMID_CORPSE_7,	// 'wolf corpse'
	ITEMID_CORPSE_8,	// 'corpse of rabbit'

	// Large composite objects here.
	ITEMID_MULTI		= 0x4000,
	ITEMID_SHIP1_N		= 0x4000,
	ITEMID_SHIP1_E		= 0x4001,
	ITEMID_SHIP1_S		= 0x4002,
	ITEMID_SHIP1_W		= 0x4003,
	ITEMID_SHIP2_N		= 0x4004,
	ITEMID_SHIP2_E,
	ITEMID_SHIP2_S,
	ITEMID_SHIP2_W,
	ITEMID_SHIP3_N		= 0x4008,
	ITEMID_SHIP4_N		= 0x400c,
	ITEMID_SHIP5_N		= 0x4010,
	ITEMID_SHIP6_N		= 0x4014,
	ITEMID_SHIP6_E,
	ITEMID_SHIP6_S,
	ITEMID_SHIP6_W   	= 0x4017,

	ITEMID_HOUSE		= 0x4064,
	ITEMID_HOUSE_FORGE	= 0x4065,
	ITEMID_HOUSE_STONE	= 0x4066,
	ITEMID_TENT_BLUE	= 0x4070,
	ITEMID_TENT_GREEN	= 0x4072,
	ITEMID_3ROOM		= 0x4074,	// 3 room house
	ITEMID_2STORY_STUKO = 0x4076,
	ITEMID_2STORY_SAND	= 0x4078,
	ITEMID_TOWER		= 0x407a,
	ITEMID_KEEP		 	= 0x407C,	// keep
	ITEMID_CASTLE		= 0x407E,	// castle 7f also.

	ITEMID_LARGESHOP	= 0x408c,	// in verdata.mul file.

	// Custom multi's
	ITEMID_M_CARPET,

	ITEMID_MULTI_MAX	= (ITEMID_MULTI + MULTI_QTY - 1),	// ??? this is higher than next !
	ITEMID_MULTI_EXT_1	= 0x4BB8,
	ITEMID_MULTI_EXT_2	= 0x5388,
	// Special scriptable objects defined after this.
	ITEMID_SCRIPT		= 0x4100,	// Script objects beyond here.
	ITEMID_SCRIPT1		= 0x5000,	// This should be the first scripted object really 
	ITEMID_M_CAGE		= 0x50a0,
	ITEMID_SCRIPT2		= 0x6000,	// Safe area for server admins.
	
	ITEMID_QTY			= 0x8000,
	ITEMID_TEMPLATE		= 0xFFFF,	// container item templates are beyond here.
	//ABYSS STUFF
	ITEMID_NECRO_BOOK   = 0x2593    //ABYSS NECRO BOOK

};

// Door ID Attribute flags. 
#define DOOR_OPENED			0x00000001	// set is open
#define DOOR_RIGHTLEFT		0x00000002
#define DOOR_INOUT			0x00000004
#define DOOR_NORTHSOUTH		0x00000008

enum CREID_TYPE		// enum the creature art work. (dont allow any others !) also know as "model number"
{    
	CREID_INVALID		= 0,

	CREID_OGRE			= 0x0001,
	CREID_ETTIN			= 0x0002,		
	CREID_ZOMBIE		= 0x0003,
	CREID_GARGOYLE		= 0x0004,
	CREID_EAGLE			= 0x0005,
	CREID_BIRD			= 0x0006,	
	CREID_ORC_LORD		= 0x0007,
	CREID_CORPSER		= 0x0008,
	CREID_DAEMON		= 0x0009,
	CREID_DAEMON_SWORD	= 0x000A,

	CREID_DRAGON_GREY	= 0x000c,
	CREID_AIR_ELEM		= 0x000d,
	CREID_EARTH_ELEM	= 0x000e,
	CREID_FIRE_ELEM		= 0x000f,
	CREID_WATER_ELEM	= 0x0010,
	CREID_ORC			= 0x0011,
	CREID_ETTIN_AXE		= 0x0012,

	CREID_GIANT_SNAKE	= 0x0015,
	CREID_GAZER			= 0x0016,

	CREID_LICH			= 0x0018,

	CREID_SPECTRE		= 0x001a,

	CREID_GIANT_SPIDER	= 0x001c,
	CREID_GORILLA		= 0x001d,
	CREID_HARPY			= 0x001e,
	CREID_HEADLESS		= 0x001f,

	CREID_LIZMAN		= 0x0021,
	CREID_LIZMAN_SPEAR	= 0x0023,
	CREID_LIZMAN_MACE	= 0x0024,

	CREID_MONGBAT		= 0x0027,

	CREID_ORC_CLUB		= 0x0029,
	CREID_RATMAN		= 0x002a,

	CREID_RATMAN_CLUB	= 0x002c,
	CREID_RATMAN_SWORD	= 0x002d,

	CREID_REAPER		= 0x002f,	// tree
	CREID_SCORP			= 0x0030,	// giant scorp.

	CREID_SKELETON      = 0x0032,
	CREID_SLIME			= 0x0033,
	CREID_Snake			= 0x0034,
	CREID_TROLL_SWORD	= 0x0035,
	CREID_TROLL			= 0x0036,
	CREID_TROLL_MACE	= 0x0037,
	CREID_SKEL_AXE		= 0x0038,
	CREID_SKEL_SW_SH	= 0x0039,	// sword and sheild
	CREID_WISP			= 0x003a,
	CREID_DRAGON_RED	= 0x003b,
	CREID_DRAKE_GREY	= 0x003c,
	CREID_DRAKE_RED		= 0x003d,

	CREID_Tera_Warrior	= 0x0046,	// T2A 0x46 = Terathen Warrior
	CREID_Tera_Drone	= 0x0047,	// T2A 0x47 = Terathen Drone
	CREID_Tera_Matriarch= 0x0048,	// T2A 0x48 = Terathen Matriarch

	CREID_Titan			= 0x004b,	// T2A 0x4b = Titan
	CREID_Cyclops		= 0x004c,	// T2A 0x4c = Cyclops
	CREID_Giant_Toad	= 0x0050,	// T2A 0x50 = Giant Toad
	CREID_Bull_Frog		= 0x0051,	// T2A 0x51 = Bull Frog

	CREID_Ophid_Mage	= 0x0055,	// T2A 0x55 = Ophidian Mage
	CREID_Ophid_Warrior	= 0x0056,	// T2A 0x56 = Ophidian Warrior
	CREID_Ophid_Queen	= 0x0057,	// T2A 0x57 = Ophidian Queen
	CREID_SEA_Creature	= 0x005f,	// T2A 0x5f = (Unknown Sea Creature)

	CREID_SEA_SERP		= 0x0096,
	CREID_Dolphin		= 0x0097,

	// Animals (Low detail critters)

	CREID_HORSE1		= 0x00C8,	// white = 200 decinal
	CREID_Cat			= 0x00c9,
	CREID_Alligator		= 0x00CA,
	CREID_Pig			= 0x00CB,
	CREID_HORSE4		= 0x00CC,	// brown
	CREID_Rabbit		= 0x00CD,
	CREID_LavaLizard	= 0x00ce,	// T2A = Lava Lizard
	CREID_Sheep			= 0x00CF,	// un-sheered.
	CREID_Chicken		= 0x00D0,
	CREID_Goat			= 0x00d1,
	CREID_Ostard_Desert = 0x00d2,	// T2A = Desert Ostard (ridable)
	CREID_BrownBear		= 0x00D3,
	CREID_GrizzlyBear	= 0x00D4,
	CREID_PolarBear		= 0x00D5,
	CREID_Panther		= 0x00d6,
	CREID_GiantRat		= 0x00D7,
	CREID_Cow_BW		= 0x00d8,
	CREID_Dog			= 0x00D9,
	CREID_Ostard_Frenz	= 0x00da,	// T2A = Frenzied Ostard (ridable)
	CREID_Ostard_Forest = 0x00db,	// T2A = Forest Ostard (ridable)
	CREID_Llama			= 0x00dc,
	CREID_Walrus		= 0x00dd,
	CREID_Sheep_Sheered	= 0x00df,	
	CREID_Wolf			= 0x00e1,
	CREID_HORSE2		= 0x00E2,
	CREID_HORSE3		= 0x00E4,
	CREID_Cow2			= 0x00e7,
	CREID_Bull_Brown	= 0x00e8,	// light brown
	CREID_Bull2			= 0x00e9,	// dark brown
	CREID_Hart			= 0x00EA,	// Male deer.
	CREID_Deer			= 0x00ED,
	CREID_Rat			= 0x00ee,
	
	CREID_Boar			= 0x0122,	// large pig
	CREID_HORSE_PACK	= 0x0123,	// Pack horse with saddle bags
	CREID_LLAMA_PACK	= 0x0124,	// Pack llama with saddle bags

	// all below here are humanish or clothing.
	CREID_MAN			= 0x0190,	// 400 decimal
	CREID_WOMAN			= 0x0191,
	CREID_GHOSTMAN		= 0x0192,	// Ghost robe is not automatic !
	CREID_GHOSTWOMAN	= 0x0193,
	CREID_EQUIP,

	// removed from T2A
	CREID_CHILD_MB		= 0x01a4, // Male Kid (Blond Hair)
	CREID_CHILD_MD		= 0x01a5, // Male Kid (Dark Hair)
	CREID_CHILD_FB		= 0x01a6, // Female Kid (Blond Hair) (toddler)
	CREID_CHILD_FD		= 0x01a7, // Female Kid (Dark Hair)

	CREID_VORTEX		= 0x023d,	// T2A = vortex
	CREID_BLADES		= 0x023e,	// blade spirits (in human range? not sure why)

	CREID_EQUIP_GM_ROBE = 0x03db,

	CREID_QTY			= 0x0400,	// Max number of base character types, based on art work.

	// re-use artwork to make other types on NPC's
	NPCID_SCRIPT		= 0x401,
	NPCID_Man_Guard		= 0x438,
	NPCID_Woman_Guard	= 0x439,

	NPCID_Cougar		= 0x600,
	NPCID_Lion,
	NPCID_Bird,
	NPCID_JungleBird,
	NPCID_Parrot,
	NPCID_Raven,

	NPCID_Ogre_Lord,
	NPCID_Ogre_Mage,

	NPCID_Energy_Vortex	= 0x1063,	// Normal (non-T2A) = vortex

	NPCID_SCRIPT2 = 0x4000,	// Safe area for server specific NPC defintions.
	NPCID_Qty = 0x8000,		// Spawn types start here.

	SPAWNTYPE_START  = 0x8001,
	SPAWNTYPE_UNDEAD = 0x8001, // undead, grave yard.
	SPAWNTYPE_ELEMS	 = 0x8002, // elementals
	SPAWNTYPE_SEWER	 = 0x8003, // dungeon/sewer vermin. rats, snakes, spiders, ratmen
	SPAWNTYPE_WOODS	 = 0x8004, // woodsy.
	SPAWNTYPE_WATER	 = 0x8005, // water
	SPAWNTYPE_DESERT = 0x8006, // desert creatures, scorps, 
	SPAWNTYPE_DOMESTIC= 0x8007, // domestic
	SPAWNTYPE_ORC	 = 0x8008, // Orc Camp, Orcs and Wargs.
	SPAWNTYPE_LIZARDMAN= 0x8009, // Lizardman Camp
	SPAWNTYPE_GIANTS = 0x800a, // Giant kin, Ogre, Trolls. Ettins
	SPAWNTYPE_MAGIC  = 0x800b, // Magic forest. Reapers, corpsers, 
	SPAWNTYPE_FLY	= 0x800c,	// Flying creatures.	
	SPAWNTYPE_DRAGONS, 
};

enum ANIM_TYPE	// not all creatures animate the same for some reason.
{
	ANIM_WALK_UNARM		= 0x00,	// Walk (unarmed) 

	// human anim. (supported by all humans)
	ANIM_WALK_ARM		= 0x01,	// Walk (armed) (but not war mode)

	ANIM_RUN_UNARM		= 0x02,	// Run (unarmed)
	ANIM_RUN_ARMED		= 0x03,	// Run (armed)

	ANIM_STAND			= 0x04,		// armed or unarmed.

	ANIM_FIDGET1		= 0x05,		// Look around
	ANIM_FIDGET2		= 0x06,		// Fidget 

	ANIM_STAND_WAR_1H	= 0x07,	// Stand for 1 hand attack.
	ANIM_STAND_WAR_2H	= 0x08,	// Stand for 2 hand attack.

	ANIM_ATTACK_WEAPON	= 0x09,	// 1H generic melee swing, Any weapon.
	ANIM_ATTACK_1H_WIDE	= 0x09,	// 1H sword type
	ANIM_ATTACK_1H_JAB	= 0x0a,	// 1H fencing jab - fencing type.
	ANIM_ATTACK_1H_DOWN	= 0x0b,	// 1H overhead mace - mace type

	ANIM_ATTACK_2H_DOWN = 0x0c,	// 2H mace jab
	ANIM_ATTACK_2H_WIDE = 0x0d,	// 2H generic melee swing 
	ANIM_ATTACK_2H_JAB	= 0x0e,	// 2H spear jab

	ANIM_WALK_WAR		= 0x0f,	// Walk (warmode)

	ANIM_CAST_DIR		= 0x10,	// Directional spellcast 
	ANIM_CAST_AREA		= 0x11,	// Area-effect spellcast 

	ANIM_ATTACK_BOW		= 0x12,	// Bow attack / Mounted bow attack 
	ANIM_ATTACK_XBOW	= 0x13,	// Crossbow attack
	ANIM_GET_HIT		= 0x14,	// Take a hit

	ANIM_DIE_BACK		= 0x15,	// (Die onto back) 
	ANIM_DIE_FORWARD	= 0x16,	// (Die onto face) 

	ANIM_TURN			= 0x1e,	// Dodge 
	ANIM_ATTACK_UNARM 	= 0x1f,	// Punch - attack while walking ?

	ANIM_BOW			= 0x20,
	ANIM_SALUTE			= 0x21,
	ANIM_EAT			= 0x22,

	// don't use these directly these are just for translation.
	// Human on horseback
	ANIM_HORSE_RIDE_SLOW	= 0x17,
	ANIM_HORSE_RIDE_FAST	= 0x18,
	ANIM_HORSE_STAND		= 0x19,
	ANIM_HORSE_ATTACK		= 0x1a,
	ANIM_HORSE_ATTACK_BOW	= 0x1b,
	ANIM_HORSE_ATTACK_XBOW	= 0x1c,
	ANIM_HORSE_SLAP			= 0x1d,

	ANIM_QTY_MAN = 35,	// 0x23

	// monster anim	- (not all anims are supported for each creature)
	ANIM_MON_WALK 		= 0x00,
	ANIM_MON_STAND		= 0x01,
	ANIM_MON_DIE1		= 0x02,	// back
	ANIM_MON_DIE2		= 0x03, // fore or side.
	ANIM_MON_ATTACK1	= 0x04,	// ALL creatures have at least this attack.
	ANIM_MON_ATTACK2	= 0x05,	// swimming monsteers don't have this.
	ANIM_MON_ATTACK3	= 0x06,

	ANIM_MON_STUMBLE 	= 0x0a,
	ANIM_MON_MISC_STOMP	= 0x0b,	// Misc, Stomp, slap ground, lich conjure.
	ANIM_MON_MISC_BREATH= 0x0c,	// Misc Cast, breath fire, elem creation.
	ANIM_MON_GETHIT1	= 0x0d,	// Trolls don't have this.

	ANIM_MON_GETHIT2	= 0x0f,
	ANIM_MON_GETHIT3	= 0x10,

	ANIM_MON_FIDGET1	= 0x11,
	ANIM_MON_FIDGET2	= 0x12,
	ANIM_MON_MISC_ROLL 	= 0x00,	// ??? Misc Roll over, 
	ANIM_MON_MISC1		= 0x00, // ??? air/fire elem = flail arms.

	ANIM_MON_FLY		= 0x13,
	ANIM_MON_LAND		= 0x14,
	ANIM_MON_DIE_FLIGHT	= 0x15,

	ANIM_QTY_MON = 22,

	// animals. (Most All animals have all anims)
	ANIM_ANI_WALK		= 0x00,
	ANIM_ANI_RUN		= 0x01,
	ANIM_ANI_STAND		= 0x02,
	ANIM_ANI_EAT		= 0x03,

	ANIM_ANI_ATTACK1	= 0x05,
	ANIM_ANI_ATTACK2	= 0x06,
	ANIM_ANI_ATTACK3 	= 0x07,
	ANIM_ANI_DIE1 		= 0x08,
	ANIM_ANI_FIDGET1	= 0x09,
	ANIM_ANI_FIDGET2	= 0x0a,
	ANIM_ANI_SLEEP		= 0x0b,	// lie down (not all have this)
	ANIM_ANI_DIE2		= 0x0c,

	ANIM_QTY_ANI = 13,

	ANIM_QTY = 0x23,
};

enum CRESND_TYPE	// Creature sound offset types.
{	// Some creatures have no random sounds. others (humans,birds) have many sounds.
	CRESND_RAND1 = 0,	// just random noise. or "yes"
	CRESND_RAND2,		// "no" response
	CRESND_HIT,
	CRESND_GETHIT,
	CRESND_DIE,
};

enum TALKMODE_TYPE	// Modes we can talk/bark in.
{
	TALKMODE_SYSTEM = 0,	// normal system message
	TALKMODE_PROMPT,		// ? not sure about this but it works fine.
	TALKMODE_EMOTE = 2,		// :	*smiles*
	TALKMODE_SAY = 3,		// A chacter speaking.
	TALKMODE_4,				// ?
	TALKMODE_5,				// ?
	TALKMODE_ITEM = 6,		// text labeling an item. Preceeded by "You see"
	TALKMODE_7,				// ?
	TALKMODE_WHISPER = 8,	// ;	only those close can here.
	TALKMODE_YELL = 9,		// ! can be heard 2 screens away.
	TALKMODE_BROADCAST = 0xFF,
};

enum FONT_TYPE
{
	FONT_BOLD,		// 0 - Bold Text = Large plain filled block letters.
	FONT_SHAD,		// 1 - Text with shadow = small gray
	FONT_BOLD_SHAD,	// 2 - Bold+Shadow = Large Gray block letters.
	FONT_NORMAL,	// 3 - Normal (default) = Filled block letters.
	FONT_GOTH,		// 4 - Gothic = Very large blue letters.
	FONT_ITAL,		// 5 - Italic Script
	FONT_SM_DARK,	// 6 - Small Dark Letters = small Blue 
	FONT_COLOR,		// 7 - Colorful Font (Buggy?) = small Gray (hazy)
	FONT_RUNE,		// 8 - Rune font (Only use capital letters with this!)
	FONT_SM_LITE,	// 9 - Small Light Letters = small roman gray font.
	FONT_QTY,
};

enum DELETE_ERR_TYPE
{
	DELETE_ERR_BAD_PASS = 0, // 0 That character password is invalid.
	DELETE_ERR_NOT_EXIST,	// 1 That character does not exist.
	DELETE_ERR_IN_USE,	// 2 That character is being played right now.
	DELETE_ERR_NOT_OLD_ENOUGH, // 3 That character is not old enough to delete. The character must be 7 days old before it can be deleted.
	DELETE_ERR_QD_4_BACKUP, // 4 That character is currently queued for backup and cannot be deleted.
};

enum LOGIN_ERR_TYPE	// error codes sent to client.
{
	LOGIN_ERR_NONE = 0,		// no account
	LOGIN_ERR_USED = 1,		// already in use.
	LOGIN_ERR_BLOCKED = 2,	// we don't like you
	LOGIN_ERR_BAD_PASS = 3,
	LOGIN_ERR_OTHER,		// like timeout.
	LOGIN_SUCCESS	= 255,
};

enum DIR_TYPE	// Walking directions. m_dir
{
	DIR_N = 0,
	DIR_NE,
	DIR_E,
	DIR_SE,
	DIR_S,
	DIR_SW,
	DIR_W,
	DIR_NW,
	DIR_QTY,
};

enum SKILLLOCK_TYPE	
{
	SKILLLOCK_UP = 0,
	SKILLLOCK_DOWN,
	SKILLLOCK_LOCK,
};

enum SKILL_TYPE	// List of skill numbers (things that can be done at a given time)
{
	SKILL_NONE = -1,

	SKILL_ALCHEMY = 0,
	SKILL_ANATOMY,
	SKILL_ANIMALLORE,
	SKILL_ITEMID,
	SKILL_ARMSLORE,
	SKILL_PARRYING,
	SKILL_BEGGING,
	SKILL_BLACKSMITHING,
	SKILL_BOWCRAFT,
	SKILL_PEACEMAKING,
	SKILL_CAMPING,	// 10
	SKILL_CARPENTRY,
	SKILL_CARTOGRAPHY,
	SKILL_COOKING,
	SKILL_DETECTINGHIDDEN,
	SKILL_ENTICEMENT,
	SKILL_EVALINT,
	SKILL_HEALING,
	SKILL_FISHING,
	SKILL_FORENSICS,
	SKILL_HERDING,	// 20
	SKILL_HIDING,
	SKILL_PROVOCATION,
	SKILL_INSCRIPTION,
	SKILL_LOCKPICKING,
	SKILL_MAGERY,		// 25
	SKILL_MAGICRESISTANCE,
	SKILL_TACTICS,
	SKILL_SNOOPING,
	SKILL_MUSICIANSHIP,
	SKILL_POISONING,	// 30
	SKILL_ARCHERY,
	SKILL_SPIRITSPEAK,
	SKILL_STEALING,
	SKILL_TAILORING,
	SKILL_TAMING,
	SKILL_TASTEID,
	SKILL_TINKERING,
	SKILL_TRACKING,
	SKILL_VETERINARY,
	SKILL_SWORDSMANSHIP,	// 40
	SKILL_MACEFIGHTING,
	SKILL_FENCING,
	SKILL_WRESTLING,		// 43
	SKILL_LUMBERJACKING,
	SKILL_MINING,
	SKILL_MEDITATION,
	SKILL_Stealth,			// 47
	SKILL_RemoveTrap,		// 48
	SKILL_Necromancy,
	SKILL_Focus,
	SKILL_Chivalry,
	SKILL_Bushido,
	SKILL_Ninjitsu,
	SKILL_Spellweaving,
	SKILL_Mysticism,
	SKILL_Imbuing,
	SKILL_Throwing,
	SKILL_QTY,	// 58

	// Actions a npc will perform. (no need to track skill level for these)
	NPCACT_FOLLOW_TARG = 100,	// 100 = following a char.
	NPCACT_STAY,			// 101
	NPCACT_GOTO,			// 102 = Go to a location x,y
	NPCACT_WANDER,			// 103 = Wander aimlessly.
	NPCACT_UNUSED,			// 104 = ??? Just wander around a central point.
	NPCACT_FLEE,			// 105 = Run away from target.
	NPCACT_TALK,			// 106 = Talking to my target.
	NPCACT_TALK_FOLLOW,		// 107 = 
	NPCACT_GUARD_TARG,		// 108 = Guard a targetted object.
	NPCACT_GO_HOME,			// 109 =
	NPCACT_GO_FETCH,		// 110 = Pick up a thing.
	NPCACT_BREATH,			// 111 = Using breath weapon.
	NPCACT_RIDDEN,			// 112 = Being ridden or shrunk as figurine.
	NPCACT_EATING,			// 113 = Animate the eating action.
	NPCACT_LOOTING,			// 114 = Looting a corpse.
	NPCACT_THROWING,		// 115 = Throwing a stone.
	NPCACT_GO_FOOD,			// 116 = walking to food.
	NPCACT_QTY,
};

enum LAYER_TYPE		// defined by UO. Only one item can be in a slot.
{
	LAYER_NONE = 0,	// spells that are layed on the CChar ?
	LAYER_HAND1,	// 1 = spellbook or weapon.
	LAYER_HAND2,	// 2 = other hand or 2 handed thing. = shield
	LAYER_SHOES,	// 3 
	LAYER_PANTS,	// 4 = bone legs + pants.
	LAYER_SHIRT,
	LAYER_HELM,		// 6
	LAYER_GLOVES,	// 7
	LAYER_RING,		
	LAYER_UNUSED9,	// 9 = not used as far as i can see.
	LAYER_COLLAR,	// 10 = gorget or necklace.
	LAYER_HAIR,		// 11 = 0x0b =
	LAYER_HALF_APRON,// 12 = 0x0c =
    LAYER_CHEST,	// 13 = 0x0d = armor chest
	LAYER_WRIST,	// 14 = 0x0e = watch
	LAYER_PACK2,	// ??? 15 = 0x0f = alternate pack ? BANKBOX ?! 
	LAYER_BEARD,	// 16 = try to have only men have this.
	LAYER_TUNIC,	// 17 = jester suit or full apron.
	LAYER_EARS,		// 18 = earrings
    LAYER_ARMS,		// 19 = armor
	LAYER_CAPE,		// 20 = cape
	LAYER_PACK,		// 21 = 0x15 = only used by ITEMID_BACKPACK
	LAYER_ROBE,		// 22 = robe over all.
    LAYER_SKIRT,	// 23 = skirt or kilt.
    LAYER_LEGS,		// 24= 0x18 = plate legs.

#define LAYER_IS_VISIBLE(layer) ((layer)> LAYER_NONE && (layer) <= LAYER_HORSE )

	// These are not part of the paper doll (but get sent to the client)
	LAYER_HORSE,		// 25 = 0x19 = ride this object. (horse objects are strange?)
	LAYER_VENDOR_STOCK,	// 26 = 0x1a = the stuff the vendor will restock and sell to the players
	LAYER_VENDOR_EXTRA,	// 27 = 0x1b = the stuff the vendor will resell to players but is not restocked. (bought from players)
	LAYER_VENDOR_BUYS,	// 28 = 0x1c = the stuff the vendor can buy from players but does not stock.
	LAYER_BANKBOX,		// 29 = 0x1d = contents of my bank box.

	// Internally used layers - Don't bother sending these to client.
	LAYER_SPECIAL,		// 30 =	Can be multiple of these. memories
	LAYER_DRAGGING,

	// Spells that are effecting us go here.
	LAYER_SPELL_STATS,		// 32 = Stats effecting spell. These cancel each other out.
	LAYER_SPELL_Reactive,	// 33 = 
	LAYER_SPELL_Night_Sight,
	LAYER_SPELL_Protection,
	LAYER_SPELL_Incognito,
	LAYER_SPELL_Magic_Reflect,
	LAYER_SPELL_Paralyze,	// or turned to stone.
	LAYER_SPELL_Invis,
	LAYER_SPELL_Polymorph,
	LAYER_SPELL_Summon,	// magical summoned creature.

	LAYER_FLAG_Poison,
	LAYER_FLAG_Criminal,		// criminal or murderer ? 
	LAYER_FLAG_Potion,			// Some magic type effect done by a potion. (they cannot be dispelled)
	LAYER_FLAG_SpiritSpeak,
	LAYER_FLAG_Wool,			// regrowing wool.
	LAYER_FLAG_Drunk,			// Booze effect.
	LAYER_FLAG_ClientLinger,
	LAYER_FLAG_Hallucination,	// shrooms etc.
	LAYER_FLAG_PotionUsed,		// track the time till we can use a potion again.
	LAYER_FLAG_Stuck,			// In a trap or web.
	LAYER_FLAG_Murders,			// How many murders do we have ?

	LAYER_QTY,
};

enum SPELL_TYPE	// List of spell numbers in spell book.
{
	SPELL_NONE = 0,

	SPELL_Clumsy = 1,
	SPELL_Create_Food,
	SPELL_Feeblemind,
	SPELL_Heal,
	SPELL_Magic_Arrow,
	SPELL_Night_Sight,	// 6
	SPELL_Reactive_Armor,
	SPELL_Weaken,

	// 2nd
	SPELL_Agility,	// 9
	SPELL_Cunning,
	SPELL_Cure,
	SPELL_Harm,
	SPELL_Magic_Trap,
	SPELL_Magic_Untrap,
	SPELL_Protection,
	SPELL_Strength,	// 16

	// 3rd
	SPELL_Bless,		// 17
	SPELL_Fireball,
	SPELL_Magic_Lock,
	SPELL_Poison,
	SPELL_Telekin,
	SPELL_Teleport,
	SPELL_Unlock,
	SPELL_Wall_of_Stone,	// 24

	// 4th
	SPELL_Arch_Cure,	// 25
	SPELL_Arch_Prot,
	SPELL_Curse,
	SPELL_Fire_Field,
	SPELL_Great_Heal,
	SPELL_Lightning,	// 30
	SPELL_Mana_Drain,
	SPELL_Recall,

		// 5th
	SPELL_Blade_Spirit,
	SPELL_Dispel_Field,
	SPELL_Incognito,		// 35
	SPELL_Magic_Reflect,
	SPELL_Mind_Blast,
	SPELL_Paralyze,
	SPELL_Poison_Field,
	SPELL_Summon,			// 40

		// 6th
	SPELL_Dispel,
	SPELL_Energy_Bolt,
	SPELL_Explosion,
	SPELL_Invis,
	SPELL_Mark,
	SPELL_Mass_Curse,
	SPELL_Paralyze_Field,
	SPELL_Reveal,

		// 7th
	SPELL_Chain_Lightning,
	SPELL_Energy_Field,
	SPELL_Flame_Strike,
	SPELL_Gate_Travel,
	SPELL_Mana_Vamp,		// 53
	SPELL_Mass_Dispel,
	SPELL_Meteor_Swarm,	// 55
	SPELL_Polymorph,

		// 8th
	SPELL_Earthquake,
	SPELL_Vortex,
	SPELL_Resurrection,	// 59
	SPELL_Air_Elem,
	SPELL_Daemon,
	SPELL_Earth_Elem,
	SPELL_Fire_Elem,
	SPELL_Water_Elem,	// 64
	SPELL_BOOK_QTY = 65,

	// Necro
	SPELL_Summon_Undead = 65,
	SPELL_Animate_Dead,
	SPELL_Bone_Armor,
	SPELL_Light,
	SPELL_Fire_Bolt,		// 69
	SPELL_Hallucination,
	SPELL_BASE_QTY,		// monsters can use these.

	// Extra special spells.
	SPELL_Stone,			

	SPELL_QTY,
};

enum INPUTBOX_STYLE	// for the various styles for GumpText
{
	STYLE_NOEDIT	= 0,		// No textbox, just a message
	STYLE_TEXTEDIT	= 1,		// Alphanumeric
	STYLE_NUMEDIT	= 2,		// Numeric
};

enum LIGHT_PATTERN	// What pattern (m_light_pattern) does the light source (CAN_LIGHT) take.
{
	LIGHT_BEAM_E = 0,	// 0=Beam to the east.
	LIGHT_LARGE,		// 1=Large round area. (bright)
	LIGHT_MID,			// 2=mid sized globe. (dim)
	LIGHT_COLUMN_R,		// 3=large column of light.
	LIGHT_COLUMN_L,
	LIGHT_OVAL_L,
	// ... etc
	LIGHT_VERYBRIGHT = 29,
	// Colored light is in here some place as well.
	LIGHT_QTY = 56,
};

enum MAPCMD_TYPE
{
	MAP_ADD = 1,
	MAP_PIN = 1,
	MAP_INSERT = 2,
	MAP_MOVE = 3,
	MAP_DELETE = 4,
	MAP_UNSENT = 5,
	MAP_CLEAR = 5,
	MAP_TOGGLE = 6,
	MAP_SENT = 7,
	MAP_READONLY = 8,	// NOT USED ?
};

enum WEATHER_TYPE
{
	WEATHER_DEFAULT = -1,
	WEATHER_DRY = 0,
	WEATHER_RAIN,
	WEATHER_SNOW,
	WEATHER_CLOUDY,	// not client supported ?
};

enum SCROLL_TYPE	// Client messages for scrolls types.
{
	SCROLL_TYPE_TIPS = 0,	// type = 0 = TIPS
	SCROLL_TYPE_NOTICE = 1,
	SCROLL_TYPE_UPDATES = 2,// type = 2 = UPDATES
};

enum EFFECT_TYPE
{
	EFFECT_BOLT = 0,	// a targetted bolt
	EFFECT_LIGHTNING,	// lightning bolt.
	EFFECT_XYZ,	// Stay at current xyz ??? not sure about this.
	EFFECT_OBJ,	// effect at single Object.
};

enum NOTO_TYPE
{
	NOTO_INVALID = 0,	// 0= not a valid color!!
	NOTO_GOOD,		// 1= good(blue), 
	NOTO_GUILD_SAME,// 2= same guild, 
	NOTO_NEUTRAL,	// 3= Neutral, 
	NOTO_CRIMINAL,	// 4= criminal
	NOTO_GUILD_WAR,	// 5= Waring guilds, 
	NOTO_EVIL,		// 6= evil(red), 
};

enum GUMP_TYPE	// The gumps. (most of these are not useful to the server.)
{
	GUMP_NONE = 0,
	GUMP_RESERVED = 1,

	GUMP_CORPSE = 0x09,
	GUMP_GRAY_MAN = 0x0c,
	GUMP_GRAY_WOMAN = 0x0d,
	GUMP_VENDOR_RECT = 0x30,	// Big blue rectangle for vendor mask.

	GUMP_PACK		= 0x3c,	// Open backpack
	GUMP_BAG		= 0x3D,		// Leather Bag
	GUMP_BARREL		= 0x3E,		// Barrel
	GUMP_BASKET_SQ	= 0x3F,	// Square picknick Basket
	GUMP_BOX_WOOD	= 0x40,		// small wood box with a lock
	GUMP_BASKET_RO	= 0x41, // Round Basket
	GUMP_CHEST_GO_SI= 0x42,	// Gold and Silver Chest.
	GUMP_BOX_WOOD_OR= 0x43, // Small wood box (ornate)(no lock)
	GUMP_CRATE		= 0x44,	// Wood Crate

	GUMP_SCRAP		= 0x47,	// Scrap of cloth ?
	GUMP_DRAWER_DK	= 0x48,
	GUMP_CHEST_WO_GO= 0x49,	// Wood with gold trim.
	GUMP_CHEST_SI	= 0x4a,	// silver chest.
	GUMP_BOX_GO_LO	= 0x4b, // Gold/Brass box with a lock.
	GUMP_SHIP_HOLD	= 0x4c,	
	GUMP_BOOK_SHELF = 0x4d,
	GUMP_CABINET_DK = 0x4e,
	GUMP_CABINET_LT = 0x4f,
	GUMP_DRAWER_LT	= 0x51,

	GUMP_PLAQUE_C = 0x64,	// Wood plaque with copper face.
	GUMP_GRAVE_1 = 0x65,
	GUMP_GRAVE_2 = 0x66,
	GUMP_PLAQUE_W = 0x67,	// Wood plaque 

	GUMP_ITEM_AXE	= 0x28d,
	GUMP_ITEM_BRITISH = 0x3de,
	GUMP_ITEM_BLACKTHORN = 0x3df,

	GUMP_CHEST_OPEN = 0x3e8,
	GUMP_START_CONSOLE,

	GUMP_SCROLL_SM	= 0x4ca,
	GUMP_SCROLL_LG	= 0x4cc,

	GUMP_START_SKILL_PICK	= 0x4ce,

	GUMP_START_CITY_PICK	= 0x514,

	GUMP_FX_CHEST1 = 0x578,
	GUMP_FX_CHESTX = 0x587,

	//GUMP_HAIR_1			= 0x74e,
	//GUMP_HAIR_X			= 0x758,
	//GUMP_BEARD_1		= 0x759,
	//GUMP_BEARD_X		= 0x75f,

	//GUMP_WOMAN			= 0x760, 
	//GUMP_MAN			= 0x761, 

	// GUMP_ITEM_SHOES		= 0x762,

	GUMP_PFRAME_PLAYER	= 0x7d0,	// paperdoll frame.
	GUMP_PFRAME_CHAR	= 0x7d1,	// frame some other char. (not u)
	GUMP_PITEM_PROFILE	= 0x7d2,

	GUMP_SFRAME_PLAY_LG = 0x802,
	GUMP_SFRAME_PLAY_SM	= 0x803,
	GUMP_SFRAME_CHAR	= 0x804,	// status frame.
	GUMP_SFRAME_PLAY_RD	= 0x807,

	GUMP_MSG			= 0x816,

	GUMP_JOURNAL		= 0x820,

	GUMP_SECURE_TRADE	= 0x866,

	GUMP_VENDOR_1		= 0x870,

	GUMP_BBOARD			= 0x87a,

	GUMP_BOOK_2			= 0x898,
	GUMP_BOOK_3			= 0x899,
	GUMP_BOOK_4			= 0x89a,
	GUMP_BOOK_5			= 0x89b,

	GUMP_BOOK_TITLE_PAGE = 0x89c,
	GUMP_BOOK_CORNER_L	= 0x89d,
	GUMP_BOOK_CORNER_R	= 0x89e,

	GUMP_SPELL_BOOK		= 0x8ac,
	GUMP_SB_RIBBON_L	= 0x8ad,
	GUMP_SB_RIBBON_R	= 0x8ae,
	GUMP_SB_MARK_L		= 0x8af,
	GUMP_SB_MARK_R		= 0x8b0,
	GUMP_SB_SPC_1		= 0x8b1,
	GUMP_SB_SPC_8		= 0x8b8,
	GUMP_SB_ICON		= 0x8ba,

	GUMP_BOOK_CORNER_L_DUPE	= 0x8bb,
	GUMP_BOOK_CORNER_R_DUPE	= 0x8bc,

	GUMP_SPELLS_1		= 0x8c0,	// small icons.
	GUMP_SPELLS_64		= 0x8ff,

	GUMP_DYE_VAT		= 0x906,

	GUMP_ITEM_MENU		= 0x910,

	GUMP_GAME_BOARD		= 0x91a,	// Chess or checker board.
	GUMP_GAME_BACKGAM	= 0x92e,	// backgammon board.

	GUMP_MAP_1_NORTH	= 0x9e1,

	GUMP_RADAR_SMALL	= 0x1392,
	GUMP_RADAR_LARGE	= 0x1393,

	GUMP_MAP_2_NORTH	= 0x139d,

	GUMP_DIALOG_DARK_RESIZE = 0x13ec,	// resize group.

	GUMP_DARK_SCRAP		= 0x1400,

	GUMP_SCROLL_RESIZE	= 0x141e,

	GUMP_DIALOG_CANCEL	= 0x1450,	// general dialog.

	GUMP_BBOARD_2		= 0x1518,

	GUMP_CHAT_GROUP		= 0x152c,

	GUMP_SPELLB_1		= 0x1b58,	// big icons.
	GUMP_SPELLB_64		= 0x1b97,

	GUMP_LIGHTNING1		= 0x4e20,
	GUMP_LIGHTNINGX		= 0x4e29,

	GUMP_ITEM_HAT		= 0xc4e4,	// start of gump clothing.
	GUMP_ITEM_PACK		= 0xc4f6,

	CUMP_ITEM_WEAPON	= 0xc5b2,

	GUMP_ITEM_HAIR		= 0xc60c,

	GUMP_DUPRE			= 0xC734,

	GUMP_ITEM_GIRL		= 0xebf4,

	// .. paperdoll items.
	GUMP_ITEM_GMROBE	= 0xee3b,
	GUMP_ITEM_CHAOS		= 0xee41,
	GUMP_QTY			= 0xee42,

	GUMP_OPEN_SPELLBOOK = 0xFFFF,
};

enum GUMPRESIZE_TYPE	// Patterns of resizable gumps.
{
	GUMPRESIZE_UL = 0,
	GUMPRESIZE_TOP,
	GUMPRESIZE_UR,
	GUMPRESIZE_LEFT,
	GUMPRESIZE_MID,
	GUMPRESIZE_RIGHT,
	GUMPRESIZE_LL,
	GUMPRESIZE_BOTTOM,
	GUMPRESIZE_LR,
	GUMPRESIZE_QTY,
};

enum GUMPCTL_TYPE	// controls we can put in a gump.
{
	GUMPCTL_RESIZEPIC = 0,	// 5 = x,y,gumpback,sx,sy	// can come first if multi page. put up some background gump
	GUMPCTL_GUMPPIC,		// 3 = x,y,gump	// put gumps in the dlg.
	GUMPCTL_TILEPIC,		// 3 = x,y,item	// put item tiles in the dlg.

	GUMPCTL_TEXT,			// 4 = x,y,page,startstringindex	// put some text here.
	GUMPCTL_BUTTON,			// 7 = X,Y,Down gump,Up gump,pressable(1/0),page,id
	GUMPCTL_RADIO,			// 6 = x,y,gump1,gump2,page,id
	GUMPCTL_CHECKBOX,		// 6 = x,y,gumpcheck,gumpuncheck,page,id

	GUMPCTL_TEXTENTRY,		// 7 = x,y,widthpix,widthchars,page,id,startstringindex

	// Not really controls but more attributes.
	GUMPCTL_PAGE,			// 1 = page number	// for multi tab dialogs.
	GUMPCTL_GROUP,		// 1 = ?no idea. (arg = 0?)
	GUMPCTL_NOMOVE,		// 0 = The gump cannot be moved around.
	GUMPCTL_NOCLOSE,		// 0 = not really used
	GUMPCTL_NODISPOSE,	// 0 = not really used  (modal?)

	GUMPCTL_AREA,	// my own creation

	GUMPCTL_QTY,
};

enum EXTCMD_TYPE
{
	EXTCMD_OPEN_SPELLBOOK	= 0x43,	// 67 = open spell book if we have one.
	EXTCMD_ANIMATE			= 0xC7,	// "bow" or "salute"
	EXTCMD_SKILL			= 0x24,	// Skill start "number of the skill"
	EXTCMD_AUTOTARG			= 47,	// bizarre new autotarget mode. "target x y z"
	EXTCMD_CAST_MACRO		= 86,	// macro spell. "spell number"
	EXTCMD_CAST_BOOK		= 39,	// cast spell from book. "spell number"
	EXTCMD_DOOR_AUTO		= 88,	// open door macro = Attempt to open a door around us.
};

enum CHATMSG_TYPE	// Chat system messages.
{
	CHATMSG_NoError = -1,				// -1 = Our return code for no error, but no message either
	CHATMSG_Error = 0,					// 0 - Error
	CHATMSG_AlreadyIgnoringMax,			// 1 - You are already ignoring the maximum amount of people
	CHATMSG_AlreadyIgnoringPlayer,		// 2 - You are already ignoring <name>
	CHATMSG_NowIgnoring,				// 3 - You are now ignoring <name>
	CHATMSG_NoLongerIgnoring,			// 4 - You no longer ignoring <name>
	CHATMSG_NotIgnoring,				// 5 - You are not ignoring <name>
	CHATMSG_NoLongerIgnoringAnyone,		// 6 - You are no longer ignoring anyone
	CHATMSG_InvalidConferenceName,		// 7 - That is not a valid conference name
	CHATMSG_AlreadyAConference,			// 8 - There is already a conference of that name
	CHATMSG_MustHaveOps,				// 9 - You must have operator status to do this
	CHATMSG_ConferenceRenamed,			// a - Conference <name> renamed to .
	CHATMSG_MustBeInAConference,		// b - You must be in a conference to do this. To join a conference, select one from the Conference menu
	CHATMSG_NoPlayer,					// c - There is no player named <name>
	CHATMSG_NoConference,				// d - There is no conference named <name>
	CHATMSG_IncorrectPassword,			// e - That is not the correct password
	CHATMSG_PlayerIsIgnoring,			// f - <name> has chosen to ignore you. None of your messages to them will get through
	CHATMSG_RevokedSpeaking,			// 10 - The moderator of this conference has not given you speaking priveledges.
	CHATMSG_ReceivingPrivate,			// 11 - You can now receive private messages
	CHATMSG_NoLongerReceivingPrivate,	// 12 - You will no longer receive private messages. Those who send you a message will be notified that you are blocking incoming messages
	CHATMSG_ShowingName,				// 13 - You are now showing your character name to any players who inquire with the whois command
	CHATMSG_NotShowingName,				// 14 - You are no long showing your character name to any players who inquire with the whois command
	CHATMSG_PlayerIsAnonymous,			// 15 - <name> is remaining anonymous
	CHATMSG_PlayerNotReceivingPrivate,	// 16 - <name> has chosen to not receive private messages at the moment
	CHATMSG_PlayerKnownAs,				// 17 - <name> is known in the lands of britania as .
	CHATMSG_PlayerIsKicked,				// 18 - <name> has been kicked out of the conference
	CHATMSG_ModeratorHasKicked,			// 19 - <name>, a conference moderator, has kicked you out of the conference
	CHATMSG_AlreadyInConference,		// 1a - You are already in the conference <name>
	CHATMSG_PlayerNoLongerModerator,	// 1b - <name> is no longer a conference moderator
	CHATMSG_PlayerIsAModerator,			// 1c - <name> is now a conference moderator
	CHATMSG_RemovedListModerators,		// 1d - <name> has removed you from the list of conference moderators.
	CHATMSG_YouAreAModerator,			// 1e - <name> has made you a conference moderator
	CHATMSG_PlayerNoSpeaking,			// 1f - <name> no longer has speaking priveledges in this conference.
	CHATMSG_PlayerNowSpeaking,			// 20 - <name> now has speaking priveledges in this conference
	CHATMSG_ModeratorRemovedSpeaking,	// 21 - <name>, a channel moderator, has removed your speaking priveledges for this conference.
	CHATMSG_ModeratorGrantSpeaking,		// 22 - <name>, a channel moderator, has granted you speaking priveledges for this conference.
	CHATMSG_SpeakingByDefault,			// 23 - From now on, everyone in the conference will have speaking priviledges by default.
	CHATMSG_ModeratorsSpeakDefault,		// 24 - From now on, only moderators in this conference will have speaking priviledges by default.
	CHATMSG_PlayerTalk,					// 25 - <name>:
	CHATMSG_PlayerEmote,				// 26 - *<name>*
	CHATMSG_PlayerPrivate,				// 27 - [<name>]:
	CHATMSG_PasswordChanged,			// 28 - The password to the conference has been changed
	CHATMSG_NoMorePlayersAllowed,		// 29 - Sorry--the conference named <name> is full and no more players are allowed in.

	CHATMSG_SendChannelName =	0x03e8, // Message to send a channel name and mode to client's channel list
	CHATMSG_RemoveChannelName =	0x03e9, // Message to remove a channel name from client's channel list
	CHATMSG_GetChatName =		0x03eb,	// Ask the client for a permanent chat name
	CHATMSG_CloseChatWindow =	0x03ec,	// Close the chat system dialogs on the client???
	CHATMSG_OpenChatWindow =	0x03ed,	// Open the chat system dialogs on the client
	CHATMSG_SendPlayerName =	0x03ee,	// Message to add a player out to client (prefixed with a "1" if they are the moderator or a "0" if not
	CHATMSG_RemoveMember =		0x03ef,	// Message to remove a player from clients channel member list
	CHATMSG_ClearMemberList =	0x03f0,	// This message clears the list of channel participants (for when leaving a channel)
	CHATMSG_UpdateChannelBar =	0x03f1,	// This message changes the name in the channel name bar
	CHATMSG_QTY,						// Error (but does 0x03f1 anyways)
};

#endif // _INC_GRAYMUL_H