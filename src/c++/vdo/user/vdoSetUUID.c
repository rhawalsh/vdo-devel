/*
 * %COPYRIGHT%
 *
 * %LICENSE%
 */

#include <err.h>
#include <getopt.h>
#include <uuid/uuid.h>
#include <stdio.h>

#include "errors.h"
#include "logger.h"
#include "memory-alloc.h"

#include "constants.h"
#include "encodings.h"
#include "types.h"
#include "status-codes.h"

#include "userVDO.h"
#include "vdoVolumeUtils.h"

static const char usageString[] = " [options...] vdoBacking";

static const char helpString[] =
  "vdosetuuid - sets a new uuid for the vdo volume stored on a backing\n"
  "             store.\n"
  "\n"
  "SYNOPSIS\n"
  "  vdosetuuid [options] <vdoBacking>\n"
  "\n"
  "DESCRIPTION\n"
  "  vdosetuuid sets a new uuid for the VDO volume stored on the\n"
  "  backing store, whether or not the VDO is running.\n"
  "\n"
  "OPTIONS\n"
  "    --help\n"
  "       Print this help message and exit.\n"
  "\n"
  "    --uuid=<uuid>\n"
  "      Sets the uuid value that is stored in the VDO device. If not\n"
  "      specified, the uuid is randomly generated.\n"
  "\n"
  "    --version\n"
  "       Show the version of the tool.\n"
  "\n";

// N.B. the option array must be in sync with the option string.
static struct option options[] = {
  { "help",                     no_argument,       NULL, 'h' },
  { "uuid",                     required_argument, NULL, 'u' },
  { "version",                  no_argument,       NULL, 'V' },
  { NULL,                       0,                 NULL,  0  },
};
static char optionString[] = "h:u";

static void usage(const char *progname, const char *usageOptionsString)
{
  errx(1, "Usage: %s%s\n", progname, usageOptionsString);
}

uuid_t uuid;

/**
 * Parse the arguments passed; print command usage if arguments are wrong.
 *
 * @param argc  Number of input arguments
 * @param argv  Array of input arguments
 *
 * @return The backing store of the VDO
 **/
static const char *processArgs(int argc, char *argv[])
{
  int c, result;
  while ((c = getopt_long(argc, argv, optionString, options, NULL)) != -1) {
    switch (c) {
    case 'h':
      printf("%s", helpString);
      exit(0);

    case 'u':
      result = uuid_parse(optarg, uuid);
      if (result != VDO_SUCCESS) {
        usage(argv[0], usageString);
      }
      break;

    case 'V':
      fprintf(stdout, "vdosetuuid version is: %s\n", CURRENT_VERSION);
      exit(0);
      break;

    default:
      usage(argv[0], usageString);
      break;
    }
  }

  // Explain usage and exit
  if (optind != (argc - 1)) {
    usage(argv[0], usageString);
  }

  return argv[optind++];
}

/**********************************************************************/
int main(int argc, char *argv[])
{
  static char errBuf[UDS_MAX_ERROR_MESSAGE_SIZE];

  int result = vdo_register_status_codes();
  if (result != VDO_SUCCESS) {
    errx(1, "Could not register status codes: %s",
	 uds_string_error(result, errBuf, UDS_MAX_ERROR_MESSAGE_SIZE));
  }

  // Generate a uuid as a default value in case the options is not specified.
  uuid_generate(uuid);

  const char *vdoBacking = processArgs(argc, argv);

  UserVDO *vdo;
  result = makeVDOFromFile(vdoBacking, false, &vdo);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not load VDO from '%s'", vdoBacking);
  }

  struct volume_geometry geometry;
  result = loadVolumeGeometry(vdo->layer, &geometry);
  if (result != VDO_SUCCESS) {
    freeVDOFromFile(&vdo);
    errx(1, "Could not load the geometry from '%s'", vdoBacking);
  }

  uuid_copy(geometry.uuid, uuid);

  result = writeVolumeGeometry(vdo->layer, &geometry);
  if (result != VDO_SUCCESS) {
    freeVDOFromFile(&vdo);
    errx(1, "Could not write the geometry to '%s' %s", vdoBacking,
	 uds_string_error(result, errBuf, UDS_MAX_ERROR_MESSAGE_SIZE));
  }

  freeVDOFromFile(&vdo);

  exit(0);
}
