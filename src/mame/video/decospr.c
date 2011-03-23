/* Data East Sprite Chip
   DECO 52

   note, we have pri callbacks and checks to drop back to plain drawgfx because not all drivers are using pdrawgfx yet, they probably should be...
   some games have different visible areas, but are confirmed as the same sprite chip.

   games with alpha aren't supported here yet, in most cases they need better mixing anyway, probably rendering to screen buffers and manual mixing.
   see m_sprite_bitmap... (note, if using that you must also use BITMAP_FORMAT_RGB32 in the machine config)

   used by:
   
   dblewing.c
   tumblep.c
   dietgo.c
   supbtime.c
   simpl156.c
   deco156.c
   pktgaldx.c
   backfire.c
   darkseal.c
   sshangha.c (could probably use pdrawgfx, not m_sprite_bitmap)
   cbuster.c (could probably use pdrawgfx, not m_sprite_bitmap)
   mirage.c (could probably use pdrawgfx, not m_sprite_bitmap)

   partially converted:
   cninja.c (mutantf uses alpha etc.)
   deco32.c - video mixing

   difficult to convert:
   rohga.c - alpha effects, extra rom banking on the sprites etc. causes problems
   lemmings.c - priority stuff (plus sprites seem somewhat different anyway??)
   dassault.c - video mixing
   boogwing.c - video mixing
*/


#include "emu.h"
#include "decospr.h"


decospr_device_config::decospr_device_config(const machine_config &mconfig, const char *tag, const device_config *owner, UINT32 clock)
	: device_config(mconfig, static_alloc_device_config, "decospr_device", tag, owner, clock)
{
	m_gfxregion = 0;
	m_pricallback = NULL;
}

device_config *decospr_device_config::static_alloc_device_config(const machine_config &mconfig, const char *tag, const device_config *owner, UINT32 clock)
{
	return global_alloc(decospr_device_config(mconfig, tag, owner, clock));
}

device_t *decospr_device_config::alloc_device(running_machine &machine) const
{
	return auto_alloc(&machine, decospr_device(machine, *this));
}

void decospr_device_config::set_gfx_region(device_config *device, int gfxregion)
{
	decospr_device_config *dev = downcast<decospr_device_config *>(device);
	dev->m_gfxregion = gfxregion;
//	printf("decospr_device_config::set_gfx_region()\n");
}

void decospr_device_config::set_pri_callback(device_config *device, decospr_priority_callback_func callback)
{
	decospr_device_config *dev = downcast<decospr_device_config *>(device);
	dev->m_pricallback = callback;
//	printf("decospr_device_config::set_pri_callback()\n");
}

decospr_device::decospr_device(running_machine &_machine, const decospr_device_config &config)
	: device_t(_machine, config),
	  m_config(config),
  	  m_gfxregion(m_config.m_gfxregion),
	  m_pricallback(m_config.m_pricallback)
{
}

void decospr_device::device_start()
{
//	sprite_kludge_x = sprite_kludge_y = 0;
//	printf("decospr_device::device_start()\n");
	m_sprite_bitmap = 0;
}

void decospr_device::device_reset()
{
	//printf("decospr_device::device_reset()\n");
}

/*
void decospr_device::decospr_sprite_kludge(int x, int y)
{
	sprite_kludge_x = x;
	sprite_kludge_y = y;
}
*/

void decospr_device::alloc_sprite_bitmap(running_machine* machine)
{
	m_sprite_bitmap =  auto_bitmap_alloc(machine, machine->primary_screen->width(), machine->primary_screen->height(), BITMAP_FORMAT_INDEXED16);
}

void decospr_device::set_pri_callback(decospr_priority_callback_func callback)
{
	m_pricallback = callback;
}


/*

STANDARD FORMAT

offs +0
-------- --------
efFbSssy yyyyyyyy

s = size (height)
S = size (width) (double wings)
f = flipy
F = flipx
b = flash/blink
y = ypos
e = extra priority bit (or at least externally detectable by mixer circuits)

offs +1
-------- --------
tttttttt tttttttt

t = sprite tile

offs +2
-------- --------
ppcccccx xxxxxxxx

c = colour palette
p = priority
x = xpos

offs +3
-------- -------- (unused)

in reality all the colour / priority (and extra priority) bits are externally detectable and can be used for
priority and mixing, the sprite palette in twocrude for example isn't in a single block (there is a gap for the tilemaps)
but the mixing circuits take care of this

ALTERNATE FORMAT (used by Mutant Fighter, Captain America)

offs +0
-------- --------
fFbyyyyy yyyyyyyy

f = flipy
F = flipx
b = flash/blink
y = ypos

offs +1
-------- --------
xxxxxxxx xxxxxxxx

x = xpos

offs +2
-------- --------
ssssSSSS pppccccc

s = size (height)
S = size (width) (double wings)

offs +3
-------- --------
tttttttt tttttttt

t = sprite tile

todo: the priotity callback for using pdrawgfx should really pack those 8 bits, and pass them instead of currently just
passing offs+2 which lacks the extra priority bit

todo: add support for alternate format + basic blend mixing

*/


void decospr_device::draw_sprites( running_machine *machine, bitmap_t *bitmap, const rectangle *cliprect, UINT16* spriteram, int sizewords, bool invert_flip )
{

	if (m_sprite_bitmap && m_pricallback)
		fatalerror("m_sprite_bitmap && m_pricallback is invalid");

	if (m_sprite_bitmap)
		bitmap_fill(m_sprite_bitmap, cliprect, 0);


	int offs, end, incr;

	int flipscreen = flip_screen_get(machine);

	if (invert_flip)
		flipscreen = !flipscreen;


	if (m_pricallback)
	{
		offs = sizewords-4;
		end = -4;
		incr = -4;
	}
	else
	{
		offs = 0;
		end = sizewords;
		incr = 4;
	}

	while (offs!=end)
	{
		int x, y, sprite, colour, multi, mult2, fx, fy, inc, flash, mult, xsize, pri;
		
		sprite = spriteram[offs + 1];

		y = spriteram[offs];
		flash = y & 0x1000;
		xsize = y & 0x0800;
		if (!(flash && (machine->primary_screen->frame_number() & 1)))
		{

			x = spriteram[offs + 2];
			
			if (!m_sprite_bitmap)
				colour = (x >> 9) & 0x1f;
			else
			{
				colour = (x >> 9) & 0x7f;
				if (y&0x8000) colour |= 0x80; // fghthist uses this to mark priority
				//colour *= 0x10; // for raw drawing
			}

	
			if (m_pricallback)
				pri = m_pricallback(x);
			else
				pri = 0;

			fx = y & 0x2000;
			fy = y & 0x4000;
			multi = (1 << ((y & 0x0600) >> 9)) - 1;	/* 1x, 2x, 4x, 8x height */

			if (cliprect->max_x>256)
			{
				x = x & 0x01ff;
				y = y & 0x01ff;
				if (x >= 320) x -= 512;
				if (y >= 256) y -= 512;
				y = 240 - y;
				x = 304 - x;
			}
			else
			{

				x = x & 0x01ff;
				y = y & 0x01ff;
				if (x >= 256) x -= 512;
				if (y >= 256) y -= 512;
				y = 240 - y;
				x = 240 - x;
			}

			//if (x <= 320)
			{

				sprite &= ~multi;
				if (fy)
					inc = -1;
				else
				{
					sprite += multi;
					inc = 1;
				}

				if (flipscreen)
				{
					y = 240 - y;

					if (cliprect->max_x>256)
						x = 304 - x;
					else
						x = 240 - x;

					if (fx) fx = 0; else fx = 1;
					if (fy) fy = 0; else fy = 1;
					mult = 16;
				}
				else
					mult = -16;

				mult2 = multi + 1;

				while (multi >= 0)
				{
					if(!m_sprite_bitmap)
					{

						if (m_pricallback) 
							pdrawgfx_transpen(bitmap,cliprect,machine->gfx[m_gfxregion],
								sprite - multi * inc,
								colour,
								fx,fy,
								x,y + mult * multi,
								machine->priority_bitmap,pri,0);
						else
							drawgfx_transpen(bitmap,cliprect,machine->gfx[m_gfxregion],
								sprite - multi * inc,
								colour,
								fx,fy,
								x,y + mult * multi,
								0);
					
						// double wing uses this flag
						if (xsize)
						{
							if (m_pricallback) 
								pdrawgfx_transpen(bitmap,cliprect,machine->gfx[m_gfxregion],
										(sprite - multi * inc)-mult2,
										colour,
										fx,fy,
										x-16,y + mult * multi,
										machine->priority_bitmap,pri,0);
							else
								drawgfx_transpen(bitmap,cliprect,machine->gfx[m_gfxregion],
										(sprite - multi * inc)-mult2,
										colour,
										fx,fy,
										x-16,y + mult * multi,
										0);
						}
					}
					else
					{
						// if we have a sprite bitmap draw raw data to it for manual mixing
						drawgfx_transpen_raw(m_sprite_bitmap,cliprect,machine->gfx[m_gfxregion],
							sprite - multi * inc,
							colour*0x10,
							fx,fy,
							x,y + mult * multi,
							0);
						if (xsize)
						{	
							drawgfx_transpen_raw(m_sprite_bitmap,cliprect,machine->gfx[m_gfxregion],
								(sprite - multi * inc)-mult2,
								colour*0x10,
								fx,fy,
								x-16,y + mult * multi,
								0);
						}
					}




					multi--;
				}
			}
		}

		offs+=incr;
	}
}


// inefficient, we should be able to mix in a single pass by comparing the existing priority bitmap from the tilemaps
void decospr_device::inefficient_copy_sprite_bitmap(running_machine* machine, bitmap_t *bitmap, const rectangle *cliprect, UINT16 pri, UINT16 priority_mask, UINT16 colbase, UINT16 palmask)
{
	if (!m_sprite_bitmap)
		fatalerror("decospr_device::inefficient_copy_sprite_bitmap with no m_sprite_bitmap");

	int y, x;
	const pen_t *paldata = machine->pens;

	UINT16* srcline;
	UINT32* dstline;

	for (y=cliprect->min_y;y<=cliprect->max_y;y++)
	{
		srcline= BITMAP_ADDR16(m_sprite_bitmap, y, 0);
		dstline= BITMAP_ADDR32(bitmap, y, 0);
	
		for (x=cliprect->min_x;x<=cliprect->max_x;x++)
		{
			UINT16 pix = srcline[x];

			if (pix&0xf)
			{
				if ((pix & priority_mask) ==pri )
				{
					dstline[x] = paldata[(pix&palmask) + colbase];
				}			
			}
		}
	}
}