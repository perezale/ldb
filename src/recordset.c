// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/recordset.c
 *
 * LDB recordset reading functions
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

/**
  * @file recordset.c
  * @date 12 Jul 2020
  * @brief Contains functions to read records from a LDB recordset
 
  * //TODO Long description
  * @see https://github.com/scanoss/ldb/blob/master/src/recordset.c
  */

/**
 * @brief Recurses all records in *table* for *key* and calls the provided handler funcion in each iteration, passing
 * subkey, subkey length, fetched data, length and iteration number. This function acts on the .ldb for the
 * provided *key*, but can also work from memory, if a pointer to a *sector* is provided (not NULL)
 * 
 * @param sector Optional: Pointer to a LDB sector allocated in memory. If NULL the function will use tha table struct and key to open the ldb
 * @param table table struct config
 * @param key key of the associated table
 * @param skip_subkey true for skip the subkey
 * @param ldb_record_handler Handler to print the data
 * @param void_ptr This pointer is passed to the handler function
 * @return uint32_t The number of records found
 */
uint32_t ldb_fetch_recordset(uint8_t *sector, struct ldb_table table, uint8_t* key, bool skip_subkey, bool (*ldb_record_handler) (uint8_t *, uint8_t *, int, uint8_t *, uint32_t, int, void *), void *void_ptr)
{
	FILE *ldb_sector = NULL;
	uint8_t *node;

	/* Open sector from disk (if *sector is not provided) */
	if (sector) node = sector;
	else
	{
		ldb_sector = ldb_open(table, key, "r+");
		if (!ldb_sector) return 0;
		node = calloc(LDB_MAX_REC_LN + 1, 1);
	}

	uint64_t next = 0;
	uint32_t node_size = 0;
	uint32_t node_ptr;
	uint8_t subkey_ln = table.key_ln - LDB_KEY_LN;

	uint32_t records = 0;
	bool done = false;

	do
	{
		/* Read node */
		next = ldb_node_read(sector, table, ldb_sector, next, key, &node_size, &node, 0);
		if (!node_size && !next) break; // reached end of list

		/* Pass entire node (fixed record length) to handler */
		if (table.rec_ln) done = ldb_record_handler(key, NULL, 0 , node, node_size, records++, void_ptr);

		/* Extract and pass variable-size records to handler */
		else
		{
			if (!ldb_validate_node(node, node_size, subkey_ln)) continue;

			/* Extract datasets from node */
			node_ptr = 0;

			while (node_ptr < node_size && !done)
			{
				/* Get subkey */
				uint8_t *subkey = node + node_ptr;
				node_ptr += subkey_ln;

				/* Get recordset length */
				int dataset_size = uint16_read(node + node_ptr);
				node_ptr += 2;

				/* Compare subkey */
				bool key_matched = true;
				if (!skip_subkey) if (subkey_ln) key_matched = (memcmp(subkey, key + 4, subkey_ln) == 0);

				if (key_matched)
				{
					/* Extract records from dataset */
					uint32_t dataset_ptr = 0;
					while (dataset_ptr < dataset_size)
					{
						uint8_t *dataset = node + node_ptr;

						/* Get record length */
						int record_size = uint16_read(dataset + dataset_ptr);
						dataset_ptr += 2;

						/* We drop records longer than the desired limit */
						if (record_size + 32 < LDB_MAX_REC_LN)
							done = ldb_record_handler(key, subkey, subkey_ln, dataset + dataset_ptr, record_size, records++, void_ptr);

						/* Move pointer to end of record */
						dataset_ptr += record_size;
					}
				}
				/* Move pointer to end of dataset */
				node_ptr += dataset_size;
			}
		}
	} while (next && !done);

	if (!sector)
	{
		free(node);
		fclose(ldb_sector);
	}

	return records;
}

/**
 * @brief Handler function for ldb_get_first_record
 * 
 * @param key Not used
 * @param subkey Not used
 * @param subkey_ln Not used
 * @param data Source for the record
 * @param datalen Length of the record
 * @param iteration Not used
 * @param ptr Buffer where the record is written
 * @return true if datalen is > 0. False otherwise
 */
bool ldb_get_first_record_handler(uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t datalen, int iteration, void *ptr)
{
	uint8_t *record = (uint8_t *) ptr;
	if (datalen)
	{
		uint32_write(record, datalen);
		memcpy(record + 4, data, datalen);
		return true;
	}
	return false;
}

/**
 * @brief Return the first record for the given table/key
 * 
 * @param table Struct with the config of the table
 * @param key Key of the table
 * @param void_ptr Pointer passed to the handler function
 */
void ldb_get_first_record(struct ldb_table table, uint8_t* key, void *void_ptr)
{
	ldb_fetch_recordset(NULL, table, key, false, ldb_get_first_record_handler, void_ptr);
}

/**
 * @brief Handler function for ldb_key_exists
 * 
 * @param key Not used
 * @param subkey Not used
 * @param subkey_ln Not used
 * @param data Not used
 * @param datalen Not used
 * @param iteration Not used
 * @param ptr Not used
 * @return true always (the key exists) 
 */
bool ldb_key_exists_handler(uint8_t *key, uint8_t *subkey, int subkey_ln, uint8_t *data, uint32_t datalen, int iteration, void *ptr)
{
	return true;
}

/**
 * @brief Returns true if there is at least a record for the "key" in the "table"
 * 
 * @param table Struct with the config of the table
 * @param key Key of the table
 * @return true if there is at least a record for the "key" in the "table"
 */
bool ldb_key_exists(struct ldb_table table, uint8_t *key)
{
	return (ldb_fetch_recordset(NULL, table, key, false, ldb_key_exists_handler, NULL) > 0);
}

