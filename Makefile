# Skate park for PlayStation 2 (ps2sdk + gsKit).  Build:  make
EE_BIN  = skate_park.elf
EE_OBJS = main.o
EE_LIBS = -lgskit -ldmakit -lpad -lm

# gsKit lives in $PS2DEV/gsKit (not in the default ps2sdk include/lib paths)
EE_INCS    += -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include
EE_CFLAGS  += -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include
EE_LDFLAGS += -L$(PS2DEV)/gsKit/lib -L$(PS2SDK)/ports/lib

# If your gsKit is very old and fails with "gsKit_prim_triangle_gouraud undeclared", uncomment:
# EE_CFLAGS += -DGSKIT_OLD_GORAUD

all: $(EE_BIN)

main.o: main.c math3d.h gfx.h level.h model.h input.h game.h draw.h ui.h

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
