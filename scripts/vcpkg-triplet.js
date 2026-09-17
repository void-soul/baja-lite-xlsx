'use strict';

/**
 * Prints the vcpkg triplet for the current platform/arch (used by
 * binding.gyp and the CI workflows).
 */

const { tripletFor } = require('./lib/triplet');

process.stdout.write(tripletFor());
