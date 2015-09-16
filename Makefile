#-------------------------------------------------------------------------
#
# Makefile for pg_rest
#
#-------------------------------------------------------------------------

# grab name and version from META.json file
EXTENSION = $(shell grep -m 1 '"name":' META.json | sed -e 's/[[:space:]]*"name":[[:space:]]*"\([^"]*\)",/\1/')
EXTVERSION = $(shell grep default_version $(EXTENSION).control | sed -e "s/default_version[[:space:]]*=[[:space:]]*'\([^']*\)'/\1/")

# installation scripts
DATA = $(wildcard updates/*--*.sql)

# documentation and executables
DOCS = $(wildcard doc/*.md)
SCRIPTS = $(wildcard bin/*)

# compilation configuration
MODULE_big = $(EXTENSION)
OBJS = $(patsubst %.c,%.o,$(wildcard src/*.c))
PG_CPPFLAGS = -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter -Iinclude
EXTRA_CLEAN += $(addprefix src/,*.gcno *.gcda) # clean up after profiling runs

# test configuration
TESTS = $(wildcard test/sql/*.sql)
REGRESS = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --inputdir=test --load-language=plpgsql
REGRESS_OPTS += --launcher=./test/launcher.sh # use custom launcher for tests

# add coverage flags if requested
ifeq ($(enable_coverage),yes)
PG_CPPFLAGS += --coverage
SHLIB_LINK += --coverage
endif

# be explicit about the default target
all:

# delegate to subdirectory makefiles as needed
include sql/Makefile
include test/Makefile

# detect whether to build with pgxs or build in-tree
ifndef NO_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_rest
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

# ensure MAJORVERSION is defined (missing in older versions)
ifndef MAJORVERSION
MAJORVERSION := $(basename $(VERSION))
endif

# if using a version older than PostgreSQL 9.3, abort
PG93 = $(shell echo $(MAJORVERSION) | grep -qE "8\.|9\.[012]" && echo no || echo yes)
ifeq ($(PG93),no)
$(error PostgreSQL 9.3 or higher is required to compile this extension)
endif
