ifeq ($(wildcard obj/kern/__EMBEDBIN), )
FORCE_EMBEDBIN := 1
else
ifneq ($(shell cat obj/kern/__EMBEDBIN), $(EMBEDBIN_ID))
FORCE_EMBEDBIN := 1
endif
endif

EMBEDBINS_DEPS := $(EMBEDBIN_ID)
ifdef FORCE_EMBEDBIN
EMBEDBINS_DEPS += force-embedbin
endif

ifdef EMBEDBIN_INIT
obj/user/init.init: $(EMBEDBIN_INIT)
	cp $(EMBEDBIN_INIT) $@

ifdef FORCE_EMBEDBIN
.PHONY: obj/user/init.init 

FORCE_EMBEDBIN_DEPS += obj/user/init.init
endif
endif

force-embedbin: $(FORCE_EMBEDBIN_DEPS)
	echo $(EMBEDBIN_ID) > obj/kern/__EMBEDBIN	



