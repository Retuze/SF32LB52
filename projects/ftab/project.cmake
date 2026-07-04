# ftab — Flash partition table generator
#
# This is a HOST tool (not embedded firmware), so it has no PROJECT_LIBS or PLATFORM.
# The CMakeLists.txt handles host compilation directly.

# No libraries needed — this is a standalone host executable
set(PROJECT_LIBS "")

# No platform — this runs on the build host
