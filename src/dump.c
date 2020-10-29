// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/dump.c
 *
 * Dump command functions
 *
 * Copyright (C) 2018-2020 SCANOSS.COM
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

void ldb_dump(struct ldb_table table, int hex_bytes)
{
	/* Read each DB sector */
	uint8_t k0 = 0;
	long total_records = 0;
	setlocale(LC_NUMERIC, "");

	do {
		uint8_t *sector = ldb_load_sector(table, &k0);
		if (sector)
		{
			/* Read each one of the (256 ^ 3) list pointers from the map */
			uint8_t k[LDB_KEY_LN];
			k[0] = k0;
			for (int k1 = 0; k1 < 256; k1++)
				for (int k2 = 0; k2 < 256; k2++)
					for (int k3 = 0; k3 < 256; k3++)
					{
						k[1] = k1;
						k[2] = k2;
						k[3] = k3;
						/* If there is a pointer, read the list */
						if (ldb_map_pointer_pos(k))
						{
							/* Process records */
							ldb_fetch_recordset(sector, table, k, true, ldb_csvprint, &hex_bytes);
						}
					}
			free(sector);
		}
	} while (k0++ < 255);
	fflush(stdout);
}
