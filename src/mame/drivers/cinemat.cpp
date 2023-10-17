// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/***************************************************************************

    Cinematronics vector hardware

    driver by Aaron Giles

    Special thanks to Neil Bradley, Zonn Moore, and Jeff Mitchell of the
    Retrocade Alliance

    Games supported:
        * Space Wars
        * Barrier
        * Starhawk
        * Star Castle
        * Tailgunner
        * Rip Off
        * Speed Freak
        * Sundance
        * Warrior
        * Armor Attack
        * Solar Quest
        * Demon
        * War of the Worlds
        * Boxing Bugs
        * QB-3
        * Space Ship

***************************************************************************/

#include "emu.h"
#include "includes/cinemat.h"
#include "speaker.h"

#include "armora.lh"
#include "barrier.lh"
#include "demon.lh"
#include "starcas.lh"
#include "solarq.lh"
#include "speedfrk.lh"
#include "sundance.lh"
#include "tailg.lh"
#include "warrior.lh"
#include "wotw.lh"

#define MASTER_CLOCK            XTAL(19'923'000)


/*************************************
 *
 *  General machine init
 *
 *************************************/

void cinemat_state::machine_start()
{
	save_item(NAME(m_coin_detected));
	save_item(NAME(m_coin_last_reset));
	save_item(NAME(m_mux_select));
	save_item(NAME(m_vector_color));
	save_item(NAME(m_lastx));
	save_item(NAME(m_lasty));
	m_led.resolve();
	m_pressed.resolve();
}


void cinemat_state::machine_reset()
{
	/* reset the coin states */
	m_coin_detected = 0;
	m_coin_last_reset = 0;

	/* reset mux select */
	m_mux_select = 0;
}



/*************************************
 *
 *  General input handlers
 *
 *************************************/

u8 cinemat_state::inputs_r(offs_t offset)
{
	return (m_inputs->read() >> offset) & 1;
}


u8 cinemat_state::switches_r(offs_t offset)
{
	static const u8 switch_shuffle[8] = { 2,5,4,3,0,1,6,7 };
	return (m_switches->read() >> switch_shuffle[offset]) & 1;
}



/*************************************
 *
 *  Coin handlers
 *
 *************************************/

INPUT_CHANGED_MEMBER(cinemat_state::coin_inserted)
{
	/* on the falling edge of a new coin, set the coin_detected flag */
	if (newval == 0)
		m_coin_detected = 1;
}


u8 cinemat_state::coin_input_r()
{
	return !m_coin_detected;
}



/*************************************
 *
 *  General output handlers
 *
 *************************************/

WRITE_LINE_MEMBER(cinemat_state::coin_reset_w)
{
	/* on the rising edge of a coin reset, clear the coin_detected flag */
	if (state)
		m_coin_detected = 0;
}


WRITE_LINE_MEMBER(cinemat_state::mux_select_w)
{
	m_mux_select = state;
}



/*************************************
 *
 *  Joystick inputs
 *
 *************************************/

u8 cinemat_state::joystick_read()
{
	if (machine().phase() != machine_phase::RUNNING)
		return 0;
	else
	{
		int const xval = s16(m_maincpu->state_int(ccpu_cpu_device::CCPU_X) << 4) >> 4;
		return ((m_mux_select ? m_analog_x : m_analog_y).read_safe(0) - xval) < 0x800;
	}
}



/*************************************
 *
 *  Speed Freak inputs
 *
 *************************************/

u8 cinemat_state::speedfrk_wheel_r(offs_t offset)
{
	static const u8 speedfrk_steer[] = {0xe, 0x6, 0x2, 0x0, 0x3, 0x7, 0xf};
	int delta_wheel;

	/* the shift register is cleared once per 'frame' */
	delta_wheel = s8(m_wheel->read()) / 8;
	if (delta_wheel > 3)
		delta_wheel = 3;
	else if (delta_wheel < -3)
		delta_wheel = -3;

	return (speedfrk_steer[delta_wheel + 3] >> offset) & 1;
}


u8 cinemat_state::speedfrk_gear_r(offs_t offset)
{
	return (m_gear != offset);
}




/*************************************
 *
 *  Sundance inputs
 *
 *************************************/

static const struct
{
	const char *portname;
	u16 bitmask;
} sundance_port_map[16] =
{
	{ "PAD1", 0x155 },  /* bit  0 is set if P1 1,3,5,7,9 is pressed */
	{ nullptr, 0 },
	{ nullptr, 0 },
	{ nullptr, 0 },

	{ nullptr, 0 },
	{ nullptr, 0 },
	{ nullptr, 0 },
	{ nullptr, 0 },

	{ "PAD2", 0x1a1 },  /* bit  8 is set if P2 1,6,8,9 is pressed */
	{ "PAD1", 0x1a1 },  /* bit  9 is set if P1 1,6,8,9 is pressed */
	{ "PAD2", 0x155 },  /* bit 10 is set if P2 1,3,5,7,9 is pressed */
	{ nullptr, 0 },

	{ "PAD1", 0x093 },  /* bit 12 is set if P1 1,2,5,8 is pressed */
	{ "PAD2", 0x093 },  /* bit 13 is set if P2 1,2,5,8 is pressed */
	{ "PAD1", 0x048 },  /* bit 14 is set if P1 4,8 is pressed */
	{ "PAD2", 0x048 },  /* bit 15 is set if P2 4,8 is pressed */
};


u8 cinemat_16level_state::sundance_inputs_r(offs_t offset)
{
	/* handle special keys first */
	if (sundance_port_map[offset].portname)
		return (ioport(sundance_port_map[offset].portname)->read() & sundance_port_map[offset].bitmask) ? 0 : 1;
	else
		return (m_inputs->read() >> offset) & 1;
}



/*************************************
 *
 *  Boxing Bugs inputs
 *
 *************************************/

u8 cinemat_color_state::boxingb_dial_r(offs_t offset)
{
	int value = ioport("DIAL")->read();
	if (!m_mux_select) offset += 4;
	return (value >> offset) & 1;
}



/*************************************
 *
 *  QB3 inputs & RAM banking
 *
 *************************************/

u8 qb3_state::qb3_frame_r()
{
	attotime next_update = m_screen->time_until_update();
	attotime frame_period = m_screen->frame_period();
	int percent = next_update.attoseconds() / (frame_period.attoseconds() / 100);

	/* note this is just an approximation... */
	return (percent >= 10);
}


void qb3_state::qb3_ram_bank_w(u8 data)
{
	membank("bank1")->set_entry(m_maincpu->state_int(ccpu_cpu_device::CCPU_P) & 3);
}



/*************************************
 *
 *  Main CPU memory handlers
 *
 *************************************/

void cinemat_state::program_map_4k(address_map &map)
{
	map.global_mask(0xfff);
	map(0x0000, 0x0fff).rom();
}

void cinemat_state::program_map_8k(address_map &map)
{
	map.global_mask(0x3fff);
	map(0x0000, 0x0fff).mirror(0x1000).rom();
	map(0x2000, 0x2fff).mirror(0x1000).rom().region("maincpu", 0x1000);
}

void cinemat_state::program_map_16k(address_map &map)
{
	map.global_mask(0x3fff);
	map(0x0000, 0x3fff).rom();
}

void cinemat_state::program_map_32k(address_map &map)
{
	map.global_mask(0x7fff);
	map(0x0000, 0x7fff).rom();
}


void cinemat_state::data_map(address_map &map)
{
	map(0x0000, 0x00ff).ram();
}

void qb3_state::data_map_qb3(address_map &map)
{
	map(0x0000, 0x03ff).bankrw("bank1").share("rambase");
}


void cinemat_state::io_map(address_map &map)
{
	map(0x00, 0x0f).r(FUNC(cinemat_state::inputs_r));
	map(0x10, 0x16).r(FUNC(cinemat_state::switches_r));
	map(0x17, 0x17).r(FUNC(cinemat_state::coin_input_r));

	map(0x00, 0x07).w(m_outlatch, FUNC(ls259_device::write_d0));
}

void qb3_state::io_map_qb3(address_map &map)
{
	io_map(map);
	// Some of the outputs here are definitely not mapped through the LS259, since they use multiple bits of data
	map(0x00, 0x00).w(FUNC(qb3_state::qb3_ram_bank_w));
	map(0x04, 0x04).w(FUNC(qb3_state::qb3_sound_fifo_w));
	map(0x0f, 0x0f).r(FUNC(qb3_state::qb3_frame_r));
}



/*************************************
 *
 *  Port definitions
 *
 *************************************/

static INPUT_PORTS_START( spacewar )
	PORT_START("INPUTS")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("Option 3") PORT_CODE(KEYCODE_3_PAD)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("Option 8") PORT_CODE(KEYCODE_8_PAD)
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("Option 4") PORT_CODE(KEYCODE_4_PAD)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("Option 9") PORT_CODE(KEYCODE_9_PAD)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("Option 1") PORT_CODE(KEYCODE_1_PAD)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("Option 6") PORT_CODE(KEYCODE_6_PAD)
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("Option 2") PORT_CODE(KEYCODE_2_PAD)
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("Option 7") PORT_CODE(KEYCODE_7_PAD)
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_2WAY PORT_PLAYER(1)
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("Option 5") PORT_CODE(KEYCODE_5_PAD)
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("Option 0") PORT_CODE(KEYCODE_0_PAD)
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_2WAY PORT_PLAYER(2)
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_2WAY PORT_PLAYER(1)
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_2WAY PORT_PLAYER(2)
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)

	PORT_START("SWITCHES")
	PORT_DIPNAME( 0x03, 0x00,  "Time" )
	PORT_DIPSETTING(    0x03, "0:45/coin" )
	PORT_DIPSETTING(    0x00, "1:00/coin" )
	PORT_DIPSETTING(    0x01, "1:30/coin" )
	PORT_DIPSETTING(    0x02, "2:00/coin" )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(1)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("Reset Playfield") PORT_CODE(KEYCODE_R)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, cinemat_state, coin_inserted, 0)
INPUT_PORTS_END


static INPUT_PORTS_START( spaceshp )
	PORT_INCLUDE( spacewar )

	PORT_MODIFY("SWITCHES")
	PORT_DIPNAME( 0x03, 0x00, "Time" ) PORT_DIPLOCATION("SW1:!4,!3")
	PORT_DIPSETTING(    0x00, "1:00/coin" )
	PORT_DIPSETTING(    0x01, "1:30/coin" )
	PORT_DIPSETTING(    0x02, "2:00/coin" )
	PORT_DIPSETTING(    0x03, "2:30/coin" )
INPUT_PORTS_END


static INPUT_PORTS_START( barrier )
	PORT_START("INPUTS")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("Skill A") PORT_CODE(KEYCODE_A)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("Skill B") PORT_CODE(KEYCODE_B)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_4WAY PORT_PLAYER(1)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("Skill C") PORT_CODE(KEYCODE_C)
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_4WAY PORT_PLAYER(2)
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_4WAY PORT_PLAYER(1)
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_4WAY PORT_PLAYER(2)
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_4WAY PORT_PLAYER(1)
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_4WAY PORT_PLAYER(2)
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_4WAY PORT_PLAYER(1)
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_4WAY PORT_PLAYER(2)

	PORT_START("SWITCHES")
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, cinemat_state, coin_inserted, 0)
INPUT_PORTS_END


static INPUT_PORTS_START( speedfrk )
	PORT_START("INPUTS")
	PORT_BIT( 0x000f, IP_ACTIVE_LOW, IPT_CUSTOM ) /* steering wheel, fake below */
	PORT_BIT( 0x0070, IP_ACTIVE_LOW, IPT_CUSTOM ) /* gear shift, fake below */
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1) /* gas */
	PORT_BIT( 0xfe00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("SWITCHES")
	PORT_DIPNAME( 0x03, 0x02, "Extra Time" )
	PORT_DIPSETTING(    0x00, "69" )
	PORT_DIPSETTING(    0x01, "99" )
	PORT_DIPSETTING(    0x02, "129" )
	PORT_DIPSETTING(    0x03, "159" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, cinemat_state, coin_inserted, 0)

	PORT_START("WHEEL")
	PORT_BIT( 0xff, 0x00, IPT_DIAL ) PORT_SENSITIVITY(100) PORT_KEYDELTA(10) PORT_RESET

	PORT_START("GEAR")
	PORT_BIT (0x03, IP_ACTIVE_HIGH, IPT_CUSTOM ) PORT_CUSTOM_MEMBER(cinemat_state, speedfrk_gear_number_r)

	PORT_START("GEARRAW")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_NAME("1st gear") PORT_PLAYER(2) PORT_WRITE_LINE_MEMBER(cinemat_state, speedfrk_gear_change_w<0>)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_NAME("2nd gear") PORT_PLAYER(2) PORT_WRITE_LINE_MEMBER(cinemat_state, speedfrk_gear_change_w<1>)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_NAME("3rd gear") PORT_PLAYER(2) PORT_WRITE_LINE_MEMBER(cinemat_state, speedfrk_gear_change_w<2>)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_NAME("4th gear") PORT_PLAYER(2) PORT_WRITE_LINE_MEMBER(cinemat_state, speedfrk_gear_change_w<3>)
INPUT_PORTS_END


/* TODO: 4way or 8way stick? */
static INPUT_PORTS_START( starhawk )
	PORT_START("INPUTS")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x00c0, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_PLAYER(1)
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(1)
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_PLAYER(2)

	PORT_START("SWITCHES")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Game_Time ) )
	PORT_DIPSETTING(    0x03, "2:00/4:00" )
	PORT_DIPSETTING(    0x01, "1:30/3:00" )
	PORT_DIPSETTING(    0x02, "1:00/2:00" )
	PORT_DIPSETTING(    0x00, "0:45/1:30" )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, cinemat_state, coin_inserted, 0)
INPUT_PORTS_END


static INPUT_PORTS_START( sundance )
	PORT_START("INPUTS")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_CUSTOM ) /* P1 Pad */
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("3 Suns") PORT_CODE(KEYCODE_STOP)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("Toggle Grid") PORT_CODE(KEYCODE_G)
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("4 Suns") PORT_CODE(KEYCODE_SLASH)
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_CUSTOM ) /* P2 Pad */
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_CUSTOM ) /* P1 Pad */
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_CUSTOM ) /* P2 Pad */
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_OTHER ) PORT_NAME("2 Suns") PORT_CODE(KEYCODE_COMMA)
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_CUSTOM ) /* P1 Pad */
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_CUSTOM ) /* P2 Pad */
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_CUSTOM ) /* P1 Pad */
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_CUSTOM ) /* P2 Pad */

	PORT_START("SWITCHES")
	PORT_DIPNAME( 0x03, 0x02, "Time" )
	PORT_DIPSETTING(    0x00, "0:45/coin" )
	PORT_DIPSETTING(    0x02, "1:00/coin" )
	PORT_DIPSETTING(    0x01, "1:30/coin" )
	PORT_DIPSETTING(    0x03, "2:00/coin" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Language ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Japanese ) )
	PORT_DIPSETTING(    0x00, DEF_STR( English ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) ) /* supposedly coinage, doesn't work */
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, cinemat_state, coin_inserted, 0)

	PORT_START("PAD1")
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P1 Pad 1") PORT_CODE(KEYCODE_7_PAD) PORT_PLAYER(1)
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P1 Pad 2") PORT_CODE(KEYCODE_8_PAD) PORT_PLAYER(1)
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P1 Pad 3") PORT_CODE(KEYCODE_9_PAD) PORT_PLAYER(1)
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P1 Pad 4") PORT_CODE(KEYCODE_4_PAD) PORT_PLAYER(1)
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P1 Pad 5") PORT_CODE(KEYCODE_5_PAD) PORT_PLAYER(1)
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P1 Pad 6") PORT_CODE(KEYCODE_6_PAD) PORT_PLAYER(1)
	PORT_BIT( 0x0040, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P1 Pad 7") PORT_CODE(KEYCODE_1_PAD) PORT_PLAYER(1)
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P1 Pad 8") PORT_CODE(KEYCODE_2_PAD) PORT_PLAYER(1)
	PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P1 Pad 9") PORT_CODE(KEYCODE_3_PAD) PORT_PLAYER(1)

	PORT_START("PAD2")
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P2 Pad 1") PORT_CODE(KEYCODE_Q) PORT_PLAYER(2)
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P2 Pad 2") PORT_CODE(KEYCODE_W) PORT_PLAYER(2)
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P2 Pad 3") PORT_CODE(KEYCODE_E) PORT_PLAYER(2)
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P2 Pad 4") PORT_CODE(KEYCODE_A) PORT_PLAYER(2)
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P2 Pad 5") PORT_CODE(KEYCODE_S) PORT_PLAYER(2)
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P2 Pad 6") PORT_CODE(KEYCODE_D) PORT_PLAYER(2)
	PORT_BIT( 0x0040, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P2 Pad 7") PORT_CODE(KEYCODE_Z) PORT_PLAYER(2)
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P2 Pad 8") PORT_CODE(KEYCODE_X) PORT_PLAYER(2)
	PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("P2 Pad 9") PORT_CODE(KEYCODE_C) PORT_PLAYER(2)
INPUT_PORTS_END


static INPUT_PORTS_START( tailg )
	PORT_START("INPUTS")
	PORT_BIT( 0x001f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("SWITCHES")
	PORT_DIPNAME( 0x23, 0x23, "Shield Points" )
	PORT_DIPSETTING(    0x00, "15" )
	PORT_DIPSETTING(    0x02, "20" )
	PORT_DIPSETTING(    0x01, "30" )
	PORT_DIPSETTING(    0x03, "40" )
	PORT_DIPSETTING(    0x20, "50" )
	PORT_DIPSETTING(    0x22, "60" )
	PORT_DIPSETTING(    0x21, "70" )
	PORT_DIPSETTING(    0x23, "80" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, cinemat_state, coin_inserted, 0)

	PORT_START("ANALOGX")
	PORT_BIT( 0xfff, 0x800, IPT_AD_STICK_X ) PORT_MINMAX(0x200,0xe00) PORT_SENSITIVITY(100) PORT_KEYDELTA(50)

	PORT_START("ANALOGY")
	PORT_BIT( 0xfff, 0x800, IPT_AD_STICK_Y ) PORT_MINMAX(0x200,0xe00) PORT_SENSITIVITY(100) PORT_KEYDELTA(50)
INPUT_PORTS_END


static INPUT_PORTS_START( warrior )
	PORT_START("INPUTS")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(2)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )  PORT_PLAYER(2)
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )    PORT_PLAYER(2)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )  PORT_PLAYER(2)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 )        PORT_PLAYER(2)
	PORT_BIT( 0x00e0, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(1)
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )  PORT_PLAYER(1)
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )    PORT_PLAYER(1)
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )  PORT_PLAYER(1)
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 )        PORT_PLAYER(1)
	PORT_BIT( 0xe000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("SWITCHES")
	PORT_DIPNAME( 0x03, 0x02, "Time" )      PORT_DIPLOCATION("SW1:!1,!2")
	PORT_DIPSETTING(    0x00, "0:30/coin" )
	PORT_DIPSETTING(    0x02, "1:00/coin" )
	PORT_DIPSETTING(    0x01, "1:30/coin" )
	PORT_DIPSETTING(    0x03, "2:00/coin" )
	PORT_SERVICE_DIPLOC( 0x04, IP_ACTIVE_HIGH, "SW1:!3" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Coinage ) )  PORT_DIPLOCATION("SW1:!4")
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_1C ) )
	PORT_DIPUNUSED_DIPLOC( 0x10, IP_ACTIVE_HIGH, "SW1:!5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, IP_ACTIVE_HIGH, "SW1:!6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, IP_ACTIVE_HIGH, "SW1:!7" )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, cinemat_state, coin_inserted, 0)
INPUT_PORTS_END


static INPUT_PORTS_START( armora )
	PORT_START("INPUTS")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_2WAY PORT_PLAYER(2)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_2WAY PORT_PLAYER(2)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x0fc0, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_2WAY PORT_PLAYER(1)
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_2WAY PORT_PLAYER(1)
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)

	PORT_START("SWITCHES")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_3C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_HIGH )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, cinemat_state, coin_inserted, 0)
INPUT_PORTS_END


static INPUT_PORTS_START( ripoff )
	PORT_START("INPUTS")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_2WAY PORT_PLAYER(2)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_2WAY PORT_PLAYER(2)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x0fc0, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_2WAY PORT_PLAYER(1)
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_2WAY PORT_PLAYER(1)
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)

	PORT_START("SWITCHES")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x03, "8" )
	PORT_DIPSETTING(    0x00, "12" )
	PORT_DIPSETTING(    0x02, "16" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_3C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Scores" )
	PORT_DIPSETTING(    0x00, "Individual" )
	PORT_DIPSETTING(    0x20, "Combined" )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, cinemat_state, coin_inserted, 0)
INPUT_PORTS_END


static INPUT_PORTS_START( starcas )
	PORT_START("INPUTS")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0038, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_2WAY
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_2WAY
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0xe000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("SWITCHES")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_3C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_HIGH )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, cinemat_state, coin_inserted, 0)
INPUT_PORTS_END

static INPUT_PORTS_START( starcasc )
	PORT_START("INPUTS")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_2WAY
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_2WAY
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x07c0, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_2WAY PORT_PLAYER(2)
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_2WAY PORT_PLAYER(2)
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("SWITCHES")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x00, "6" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_3C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_HIGH )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, cinemat_state, coin_inserted, 0)
INPUT_PORTS_END

static INPUT_PORTS_START( solarq )
	PORT_START("INPUTS")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_PLAYER(1) /* nova */
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1) /* fire */
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1) /* thrust */
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(1) /* hyperspace */
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_2WAY PORT_PLAYER(1)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_2WAY PORT_PLAYER(1)
	PORT_BIT( 0xffc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("SWITCHES")
	PORT_DIPNAME( 0x05, 0x05, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_3C ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x02, "25 captures" )
	PORT_DIPSETTING(    0x00, "40 captures" )
	PORT_DIPNAME( 0x18, 0x10, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x18, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_HIGH )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, cinemat_state, coin_inserted, 0)
INPUT_PORTS_END


static INPUT_PORTS_START( boxingb )
	PORT_START("INPUTS")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW,  IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW,  IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x0004, IP_ACTIVE_LOW,  IPT_BUTTON3 ) PORT_PLAYER(2)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW,  IPT_BUTTON3 ) PORT_PLAYER(1)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW,  IPT_BUTTON2 ) PORT_PLAYER(1)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW,  IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x0fc0, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0xf000, IP_ACTIVE_HIGH, IPT_CUSTOM ) /* dial */

	PORT_START("SWITCHES")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_3C ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "30,000" )
	PORT_DIPSETTING(    0x08, "50,000" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, cinemat_state, coin_inserted, 0)

	PORT_START("DIAL")
	PORT_BIT( 0xff, 0x00, IPT_DIAL ) PORT_REVERSE PORT_SENSITIVITY(100) PORT_KEYDELTA(5)
INPUT_PORTS_END


static INPUT_PORTS_START( wotw )
	PORT_START("INPUTS")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0038, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_2WAY
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_2WAY
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0xe000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("SWITCHES")
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, cinemat_state, coin_inserted, 0)
INPUT_PORTS_END


static INPUT_PORTS_START( demon )
	PORT_START("INPUTS")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_2WAY PORT_PLAYER(1)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_2WAY PORT_PLAYER(1)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_SERVICE( 0x0080, IP_ACTIVE_LOW )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(1)
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_2WAY PORT_PLAYER(2)
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_2WAY PORT_PLAYER(2)
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN ) /* also mapped to Button 3, player 2 */

	PORT_START("SWITCHES")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_3C ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3")
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x08, "5" )
	PORT_DIPSETTING(    0x0c, "6" )
	PORT_DIPNAME( 0x30, 0x30, "Starting Difficulty" )
	PORT_DIPSETTING(    0x30, "1" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPSETTING(    0x00, "10" )
/*  PORT_DIPSETTING(    0x20, "1" )*/
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, cinemat_state, coin_inserted, 0)
INPUT_PORTS_END


static INPUT_PORTS_START( qb3 )
	PORT_START("INPUTS")
	PORT_BIT( 0x0001, IP_ACTIVE_LOW,  IPT_JOYSTICKLEFT_UP )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW,  IPT_JOYSTICKLEFT_DOWN )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW,  IPT_JOYSTICKRIGHT_LEFT )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW,  IPT_JOYSTICKRIGHT_UP )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW,  IPT_JOYSTICKRIGHT_RIGHT )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW,  IPT_JOYSTICKRIGHT_DOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW,  IPT_START1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW,  IPT_START2 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW,  IPT_BUTTON4 )                 // read at $1a5; if 0 add 8 to $25
	PORT_DIPNAME( 0x0200, 0x0200, "Debug" )
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW,  IPT_BUTTON2 )                 // read at $c7; jmp to $3AF1 if 0
	PORT_BIT( 0x0800, IP_ACTIVE_LOW,  IPT_JOYSTICKLEFT_RIGHT )
	PORT_DIPNAME( 0x1000, 0x1000, "Infinite Lives" )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW,  IPT_JOYSTICKLEFT_LEFT )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW,  IPT_BUTTON1 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW,  IPT_CUSTOM )

	PORT_START("SWITCHES")
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Free_Play ) )    // read at $244, $2c1
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )  // read at $27d
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )   // read at $282
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, cinemat_state, coin_inserted, 0)
INPUT_PORTS_END



/*************************************
 *
 *  Core machine drivers
 *
 *************************************/

void cinemat_state::cinemat_nojmi_4k(machine_config &config)
{
	/* basic machine hardware */
	CCPU(config, m_maincpu, MASTER_CLOCK/4);
	m_maincpu->set_vector_func(FUNC(cinemat_state::cinemat_vector_callback));
	m_maincpu->external_func().set(FUNC(cinemat_state::joystick_read));
	m_maincpu->set_addrmap(AS_PROGRAM, &cinemat_state::program_map_4k);
	m_maincpu->set_addrmap(AS_DATA, &cinemat_state::data_map);
	m_maincpu->set_addrmap(AS_IO, &cinemat_state::io_map);

	LS259(config, m_outlatch); // 7J on CCG-1
	m_outlatch->q_out_cb<5>().set(FUNC(cinemat_state::coin_reset_w));
	m_outlatch->q_out_cb<6>().set(FUNC(cinemat_state::vector_control_w));

	/* video hardware */
	VECTOR(config, "vector", 0);
	SCREEN(config, m_screen, SCREEN_TYPE_VECTOR);
	m_screen->set_video_attributes(VIDEO_ALWAYS_UPDATE);
	m_screen->set_refresh_hz(MASTER_CLOCK/4/16/16/16/16/2);
	m_screen->set_size(1024, 768);
	m_screen->set_visarea(0, 1023, 0, 767);
	m_screen->set_screen_update(FUNC(cinemat_state::screen_update_cinemat));
}

void cinemat_state::cinemat_jmi_4k(machine_config &config)
{
	cinemat_nojmi_4k(config);
	m_maincpu->set_vector_func(FUNC(cinemat_state::cinemat_vector_callback));
	m_maincpu->external_func().set("maincpu", FUNC(ccpu_cpu_device::read_jmi));
}

void cinemat_state::cinemat_nojmi_8k(machine_config &config)
{
	cinemat_nojmi_4k(config);
	m_maincpu->set_addrmap(AS_PROGRAM, &cinemat_state::program_map_8k);
}

void cinemat_state::cinemat_jmi_8k(machine_config &config)
{
	cinemat_jmi_4k(config);
	m_maincpu->set_addrmap(AS_PROGRAM, &cinemat_state::program_map_8k);
}

void cinemat_state::cinemat_jmi_16k(machine_config &config)
{
	cinemat_jmi_4k(config);
	m_maincpu->set_addrmap(AS_PROGRAM, &cinemat_state::program_map_16k);
}

void cinemat_state::cinemat_jmi_32k(machine_config &config)
{
	cinemat_jmi_4k(config);
	m_maincpu->set_addrmap(AS_PROGRAM, &cinemat_state::program_map_32k);
}



/*************************************
 *
 *  Game-specific machine drivers
 *
 *************************************/

void cinemat_state::spacewar(machine_config &config)
{
	cinemat_nojmi_4k(config);

	SPEAKER(config, "mono").front_center();
	SPACE_WARS_AUDIO(config, "soundboard", 0)
		.configure_latch_inputs(*m_outlatch)
		.add_route(ALL_OUTPUTS, "mono", 1.0);

	m_screen->set_screen_update(FUNC(cinemat_state::screen_update_spacewar));
}

void cinemat_state::barrier(machine_config &config)
{
	cinemat_jmi_4k(config);

	SPEAKER(config, "mono").front_center();
	BARRIER_AUDIO(config, "soundboard", 0)
		.configure_latch_inputs(*m_outlatch)
		.add_route(ALL_OUTPUTS, "mono", 1.0);
}

WRITE_LINE_MEMBER(cinemat_state::speedfrk_start_led_w)
{
	/* start LED is controlled by bit 0x02 */
	m_led = !state;
}

void cinemat_state::speedfrk(machine_config &config)
{
	cinemat_nojmi_8k(config);

	SPEAKER(config, "mono").front_center();
	SPEED_FREAK_AUDIO(config, "soundboard", 0)
		.configure_latch_inputs(*m_outlatch)
		.add_route(ALL_OUTPUTS, "mono", 1.0);
//  m_outlatch->q_out_cb<1>().set(FUNC(cinemat_state::speedfrk_start_led_w));
}

void cinemat_state::starhawk(machine_config &config)
{
	cinemat_jmi_4k(config);

	SPEAKER(config, "mono").front_center();
	STAR_HAWK_AUDIO(config, "soundboard", 0)
		.configure_latch_inputs(*m_outlatch)
		.add_route(ALL_OUTPUTS, "mono", 1.0);
}

void cinemat_16level_state::sundance(machine_config &config)
{
	cinemat_jmi_8k(config);

	SPEAKER(config, "mono").front_center();
	SUNDANCE_AUDIO(config, "soundboard", 0)
		.configure_latch_inputs(*m_outlatch)
		.add_route(ALL_OUTPUTS, "mono", 1.0);
}

void cinemat_state::tailg(machine_config &config)
{
	cinemat_nojmi_8k(config);

	SPEAKER(config, "mono").front_center();
	TAIL_GUNNER_AUDIO(config, "soundboard", 0)
		.configure_latch_inputs(*m_outlatch)
		.add_route(ALL_OUTPUTS, "mono", 1.0);
	m_outlatch->q_out_cb<7>().set(FUNC(cinemat_state::mux_select_w));
}

void cinemat_state::warrior(machine_config &config)
{
	cinemat_jmi_8k(config);

	SPEAKER(config, "mono").front_center();
	WARRIOR_AUDIO(config, "soundboard", 0)
		.configure_latch_inputs(*m_outlatch)
		.add_route(ALL_OUTPUTS, "mono", 1.0);
}

void cinemat_state::armora(machine_config &config)
{
	cinemat_jmi_16k(config);

	SPEAKER(config, "mono").front_center();
	ARMOR_ATTACK_AUDIO(config, "soundboard", 0)
		.configure_latch_inputs(*m_outlatch)
		.add_route(ALL_OUTPUTS, "mono", 1.0);
}

void cinemat_state::ripoff(machine_config &config)
{
	cinemat_jmi_8k(config);

	SPEAKER(config, "mono").front_center();
	RIPOFF_AUDIO(config, "soundboard", 0)
		.configure_latch_inputs(*m_outlatch)
		.add_route(ALL_OUTPUTS, "mono", 1.0);
}

void cinemat_state::starcas(machine_config &config)
{
	cinemat_jmi_8k(config);

	SPEAKER(config, "mono").front_center();
	STAR_CASTLE_AUDIO(config, "soundboard", 0)
		.configure_latch_inputs(*m_outlatch)
		.add_route(ALL_OUTPUTS, "mono", 1.0);
}

void cinemat_64level_state::solarq(machine_config &config)
{
	cinemat_jmi_16k(config);

	SPEAKER(config, "mono").front_center();
	SOLAR_QUEST_AUDIO(config, "soundboard", 0)
		.configure_latch_inputs(*m_outlatch)
		.add_route(ALL_OUTPUTS, "mono", 1.0);
}

void cinemat_color_state::boxingb(machine_config &config)
{
	cinemat_jmi_32k(config);

	SPEAKER(config, "mono").front_center();
//	BOXING_BUGS_AUDIO(config, "soundboard", 0)
//		.configure_latch_inputs(*m_outlatch)
//		.add_route(ALL_OUTPUTS, "mono", 1.0);
	m_outlatch->q_out_cb<7>().append(FUNC(cinemat_state::mux_select_w));

	m_screen->set_visarea(0, 1024, 0, 788);
}

void cinemat_state::wotw(machine_config &config)
{
	cinemat_jmi_16k(config);
	m_screen->set_visarea(0, 1120, 0, 767);

	SPEAKER(config, "mono").front_center();
	WAR_OF_THE_WORLDS_AUDIO(config, "soundboard", 0)
		.configure_latch_inputs(*m_outlatch)
		.add_route(ALL_OUTPUTS, "mono", 1.0);
}

void cinemat_color_state::wotwc(machine_config &config)
{
	cinemat_jmi_16k(config);

	SPEAKER(config, "mono").front_center();
	WAR_OF_THE_WORLDS_AUDIO(config, "soundboard", 0)
		.configure_latch_inputs(*m_outlatch)
		.add_route(ALL_OUTPUTS, "mono", 1.0);
}

void demon_state::demon(machine_config &config)
{
	cinemat_jmi_16k(config);
	demon_sound(config);
	m_screen->set_visarea(0, 1024, 0, 805);
}

void qb3_state::qb3(machine_config &config)
{
	cinemat_jmi_32k(config);
	qb3_sound(config);
	m_maincpu->set_addrmap(AS_DATA, &qb3_state::data_map_qb3);
	m_maincpu->set_addrmap(AS_IO, &qb3_state::io_map_qb3);
	m_screen->set_visarea(0, 1120, 0, 780);
}




/*************************************
 *
 *  ROM definitions
 *
 *************************************/

#define CCPU_PROMS \
	ROM_REGION( 0x1a0, "proms", 0 ) \
	ROM_LOAD("prom.f14", 0x000, 0x100, CRC(9edbf536) SHA1(036ad8a231284e05f44b1106d38fc0c7e041b6e8) ) \
	ROM_LOAD("prom.e14", 0x100, 0x020, CRC(29dbfb87) SHA1(d8c40ab010b2ea30f29b2c443819e2b69f376c04) ) \
	ROM_LOAD("prom.d14", 0x120, 0x020, CRC(9a05afbf) SHA1(5d806a42424942ba5ef0b70a1d629315b37f931b) ) \
	ROM_LOAD("prom.c14", 0x140, 0x020, CRC(07492cda) SHA1(32df9148797c23f70db47b840139c40e046dd710) ) \
	ROM_LOAD("prom.j14", 0x160, 0x020, CRC(a481ca71) SHA1(ce145d61686f600cc16b77febfd5c783bf8c13b0) ) \
	ROM_LOAD("prom.e8",  0x180, 0x020, CRC(791ec9e1) SHA1(6f7fcce4aa3be9020595235568381588adaab88e) )

ROM_START( spacewar )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "spacewar.1l", 0x0000, 0x0800, CRC(edf0fd53) SHA1(a543d8b95bc77ec061c6b10161a6f3e07401e251) )
	ROM_LOAD16_BYTE( "spacewar.2r", 0x0001, 0x0800, CRC(4f21328b) SHA1(8889f1a9353d6bb1e1078829c1ba77557853739b) )

	CCPU_PROMS
ROM_END


ROM_START( spaceshp )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROMX_LOAD( "pr08.61", 0x0000, 0x0400, CRC(556c4ff4) SHA1(c8c1f3e5fe7bf48ecaa92dabf376adfd6a0a9b72), ROM_NIBBLE | ROM_SHIFT_NIBBLE_HI | ROM_SKIP(1) )
	ROMX_LOAD( "pr07.63", 0x0000, 0x0400, CRC(ba7747d1) SHA1(e9eb9de07ad5a306f815ee0d8371c64f8f242de6), ROM_NIBBLE | ROM_SHIFT_NIBBLE_LO | ROM_SKIP(1) )
	ROMX_LOAD( "pr04.83", 0x0001, 0x0400, CRC(19966799) SHA1(ffadb6cbcf4e4c4a60a251eb239eddc7d1030e6e), ROM_NIBBLE | ROM_SHIFT_NIBBLE_HI | ROM_SKIP(1) )
	ROMX_LOAD( "pr03.85", 0x0001, 0x0400, CRC(d6557503) SHA1(c226fdf85236558208942e43bcc3ce5af7e3d588), ROM_NIBBLE | ROM_SHIFT_NIBBLE_LO | ROM_SKIP(1) )
	ROMX_LOAD( "pr10.62", 0x0800, 0x0400, CRC(3ee163f9) SHA1(30269158434fb66049620bbac5f1c9b878416468), ROM_NIBBLE | ROM_SHIFT_NIBBLE_HI | ROM_SKIP(1) )
	ROMX_LOAD( "pr09.64", 0x0800, 0x0400, CRC(7946086c) SHA1(09d5435bc602a10ddd4206fd546f5b758e746cb2), ROM_NIBBLE | ROM_SHIFT_NIBBLE_LO | ROM_SKIP(1) )
	ROMX_LOAD( "pr06.84", 0x0801, 0x0400, CRC(f19c8eb0) SHA1(80f66d00caaf258232ea5e6adf515899abf53896), ROM_NIBBLE | ROM_SHIFT_NIBBLE_HI | ROM_SKIP(1) )
	ROMX_LOAD( "pr05.86", 0x0801, 0x0400, CRC(3dbc6360) SHA1(8d59dfee6e02ec29f755cc1c85ae236621009715), ROM_NIBBLE | ROM_SHIFT_NIBBLE_LO | ROM_SKIP(1) )

	ROM_REGION( 0x1a0, "proms", 0 ) // CCPU PROMS
	ROM_LOAD( "pr13.139", 0x0000, 0x0100, CRC(9edbf536) SHA1(036ad8a231284e05f44b1106d38fc0c7e041b6e8) )
	ROM_LOAD( "pr17.138", 0x0100, 0x0020, CRC(29dbfb87) SHA1(d8c40ab010b2ea30f29b2c443819e2b69f376c04) )
	ROM_LOAD( "pr18.137", 0x0120, 0x0020, CRC(98b7bd46) SHA1(fd7d0cac8783964bac36918e0ffcc07e2ea2081a) ) // this one differs from default
	ROM_LOAD( "pr19.136", 0x0140, 0x0020, CRC(07492cda) SHA1(32df9148797c23f70db47b840139c40e046dd710) )
	ROM_LOAD( "pr21.143", 0x0160, 0x0020, CRC(a481ca71) SHA1(ce145d61686f600cc16b77febfd5c783bf8c13b0) )
	ROM_LOAD( "pr20.72" , 0x0180, 0x0020, CRC(791ec9e1) SHA1(6f7fcce4aa3be9020595235568381588adaab88e) )
ROM_END


ROM_START( barrier )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "barrier.t7", 0x0000, 0x0800, CRC(7c3d68c8) SHA1(1138029552b73e94522b3b48096befc057d603c7) )
	ROM_LOAD16_BYTE( "barrier.p7", 0x0001, 0x0800, CRC(aec142b5) SHA1(b268936b82e072f38f1f1dd54e0bc88bcdf19925) )

	CCPU_PROMS
ROM_END


ROM_START( speedfrk )
	ROM_REGION( 0x2000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "speedfrk.t7", 0x0000, 0x0800, CRC(3552c03f) SHA1(c233dd064195b336556d7405b51065389b228c78) )
	ROM_LOAD16_BYTE( "speedfrk.p7", 0x0001, 0x0800, CRC(4b90cdec) SHA1(69e2312acdc22ef52236b1c4dfee9f51fcdcaa52) )
	ROM_LOAD16_BYTE( "speedfrk.u7", 0x1000, 0x0800, CRC(616c7cf9) SHA1(3c5bf59a09d85261f69e4b9d499cb7a93d79fb57) )
	ROM_LOAD16_BYTE( "speedfrk.r7", 0x1001, 0x0800, CRC(fbe90d63) SHA1(e42b17133464ae48c90263bba01a7d041e938a05) )

	CCPU_PROMS
ROM_END


ROM_START( starhawk )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "u7", 0x0000, 0x0800, CRC(376e6c5c) SHA1(7d9530ed2e75464578b541f61408ba64ee9d2a95) )
	ROM_LOAD16_BYTE( "r7", 0x0001, 0x0800, CRC(bb71144f) SHA1(79591cd3ef8df78ec26e158f7e82ca0dcd72260d) )

	CCPU_PROMS

	ROM_REGION( 0x100, "soundboard:sound_nl:2085.5e8e", 0 )
	ROM_LOAD("2085.5e8e", 0x000, 0x100, CRC(9edbf536) SHA1(036ad8a231284e05f44b1106d38fc0c7e041b6e8) )
ROM_END


ROM_START( sundance )
	ROM_REGION( 0x2000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "sundance.t7", 0x0000, 0x0800, CRC(d5b9cb19) SHA1(72dca386b48a582186898c32123d61b4fd58632e) )
	ROM_LOAD16_BYTE( "sundance.p7", 0x0001, 0x0800, CRC(445c4f20) SHA1(972d0b0613f154ee3347206cae05ee8c36796f84) )
	ROM_LOAD16_BYTE( "sundance.u7", 0x1000, 0x0800, CRC(67887d48) SHA1(be225dbd3508fad2711286834880065a4fc0a2fc) )
	ROM_LOAD16_BYTE( "sundance.r7", 0x1001, 0x0800, CRC(10b77ebd) SHA1(3d43bd47c498d5ea74a7322f8d25dbc0c0187534) )

	CCPU_PROMS
ROM_END


ROM_START( tailg )
	ROM_REGION( 0x2000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "tgunner.t70", 0x0000, 0x0800, CRC(21ec9a04) SHA1(b442f34360d1d4769e7bca73a2d79ce97d335460) )
	ROM_LOAD16_BYTE( "tgunner.p70", 0x0001, 0x0800, CRC(8d7410b3) SHA1(59ead49bd229a873f15334d0999c872d3d6581d4) )
	ROM_LOAD16_BYTE( "tgunner.t71", 0x1000, 0x0800, CRC(2c954ab6) SHA1(9edf189a19b50a9abf458d4ef8ba25b53934385e) )
	ROM_LOAD16_BYTE( "tgunner.p71", 0x1001, 0x0800, CRC(8e2c8494) SHA1(65e461ec4938f9895e5ac31442193e06c8731dc1) )

	CCPU_PROMS
ROM_END


ROM_START( skyfire ) // found on a set of 2 PCBs manufactured by Microhard, stickered 'Sky Fire'. Only difference from the original is that the title's ROM area has been 0-ed out
	ROM_REGION( 0x2000, "maincpu", 0 ) // all 2708
	ROM_LOAD16_BYTE( "rom3-x1-2.3t", 0x0000, 0x0400, CRC(211182e2) SHA1(0a5828c93f85d861fe0619c3681bf1f41269b889) )
	ROM_LOAD16_BYTE( "x9-1.3t",      0x0001, 0x0400, CRC(99bd1c6b) SHA1(37040caac25e0334e83333348cc19be3009f4b52) )
	ROM_LOAD16_BYTE( "rom4-y1-2.4t", 0x0800, 0x0400, CRC(e915fae5) SHA1(0f8edd7d5be0d37dd6b5f803f8ea5c2c6e362ed2) )
	ROM_LOAD16_BYTE( "y9-1.4t",      0x0801, 0x0400, CRC(e22adbd2) SHA1(864d22a66004c7b3b559f8e3578b11cdd913db51) )
	ROM_LOAD16_BYTE( "rom1-v1-2.1t", 0x1000, 0x0400, CRC(9850ba5e) SHA1(453cfa09f77faf6b9090b2f209a980c25f890850) )
	ROM_LOAD16_BYTE( "v9-1.1t",      0x1001, 0x0400, CRC(51509bc6) SHA1(42c8786c39c92df3d5560a772ffaf011ccf8927c) )
	ROM_LOAD16_BYTE( "rom2-w1-2.2t", 0x1800, 0x0400, CRC(85bd4353) SHA1(a2f7371ec528feb0c6cb4470fcc4eb35adc5aeb0) )
	ROM_LOAD16_BYTE( "w9-1.2t",      0x1801, 0x0400, CRC(b5f45d46) SHA1(d1fe69a630ee244645952bcbcdb9a44a3f67b04a) )

	ROM_REGION( 0x1a0, "proms", 0 ) // all MMI 6331 but f14 which is Fairchild 93417
	ROM_LOAD("f14", 0x000, 0x100, CRC(025996b1) SHA1(16e927c3a94c46ab2d870a37aa0dfacb4f95bdbf) ) // this one differs from the default, bad or intended?
	ROM_LOAD("e14", 0x100, 0x020, CRC(29dbfb87) SHA1(d8c40ab010b2ea30f29b2c443819e2b69f376c04) )
	ROM_LOAD("d14", 0x120, 0x020, CRC(9a05afbf) SHA1(5d806a42424942ba5ef0b70a1d629315b37f931b) )
	ROM_LOAD("c14", 0x140, 0x020, CRC(07492cda) SHA1(32df9148797c23f70db47b840139c40e046dd710) )
	ROM_LOAD("j14", 0x160, 0x020, CRC(a481ca71) SHA1(ce145d61686f600cc16b77febfd5c783bf8c13b0) )
	ROM_LOAD("e8",  0x180, 0x020, CRC(791ec9e1) SHA1(6f7fcce4aa3be9020595235568381588adaab88e) )
ROM_END


ROM_START( warrior )
	ROM_REGION( 0x2000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "warrior.t7", 0x0000, 0x0800, CRC(ac3646f9) SHA1(515c3acb638fad27fa57f6b438c8ec0b5b76f319) )
	ROM_LOAD16_BYTE( "warrior.p7", 0x0001, 0x0800, CRC(517d3021) SHA1(0483dcaf92c336a07d2c535823348ee886567e85) )
	ROM_LOAD16_BYTE( "warrior.u7", 0x1000, 0x0800, CRC(2e39340f) SHA1(4b3cfb3674dd2a668d4d65e28cb37d7ad20f118d) )
	ROM_LOAD16_BYTE( "warrior.r7", 0x1001, 0x0800, CRC(8e91b502) SHA1(27614c3a8613f49187039cfb05ee96303caf72ba) )

	CCPU_PROMS
ROM_END


ROM_START( armora )
	ROM_REGION( 0x4000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "ar414le.t6", 0x0000, 0x1000, CRC(d7e71f84) SHA1(0b29278a6a698f07eae597bc0a8650e91eaabffa) )
	ROM_LOAD16_BYTE( "ar414lo.p6", 0x0001, 0x1000, CRC(df1c2370) SHA1(b74834d1a591a741892ec41269a831d3590ff766) )
	ROM_LOAD16_BYTE( "ar414ue.u6", 0x2000, 0x1000, CRC(b0276118) SHA1(88f33cb2f46a89819c85f810c7cff812e918391e) )
	ROM_LOAD16_BYTE( "ar414uo.r6", 0x2001, 0x1000, CRC(229d779f) SHA1(0cbdd83eb224146944049346f30d9c72d3ad5f52) )

	CCPU_PROMS
ROM_END

ROM_START( armorap )
	ROM_REGION( 0x4000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "ar414le.t6", 0x0000, 0x1000, CRC(d7e71f84) SHA1(0b29278a6a698f07eae597bc0a8650e91eaabffa) )
	ROM_LOAD16_BYTE( "ar414lo.p6", 0x0001, 0x1000, CRC(df1c2370) SHA1(b74834d1a591a741892ec41269a831d3590ff766) )
	ROM_LOAD16_BYTE( "armorp.u7",  0x2000, 0x1000, CRC(4a86bd8a) SHA1(36647805c40688588dde81c7cbf4fe356b0974fc) )
	ROM_LOAD16_BYTE( "armorp.r7",  0x2001, 0x1000, CRC(d2dd4eae) SHA1(09afaeb0b8f88edb17e42bd2d754af0ae53e609a) )

	CCPU_PROMS
ROM_END

ROM_START( armorar )
	ROM_REGION( 0x4000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "armorr.t7", 0x0000, 0x0800, CRC(256d1ed9) SHA1(8c101356c3fe93f2f49d5dc9d739f3b37cdb98b5) )
	ROM_RELOAD(                   0x1000, 0x0800 )
	ROM_LOAD16_BYTE( "armorr.p7", 0x0001, 0x0800, CRC(bf75c158) SHA1(4d52630ae0ea2ad16bb5f577ad6d21f52e2f0a3c) )
	ROM_RELOAD(                   0x1001, 0x0800 )
	ROM_LOAD16_BYTE( "armorr.u7", 0x2000, 0x0800, CRC(ba68331d) SHA1(871c3f5b6c2845f270e3a272fdb07aed8b527641) )
	ROM_RELOAD(                   0x3000, 0x0800 )
	ROM_LOAD16_BYTE( "armorr.r7", 0x2001, 0x0800, CRC(fa14c0b3) SHA1(37b233f0dac51eaf7d325628a6cced9367b6b6cb) )
	ROM_RELOAD(                   0x3001, 0x0800 )

	CCPU_PROMS
ROM_END


ROM_START( ripoff )
	ROM_REGION( 0x2000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "ripoff.t7", 0x0000, 0x0800, CRC(40c2c5b8) SHA1(bc1f3b540475c9868443a72790a959b1f36b93c6) )
	ROM_LOAD16_BYTE( "ripoff.p7", 0x0001, 0x0800, CRC(a9208afb) SHA1(ea362494855be27a07014832b01e65c1645385d0) )
	ROM_LOAD16_BYTE( "ripoff.u7", 0x1000, 0x0800, CRC(29c13701) SHA1(5e7672deffac1fa8f289686a5527adf7e51eb0bb) )
	ROM_LOAD16_BYTE( "ripoff.r7", 0x1001, 0x0800, CRC(150bd4c8) SHA1(e1e2f0dfec4f53d8ff67b0e990514c304f496b3a) )

	CCPU_PROMS
ROM_END


ROM_START( starcas )
	ROM_REGION( 0x2000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "starcas3.t7", 0x0000, 0x0800, CRC(b5838b5d) SHA1(6ac30be55514cba55180c85af69072b5056d1d4c) )
	ROM_LOAD16_BYTE( "starcas3.p7", 0x0001, 0x0800, CRC(f6bc2f4d) SHA1(ef6f01556b154cfb3e37b2a99d6ea6292e5ec844) )
	ROM_LOAD16_BYTE( "starcas3.u7", 0x1000, 0x0800, CRC(188cd97c) SHA1(c021e93a01e9c65013073de551a8c24fd1a68bde) )
	ROM_LOAD16_BYTE( "starcas3.r7", 0x1001, 0x0800, CRC(c367b69d) SHA1(98354d34ceb03e080b1846611d533be7bdff01cc) )

	CCPU_PROMS
ROM_END

ROM_START( starcasc )
	ROM_REGION( 0x2000, "maincpu", 0 ) // all HN462716G, all labels hand-written
	ROM_LOAD16_BYTE( "ctsc926_ue_3265.t7", 0x0000, 0x0800, CRC(c140a1bb) SHA1(82c5871af7171408cccb93b4905312856f16a607) )
	ROM_LOAD16_BYTE( "ctsc926_le_deac.p7", 0x0001, 0x0800, CRC(8a074f6c) SHA1(e6be9897b4e8b94a9a75ab03c39637f499811d3a) )
	ROM_LOAD16_BYTE( "ctsc926_u0_48e7.u7", 0x1000, 0x0800, CRC(ed136f11) SHA1(75965b79a7e5466fb80b61d3dd024907a9a0248a) )
	ROM_LOAD16_BYTE( "ctsc926_l0_f707.r7", 0x1001, 0x0800, CRC(1930b1fb) SHA1(8f8370f62536ab7529ad74e51698973ee61b97e2) )

	CCPU_PROMS
ROM_END

ROM_START( starcasp )
	ROM_REGION( 0x2000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "starcasp.t7", 0x0000, 0x0800, CRC(d2c551a2) SHA1(90b5e1c6988839b812028f1baaea16420c011c08) )
	ROM_LOAD16_BYTE( "starcasp.p7", 0x0001, 0x0800, CRC(baa4e422) SHA1(9035ac675fcbbb93ae3f658339fdfaef47796dab) )
	ROM_LOAD16_BYTE( "starcasp.u7", 0x1000, 0x0800, CRC(26941991) SHA1(4417f2f3e437c1f39ff389362467928f57045d74) )
	ROM_LOAD16_BYTE( "starcasp.r7", 0x1001, 0x0800, CRC(5dd151e5) SHA1(f3b0e2bd3121ac0649938eb2f676d171bcc7d4dd) )

	CCPU_PROMS
ROM_END

ROM_START( starcas1 )
	ROM_REGION( 0x2000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "starcast.t7", 0x0000, 0x0800, CRC(65d0a225) SHA1(e1fbee5ff42dd040ab2e90bbe2189fcb76d6167e) )
	ROM_LOAD16_BYTE( "starcast.p7", 0x0001, 0x0800, CRC(d8f58d9a) SHA1(abba459431dcacc75099b0d340b957be71b89cfd) )
	ROM_LOAD16_BYTE( "starcast.u7", 0x1000, 0x0800, CRC(d4f35b82) SHA1(cd4561ce8e1d0554ac1a8925bbf46d2c676a3b80) )
	ROM_LOAD16_BYTE( "starcast.r7", 0x1001, 0x0800, CRC(9fd3de54) SHA1(17195a490b190e68660829850ff9d702ca1939bb) )

	CCPU_PROMS
ROM_END

ROM_START( starcase )
	ROM_REGION( 0x2000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "starcast.t7", 0x0000, 0x0800, CRC(65d0a225) SHA1(e1fbee5ff42dd040ab2e90bbe2189fcb76d6167e) )
	ROM_LOAD16_BYTE( "starcast.p7", 0x0001, 0x0800, CRC(d8f58d9a) SHA1(abba459431dcacc75099b0d340b957be71b89cfd) )
	ROM_LOAD16_BYTE( "starcast.u7", 0x1000, 0x0800, CRC(d4f35b82) SHA1(cd4561ce8e1d0554ac1a8925bbf46d2c676a3b80) )
	ROM_LOAD16_BYTE( "mottoeis.r7", 0x1001, 0x0800, CRC(a2c1ed52) SHA1(ed9743f44ee98c9e7c2a6819ec681af7c7a97fc9) )

	CCPU_PROMS
ROM_END

ROM_START( stellcas )
	ROM_REGION( 0x2000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "starcast.t7", 0x0000, 0x0800, CRC(65d0a225) SHA1(e1fbee5ff42dd040ab2e90bbe2189fcb76d6167e) )
	ROM_LOAD16_BYTE( "starcast.p7", 0x0001, 0x0800, CRC(d8f58d9a) SHA1(abba459431dcacc75099b0d340b957be71b89cfd) )
	ROM_LOAD16_BYTE( "elttron.u7",  0x1000, 0x0800, CRC(d5b44050) SHA1(a5dd6050ab1a3b0275a229845bc5e9524e2da69c) )
	ROM_LOAD16_BYTE( "elttron.r7",  0x1001, 0x0800, CRC(6f1f261e) SHA1(a22a52af12a5cfbb9031fdd12c9c78db28f28ff1) )

	CCPU_PROMS
ROM_END

ROM_START( spaceftr )
	ROM_REGION( 0x2000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "fortrest7.7t", 0x0000, 0x0800, CRC(65d0a225) SHA1(e1fbee5ff42dd040ab2e90bbe2189fcb76d6167e) )
	ROM_LOAD16_BYTE( "fortresp7.7p", 0x0001, 0x0800, CRC(d8f58d9a) SHA1(abba459431dcacc75099b0d340b957be71b89cfd) )
	ROM_LOAD16_BYTE( "fortresu7.7u", 0x1000, 0x0800, CRC(13b0287c) SHA1(366a23fd10684975bd5ee190e5227e47a0298ad5) )
	ROM_LOAD16_BYTE( "fortresr7.7r", 0x1001, 0x0800, CRC(a2c1ed52) SHA1(ed9743f44ee98c9e7c2a6819ec681af7c7a97fc9) )

	ROM_REGION( 0x1a0, "proms", 0 ) // CCPU PROMS
	ROM_LOAD("prom.f14",       0x000, 0x100, CRC(9edbf536) SHA1(036ad8a231284e05f44b1106d38fc0c7e041b6e8) )
	ROM_LOAD("prom.e14",       0x100, 0x020, CRC(29dbfb87) SHA1(d8c40ab010b2ea30f29b2c443819e2b69f376c04) )
	ROM_LOAD("prom.d14",       0x120, 0x020, CRC(9a05afbf) SHA1(5d806a42424942ba5ef0b70a1d629315b37f931b) )
	ROM_LOAD("prom.c14",       0x140, 0x020, CRC(07492cda) SHA1(32df9148797c23f70db47b840139c40e046dd710) )
	ROM_LOAD("prom.j14",       0x160, 0x020, CRC(a481ca71) SHA1(ce145d61686f600cc16b77febfd5c783bf8c13b0) )
	ROM_LOAD("prom6331-1j.e8", 0x180, 0x020, CRC(8c85e786) SHA1(b95be00ea97263196b40672b5b239d53218eca4d) ) // this one differs from default
ROM_END


ROM_START( solarq )
	ROM_REGION( 0x4000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "sq-2_le.6t", 0x0000, 0x1000, CRC(1f3c5333) SHA1(58d847b5f009a0363ae116768b22d0bcfb3d60a4) )
	ROM_LOAD16_BYTE( "sq-2_lo.6p", 0x0001, 0x1000, CRC(d6c16bcc) SHA1(6953bdc698da060d37f6bc33a810ba44595b1257) )
	ROM_LOAD16_BYTE( "sq-2_ue.6u", 0x2000, 0x1000, CRC(a5970e5c) SHA1(9ac07924ca86d003964022cffdd6a0436dde5624) )
	ROM_LOAD16_BYTE( "sq-2_uo.6r", 0x2001, 0x1000, CRC(b763fff2) SHA1(af1fd978e46a4aee3048e6e36c409821d986f7ee) )

	CCPU_PROMS
ROM_END


ROM_START( boxingb )
	ROM_REGION( 0x8000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "u1a", 0x0000, 0x1000, CRC(d3115b0f) SHA1(9448e7ac1cdb5c7e0739623151be230ab630c4ea) )
	ROM_LOAD16_BYTE( "u1b", 0x0001, 0x1000, CRC(3a44268d) SHA1(876ebe942ded787cfe357563a33d3e26a1483c5a) )
	ROM_LOAD16_BYTE( "u2a", 0x2000, 0x1000, CRC(c97a9cbb) SHA1(8bdeb9ee6b24c0a4554bbf4532a43481a0360019) )
	ROM_LOAD16_BYTE( "u2b", 0x2001, 0x1000, CRC(98d34ff5) SHA1(6767a02a99a01712383300f9acb96cdeffbc9c69) )
	ROM_LOAD16_BYTE( "u3a", 0x4000, 0x1000, CRC(5bb3269b) SHA1(a9dbc91b1455760f10bad0d2ccf540e040a00d4e) )
	ROM_LOAD16_BYTE( "u3b", 0x4001, 0x1000, CRC(85bf83ad) SHA1(9229042e39c53fae56dc93f8996bf3a3fcd35cb8) )
	ROM_LOAD16_BYTE( "u4a", 0x6000, 0x1000, CRC(25b51799) SHA1(46465fe62907ae66a0ce730581e4e9ba330d4369) )
	ROM_LOAD16_BYTE( "u4b", 0x6001, 0x1000, CRC(7f41de6a) SHA1(d01dffad3cb6e76c535a034ea0277dce5801c5f1) )

	CCPU_PROMS
ROM_END


ROM_START( wotw )
	ROM_REGION( 0x4000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "wow_le.t7", 0x0000, 0x1000, CRC(b16440f9) SHA1(9656a26814736f8ff73575063b5ebbb2e8aa7dd0) )
	ROM_LOAD16_BYTE( "wow_lo.p7", 0x0001, 0x1000, CRC(bfdf4a5a) SHA1(db4eceb68e17020d0a597ba105ec3b91ce48b7c1) )
	ROM_LOAD16_BYTE( "wow_ue.u7", 0x2000, 0x1000, CRC(9b5cea48) SHA1(c2bc002e550a0d36e713d07f6aefa79c70b8e284) )
	ROM_LOAD16_BYTE( "wow_uo.r7", 0x2001, 0x1000, CRC(c9d3c866) SHA1(57a47bf06838fe562981321249fe5ae585316f22) )

	CCPU_PROMS
ROM_END

ROM_START( wotwc )
	ROM_REGION( 0x4000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "wow_le.t7", 0x0000, 0x1000, CRC(b16440f9) SHA1(9656a26814736f8ff73575063b5ebbb2e8aa7dd0) )
	ROM_LOAD16_BYTE( "wow_lo.p7", 0x0001, 0x1000, CRC(bfdf4a5a) SHA1(db4eceb68e17020d0a597ba105ec3b91ce48b7c1) )
	ROM_LOAD16_BYTE( "wow_ue.u7", 0x2000, 0x1000, CRC(9b5cea48) SHA1(c2bc002e550a0d36e713d07f6aefa79c70b8e284) )
	ROM_LOAD16_BYTE( "wow_uo.r7", 0x2001, 0x1000, CRC(c9d3c866) SHA1(57a47bf06838fe562981321249fe5ae585316f22) )

	CCPU_PROMS
ROM_END


ROM_START( demon )
	ROM_REGION( 0x4000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "demon.7t",  0x0000, 0x1000, CRC(866596c1) SHA1(65202dcd5c6bf6c11fe76a89682a1505b1870cc9) )
	ROM_LOAD16_BYTE( "demon.7p",  0x0001, 0x1000, CRC(1109e2f1) SHA1(c779b6af1ca09e2e295fc9a0e221ddf283b683ed) )
	ROM_LOAD16_BYTE( "demon.7u",  0x2000, 0x1000, CRC(d447a3c3) SHA1(32f6fb01231aa4f3d93e32d639a89f0cf9624a71) )
	ROM_LOAD16_BYTE( "demon.7r",  0x2001, 0x1000, CRC(64b515f0) SHA1(2dd9a6d784ec1baf31e8c6797ddfdc1423c69470) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "demon.snd", 0x0000, 0x1000, CRC(1e2cc262) SHA1(2aae537574ac69c92a3c6400b971e994de88d915) )

	CCPU_PROMS
ROM_END


ROM_START( qb3 )
	ROM_REGION( 0x8000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "qb3_le_t7.bin",  0x0000, 0x2000, CRC(adaaee4c) SHA1(35c6bbb50646a3ddec12f115fcf3f2283e15b0a0) )
	ROM_LOAD16_BYTE( "qb3_lo_p7.bin",  0x0001, 0x2000, CRC(72f6199f) SHA1(ae8f81f218940cfc3aef8f82dfe8cc14220770ce) )
	ROM_LOAD16_BYTE( "qb3_ue_u7.bin",  0x4000, 0x2000, CRC(050a996d) SHA1(bf29236112746b5925b29fb231f152a4bde3f4f9) )
	ROM_LOAD16_BYTE( "qb3_uo_r7.bin",  0x4001, 0x2000, CRC(33fa77a2) SHA1(27a6853f8c2614a2abd7bfb9a62c357797312068) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "qb3_snd_u12.bin", 0x0000, 0x1000, CRC(f86663de) SHA1(29c7e75ba22be00d59fc8de5de6d94fcee287a09) )
	ROM_LOAD( "qb3_snd_u11.bin", 0x1000, 0x1000, CRC(32ed58fc) SHA1(483a19f0d540d7d348fce4274fba254ee95bc8d6) )

	CCPU_PROMS
ROM_END



/*************************************
 *
 *  Driver initialization
 *
 *************************************/

void cinemat_state::init_speedfrk()
{
	m_maincpu->space(AS_IO).install_read_handler(0x00, 0x03, read8sm_delegate(*this, FUNC(cinemat_state::speedfrk_wheel_r)));
	m_maincpu->space(AS_IO).install_read_handler(0x04, 0x06, read8sm_delegate(*this, FUNC(cinemat_state::speedfrk_gear_r)));
	save_item(NAME(m_gear));
}


void cinemat_16level_state::init_sundance()
{
	m_maincpu->space(AS_IO).install_read_handler(0x00, 0x0f, read8sm_delegate(*this, FUNC(cinemat_16level_state::sundance_inputs_r)));
}


void cinemat_color_state::init_boxingb()
{
	m_maincpu->space(AS_IO).install_read_handler(0x0c, 0x0f, read8sm_delegate(*this, FUNC(cinemat_color_state::boxingb_dial_r)));
}


void qb3_state::init_qb3()
{
	membank("bank1")->configure_entries(0, 4, m_rambase, 0x100*2);

	save_item(NAME(m_qb3_lastx));
	save_item(NAME(m_qb3_lasty));
}



/*************************************
 *
 *  Game drivers
 *
 *************************************/

GAME(  1977, spacewar, 0,        spacewar, spacewar, cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "Cinematronics", "Space Wars", MACHINE_SUPPORTS_SAVE )
GAME(  1978, spaceshp, spacewar, spacewar, spaceshp, cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "Cinematronics (Sega license)", "Space Ship", MACHINE_SUPPORTS_SAVE )
GAMEL( 1979, barrier,  0,        barrier,  barrier,  cinemat_state,         empty_init,    ORIENTATION_FLIP_X ^ ROT270, "Cinematronics (Vectorbeam license)", "Barrier", MACHINE_SUPPORTS_SAVE, layout_barrier ) // developed by Cinematronics, then (when they noticed it wasn't going to be a successful game) sold to Vectorbeam, and ultimately back in the hands of Cinematronics again after they bought the dying company Vectorbeam
GAMEL( 1979, speedfrk, 0,        speedfrk, speedfrk, cinemat_state,         init_speedfrk, ORIENTATION_FLIP_Y,   "Vectorbeam", "Speed Freak", MACHINE_SUPPORTS_SAVE, layout_speedfrk )
GAME(  1979, starhawk, 0,        starhawk, starhawk, cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "Cinematronics", "Starhawk", MACHINE_SUPPORTS_SAVE )
GAMEL( 1979, sundance, 0,        sundance, sundance, cinemat_16level_state, init_sundance, ORIENTATION_FLIP_X ^ ROT270, "Cinematronics", "Sundance", MACHINE_SUPPORTS_SAVE, layout_sundance )
GAMEL( 1979, tailg,    0,        tailg,    tailg,    cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "Cinematronics", "Tailgunner", MACHINE_SUPPORTS_SAVE, layout_tailg )
GAMEL( 1979, skyfire,  tailg,    tailg,    tailg,    cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "bootleg (Microhard)", "Sky Fire", MACHINE_SUPPORTS_SAVE, layout_tailg )
GAMEL( 1979, warrior,  0,        warrior,  warrior,  cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "Vectorbeam", "Warrior", MACHINE_SUPPORTS_SAVE, layout_warrior )
GAMEL( 1980, armora,   0,        armora,   armora,   cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "Cinematronics", "Armor Attack", MACHINE_SUPPORTS_SAVE, layout_armora )
GAMEL( 1980, armorap,  armora,   armora,   armora,   cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "Cinematronics", "Armor Attack (prototype)", MACHINE_SUPPORTS_SAVE, layout_armora )
GAMEL( 1980, armorar,  armora,   armora,   armora,   cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "Cinematronics (Rock-Ola license)", "Armor Attack (Rock-Ola)",  MACHINE_SUPPORTS_SAVE, layout_armora )
GAME(  1980, ripoff,   0,        ripoff,   ripoff,   cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "Cinematronics", "Rip Off", MACHINE_SUPPORTS_SAVE )
GAMEL( 1980, starcas,  0,        starcas,  starcas,  cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "Cinematronics", "Star Castle (version 3)", MACHINE_SUPPORTS_SAVE, layout_starcas )
GAMEL( 1980, starcas1, starcas,  starcas,  starcas,  cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "Cinematronics", "Star Castle (older)", MACHINE_SUPPORTS_SAVE, layout_starcas )
GAMEL( 1980, starcasc, starcas,  starcas,  starcasc, cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "Cinematronics", "Star Castle (cocktail)", MACHINE_SUPPORTS_SAVE, layout_starcas )
GAMEL( 1980, starcasp, starcas,  starcas,  starcas,  cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "Cinematronics", "Star Castle (prototype)", MACHINE_SUPPORTS_SAVE, layout_starcas )
GAMEL( 1980, starcase, starcas,  starcas,  starcas,  cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "Cinematronics (Mottoeis license)", "Star Castle (Mottoeis)", MACHINE_SUPPORTS_SAVE, layout_starcas )
GAMEL( 1980, stellcas, starcas,  starcas,  starcas,  cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "bootleg (Elettronolo)", "Stellar Castle (Elettronolo)", MACHINE_SUPPORTS_SAVE, layout_starcas )
GAMEL( 1981, spaceftr, starcas,  starcas,  starcas,  cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "Cinematronics (Zaccaria license)", "Space Fortress (Zaccaria)", MACHINE_SUPPORTS_SAVE, layout_starcas )
GAMEL( 1981, solarq,   0,        solarq,   solarq,   cinemat_64level_state, empty_init,    ORIENTATION_FLIP_Y ^ ORIENTATION_FLIP_X, "Cinematronics", "Solar Quest (rev 10 8 81)", MACHINE_SUPPORTS_SAVE, layout_solarq )
GAME(  1981, boxingb,  0,        boxingb,  boxingb,  cinemat_color_state,   init_boxingb,  ORIENTATION_FLIP_Y,   "Cinematronics", "Boxing Bugs", MACHINE_SUPPORTS_SAVE )
GAMEL( 1981, wotw,     0,        wotw,     wotw,     cinemat_state,         empty_init,    ORIENTATION_FLIP_Y,   "Cinematronics", "War of the Worlds", MACHINE_SUPPORTS_SAVE, layout_wotw )
GAME(  1981, wotwc,    wotw,     wotwc,    wotw,     cinemat_color_state,   empty_init,    ORIENTATION_FLIP_Y,   "Cinematronics", "War of the Worlds (color)", MACHINE_SUPPORTS_SAVE )
GAMEL( 1982, demon,    0,        demon,    demon,    demon_state,           empty_init,    ORIENTATION_FLIP_Y,   "Rock-Ola", "Demon", MACHINE_SUPPORTS_SAVE, layout_demon )
GAME(  1982, qb3,      0,        qb3,      qb3,      qb3_state,             init_qb3,      ORIENTATION_FLIP_Y,   "Rock-Ola", "QB-3 (prototype)", MACHINE_IMPERFECT_GRAPHICS | MACHINE_SUPPORTS_SAVE )