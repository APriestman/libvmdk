/* 
 * Array type functions
 *
 * Copyright (c) 2010, Joachim Metz <jbmetz@users.sourceforge.net>
 *
 * Refer to AUTHORS for acknowledgements.
 *
 * This software is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined( _LIBVMDK_ARRAY_TYPE_H )
#define _LIBVMDK_ARRAY_TYPE_H

#include <common.h>
#include <types.h>

#include <liberror.h>

#if defined( __cplusplus )
extern "C" {
#endif

typedef struct libvmdk_array libvmdk_array_t;

struct libvmdk_array
{
	/* The amount of entries
	 */
	int amount_of_entries;

	/* The entries
	 */
	intptr_t **entry;
};

int libvmdk_array_initialize(
     libvmdk_array_t **array,
     int amount_of_entries,
     liberror_error_t **error );

int libvmdk_array_free(
     libvmdk_array_t **array,
     int (*entry_free_function)(
            intptr_t *entry,
            liberror_error_t **error ),
     liberror_error_t **error );

int libvmdk_array_resize(
     libvmdk_array_t *array,
     int amount_of_entries,
     liberror_error_t **error );

int libvmdk_array_get_amount_of_entries(
     libvmdk_array_t *array,
     int *amount_of_entries,
     liberror_error_t **error );

int libvmdk_array_get_entry(
     libvmdk_array_t *array,
     int entry_index,
     intptr_t **entry,
     liberror_error_t **error );

int libvmdk_array_set_entry(
     libvmdk_array_t *array,
     int entry_index,
     intptr_t *entry,
     liberror_error_t **error );

int libvmdk_array_append_entry(
     libvmdk_array_t *array,
     int *entry_index,
     intptr_t *entry,
     liberror_error_t **error );

#if defined( __cplusplus )
}
#endif

#endif

