// license:BSD-3-Clause
// copyright-holders:AJR
/***************************************************************************

    GI ER1400 1400 Bit Serial Electrically Alterable Read-Only Memory

    This device uses negative logic. The emulation uses logic levels for
    all data, which means that inputs should be inverted if non-inverting
    drivers are used and vice versa.

    Vgg should be 35 volts below Vss. Logic one is output as 12 volts below
    Vss. Vss represents logic zero, which is nominally GND but may be +12V.

    The frequency of the input clock should be between 10 kHz and 17 kHz,
    with a 35–65% duty cycle when the device is in operation. The clock may
    be left at logic zero when the device is in standby.

    Write and erase times are 10 ms (min), 15 ms (typical), 24 ms (max).

***************************************************************************/

#include "emu.h"
#include "machine/er1400.h"

// device type definition
DEFINE_DEVICE_TYPE(ER1400, er1400_device, "er1400", "ER1400 Serial EAROM (100x14)")


//**************************************************************************
//  ER1400 DEVICE
//**************************************************************************

//-------------------------------------------------
//  er1400_device - constructor
//-------------------------------------------------

er1400_device::er1400_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: device_t(mconfig, ER1400, tag, owner, clock)
	, device_nvram_interface(mconfig, *this)
	, m_clock_input(0)
	, m_code_input(0)
	, m_data_input(0)
	, m_data_output(0)
	, m_write_time(attotime::never)
	, m_erase_time(attotime::never)
	, m_data_register(0)
	, m_address_register(0)
{
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void er1400_device::device_start()
{
	save_item(NAME(m_clock_input));
	save_item(NAME(m_code_input));
	save_item(NAME(m_data_input));
	save_item(NAME(m_data_output));

	save_item(NAME(m_write_time));
	save_item(NAME(m_erase_time));

	save_item(NAME(m_data_register));
	save_item(NAME(m_address_register));

	m_data_array = std::make_unique<u16[]>(100);
	save_pointer(m_data_array.get(), "m_data_array", 100);
}


//-------------------------------------------------
//  nvram_default - called to initialize NVRAM to
//  its default state
//-------------------------------------------------

void er1400_device::nvram_default()
{
	// all locations erased
	std::fill(&m_data_array[0], &m_data_array[100], 0x3fff);
}


//-------------------------------------------------
//  nvram_read - called to read NVRAM from the
//  .nv file
//-------------------------------------------------

void er1400_device::nvram_read(emu_file &file)
{
	file.read(&m_data_array[0], 200);
}


//-------------------------------------------------
//  nvram_write - called to write NVRAM to the
//  specified file
//-------------------------------------------------

void er1400_device::nvram_write(emu_file &file)
{
	file.write(&m_data_array[0], 200);
}


//-------------------------------------------------
//  read_data - read addressed word into data
//  register
//-------------------------------------------------

void er1400_device::read_data()
{
	int selected = 0;
	m_data_register = 0;
	for (int tens = 10; tens < 20; tens++)
	{
		for (int units = 0; units < 10; units++)
		{
			if (BIT(m_address_register, tens) && BIT(m_address_register, units))
			{
				offs_t offset = 10 * (tens - 10) + units;
				logerror("Reading data at %d (%04X) into register\n", offset, m_data_array[offset]);
				m_data_register |= m_data_array[offset];
				selected++;
			}
		}
	}

	if (selected != 1)
		logerror("%d addresses selected for read operation\n", selected);
}


//-------------------------------------------------
//  write_data - write data register to addressed
//  word (which must have been erased first)
//-------------------------------------------------

void er1400_device::write_data()
{
	int selected = 0;
	for (int tens = 10; tens < 20; tens++)
	{
		for (int units = 0; units < 10; units++)
		{
			if (BIT(m_address_register, tens) && BIT(m_address_register, units))
			{
				offs_t offset = 10 * (tens - 10) + units;
				if ((m_data_array[offset] & ~m_data_register) != 0)
				{
					logerror("Writing data %04X at %d\n", m_data_register, offset);
					m_data_array[offset] &= m_data_register;
				}
				selected++;
			}
		}
	}

	if (selected != 1)
		logerror("%d addresses selected for write operation\n", selected);
}


//-------------------------------------------------
//  erase_data - erase data at addressed word to
//  all ones
//-------------------------------------------------

void er1400_device::erase_data()
{
	int selected = 0;
	for (int tens = 10; tens < 20; tens++)
	{
		for (int units = 0; units < 10; units++)
		{
			if (BIT(m_address_register, tens) && BIT(m_address_register, units))
			{
				offs_t offset = 10 * (tens - 10) + units;
				if (m_data_array[offset] != 0x3fff)
				{
					logerror("Erasing data at %d\n", offset);
					m_data_array[offset] = 0x3fff;
				}
				selected++;
			}
		}
	}

	if (selected != 1)
		logerror("%d addresses selected for erase operation\n", selected);
}


//-------------------------------------------------
//  data_w - write data input line
//-------------------------------------------------

WRITE_LINE_MEMBER(er1400_device::data_w)
{
	m_data_input = state;
}


//-------------------------------------------------
//  c1_w - write to first control line
//-------------------------------------------------

WRITE_LINE_MEMBER(er1400_device::c1_w)
{
	m_code_input = (m_code_input & 3) | (state << 2);
}


//-------------------------------------------------
//  c2_w - write to second control line
//-------------------------------------------------

WRITE_LINE_MEMBER(er1400_device::c2_w)
{
	m_code_input = (m_code_input & 5) | (state << 1);
}


//-------------------------------------------------
//  c3_w - write to third control line
//-------------------------------------------------

WRITE_LINE_MEMBER(er1400_device::c3_w)
{
	m_code_input = (m_code_input & 6) | state;
}


//-------------------------------------------------
//  clock_w - write to clock line
//-------------------------------------------------

WRITE_LINE_MEMBER(er1400_device::clock_w)
{
	if (m_clock_input == bool(state))
		return;
	m_clock_input = bool(state);

	// Commands are clocked by a logical 1 -> 0 transition (i.e. rising edge)
	if (!state)
	{
		m_data_output = false;

		if (machine().time() >= m_write_time)
			write_data();
		if (m_code_input != 6)
			m_write_time = attotime::never;

		if (machine().time() >= m_erase_time)
			erase_data();
		if (m_code_input != 2)
			m_erase_time = attotime::never;

		switch (m_code_input)
		{
		case 0: // standby
			break;

		case 1: default: // not used
			break;

		case 2: // erase
			if (m_erase_time == attotime::never)
			{
				logerror("Entering erase command\n");
				m_erase_time = machine().time() + attotime::from_msec(15);
			}
			break;

		case 3: // accept address
			m_address_register = (m_address_register << 1) | m_data_input;
			m_address_register &= 0xfffff;
			break;

		case 4: // read
			read_data();
			break;

		case 5: // shift data out
			m_data_output = BIT(m_data_register, 13);
			m_data_register = (m_data_register & 0x1fff) << 1;
			break;

		case 6: // write
			if (m_write_time == attotime::never)
			{
				logerror("Entering write command\n");
				m_write_time = machine().time() + attotime::from_msec(15);
			}
			break;

		case 7: // accept data
			m_data_register = (m_data_register & 0x1fff) << 1;
			m_data_register |= m_data_input;
			break;
		}
	}
}


//-------------------------------------------------
//  data_r - read data line
//-------------------------------------------------

READ_LINE_MEMBER(er1400_device::data_r)
{
	return m_data_input | m_data_output;
}
