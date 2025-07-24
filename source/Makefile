SUBDIRS = classification_data_to_ascii
SUBDIRS += device_config_to_ascii
SUBDIRS += timesignal_data_to_ascii
SUBDIRS += trend_data_to_ascii

all: $(SUBDIRS)
clean: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)


.PHONY: all clean $(SUBDIRS) 
