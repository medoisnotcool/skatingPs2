# Skate park for PlayStation 2 (ps2sdk + gsKit).  Build:  make
EE_BIN  = skate_park.elf
EE_OBJS = main.o
EE_LIBS = -lgskit -ldmakit -lpad -lm
# If your gsKit is very old and fails with "gsKit_prim_triangle_gouraud undeclared", uncomment:
# EE_CFLAGS += -DGSKIT_OLD_GORAUD

all: $(EE_BIN)

main.o: main.c math3d.h gfx.h level.h model.h input.h game.h draw.h ui.h

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
