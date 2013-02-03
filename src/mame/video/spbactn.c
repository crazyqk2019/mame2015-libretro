
#include "emu.h"
#include "includes/spbactn.h"


static void blendbitmaps(running_machine &machine,
		bitmap_rgb32 &dest,bitmap_ind16 &src1,bitmap_ind16 &src2,
		const rectangle &cliprect)
{
	int y,x;
	const pen_t *paldata = machine.pens;

	for (y = cliprect.min_y; y <= cliprect.max_y; y++)
	{
		UINT32 *dd  = &dest.pix32(y);
		UINT16 *sd1 = &src1.pix16(y);
		UINT16 *sd2 = &src2.pix16(y);

		for (x = cliprect.min_x; x <= cliprect.max_x; x++)
		{
			if (sd2[x])
			{
				if (sd2[x] & 0x1000)
					dd[x] = paldata[sd1[x] & 0x07ff] + paldata[sd2[x]];
				else
					dd[x] = paldata[sd2[x]];
			}
			else
				dd[x] = paldata[sd1[x]];
		}
	}
}


/* from gals pinball (which was in turn from ninja gaiden) */
static int draw_sprites(running_machine &machine, bitmap_ind16 &bitmap, const rectangle &cliprect, int priority, bool alt_sprites)
{
	static const UINT8 layout[8][8] =
	{
		{ 0, 1, 4, 5,16,17,20,21},
		{ 2, 3, 6, 7,18,19,22,23},
		{ 8, 9,12,13,24,25,28,29},
		{10,11,14,15,26,27,30,31},
		{32,33,36,37,48,49,52,53},
		{34,35,38,39,50,51,54,55},
		{40,41,44,45,56,57,60,61},
		{42,43,46,47,58,59,62,63}
	};

	spbactn_state *state = machine.driver_data<spbactn_state>();
	int count = 0;
	int offs;

	for (offs = (0x1000 - 16) / 2; offs >= 0; offs -= 8)
	{
		int sx, sy, code, color, size, attr, flipx, flipy;
		int col, row;

		attr = state->m_spvideoram[offs];

		int pri = (state->m_spvideoram[offs] & 0x0030);
//		int pri = (state->m_spvideoram[offs+2] & 0x0030);


		if ((attr & 0x0004) &&
			((pri & 0x0030) >> 4) == priority)
		{
			flipx = attr & 0x0001;
			flipy = attr & 0x0002;

			code = state->m_spvideoram[offs + 1];

			if (alt_sprites)
			{
				color = state->m_spvideoram[offs + 0];
			}
			else
			{
				color = state->m_spvideoram[offs + 2];
			}

			size = 1 << (state->m_spvideoram[offs + 2] & 0x0003);               /* 1,2,4,8 */
			color = (color & 0x00f0) >> 4;

			sx = state->m_spvideoram[offs + 4];
			sy = state->m_spvideoram[offs + 3];

			attr &= ~0x0040;                            /* !!! */

			if (attr & 0x0040)
				color |= 0x0180;
			else
				color |= 0x0080;


			for (row = 0; row < size; row++)
			{
				for (col = 0; col < size; col++)
				{
					int x = sx + 8 * (flipx ? (size - 1 - col) : col);
					int y = sy + 8 * (flipy ? (size - 1 - row) : row);

					drawgfx_transpen_raw(bitmap, cliprect, machine.gfx[2],
						code + layout[row][col],
						machine.gfx[2]->colorbase() + color * machine.gfx[2]->granularity(),
						flipx, flipy,
						x, y,
						0);
				}
			}

			count++;
		}
	}

	return count;
}


WRITE16_MEMBER(spbactn_state::bg_videoram_w)
{
	COMBINE_DATA(&m_bgvideoram[offset]);
	m_bg_tilemap->mark_tile_dirty(offset&0x1fff);
}

TILE_GET_INFO_MEMBER(spbactn_state::get_bg_tile_info)
{
	int attr = m_bgvideoram[tile_index];
	int tileno = m_bgvideoram[tile_index+0x2000];
	SET_TILE_INFO_MEMBER(1, tileno, ((attr & 0x00f0)>>4)+0x80, 0);
}


WRITE16_MEMBER(spbactn_state::fg_videoram_w)
{
	COMBINE_DATA(&m_fgvideoram[offset]);
	m_fg_tilemap->mark_tile_dirty(offset&0x1fff);
}

TILE_GET_INFO_MEMBER(spbactn_state::get_fg_tile_info)
{
	int attr = m_fgvideoram[tile_index];
	int tileno = m_fgvideoram[tile_index+0x2000];

	int color = ((attr & 0x00f0)>>4);
	
	/* blending */
	if (attr & 0x0008)
		color += 0x00f0;
	else
		color |= 0x0080;

	SET_TILE_INFO_MEMBER(0, tileno, color, 0);
}



VIDEO_START_MEMBER(spbactn_state,spbactn)
{
	/* allocate bitmaps */
	machine().primary_screen->register_screen_bitmap(m_tile_bitmap_bg);
	machine().primary_screen->register_screen_bitmap(m_tile_bitmap_fg);

	m_bg_tilemap = &machine().tilemap().create(tilemap_get_info_delegate(FUNC(spbactn_state::get_bg_tile_info),this), TILEMAP_SCAN_ROWS, 16, 8, 64, 128);
	m_fg_tilemap = &machine().tilemap().create(tilemap_get_info_delegate(FUNC(spbactn_state::get_fg_tile_info),this), TILEMAP_SCAN_ROWS, 16, 8, 64, 128);
	m_bg_tilemap->set_transparent_pen(0);
	m_fg_tilemap->set_transparent_pen(0);

}

VIDEO_START_MEMBER(spbactn_state,spbactnp)
{
	VIDEO_START_CALL_MEMBER(spbactn);
	// no idea..
	m_extra_tilemap = &machine().tilemap().create(tilemap_get_info_delegate(FUNC(spbactn_state::get_extra_tile_info),this), TILEMAP_SCAN_ROWS, 8, 8, 16, 16);
}
WRITE16_MEMBER( spbactn_state::spbatnp_90002_w )
{
	//printf("spbatnp_90002_w %04x\n",data);
}

WRITE16_MEMBER( spbactn_state::spbatnp_90006_w )
{
	//printf("spbatnp_90006_w %04x\n",data);
}


WRITE16_MEMBER( spbactn_state::spbatnp_9000c_w )
{
	//printf("spbatnp_9000c_w %04x\n",data);
}

WRITE16_MEMBER( spbactn_state::spbatnp_9000e_w )
{
	//printf("spbatnp_9000e_w %04x\n",data);
}

WRITE16_MEMBER( spbactn_state::spbatnp_9000a_w )
{
	//printf("spbatnp_9000a_w %04x\n",data);
}

WRITE16_MEMBER( spbactn_state::spbatnp_90124_w )
{
	//printf("spbatnp_90124_w %04x\n",data);
	m_bg_tilemap->set_scrolly(0, data);

}

WRITE16_MEMBER( spbactn_state::spbatnp_9012c_w )
{
	//printf("spbatnp_9012c_w %04x\n",data);
	m_bg_tilemap->set_scrollx(0, data);
}


WRITE8_MEMBER(spbactn_state::extraram_w)
{
	COMBINE_DATA(&m_extraram[offset]);
	m_extra_tilemap->mark_tile_dirty(offset/2);
}

TILE_GET_INFO_MEMBER(spbactn_state::get_extra_tile_info)
{
	int tileno = m_extraram[(tile_index*2)+1];
	tileno |= m_extraram[(tile_index*2)] << 8;
	SET_TILE_INFO_MEMBER(3, tileno, 0, 0);
}




int spbactn_state::draw_video(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect, bool alt_sprites)
{
	m_tile_bitmap_fg.fill(0, cliprect);

	m_bg_tilemap->draw(m_tile_bitmap_bg, cliprect, TILEMAP_DRAW_OPAQUE, 0);



	if (draw_sprites(machine(), m_tile_bitmap_bg, cliprect, 0, alt_sprites))
	{
		m_bg_tilemap->draw(m_tile_bitmap_bg, cliprect, 0, 0);
	}

	draw_sprites(machine(), m_tile_bitmap_bg, cliprect, 1, alt_sprites);

	m_fg_tilemap->draw(m_tile_bitmap_fg, cliprect, 0, 0);


	draw_sprites(machine(), m_tile_bitmap_fg, cliprect, 2, alt_sprites);
	draw_sprites(machine(), m_tile_bitmap_fg, cliprect, 3, alt_sprites);

	/* mix & blend the tilemaps and sprites into a 32-bit bitmap */
	blendbitmaps(machine(), bitmap, m_tile_bitmap_bg, m_tile_bitmap_fg, cliprect);
	return 0;
}

UINT32 spbactn_state::screen_update_spbactn(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	return draw_video(screen,bitmap,cliprect,false);
}

UINT32 spbactn_state::screen_update_spbactnp(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	// hack to make the extra cpu do something..
	m_extraram2[0x104] = machine().rand();
	m_extraram2[0x105] = machine().rand();

	return draw_video(screen,bitmap,cliprect,true);
}
