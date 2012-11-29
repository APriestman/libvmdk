/*
 * Mounts a VMware Virtual Disk (VMDK) file
 *
 * Copyright (C) 2009-2012, Joachim Metz <joachim.metz@gmail.com>
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

#include <common.h>
#include <file_stream.h>
#include <memory.h>
#include <types.h>

#if defined( HAVE_ERRNO_H )
#include <errno.h>
#endif

#if defined( HAVE_UNISTD_H )
#include <unistd.h>
#endif

#if defined( HAVE_STDLIB_H ) || defined( WINAPI )
#include <stdlib.h>
#endif

#if defined( HAVE_LIBFUSE ) || defined( HAVE_LIBOSXFUSE )
#define FUSE_USE_VERSION	26

#if defined( HAVE_LIBFUSE )
#include <fuse.h>

#elif defined( HAVE_LIBOSXFUSE )
#include <osxfuse/fuse.h>
#endif

#endif /* defined( HAVE_LIBFUSE ) || defined( HAVE_LIBOSXFUSE ) */

#include "mount_handle.h"
#include "vmdkoutput.h"
#include "vmdktools_libcerror.h"
#include "vmdktools_libclocale.h"
#include "vmdktools_libcnotify.h"
#include "vmdktools_libcstring.h"
#include "vmdktools_libcsystem.h"
#include "vmdktools_libvmdk.h"

mount_handle_t *vmdkmount_mount_handle = NULL;
int vmdkmount_abort                    = 0;

/* Prints the executable usage information
 */
void usage_fprint(
      FILE *stream )
{
	if( stream == NULL )
	{
		return;
	}
	fprintf( stream, "Use vmdkmount to mount the VMware Virtual Disk (VMDK)\n"
                         "image file\n\n" );

	fprintf( stream, "Usage: vmdkmount [ -X extended_options ] [ -hvV ]\n"
	                 "                 vmdk_file mount_point\n\n" );

	fprintf( stream, "\tvmdk_file:   the VMDK image file\n\n" );
	fprintf( stream, "\tmount_point: the directory to serve as mount point\n\n" );

	fprintf( stream, "\t-h:          shows this help\n" );
	fprintf( stream, "\t-v:          verbose output to stderr\n"
	                 "\t             vmdkmount will remain running in the foregroud\n" );
	fprintf( stream, "\t-V:          print version\n" );
	fprintf( stream, "\t-X:          extended options to pass to sub system\n" );
}

/* Signal handler for vmdkmount
 */
void vmdkmount_signal_handler(
      libcsystem_signal_t signal LIBCSYSTEM_ATTRIBUTE_UNUSED )
{
	libcerror_error_t *error = NULL;
	static char *function   = "vmdkmount_signal_handler";

	LIBCSYSTEM_UNREFERENCED_PARAMETER( signal )

	vmdkmount_abort = 1;

	if( vmdkmount_mount_handle != NULL )
	{
		if( mount_handle_signal_abort(
		     vmdkmount_mount_handle,
		     &error ) != 1 )
		{
			libcnotify_printf(
			 "%s: unable to signal mount handle to abort.\n",
			 function );

			libcnotify_print_error_backtrace(
			 error );
			libcerror_error_free(
			 &error );
		}
	}
	/* Force stdin to close otherwise any function reading it will remain blocked
	 */
	if( libcsystem_file_io_close(
	     0 ) != 0 )
	{
		libcnotify_printf(
		 "%s: unable to close stdin.\n",
		 function );
	}
}

#if defined( HAVE_LIBFUSE ) || defined( HAVE_LIBOSXFUSE )

#if ( SIZEOF_OFF_T != 8 ) && ( SIZEOF_OFF_T != 4 )
#error Size of off_t not supported
#endif

static char *vmdkmount_fuse_path_prefix         = "/vmdk";
static size_t vmdkmount_fuse_path_prefix_length = 5;

/* Opens a file
 * Returns 0 if successful or a negative errno value otherwise
 */
int vmdkmount_fuse_open(
     const char *path,
     struct fuse_file_info *file_info )
{
	libcerror_error_t *error = NULL;
	static char *function   = "vmdkmount_fuse_open";
	size_t path_length      = 0;
	int result              = 0;

	if( path == NULL )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid path.",
		 function );

		result = -EINVAL;

		goto on_error;
	}
	if( file_info == NULL )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid file info.",
		 function );

		result = -EINVAL;

		goto on_error;
	}
	path_length = libcstring_narrow_string_length(
	               path );

	if( ( path_length <= vmdkmount_fuse_path_prefix_length )
	 || ( libcstring_narrow_string_compare(
	       path,
	       vmdkmount_fuse_path_prefix,
	       vmdkmount_fuse_path_prefix_length ) != 0 ) )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported path.",
		 function );

		result = -ENOENT;

		goto on_error;
	}
	if( ( file_info->flags & 0x03 ) != O_RDONLY )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_UNSUPPORTED_VALUE,
		 "%s: write access currently not supported.",
		 function );

		result = -EACCES;

		goto on_error;
	}
	return( 0 );

on_error:
	if( error != NULL )
	{
		libcnotify_print_error_backtrace(
		 error );
		libcerror_error_free(
		 &error );
	}
	return( result );
}

/* Reads a buffer of data at the specified offset
 * Returns number of bytes read if successful or a negative errno value otherwise
 */
int vmdkmount_fuse_read(
     const char *path,
     char *buffer,
     size_t size,
     off_t offset,
     struct fuse_file_info *file_info )
{
	libcerror_error_t *error = NULL;
	static char *function   = "vmdkmount_fuse_read";
	size_t path_length      = 0;
	ssize_t read_count      = 0;
	int input_handle_index  = 0;
	int result              = 0;
	int string_index        = 0;

	if( path == NULL )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid path.",
		 function );

		result = -EINVAL;

		goto on_error;
	}
	if( size > (size_t) INT_MAX )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_VALUE_EXCEEDS_MAXIMUM,
		 "%s: invalid size value exceeds maximum.",
		 function );

		result = -EINVAL;

		goto on_error;
	}
	if( file_info == NULL )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid file info.",
		 function );

		result = -EINVAL;

		goto on_error;
	}
	path_length = libcstring_narrow_string_length(
	               path );

	if( ( path_length <= vmdkmount_fuse_path_prefix_length )
	 || ( path_length > ( vmdkmount_fuse_path_prefix_length + 3 ) )
	 || ( libcstring_narrow_string_compare(
	       path,
	       vmdkmount_fuse_path_prefix,
	       vmdkmount_fuse_path_prefix_length ) != 0 ) )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported path.",
		 function );

		result = -ENOENT;

		goto on_error;
	}
	string_index = vmdkmount_fuse_path_prefix_length;

	input_handle_index = path[ string_index++ ] - '0';

	if( string_index < (int) path_length )
	{
		input_handle_index *= 10;
		input_handle_index += path[ string_index++ ] - '0';
	}
	if( string_index < (int) path_length )
	{
		input_handle_index *= 10;
		input_handle_index += path[ string_index++ ] - '0';
	}
	input_handle_index -= 1;

	if( mount_handle_seek_offset(
	     vmdkmount_mount_handle,
	     input_handle_index,
	     (off64_t) offset,
	     SEEK_SET,
	     &error ) == -1 )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_SEEK_FAILED,
		 "%s: unable to seek offset in mount handle.",
		 function );

		result = -EIO;

		goto on_error;
	}
	read_count = mount_handle_read_buffer(
	              vmdkmount_mount_handle,
	              input_handle_index,
	              (uint8_t *) buffer,
	              size,
	              &error );

	if( read_count == -1 )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_IO,
		 LIBCERROR_IO_ERROR_READ_FAILED,
		 "%s: unable to read from mount handle.",
		 function );

		result = -EIO;

		goto on_error;
	}
	return( (int) read_count );

on_error:
	if( error != NULL )
	{
		libcnotify_print_error_backtrace(
		 error );
		libcerror_error_free(
		 &error );
	}
	return( result );
}

/* Reads a directory
 * Returns 0 if successful or a negative errno value otherwise
 */
int vmdkmount_fuse_readdir(
     const char *path,
     void *buffer,
     fuse_fill_dir_t filler,
     off_t offset LIBCSYSTEM_ATTRIBUTE_UNUSED,
     struct fuse_file_info *file_info LIBCSYSTEM_ATTRIBUTE_UNUSED )
{
	char vmdkmount_fuse_path[ 10 ];

	libcerror_error_t *error    = NULL;
	static char *function       = "vmdkmount_fuse_readdir";
	size_t path_length          = 0;
	int input_handle_index      = 0;
	int number_of_input_handles = 0;
	int result                  = 0;
	int string_index            = 0;

	LIBCSYSTEM_UNREFERENCED_PARAMETER( offset )
	LIBCSYSTEM_UNREFERENCED_PARAMETER( file_info )

	if( path == NULL )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid path.",
		 function );

		result = -EINVAL;

		goto on_error;
	}
	path_length = libcstring_narrow_string_length(
	               path );

	if( ( path_length != 1 )
	 || ( path[ 0 ] != '/' ) )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported path.",
		 function );

		result = -ENOENT;

		goto on_error;
	}
	if( memory_copy(
	     vmdkmount_fuse_path,
	     vmdkmount_fuse_path_prefix,
	     vmdkmount_fuse_path_prefix_length ) == NULL )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_COPY_FAILED,
		 "%s: unable to copy fuse path prefix.",
		 function );

		result = -errno;

		goto on_error;
	}
	if( mount_handle_get_number_of_input_handles(
	     vmdkmount_mount_handle,
	     &number_of_input_handles,
	     &error ) != 1 )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve number of input handles.",
		 function );

		result = -EIO;

		goto on_error;
	}
	if( ( number_of_input_handles < 0 )
	 || ( number_of_input_handles > 99 ) )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported number of input handles.",
		 function );

		result = -ENOENT;

		goto on_error;
	}
	if( filler(
	     buffer,
	     ".",
	     NULL,
	     0 ) == 1 )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to set directory entry.",
		 function );

		result = -EIO;

		goto on_error;
	}
	if( filler(
	     buffer,
	     "..",
	     NULL,
	     0 ) == 1 )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_RUNTIME,
		 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to set directory entry.",
		 function );

		result = -EIO;

		goto on_error;
	}
	for( input_handle_index = 1;
	     input_handle_index <= number_of_input_handles;
	     input_handle_index++ )
	{
		string_index = vmdkmount_fuse_path_prefix_length;

		if( input_handle_index >= 100 )
		{
			vmdkmount_fuse_path[ string_index++ ] = '0' + (char) ( input_handle_index / 100 );
		}
		if( input_handle_index >= 10 )
		{
			vmdkmount_fuse_path[ string_index++ ] = '0' + (char) ( input_handle_index / 10 );
		}
		vmdkmount_fuse_path[ string_index++ ] = '0' + (char) ( input_handle_index % 10 );
		vmdkmount_fuse_path[ string_index++ ] = 0;

		if( filler(
		     buffer,
		     &( vmdkmount_fuse_path[ 1 ] ),
		     NULL,
		     0 ) == 1 )
		{
			libcerror_error_set(
			 &error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to set directory entry.",
			 function );

			result = -EIO;

			goto on_error;
		}
	}
	return( 0 );

on_error:
	if( error != NULL )
	{
		libcnotify_print_error_backtrace(
		 error );
		libcerror_error_free(
		 &error );
	}
	return( result );
}

/* Retrieves the file stat info
 * Returns 0 if successful or a negative errno value otherwise
 */
int vmdkmount_fuse_getattr(
     const char *path,
     struct stat *stat_info )
{
	libcerror_error_t *error = NULL;
	static char *function   = "vmdkmount_fuse_getattr";
	size64_t media_size     = 0;
	size_t path_length      = 0;
	int input_handle_index  = 0;
	int result              = -ENOENT;
	int string_index        = 0;

	if( path == NULL )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid path.",
		 function );

		result = -EINVAL;

		goto on_error;
	}
	if( stat_info == NULL )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBCERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid stat info.",
		 function );

		result = -EINVAL;

		goto on_error;
	}
	if( memory_set(
	     stat_info,
	     0,
	     sizeof( struct stat ) ) == NULL )
	{
		libcerror_error_set(
		 &error,
		 LIBCERROR_ERROR_DOMAIN_MEMORY,
		 LIBCERROR_MEMORY_ERROR_SET_FAILED,
		 "%s: unable to clear stat info.",
		 function );

		result = errno;

		goto on_error;
	}
	path_length = libcstring_narrow_string_length(
	               path );

	if( path_length == 1 )
	{
		if( path[ 0 ] == '/' )
		{
			stat_info->st_mode  = S_IFDIR | 0755;
			stat_info->st_nlink = 2;

			result = 0;
		}
	}
	else if( ( path_length > vmdkmount_fuse_path_prefix_length )
	      && ( path_length <= ( vmdkmount_fuse_path_prefix_length + 3 ) ) )
	{
		if( libcstring_narrow_string_compare(
		     path,
		     vmdkmount_fuse_path_prefix,
		     vmdkmount_fuse_path_prefix_length ) == 0 )
		{
			string_index = vmdkmount_fuse_path_prefix_length;

			input_handle_index = path[ string_index++ ] - '0';

			if( string_index < (int) path_length )
			{
				input_handle_index *= 10;
				input_handle_index += path[ string_index++ ] - '0';
			}
			if( string_index < (int) path_length )
			{
				input_handle_index *= 10;
				input_handle_index += path[ string_index++ ] - '0';
			}
			input_handle_index -= 1;

			stat_info->st_mode  = S_IFREG | 0444;
			stat_info->st_nlink = 1;

			if( mount_handle_get_media_size(
			     vmdkmount_mount_handle,
			     input_handle_index,
			     &media_size,
			     &error ) != 1 )
			{
				libcerror_error_set(
				 &error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_GET_FAILED,
				 "%s: unable to retrieve media size.",
				 function );

				result = -EIO;

				goto on_error;
			}
#if SIZEOF_OFF_T == 4
			if( media_size > (size64_t) UINT32_MAX )
			{
				libcerror_error_set(
				 &error,
				 LIBCERROR_ERROR_DOMAIN_RUNTIME,
				 LIBCERROR_RUNTIME_ERROR_VALUE_OUT_OF_BOUNDS,
				 "%s: invalid media size value out of bounds.",
				 function );

				result = -ERANGE;

				goto on_error;
			}
#endif
			stat_info->st_size = (off_t) media_size;

			result = 0;
		}
	}
	if( result == 0 )
	{
		stat_info->st_atime = libcsystem_date_time_time(
		                       NULL );

		stat_info->st_mtime = libcsystem_date_time_time(
		                       NULL );

		stat_info->st_ctime = libcsystem_date_time_time(
		                       NULL );

#if defined( HAVE_GETEUID )
		stat_info->st_uid = geteuid();
#else
		stat_info->st_uid = 0;
#endif
#if defined( HAVE_GETEGID )
		stat_info->st_gid = getegid();
#else
		stat_info->st_gid = 0;
#endif
	}
	return( result );

on_error:
	if( error != NULL )
	{
		libcnotify_print_error_backtrace(
		 error );
		libcerror_error_free(
		 &error );
	}
	return( result );
}

/* Cleans up when fuse is done
 */
void vmdkmount_fuse_destroy(
      void *private_data LIBCSYSTEM_ATTRIBUTE_UNUSED )
{
	libcerror_error_t *error = NULL;
	static char *function    = "vmdkmount_fuse_destroy";

	LIBCSYSTEM_UNREFERENCED_PARAMETER( private_data )

	if( vmdkmount_mount_handle != NULL )
	{
		if( mount_handle_free(
		     &vmdkmount_mount_handle,
		     &error ) != 1 )
		{
			libcerror_error_set(
			 &error,
			 LIBCERROR_ERROR_DOMAIN_RUNTIME,
			 LIBCERROR_RUNTIME_ERROR_FINALIZE_FAILED,
			 "%s: unable to free mount handle.",
			 function );

			goto on_error;
		}
	}
	return;

on_error:
	if( error != NULL )
	{
		libcnotify_print_error_backtrace(
		 error );
		libcerror_error_free(
		 &error );
	}
	return;
}

#endif /* defined( HAVE_LIBFUSE ) || defined( HAVE_LIBOSXFUSE ) */

/* The main program
 */
#if defined( LIBCSTRING_HAVE_WIDE_SYSTEM_CHARACTER )
int wmain( int argc, wchar_t * const argv[] )
#else
int main( int argc, char * const argv[] )
#endif
{
	libcstring_system_character_t * const *source_filenames = NULL;
	libvmdk_error_t *error                                  = NULL;
	libcstring_system_character_t *mount_point              = NULL;
	libcstring_system_character_t *option_extended_options  = NULL;
	char *program                                           = "vmdkmount";
	libcstring_system_integer_t option                      = 0;
	int number_of_source_filenames                          = 0;
	int result                                              = 0;
	int verbose                                             = 0;

#if defined( HAVE_LIBFUSE ) || defined( HAVE_LIBOSXFUSE )
	struct fuse_operations vmdkmount_fuse_operations;

	struct fuse_args vmdkmount_fuse_arguments               = FUSE_ARGS_INIT(0, NULL);
	struct fuse_chan *vmdkmount_fuse_channel                = NULL;
	struct fuse *vmdkmount_fuse_handle                      = NULL;
#endif

	libcnotify_stream_set(
	 stderr,
	 NULL );
	libcnotify_verbose_set(
	 1 );

	if( libclocale_initialize(
             "vmdktools",
	     &error ) != 1 )
	{
		fprintf(
		 stderr,
		 "Unable to initialize locale values.\n" );

		goto on_error;
	}
	if( libcsystem_initialize(
             _IONBF,
             &error ) != 1 )
	{
		fprintf(
		 stderr,
		 "Unable to initialize system values.\n" );

		goto on_error;
	}
	vmdkoutput_version_fprint(
	 stdout,
	 program );

	while( ( option = libcsystem_getopt(
	                   argc,
	                   argv,
	                   _LIBCSTRING_SYSTEM_STRING( "hvVX:" ) ) ) != (libcstring_system_integer_t) -1 )
	{
		switch( option )
		{
			case (libcstring_system_integer_t) '?':
			default:
				fprintf(
				 stderr,
				 "Invalid argument: %" PRIs_LIBCSTRING_SYSTEM "\n",
				 argv[ optind - 1 ] );

				usage_fprint(
				 stdout );

				return( EXIT_FAILURE );

			case (libcstring_system_integer_t) 'h':
				usage_fprint(
				 stdout );

				return( EXIT_SUCCESS );

			case (libcstring_system_integer_t) 'v':
				verbose = 1;

				break;

			case (libcstring_system_integer_t) 'V':
				vmdkoutput_copyright_fprint(
				 stdout );

				return( EXIT_SUCCESS );

			case (libcstring_system_integer_t) 'X':
				option_extended_options = optarg;

				break;
		}
	}
	if( optind == argc )
	{
		fprintf(
		 stderr,
		 "Missing source file(s).\n" );

		usage_fprint(
		 stdout );

		return( EXIT_FAILURE );
	}
	source_filenames           = &( argv[ optind ] );
	number_of_source_filenames = argc - optind - 1;

	if( ( optind + 1 ) == argc )
	{
		fprintf(
		 stderr,
		 "Missing mount point.\n" );

		usage_fprint(
		 stdout );

		return( EXIT_FAILURE );
	}
	mount_point = argv[ argc - 1 ];

	libcnotify_verbose_set(
	 verbose );
	libvmdk_notify_set_stream(
	 stderr,
	 NULL );
	libvmdk_notify_set_verbose(
	 verbose );

	if( mount_handle_initialize(
	     &vmdkmount_mount_handle,
	     &error ) != 1 )
	{
		fprintf(
		 stderr,
		 "Unable to initialize mount handle.\n" );

		goto on_error;
	}
	if( mount_handle_open_input(
	     vmdkmount_mount_handle,
	     source_filenames,
	     number_of_source_filenames,
	     &error ) != 1 )
	{
		fprintf(
		 stderr,
		 "Unable to open source file(s).\n" );

		goto on_error;
	}
#if defined( HAVE_LIBFUSE ) || defined( HAVE_LIBOSXFUSE )
	if( memory_set(
	     &vmdkmount_fuse_operations,
	     0,
	     sizeof( struct fuse_operations ) ) == NULL )
	{
		fprintf(
		 stderr,
		 "Unable to clear fuse operations.\n" );

		goto on_error;
	}
	if( option_extended_options != NULL )
	{
		/* This argument is required but ignored
		 */
		if( fuse_opt_add_arg(
		     &vmdkmount_fuse_arguments,
		     "" ) != 0 )
		{
			fprintf(
			 stderr,
			 "Unable add fuse arguments.\n" );

			goto on_error;
		}
		if( fuse_opt_add_arg(
		     &vmdkmount_fuse_arguments,
		     "-o" ) != 0 )
		{
			fprintf(
			 stderr,
			 "Unable add fuse arguments.\n" );

			goto on_error;
		}
		if( fuse_opt_add_arg(
		     &vmdkmount_fuse_arguments,
		     option_extended_options ) != 0 )
		{
			fprintf(
			 stderr,
			 "Unable add fuse arguments.\n" );

			goto on_error;
		}
	}
	vmdkmount_fuse_operations.open    = &vmdkmount_fuse_open;
	vmdkmount_fuse_operations.read    = &vmdkmount_fuse_read;
	vmdkmount_fuse_operations.readdir = &vmdkmount_fuse_readdir;
	vmdkmount_fuse_operations.getattr = &vmdkmount_fuse_getattr;
	vmdkmount_fuse_operations.destroy = &vmdkmount_fuse_destroy;

	vmdkmount_fuse_channel = fuse_mount(
	                          mount_point,
	                          &vmdkmount_fuse_arguments );

	if( vmdkmount_fuse_channel == NULL )
	{
		fprintf(
		 stderr,
		 "Unable to create fuse channel.\n" );

		goto on_error;
	}
	vmdkmount_fuse_handle = fuse_new(
	                         vmdkmount_fuse_channel,
	                         &vmdkmount_fuse_arguments,
	                         &vmdkmount_fuse_operations,
	                         sizeof( struct fuse_operations ),
	                         vmdkmount_mount_handle );
	               
	if( vmdkmount_fuse_handle == NULL )
	{
		fprintf(
		 stderr,
		 "Unable to create fuse handle.\n" );

		goto on_error;
	}
	if( verbose == 0 )
	{
		if( fuse_daemonize(
		     0 ) != 0 )
		{
			fprintf(
			 stderr,
			 "Unable to daemonize fuse.\n" );

			goto on_error;
		}
	}
	result = fuse_loop(
	          vmdkmount_fuse_handle );

	if( result != 0 )
	{
		fprintf(
		 stderr,
		 "Unable to run fuse loop.\n" );

		goto on_error;
	}
	fuse_destroy(
	 vmdkmount_fuse_handle );

	fuse_opt_free_args(
	 &vmdkmount_fuse_arguments );

	return( EXIT_SUCCESS );
#else
	fprintf(
	 stderr,
	 "No sub system to mount VMDK format.\n" );

	return( EXIT_FAILURE );
#endif

on_error:
	if( error != NULL )
	{
		libcnotify_print_error_backtrace(
		 error );
		libcerror_error_free(
		 &error );
	}
#if defined( HAVE_LIBFUSE ) || defined( HAVE_LIBOSXFUSE )
	if( vmdkmount_fuse_handle != NULL )
	{
		fuse_destroy(
		 vmdkmount_fuse_handle );
	}
	fuse_opt_free_args(
	 &vmdkmount_fuse_arguments );
#endif
	if( vmdkmount_mount_handle != NULL )
	{
		mount_handle_free(
		 &vmdkmount_mount_handle,
		 NULL );
	}
	return( EXIT_FAILURE );
}

